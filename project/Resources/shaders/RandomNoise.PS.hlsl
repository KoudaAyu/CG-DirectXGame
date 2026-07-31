#include "CopyImage.hlsli"

struct NoiseData
{
    float32_t time;
};

ConstantBuffer<NoiseData> gNoiseData : register(b0);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// 2次元の座標から1次元の疑似乱数（0以上1未満）を生成する関数
float32_t rand2dTo1d(float32_t2 uv)
{
    return frac(sin(dot(uv, float32_t2(12.9898f, 78.233f))) * 43758.5453f);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // 経過時間timeを掛けてSeed値にする
    float32_t random = rand2dTo1d(input.texcoord * gNoiseData.time);
    
    // 色にする（白黒・グレースケール）
    output.color = float32_t4(random, random, random, 1.0f);
    
    return output;
}
