#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    int32_t specularModel;
    float32_t reflectionFactor; // time (経過時間)
    float32_t fresnelF0;        // mode: 0.0=3Dボリュメトリック光条コーン, 1.0=地上コースティクス投光スポット, 2.0=3D地上パルス波紋
    float32_t4x4 uvTransform;
    float32_t shininess;        // 補助パラメータ (波紋進行度・減衰係数)
    float32_t3 padding2;
};

ConstantBuffer<Material> gMaterial : register(b0);

struct Camera
{
    float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// 軽量・高品質な3Dプロシージャル大気乱流ノイズ（ミー散乱・煙塵の濃淡表現用）
float32_t Hash31(float32_t3 p)
{
    p = frac(p * float32_t3(0.1031f, 0.1030f, 0.0973f));
    p += dot(p, p.yxz + 33.33f);
    return frac((p.x + p.y) * p.z);
}

float32_t Noise3D(float32_t3 x)
{
    float32_t3 i = floor(x);
    float32_t3 f = frac(x);
    f = f * f * (3.0f - 2.0f * f);

    return lerp(
        lerp(lerp(Hash31(i + float32_t3(0, 0, 0)), Hash31(i + float32_t3(1, 0, 0)), f.x),
             lerp(Hash31(i + float32_t3(0, 1, 0)), Hash31(i + float32_t3(1, 1, 0)), f.x), f.y),
        lerp(lerp(Hash31(i + float32_t3(0, 0, 1)), Hash31(i + float32_t3(1, 0, 1)), f.x),
             lerp(Hash31(i + float32_t3(0, 1, 1)), Hash31(i + float32_t3(1, 1, 1)), f.x), f.y),
        f.z);
}

float32_t FbmTurbulence(float32_t3 p)
{
    float32_t v = 0.0f;
    float32_t amp = 0.5f;
    for (int o = 0; o < 3; ++o)
    {
        v += amp * Noise3D(p);
        p = p * 2.07f + float32_t3(13.1f, 7.7f, 5.3f);
        amp *= 0.5f;
    }
    return v;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t time = gMaterial.reflectionFactor;
    float32_t mode = gMaterial.fresnelF0;

    // =========================================================================
    // Mode 0: 3D ボリュメトリック・サーチライト / キャビン投光コーン（チンダル散乱光条）
    // =========================================================================
    if (mode < 0.5f)
    {
        float32_t3 N = normalize(input.normal);
        float32_t3 V = normalize(gCamera.worldPosition - input.worldPosition);

        // 1. 視線と法線の角度によるシルエットエッジ完全ソフト減衰（ポリゴン境界線が100%消える）
        float32_t ndotv = abs(dot(N, V));
        float32_t edgeSoftness = pow(saturate(ndotv), 2.35f);

        // 2. 光源レンズ(uv.y=0)から照射先(uv.y=1)への距離減衰＋地表フェザー融合
        float32_t distT = saturate(input.texcoord.y);
        float32_t nearSourceHotspot = exp(-distT * 6.0f) * 1.35f;
        float32_t shaftBody = pow(saturate(1.0f - distT * 0.62f), 1.30f);
        float32_t tipFeather = smoothstep(0.0f, 0.025f, distT) * smoothstep(1.0f, 0.82f, distT);

        // 3. ローター下降気流でビーム内を渦巻く3D大気ダスト・煙霧の濃淡
        float32_t3 flowCoord = input.worldPosition * 1.15f + float32_t3(time * 0.85f, time * 2.35f, -time * 0.70f);
        float32_t dustScatter = 0.62f + 0.76f * FbmTurbulence(flowCoord);

        // 4. 微細な光学シンチレーション（光の筋の揺らぎ）
        float32_t rayAngle = input.texcoord.x * 6.2831853f;
        float32_t streaks = 0.86f + 0.14f * sin(rayAngle * 7.0f + time * 3.2f) * cos(rayAngle * 4.0f - time * 2.1f);

        float32_t density = edgeSoftness * (shaftBody * dustScatter * streaks + nearSourceHotspot) * tipFeather;
        float32_t finalAlpha = saturate(density * gMaterial.color.a);
        if (finalAlpha <= 0.002f)
        {
            discard;
        }

        // 光源直近は白熱ハロゲン色へシフト
        float32_t3 coreTint = lerp(gMaterial.color.rgb, float32_t3(1.0f, 0.98f, 0.92f), saturate(nearSourceHotspot * 0.55f));
        output.color = float32_t4(coreTint, finalAlpha);
        return output;
    }

    // =========================================================================
    // Mode 1: 3D 地上ハロゲン投光コースティクス・ライトプール（柔らかいガウス減衰＆大気揺らぎ）
    // =========================================================================
    if (mode < 1.5f)
    {
        float32_t2 centered = (input.texcoord - float32_t2(0.5f, 0.5f)) * 2.0f;
        float32_t r = length(centered);
        if (r >= 1.0f)
        {
            discard;
        }

        // ガウス中心光 ＋ リフレクター外縁の滑らかなペナンブラ（半影）＋ レンズ集光リング
        float32_t gaussCore = exp(-r * r * 3.8f);
        float32_t penumbra = pow(saturate(1.0f - r), 2.4f);
        float32_t lensCaustic = exp(-pow((r - 0.62f) * 6.5f, 2.0f)) * 0.22f;

        float32_t3 groundFlow = float32_t3(input.worldPosition.x * 1.4f + time * 1.2f, time * 0.8f, input.worldPosition.z * 1.4f - time * 0.9f);
        float32_t smokeShadow = 0.75f + 0.35f * FbmTurbulence(groundFlow);

        float32_t intensity = (gaussCore * 0.85f + penumbra * 0.45f + lensCaustic) * smokeShadow;
        float32_t finalAlpha = saturate(intensity * gMaterial.color.a);
        if (finalAlpha <= 0.002f)
        {
            discard;
        }

        float32_t3 col = lerp(gMaterial.color.rgb, float32_t3(1.0f, 0.97f, 0.88f), saturate(gaussCore * 0.5f));
        output.color = float32_t4(col, finalAlpha);
        return output;
    }

    // =========================================================================
    // Mode 2: 3D 地上救難ビーコン電磁波紋 / 衝撃波リング（遮蔽物の奥行きと正確に前後判定）
    // =========================================================================
    float32_t2 centered = (input.texcoord - float32_t2(0.5f, 0.5f)) * 2.0f;
    float32_t r = length(centered);
    if (r >= 1.0f)
    {
        discard;
    }

    float32_t ringWidth = max(0.032f, mode - 2.0f);
    float32_t distFromEdge = abs(r - 0.88f);
    float32_t sharpFront = exp(-pow(distFromEdge / ringWidth, 2.0f));
    float32_t innerWake = smoothstep(0.45f, 0.88f, r) * step(r, 0.88f) * 0.28f;

    float32_t finalAlpha = saturate((sharpFront + innerWake) * gMaterial.color.a);
    if (finalAlpha <= 0.002f)
    {
        discard;
    }

    float32_t3 waveCol = lerp(gMaterial.color.rgb, float32_t3(1.0f, 0.92f, 0.85f), sharpFront * 0.45f);
    output.color = float32_t4(waveCol, finalAlpha);
    return output;
}
