#include"Object3d.hlsli"

// ====================================================================
// 修正1: struct Material の定義を ConstantBuffer の宣言よりも前に移動
// ====================================================================
struct Material
{
    float32_t4 color;
};

ConstantBuffer<Material> gMaterial0 : register(b0);
ConstantBuffer<Material> gMaterial1 : register(b1);
ConstantBuffer<Material> gMaterial2 : register(b2);

// ====================================================================
// 修正2: gTexture のレジスタを t0 から t3 に変更 (C++のルートシグネチャと一致させるため)
// ====================================================================
Texture2D<float32_t4> gTexture : register(t3);
Texture2D<float32_t4> gTexture0 : register(t3); // もしgTextureとgTexture0が同じものを指すならどちらか一方でOK
Texture2D<float32_t4> gTexture1 : register(t4);

// ====================================================================
// 修正3: SampleState のスペルミスを SamplerState に修正
// ====================================================================
SamplerState gSample : register(s0);


struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t4 textureColor = gTexture.Sample(gSample, input.texcoord);
    output.color = gMaterial0.color * textureColor;
    return output;
}