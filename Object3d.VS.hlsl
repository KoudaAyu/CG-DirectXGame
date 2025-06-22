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
    output.posision = input.position;
    return output;
}

