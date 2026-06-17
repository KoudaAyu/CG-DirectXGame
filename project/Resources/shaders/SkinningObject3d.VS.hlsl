#include "../shaders/Object3d.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 WorldInverseTranspose;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct Well
{
    float32_t4x4 skeletonSpaceMatrix;
    float32_t4x4 skeletonSpaceInverseTransposeMatrix;
};
StructuredBuffer<Well> gMatrixPalette : register(t0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 weight : WEIGHT0;
    int32_t4 index : INDEX0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    float32_t4 skinnedPosition = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    float32_t3 skinnedNormal = float32_t3(0.0f, 0.0f, 0.0f);
    
    for (int32_t i = 0; i < 4; ++i)
    {
        float32_t weight = input.weight[i];
        int32_t index = input.index[i];
        if (weight > 0.0f)
        {
            skinnedPosition += mul(input.position, gMatrixPalette[index].skeletonSpaceMatrix) * weight;
            skinnedNormal += mul(input.normal, (float32_t3x3)gMatrixPalette[index].skeletonSpaceInverseTransposeMatrix) * weight;
        }
    }
    
    skinnedPosition.w = 1.0f;
    skinnedNormal = normalize(skinnedNormal);
    
    output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(skinnedNormal, (float32_t3x3)gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;
    
    return output;
}
