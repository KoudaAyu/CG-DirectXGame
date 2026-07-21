#include "Resources/shaders/Object3d.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 WorldInverseTranspose;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct WaterParams
{
    float32_t time;
    float32_t flowSpeed;
    float32_t waveScale;
    float32_t padding;
};
ConstantBuffer<WaterParams> gWaterParams : register(b3);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // 時間経過による水面の波揺れ（Y高さ成分をサイン波でリアルタイムアニメーション）
    float4 pos = input.position;
    float wave = sin(pos.x * gWaterParams.waveScale + gWaterParams.time * 3.0f) * 0.08f;
    pos.y += wave;

    output.position = mul(pos, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float32_t3x3) gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = mul(pos, gTransformationMatrix.World).xyz;
    
    return output;
}
