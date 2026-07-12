#include "Object3d.hlsli"

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    // Neon green-blue (cyan-greenish) color matching our debug wires
    output.color = float32_t4(0.0f, 1.0f, 0.5f, 0.7f);
    return output;
}
