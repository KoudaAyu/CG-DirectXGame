struct BotanicalDrawConstants
{
    float32_t4x4 world;
    float32_t4x4 viewProj;
    float32_t4x4 worldInvTranspose;
    float32_t4   baseTint;
    float32_t    waterLevel;
    float32_t    wetShoreHeight;
    float32_t    mossIntensity;
    float32_t    sssIntensity;
    float32_t3   fogColor;
    float32_t    fogDensity;
};
ConstantBuffer<BotanicalDrawConstants> gDraw : register(b0);

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float32_t  intensity;
};

struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float32_t  intensity;
    float32_t  radius;
    float32_t  decay;
    float32_t2 padding;
};

struct LightGroup
{
    DirectionalLight directionalLight;
    PointLight       pointLight;
};
ConstantBuffer<LightGroup> gLightGroup : register(b1);

struct Camera
{
    float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

TextureCube<float32_t4> gEnvironmentMap : register(t0);
SamplerState gSampler : register(s0);

struct PSInput
{
    float32_t4 position : SV_POSITION;
    float32_t3 worldPos : TEXCOORD0;
    float32_t3 normal   : NORMAL0;
    float32_t2 uv       : TEXCOORD1;
    float32_t4 color    : COLOR0;
    float32_t4 params   : TEXCOORD2; // x: windWeight, y: sss, z: moss, w: ao
};

float32_t Hash31(float32_t3 p)
{
    p = frac(p * float32_t3(0.1031f, 0.1030f, 0.0973f));
    p += dot(p, p.yxz + 33.33f);
    return frac((p.x + p.y) * p.z);
}

float32_t Noise3(float32_t3 x)
{
    float32_t3 i = floor(x);
    float32_t3 f = frac(x);
    f = f * f * (3.0f - 2.0f * f);

    float32_t n000 = Hash31(i + float32_t3(0, 0, 0));
    float32_t n100 = Hash31(i + float32_t3(1, 0, 0));
    float32_t n010 = Hash31(i + float32_t3(0, 1, 0));
    float32_t n110 = Hash31(i + float32_t3(1, 1, 0));
    float32_t n001 = Hash31(i + float32_t3(0, 0, 1));
    float32_t n101 = Hash31(i + float32_t3(1, 0, 1));
    float32_t n011 = Hash31(i + float32_t3(0, 1, 1));
    float32_t n111 = Hash31(i + float32_t3(1, 1, 1));

    float32_t nx00 = lerp(n000, n100, f.x);
    float32_t nx10 = lerp(n010, n110, f.x);
    float32_t nx01 = lerp(n001, n101, f.x);
    float32_t nx11 = lerp(n011, n111, f.x);

    float32_t nxy0 = lerp(nx00, nx10, f.y);
    float32_t nxy1 = lerp(nx01, nx11, f.y);
    return lerp(nxy0, nxy1, f.z);
}

// 1枚の葉のティアドロップ＋鋸歯（ギザギザ縁）SDF評価
float32_t EvaluateSingleLeaf(float32_t2 p, float32_t2 center, float32_t angle, float32_t2 scale, out float32_t vein)
{
    float32_t2 d = p - center;
    float32_t c = cos(angle);
    float32_t s = sin(angle);
    float32_t2 lp = float32_t2(d.x * c + d.y * s, -d.x * s + d.y * c) / scale;

    // 縦軸 [-1, 1] に対して先端が細くなる葉形プロファイル
    float32_t yNorm = clamp(lp.y * 0.5f + 0.5f, 0.0f, 1.0f);
    float32_t widthCurve = sin(pow(yNorm, 0.72f) * 3.14159265f) * (1.0f - 0.18f * yNorm);
    float32_t serration = sin(yNorm * 28.0f) * 0.045f * widthCurve;
    float32_t edge = (widthCurve + serration) - abs(lp.x);
    float32_t inRange = step(abs(lp.y), 1.02f);

    // 葉脈ディテール
    float32_t midrib = exp(-abs(lp.x) * 18.0f);
    float32_t sideVein = exp(-abs(frac((lp.y - abs(lp.x) * 0.85f) * 4.5f) - 0.5f) * 8.0f) * 0.45f;
    vein = saturate(midrib * 0.7f + sideVein);

    return inRange * edge;
}

float32_t4 main(PSInput input) : SV_TARGET0
{
    float32_t3 albedo = input.color.rgb * gDraw.baseTint.rgb;
    float32_t sssMask = input.params.y;
    float32_t mossWeight = input.params.z * gDraw.mossIntensity;
    float32_t ao = saturate(input.params.w);

    // --- 1. 樹木の葉カード (color.a < 0.9) はプロシージャルな小枝5枚葉シルエットで切り抜く ---
    if (input.color.a < 0.9f)
    {
        float32_t2 uv = input.uv * 2.0f - 1.0f;
        float32_t v0, v1, v2, v3, v4;
        float32_t m0 = EvaluateSingleLeaf(uv, float32_t2( 0.00f,  0.26f),  0.00f, float32_t2(0.42f, 0.58f), v0);
        float32_t m1 = EvaluateSingleLeaf(uv, float32_t2(-0.36f,  0.02f),  0.62f, float32_t2(0.36f, 0.48f), v1);
        float32_t m2 = EvaluateSingleLeaf(uv, float32_t2( 0.36f,  0.02f), -0.62f, float32_t2(0.36f, 0.48f), v2);
        float32_t m3 = EvaluateSingleLeaf(uv, float32_t2(-0.28f, -0.38f),  0.95f, float32_t2(0.30f, 0.40f), v3);
        float32_t m4 = EvaluateSingleLeaf(uv, float32_t2( 0.28f, -0.38f), -0.95f, float32_t2(0.30f, 0.40f), v4);

        float32_t maxEdge = max(m0, max(max(m1, m2), max(m3, m4)));
        if (maxEdge <= 0.02f)
        {
            discard;
        }

        float32_t vein = max(v0, max(max(v1, v2), max(v3, v4)));
        albedo = lerp(albedo, albedo * float32_t3(1.22f, 1.28f, 0.92f), vein * 0.35f);
    }

    // --- 2. 3Dマイクロノイズによる岩肌・土肌の質感付与 ---
    float32_t detailNoise = Noise3(input.worldPos * 3.8f) * 0.65f + Noise3(input.worldPos * 11.5f) * 0.35f;
    if (sssMask < 0.2f)
    {
        albedo *= (0.86f + 0.28f * detailNoise);
    }

    float32_t3 N = normalize(input.normal);
    float32_t3 V = normalize(gCamera.worldPosition - input.worldPos);
    float32_t3 L = normalize(-gLightGroup.directionalLight.direction);

    // --- 3. ベルベット苔シェーディング（岩や岸の上面に自然な苔をブレンド） ---
    float32_t mossMask = saturate(mossWeight * (0.55f + 0.75f * detailNoise) * saturate(N.y * 0.8f + 0.35f));
    if (mossMask > 0.01f && sssMask < 0.2f)
    {
        // 水面より下では苔を抑えて水中藻のトーンにする
        float32_t aboveWater = saturate((input.worldPos.y - gDraw.waterLevel + 0.05f) * 4.0f);
        float32_t3 lushMoss = lerp(float32_t3(0.11f, 0.24f, 0.10f), float32_t3(0.24f, 0.48f, 0.14f), detailNoise);
        albedo = lerp(albedo, lushMoss, mossMask * aboveWater * 0.88f);
    }

    // --- 4. 水際の濡れ表現（水面付近〜水中の岩や砂が濡れて濃く艶やかに見える） ---
    float32_t heightAboveWater = input.worldPos.y - gDraw.waterLevel;
    float32_t wetness = (sssMask < 0.2f)
        ? (1.0f - smoothstep(-0.08f, gDraw.wetShoreHeight, heightAboveWater))
        : 0.0f;
    albedo *= lerp(1.0f, 0.50f, wetness);

    // --- 5. 半球スカイアンビエント + 太陽光拡散・鏡面反射 ---
    float32_t3 lightColor = gLightGroup.directionalLight.color.rgb * gLightGroup.directionalLight.intensity;
    float32_t NdotL = max(dot(N, L), 0.0f);
    float32_t wrapDiffuse = saturate((dot(N, L) + 0.28f) / 1.28f);

    float32_t3 skyAmbient = float32_t3(0.24f, 0.34f, 0.48f);
    float32_t3 groundBounce = float32_t3(0.14f, 0.16f, 0.10f);
    float32_t3 ambient = lerp(groundBounce, skyAmbient, N.y * 0.5f + 0.5f) * ao;

    float32_t3 diffuse = albedo * (ambient + lightColor * wrapDiffuse * (0.35f + 0.65f * ao));

    // 濡れた岩や葉表面のスペキュラハイライト
    float32_t3 H = normalize(L + V);
    float32_t shininess = lerp(20.0f, 96.0f, wetness);
    float32_t specStrength = lerp(0.06f, 0.42f, wetness) * (sssMask > 0.2f ? 0.18f : 1.0f);
    float32_t spec = pow(max(dot(N, H), 0.0f), shininess) * specStrength * NdotL;
    float32_t3 specular = lightColor * spec;

    // 苔のベルベットリム光
    float32_t rim = pow(1.0f - saturate(dot(N, V)), 3.2f) * mossMask * 0.18f;
    float32_t3 mossRim = float32_t3(0.45f, 0.68f, 0.22f) * rim * lightColor;

    // --- 6. 葉の逆光透過（疑似 Subsurface Scattering / Translucency） ---
    float32_t3 translucency = float32_t3(0.0f, 0.0f, 0.0f);
    if (sssMask > 0.01f)
    {
        float32_t backLight = pow(saturate(dot(V, -L)), 3.2f) + 0.35f * saturate(-dot(N, L));
        float32_t3 warmTransColor = albedo * float32_t3(1.45f, 1.55f, 0.65f);
        translucency = warmTransColor * lightColor * backLight * sssMask * gDraw.sssIntensity * (0.4f + 0.6f * ao);
    }

    float32_t3 finalColor = diffuse + specular + mossRim + translucency;

    // 濡れた水際へのわずかな空環境マップ反射
    if (wetness > 0.05f)
    {
        float32_t3 R = reflect(-V, N);
        float32_t3 envCol = gEnvironmentMap.Sample(gSampler, R).rgb;
        float32_t fresnel = 0.04f + 0.96f * pow(1.0f - saturate(dot(N, V)), 5.0f);
        finalColor = lerp(finalColor, envCol, fresnel * wetness * 0.35f);
    }

    // --- 7. 大気遠近フォグ（遠景の空気感） ---
    float32_t dist = length(gCamera.worldPosition - input.worldPos);
    float32_t fogFactor = 1.0f - exp(-max(0.0f, dist - 12.0f) * gDraw.fogDensity);
    finalColor = lerp(finalColor, gDraw.fogColor, saturate(fogFactor));

    return float32_t4(finalColor, 1.0f);
}
