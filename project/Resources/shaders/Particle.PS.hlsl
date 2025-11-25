#include "Resources/shaders/Particle.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
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

PixelShaderOutput main(VertexShaderOutput input, bool isFrontFace : SV_IsFrontFace)
{
    PixelShaderOutput output;

    float2 uv = input.texcoord;
    if (!isFrontFace)
    {
        uv.x = 1.0f - uv.x;
    }

    float3 transformedUV = mul(float32_t4(uv, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSample, transformedUV.xy);
    output.color = gMaterial.color * textureColor;

    // Modulate final alpha by per-instance alpha passed from VS
    output.color.a *= input.alpha;

    if (output.color.a <= 0.01)
    {
        discard;
    }

    return output;
}