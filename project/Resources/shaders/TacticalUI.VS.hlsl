cbuffer UIFrameConstants : register(b0)
{
    float2 gInvDisplaySize;
    float2 gPadding;
};

struct VSInput
{
    float2 pos   : POSITION0;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

struct PSInput
{
    float4 svPos : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.svPos = float4(
        input.pos.x * gInvDisplaySize.x * 2.0f - 1.0f,
        1.0f - input.pos.y * gInvDisplaySize.y * 2.0f,
        0.0f,
        1.0f);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

