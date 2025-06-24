struct TransformationMatrix
{
    float32_t4x4 WVP;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertecShaderOutput
{
    float32_t4 posision : SV_POSITION;
};

struct VertecShederInput
{
    float32_t4 position : POSITION0;
};

VertecShaderOutput main(VertecShederInput input)
{
    VertecShaderOutput output;
    output.posision = mul(input.position, gTransformationMatrix.WVP);
    return output;
}

