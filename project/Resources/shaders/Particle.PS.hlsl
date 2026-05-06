#include "Resources/shaders/Particle.hlsli"

struct Material
{
    float4 color;
    int enableLighting;
    int specularModel; // 0: Blinn-Phong, 1: Phong
    float2 padding; // 16バイトアライメントのためのパディング
    float4x4 uvTransform;
    float shininess;
    float3 padding2;
};

ConstantBuffer<Material> gMaterial : register(b0);

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float4> gTexture : register(t3);

SamplerState gSample : register(s0);


struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};


PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 uv4 = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSample, uv4.xy);
    output.color = gMaterial.color * textureColor * input.color;
    
    if(output.color.a == 0.0)
    {
        discard;
    }
    
  
    return output;
}
