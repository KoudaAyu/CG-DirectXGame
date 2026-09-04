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

    // 1. 洋梨・富士山・お餅型 下膨らみ変形（Gravity Sag: 下のほうが圧倒的に大きくどっしり広がる）
    // y が高いほど細く（最上部: ~0.72）、y が低いほど横に超ワイド（最下部: ~2.3倍）に広がる
    float heightRatio = saturate((1.0f - p.y) * 0.5f); // 0.0(最上部) -> 1.0(最下部)
    float sagFactor = 0.72f + heightRatio * 0.65f + (heightRatio * heightRatio) * 0.95f;

    float3 def = p;
    def.x *= sagFactor;
    def.z *= sagFactor;
    // 重力による全体の沈み込み（高さを圧縮してお餅のようにどっしり座らせる）
    def.y = p.y * 0.72f - 0.16f;

    // 2. 傾斜・重力による内容物の流動と偏り（Fluid Mass Shift: 傾けた下り坂側がさらに巨大に広がる）
    float3 squash = gSlimeParams.squashStretch;
    float2 flowVec = float2(squash.x, squash.z);
    float flowMag = length(flowVec);

    if (flowMag > 0.001f)
    {
        float2 flowDir = flowVec / flowMag;
        // 頂点の傾斜方向成分（+1: 傾斜の下側・前, -1: 傾斜の上側・後ろ）
        float forwardDot = dot(p.xz, flowDir);
        // 下部ほど、かつ傾斜下側ほど、ドサッと大きく水が溜まる
        float bottomWeight = saturate((1.3f - p.y) * 0.70f);
        float frontBias = 1.0f + forwardDot * 0.90f;

        // 傾斜下側への重心移動と大膨らみ
        float shiftDist = flowMag * bottomWeight * frontBias * 0.90f;
        def.x += flowDir.x * shiftDist;
        def.z += flowDir.y * shiftDist;

        // 傾斜下側（前）に水が溜まってプックリ巨大に膨らみ、反対側（後ろ）は萎む
        if (forwardDot > -0.2f)
        {
            float bulge = saturate(forwardDot + 0.2f) * flowMag * 0.60f * bottomWeight;
            def.x += flowDir.x * bulge;
            def.z += flowDir.y * bulge;
            // 横（側方）にも水が溜まってプクッと広がる
            float2 sideDir = float2(-flowDir.y, flowDir.x);
            def.xz += sideDir * (dot(p.xz, sideDir) * bulge * 0.8f);
        }
        else
        {
            // 後ろ側は中身が抜けてペタンコに萎む
            float shrink = saturate(-forwardDot - 0.2f) * flowMag * 0.45f;
            def.xz -= flowDir * shrink;
            def.y *= (1.0f - shrink * 0.40f);
        }
    }

    // 3. 体積保存スクワッシュ（上下の全体的な潰れ）
    float volumeCompY = 1.0f + squash.y;
    float volumeCompXZ = 1.0f;
    if (abs(volumeCompY) > 0.01f)
    {
        volumeCompXZ = 1.0f / sqrt(abs(volumeCompY));
    }
    def.y *= volumeCompY;
    def.x *= volumeCompXZ;
    def.z *= volumeCompXZ;

    // 4. 底面の絶対接地平坦化（Ground Flattening: 床に沿って底面が広くペタッと完全密着）
    if (def.y < -0.42f)
    {
        float flattenRate = saturate((-0.42f - def.y) / 0.45f);
        def.y = lerp(def.y, -0.65f, flattenRate * 0.92f);
        def.x *= (1.0f + flattenRate * 0.40f);
        def.z *= (1.0f + flattenRate * 0.40f);
    }

    // 法線方向へ膨らませる
    def += n * wobbleOffset;

    // 6. 衝撃波紋（Impulse Ripple）
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
