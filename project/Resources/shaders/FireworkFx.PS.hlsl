// 花火用のバッチ描画シェーダ（このプロジェクトで追加）
//
// ライティングは無し。テクスチャ x 頂点カラーをそのまま出す。
// 加算合成の PSO と組み合わせて使う。

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    output.color = textureColor * input.color;

    // 完全に透明な画素は捨てる。加算合成では見た目に影響しないが、
    // アルファブレンドに切り替えたときの重なりが素直になる
    if (output.color.a <= 0.0f)
    {
        discard;
    }

    return output;
}
