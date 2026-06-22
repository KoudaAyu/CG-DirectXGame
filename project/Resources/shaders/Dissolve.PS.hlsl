#include "CopyImage.hlsli"

struct DissolveParam
{
    float32_t threshold;
    float32_t edgeWidth;
    float32_t3 edgeColor;
};

ConstantBuffer<DissolveParam> gDissolve : register(b0);

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gMaskTexture : register(t1);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float32_t mask = gMaskTexture.Sample(gSampler, input.texcoord);

    // 閾値以下の部分を切り抜く（discard）
    if (mask <= gDissolve.threshold)
    {
        discard;
    }

    // 元のテクスチャカラーをサンプリング
    output.color = gTexture.Sample(gSampler, input.texcoord);

    // エッジ部分（境界線）に色を乗せる処理
    // 閾値 〜 閾値 + edgeWidth の間を 0.0〜1.0 に補間し、それを1.0から引いてエッジの強さとする
    float32_t edge = 1.0f - smoothstep(gDissolve.threshold, gDissolve.threshold + gDissolve.edgeWidth, mask);
    
    // エッジ部分に指定された色（発光色など）を加算する
    output.color.rgb += edge * gDissolve.edgeColor;

    return output;
}
