#include "AppParticle.hlsli"

struct ParticleForGPU
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
    uint32_t textureIndex;
    uint32_t padding0;
    uint32_t padding1;
    uint32_t padding2;
};
StructuredBuffer<ParticleForGPU> gParticle : register(t0);

cbuffer InstanceOffset : register(b3)
{
    uint32_t gInstanceOffset;
}

struct VertecShederInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertecShederInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    uint32_t particleIndex = gInstanceOffset + instanceId;
    output.position = mul(input.position, gParticle[particleIndex].WVP);
    output.texcoord = input.texcoord;
    output.color = gParticle[particleIndex].color;
    
    uint32_t texIdx = gParticle[particleIndex].textureIndex;
    if (texIdx == 5u)
    {
        output.color.rgb *= float32_t3(1.0f, 0.25f, 0.25f);
    }
    return output;
}
