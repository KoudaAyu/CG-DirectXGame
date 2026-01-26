#include "Resources/shaders/Particle.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t3 padding;
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



PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float3 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSample, transformedUV.xy);
    output.color = gMaterial.color * textureColor * input.color;

    if(output.color.a == 0.0)

    {
        discard;
    }
    
  
    return output;
}