#include "Resources/shaders/Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    int32_t specularModel; // 0: Blinn-Phong, 1: Phong
    float32_t2 padding;
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

Texture2D<float32_t4> gTexture : register(t3);
SamplerState gSample : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

struct Camera
{
    float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t4 uv4 = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t2 uv = uv4.xy;
    float32_t4 textureColor = gTexture.Sample(gSample, uv);
    output.color = gMaterial.color * textureColor;

    if (textureColor.a <= 0.1f || output.color.a == 0.0f)
    {
        discard;
    }

    if (gMaterial.enableLighting != 0)
    {
        float32_t3 N = normalize(input.normal);
        float32_t3 L = normalize(-gDirectionalLight.direction);
        float32_t NdotL = max(dot(N, L), 0.0f);
        float32_t3 V = normalize(gCamera.worldPosition - input.worldPosition);

        float32_t specularPow = 0.0f;
        if (gMaterial.specularModel == 0)
        {
            // Blinn-Phong
            float32_t3 H = normalize(L + V);
            float32_t NdotH = max(dot(N, H), 0.0f);
            specularPow = pow(NdotH, gMaterial.shininess);
        }
        else
        {
            // Phong
            float32_t3 R = reflect(-L, N);
            float32_t RdotV = max(dot(R, V), 0.0f);
            specularPow = pow(RdotV, gMaterial.shininess);
        }

        // diffuse (half-Lambert approximation kept)
        float32_t diffuseFactor = pow(NdotL * 0.5f + 0.5f, 2.0f);
        float32_t3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * diffuseFactor * gDirectionalLight.intensity;

        float32_t3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * saturate(NdotL);

        output.color.rgb = diffuse + specular;
    }

    return output;
}