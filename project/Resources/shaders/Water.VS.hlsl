struct VSInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 worldPosition : TEXCOORD1;
};

cbuffer TransformationMatrix : register(b0)
{
    matrix WVP;
    matrix World;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(input.position, WVP);
    output.worldPosition = mul(input.position, World).xyz;
    output.uv = input.uv;
    output.normal = mul(input.normal, (float3x3)World);
    return output;
}
