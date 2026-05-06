#include "Resources/shaders/Particle.hlsli"

struct ParticleForGPU
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
    uint textureIndex;
    uint padding0;
    uint padding1;
    uint padding2;
};
StructuredBuffer<ParticleForGPU> gParticle : register(t0);

cbuffer InstanceOffset : register(b3)
{
    uint gInstanceOffset;
}

struct VertecShederInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};



VertexShaderOutput main(VertecShederInput input,uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    uint particleIndex = gInstanceOffset + instanceId;
    output.position = mul(input.position, gParticle[particleIndex].WVP);
    output.texcoord = input.texcoord;
    output.color = gParticle[particleIndex].color;
    // debug用 : このインスタンスが代替パーティクルテクスチャ (circle2 -> srv インデックス 5) を使用している場合、赤色に着色します
    // シェーダーが実行時にインスタンスを正しくインデックス付けしていることを検証
    uint texIdx = gParticle[particleIndex].textureIndex;
    if (texIdx == 5u)
    {
        output.color.rgb *= float3(1.0f, 0.25f, 0.25f);
    }
    return output;
}

