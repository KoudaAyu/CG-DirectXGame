#include "Resources/shaders/Skybox.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.potision = mul(input.position, gTransformationMatrix.WVP).xyww;

    
    float3 worldPos = mul(input.position, gTransformationMatrix.World).xyz;
    output.texcoord = normalize(worldPos);

    return output;
}
