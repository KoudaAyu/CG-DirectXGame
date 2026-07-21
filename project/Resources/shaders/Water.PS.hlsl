#include "Resources/shaders/Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    int32_t specularModel;
    float32_t reflectionFactor;
    float32_t fresnelF0;
    float32_t4x4 uvTransform;
    float32_t shininess;
    float32_t3 padding2;
};
ConstantBuffer<Material> gMaterial : register(b0);

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct Camera
{
    float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

struct WaterParams
{
    float32_t time;
    float32_t flowSpeed;
    float32_t waveScale;
    float32_t padding;
};
ConstantBuffer<WaterParams> gWaterParams : register(b3);

Texture2D<float32_t4> gTexture : register(t3);
SamplerState gSample : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 流速に基づいたUVスクロールアニメーション (V方向の流れ)
    float2 scrolledUV = input.texcoord;
    scrolledUV.y += gWaterParams.time * gWaterParams.flowSpeed * 0.15f;
    scrolledUV.x += sin(scrolledUV.y * 8.0f + gWaterParams.time * 2.0f) * 0.03f; // 横方向の水のうねり

    float4 textureColor = gTexture.Sample(gSample, scrolledUV);
    
    // 水のベース色とテクスチャの合成
    float4 baseWaterColor = gMaterial.color;
    output.color = baseWaterColor * textureColor;

    // 水面のきらめきと光沢ハイライト
    if (gMaterial.enableLighting != 0)
    {
        float32_t3 N = normalize(input.normal);
        // 波の細かな凹凸による法線の歪み
        N.x += sin(input.worldPosition.z * 5.0f + gWaterParams.time * 3.0f) * 0.15f;
        N.z += cos(input.worldPosition.x * 5.0f + gWaterParams.time * 3.0f) * 0.15f;
        N = normalize(N);

        float32_t3 L = normalize(-gDirectionalLight.direction);
        float32_t3 V = normalize(gCamera.worldPosition - input.worldPosition);
        float32_t3 H = normalize(L + V);

        float32_t NdotL = max(dot(N, L), 0.0f);
        float32_t NdotH = max(dot(N, H), 0.0f);
        float32_t specularPow = pow(NdotH, 96.0f); // 太陽光のギラつくハイライト

        float32_t3 diffuse = baseWaterColor.rgb * textureColor.rgb * gDirectionalLight.color.rgb * (NdotL * 0.4f + 0.6f);
        float32_t3 specular = float32_t3(1.0f, 1.0f, 1.0f) * specularPow * gDirectionalLight.intensity;

        output.color.rgb = diffuse + specular;
    }

    output.color.a = baseWaterColor.a * 0.85f;

    return output;
}
