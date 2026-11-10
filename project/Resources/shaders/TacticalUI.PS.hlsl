Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSInput
{
    float4 svPos : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PSInput input) : SV_TARGET0
{
    return input.color * gTexture.Sample(gSampler, input.uv);
}

