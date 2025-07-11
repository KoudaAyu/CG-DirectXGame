#include"Object3d.hlsli"

// ====================================================================
// 修正1: struct Material の定義を ConstantBuffer の宣言よりも前に移動
// ====================================================================
struct Material
{
    float32_t4 color;
    int32_t enableLighting;
};

ConstantBuffer<Material> gMaterial : register(b0);

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);


// ====================================================================
// 修正2: gTexture のレジスタを t0 から t3 に変更 (C++のルートシグネチャと一致させるため)
// ====================================================================
Texture2D<float32_t4> gTexture : register(t0);

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
    output.color = gMaterial.color * textureColor;
    
    if(gMaterial.enableLighting != 0)
    {
        float cos = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
        output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    return output;
}