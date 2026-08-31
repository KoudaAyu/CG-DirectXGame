#include "Resources/shaders/Slime.hlsli"

// 変換行列
struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 WorldInverseTranspose;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// スライムパラメータ (b1)
ConstantBuffer<SlimeParams> gSlimeParams : register(b1);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal   : NORMAL0;
};

// --- ノイズ関数 ---
float hash(float3 p)
{
    float h = dot(p, float3(127.1f, 311.7f, 74.7f));
    return frac(sin(h) * 43758.5453f);
}

float noise3D(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0f - 2.0f * f); // smoothstep

    float n000 = hash(i);
    float n100 = hash(i + float3(1, 0, 0));
    float n010 = hash(i + float3(0, 1, 0));
    float n110 = hash(i + float3(1, 1, 0));
    float n001 = hash(i + float3(0, 0, 1));
    float n101 = hash(i + float3(1, 0, 1));
    float n011 = hash(i + float3(0, 1, 1));
    float n111 = hash(i + float3(1, 1, 1));

    float x0 = lerp(n000, n100, f.x);
    float x1 = lerp(n010, n110, f.x);
    float x2 = lerp(n001, n101, f.x);
    float x3 = lerp(n011, n111, f.x);

    float y0 = lerp(x0, x1, f.y);
    float y1 = lerp(x2, x3, f.y);

    return lerp(y0, y1, f.z);
}

// --- 頂点変形関数（有限差分法による法線計算のために共通化） ---
float3 CalculateDeformedPosition(float3 p, float3 n)
{
    float time = gSlimeParams.time;
    float wobbleStr = gSlimeParams.wobbleStrength;
    float wobbleFreq = gSlimeParams.wobbleFrequency;

    // 1. ぶよぶよ波打ち変形（マルチ周波数サイン波 + 3Dノイズ）
    float wave1 = sin(p.x * wobbleFreq + time * 3.5f) *
                  cos(p.z * wobbleFreq * 0.8f + time * 2.7f);
    float wave2 = sin(p.y * wobbleFreq * 1.3f + time * 4.2f) *
                  cos(p.x * wobbleFreq * 0.6f + time * 1.8f);
    float wave3 = noise3D(p * wobbleFreq * 0.5f + time * 1.5f) * 2.0f - 1.0f;

    float wobbleOffset = (wave1 * 0.45f + wave2 * 0.35f + wave3 * 0.20f) * wobbleStr;

    // 2. スクワッシュ＆ストレッチ（慣性による体積保存変形）
    float3 squash = gSlimeParams.squashStretch;
    float volumeCompY = 1.0f + squash.y;
    float volumeCompXZ = 1.0f;
    if (abs(volumeCompY) > 0.01f)
    {
        volumeCompXZ = 1.0f / sqrt(abs(volumeCompY));
    }

    float3 def = p;
    def.x *= (volumeCompXZ + squash.x);
    def.y *= volumeCompY;
    def.z *= (volumeCompXZ + squash.z);

    // 法線方向へ膨らませる
    def += n * wobbleOffset;

    // 3. 衝撃波紋（Impulse Ripple）
    float impulse = gSlimeParams.impulseStrength;
    if (impulse > 0.001f)
    {
        float distFromBottom = saturate((p.y + 1.0f) * 0.5f);
        float ripple = sin(distFromBottom * 12.0f - time * 15.0f) * impulse;
        ripple *= exp(-distFromBottom * 3.0f);
        def += n * ripple;
    }

    return def;
}

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    float3 localPos = input.position.xyz;
    float3 normal = normalize(input.normal);

    // 中心位置の変形
    float3 deformedPos = CalculateDeformedPosition(localPos, normal);

    // --- 正確な法線計算（Tangent / Bitangent 有限差分法） ---
    // normal に直交する接線ベクトルを安定して生成
    float3 c1 = cross(normal, float3(0.0f, 0.0f, 1.0f));
    float3 c2 = cross(normal, float3(0.0f, 1.0f, 0.0f));
    float3 tangent = normalize(length(c1) > length(c2) ? c1 : c2);
    float3 bitangent = cross(normal, tangent);

    float eps = 0.008f;
    float3 pT = localPos + tangent * eps;
    float3 pB = localPos + bitangent * eps;

    // 球面上の近傍点における元の法線
    float3 nT = normalize(pT);
    float3 nB = normalize(pB);

    float3 defT = CalculateDeformedPosition(pT, nT);
    float3 defB = CalculateDeformedPosition(pB, nB);

    // 変形後の接線ベクトル
    float3 dPosT = defT - deformedPos;
    float3 dPosB = defB - deformedPos;

    // 外積により変形曲面の正確で滑らかな法線を算出
    float3 deformedNormal = normalize(cross(dPosT, dPosB));
    if (dot(deformedNormal, normal) < 0.0f)
    {
        deformedNormal = -deformedNormal;
    }

    // 出力
    float4 worldPos = mul(float4(deformedPos, 1.0f), gTransformationMatrix.World);
    output.position = mul(float4(deformedPos, 1.0f), gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(deformedNormal, (float32_t3x3)gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = worldPos.xyz;
    output.deformAmount = length(deformedPos - localPos);

    return output;
}
