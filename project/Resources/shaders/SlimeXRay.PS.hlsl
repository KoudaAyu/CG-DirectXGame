#include "Resources/shaders/Slime.hlsli"

// マテリアル (b0)
struct Material
{
    float32_t4   color;
    int32_t      enableLighting;
    int32_t      specularModel;
    float32_t    reflectionFactor;
    float32_t    fresnelF0;
    float32_t4x4 uvTransform;
    float32_t    shininess;
    float32_t3   padding2;
};
ConstantBuffer<Material> gMaterial : register(b0);

// スライムパラメータ (b1)
ConstantBuffer<SlimeParams> gSlimeParams : register(b1);

// ディレクショナルライト (b2)
struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float      intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);

// カメラ (b3)
struct Camera
{
    float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b3);

// テクスチャ・環境マップ
Texture2D<float32_t4> gTexture : register(t3);
TextureCube<float32_t4> gEnvironmentMap : register(t4);
SamplerState gSample : register(s0);
SamplerState gEnvironmentSampler : register(s1);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float3 N = normalize(input.normal);
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);
    float3 L = normalize(-gDirectionalLight.direction);

    float NdotL = max(dot(N, L), 0.0f);
    float NdotV = max(dot(N, V), 0.0f);

    // --- ベースカラー（スライム本来の鮮やかな水色・黄色・赤など）---
    float4 slimeColor = gSlimeParams.baseColor * gMaterial.color;

    // 変形量による色変調
    float deformTint = 1.0f + input.deformAmount * 0.3f;
    slimeColor.rgb *= deformTint;

    // --- ディフューズ（陰影をつけて3Dスライム球体感を維持）---
    float diffuseFactor = pow(NdotL * 0.5f + 0.5f, 1.5f);
    float3 diffuse = slimeColor.rgb * (diffuseFactor * 0.70f + 0.40f);

    // --- リムライト（スライム本来の色を基準にした輪郭光）---
    float fresnel = pow(1.0f - NdotV, 2.5f);
    float3 rimGlow = slimeColor.rgb * (fresnel * 1.2f);

    // --- 合成（白浮きせず、スライム本来の綺麗な色）---
    float3 finalColor = diffuse + rimGlow;

    // --- 遮蔽時の半透明アルファ（自然に障害物の奥に透けて見える）---
    float alpha = slimeColor.a * (0.65f + fresnel * 0.25f);

    output.color = float4(finalColor, alpha);
    return output;
}
