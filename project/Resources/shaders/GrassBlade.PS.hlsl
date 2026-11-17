struct GrassFieldConstants
{
    float32_t4x4 viewProj;
    float32_t3   baseColorRoot;
    float32_t    sssIntensity;
    float32_t3   baseColorTip;
    float32_t    maxDrawDistance;
    float32_t3   dryTipColor;
    float32_t    specularIntensity;
    float32_t3   fogColor;
    float32_t    fogDensity;
};
ConstantBuffer<GrassFieldConstants> gField : register(b0);

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

struct PSInput
{
    float32_t4 position : SV_POSITION;
    float32_t3 worldPos : TEXCOORD0;
    float32_t3 normal   : NORMAL0;
    float32_t  heightT  : TEXCOORD1;
    float32_t  colorVar : TEXCOORD2;
    float32_t  gustWave : TEXCOORD3;
};

float32_t4 main(PSInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET0
{
    float32_t t = saturate(input.heightT);

    // 根元の深い緑から、穂先の瑞々しい黄緑・黄金色へ滑らかに遷移
    float32_t3 tipPalette = lerp(gField.baseColorTip, gField.dryTipColor, saturate(input.colorVar * 0.65f));
    float32_t3 bladeAlbedo = lerp(gField.baseColorRoot, tipPalette, pow(t, 0.82f));

    // 根元のセルフアンビエントオクルージョン（地面の草土と自然に馴染む）
    float32_t rootAO = lerp(0.32f, 1.0f, smoothstep(0.0f, 0.78f, t));

    float32_t3 N = normalize(isFrontFace ? input.normal : -input.normal);
    // 草原全体として滑らかな丸みを持たせるため、上向き法線をブレンド（Terrain Normal Blending）
    float32_t3 macroNormal = normalize(lerp(N, float32_t3(0.0f, 1.0f, 0.0f), 0.52f));

    float32_t3 V = normalize(gCamera.worldPosition - input.worldPos);
    float32_t3 L = normalize(-gLightGroup.directionalLight.direction);
    float32_t3 lightColor = gLightGroup.directionalLight.color.rgb * gLightGroup.directionalLight.intensity;

    // 1. ラップ拡散反射 + 半球環境光
    float32_t wrapNdotL = saturate((dot(macroNormal, L) + 0.42f) / 1.42f);
    float32_t3 ambient = float32_t3(0.20f, 0.30f, 0.38f) * rootAO;
    float32_t3 diffuse = bladeAlbedo * (ambient + lightColor * wrapNdotL * rootAO);

    // 2. 逆光で葉が黄金黄緑に透けて輝く Subsurface Scattering (SSS)
    float32_t viewBacklight = pow(saturate(dot(V, -L)), 3.0f);
    float32_t grazingSss = saturate(1.0f - abs(dot(N, L))) * 0.35f;
    float32_t thickness = smoothstep(0.1f, 1.0f, t); // 穂先ほど薄くよく透ける
    float32_t3 sssColor = bladeAlbedo * float32_t3(1.55f, 1.65f, 0.58f);
    float32_t3 translucency = sssColor * lightColor * (viewBacklight + grazingSss) * thickness * gField.sssIntensity;

    // 3. 風の波頭に沿って走る太陽光のスペキュラシーン（風の可視化）
    float32_t3 H = normalize(L + V);
    float32_t specBase = pow(max(dot(N, H), 0.0f), 28.0f);
    float32_t gustSheen = (0.45f + 0.85f * input.gustWave) * smoothstep(0.25f, 1.0f, t);
    float32_t3 specular = lightColor * specBase * gustSheen * gField.specularIntensity;

    float32_t3 finalColor = diffuse + translucency + specular;

    // 4. 大気遠近フォグ
    float32_t dist = length(gCamera.worldPosition - input.worldPos);
    float32_t fogFactor = 1.0f - exp(-max(0.0f, dist - 12.0f) * gField.fogDensity);
    finalColor = lerp(finalColor, gField.fogColor, saturate(fogFactor));

    return float32_t4(finalColor, 1.0f);
}
