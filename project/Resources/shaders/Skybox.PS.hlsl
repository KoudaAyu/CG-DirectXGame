#include "Resources/shaders/Skybox.hlsli"

TextureCube<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float3 dir = normalize(input.texcoord);
    float4 textureColor = gTexture.Sample(gSampler, dir);
    output.color = textureColor;

    return output;
}