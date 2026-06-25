#include "Resources/shaders/Skybox.hlsli"

TextureCube<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float32_t3 dir = normalize(input.texcoord);
    float32_t4 textureColor = gTexture.Sample(gSampler, dir);
    output.color = textureColor;

    return output;
}