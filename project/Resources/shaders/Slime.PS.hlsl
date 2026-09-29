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

    // --- ベースカラー ---
    float4 slimeColor = gSlimeParams.baseColor * gMaterial.color;

    // 変形量による色変調（伸びた部分がわずかに明るくなる）
    float deformTint = 1.0f + input.deformAmount * 0.5f;
    slimeColor.rgb *= deformTint;

    // --- ディフューズ（Half-Lambert）---
    float diffuseFactor = pow(NdotL * 0.5f + 0.5f, 2.0f);
    float3 diffuse = slimeColor.rgb * gDirectionalLight.color.rgb * diffuseFactor * gDirectionalLight.intensity;

    // --- スペキュラ（Blinn-Phong）---
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0f);
    float specularPow = pow(NdotH, gSlimeParams.specularShininess);
    float3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * 0.8f;

    // --- フレネル反射（Schlick 近似）---
    float fresnelBase = 1.0f - NdotV;
    float fresnel = pow(fresnelBase, gSlimeParams.fresnelPower);
    // エッジグロー（輪郭が明るく光る）
    float3 edgeGlow = slimeColor.rgb * fresnel * 1.5f;

    // --- 環境マップ反射 ---
    float3 R = reflect(-V, N);
    float3 envColor = gEnvironmentMap.Sample(gEnvironmentSampler, R).rgb;
    float3 envReflection = envColor * fresnel * gSlimeParams.envReflection;

    // --- 内部散乱グロー（擬似SSS：中心が濃く端が透明） ---
    float innerGlow = gSlimeParams.innerGlow * (1.0f - NdotV) * 0.5f;
    float3 sssColor = slimeColor.rgb * innerGlow;

    // --- 合成 ---
    float3 finalColor = diffuse + specular + edgeGlow + envReflection + sssColor;

    // --- 半透明度 ---
    // 中心ほど不透明、端ほど透明（ゼリー感）
    float alpha = slimeColor.a * (0.75f + 0.25f * NdotV);
    // フレネルでエッジの透明度をわずかに上げる
    alpha = lerp(alpha, 1.0f, fresnel * 0.3f);

    output.color = float4(finalColor, alpha);

    // 完全に透明なピクセルは破棄
    if (output.color.a < 0.01f)
    {
        discard;
    }

    return output;
}
