#include "Resources/shaders/Particle.hlsli"

struct InstanceData
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float alpha;
    float3 pad; // padding to 16-byte boundary
};

StructuredBuffer<InstanceData> gInstanceData : register(t0);

struct VertecShederInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertecShederInput input,uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gInstanceData[instanceId].WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float32_t3x3) gInstanceData[instanceId].World));
    // pack alpha into texcoord.z (or extend VertexShaderOutput to carry alpha) but reuse available fields: here extend by adding .w component to texcoord isn't possible
    // So add a new semantic in VertexShaderOutput for alpha. Update Particle.hlsli accordingly.
    output.alpha = gInstanceData[instanceId].alpha;
    return output;
}

