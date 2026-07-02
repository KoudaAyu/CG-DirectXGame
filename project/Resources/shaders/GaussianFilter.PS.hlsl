#include "Resources/shaders/CopyImage.hlsli"
#include "CopyImage.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

static const float32_t2 kIndex3x3[3][3] =
{
    { float32_t2(-1.0f, -1.0f), float32_t2(0.0f, -1.0f), float32_t2(1.0f, -1.0f) },
    { float32_t2(-1.0f, 0.0f), float32_t2(0.0f, 0.0f), float32_t2(1.0f, 0.0f) },
    { float32_t2(-1.0f, 1.0f), float32_t2(0.0f, 1.0f), float32_t2(1.0f, 1.0f) },
};

static const float32_t PI = 3.14159265f;

float gauss(float x, float y, float sigma)
{
    float exponent = -(x * x + y * y) * rcp(2.0f * sigma * sigma);
    float denominator = 2.0f * PI * sigma * sigma;
    return exp(exponent) * rcp(denominator);
}

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};


PixelShaderOutput main(VertexShaderOutput input)
{
    // ①【準備】テクスチャのサイズからUVの1ピクセルあたりの移動量（uvStepSize）を計算する
    uint width;
    uint height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp((float32_t) width), rcp((float32_t) height));

    // ②【準備】出力用カラーの初期化
    PixelShaderOutput output;
    output.color.rgb = float32_t3(0.0f, 0.0f, 0.0f);
    output.color.a = 1.0f;

    // ③【重み計算】3x3のガウス関数カーネルを求める
    float32_t weight = 0.0f;
    float32_t kernel3x3[3][3];
    for (int32_t x = 0; x < 3; ++x)
    {
        for (int32_t y = 0; y < 3; ++y) // ← yループを追加します！
        {
            kernel3x3[x][y] = gauss(kIndex3x3[x][y].x, kIndex3x3[x][y].y, 2.0f);
            weight += kernel3x3[x][y];
        }
    }

    // ④【畳み込み】BoxFilterと同じようにテクスチャをサンプリングし、求めたkernelの重みを掛けて合計する
    for (int32_t x = 0; x < 3; ++x)
    {
        for (int32_t y = 0; y < 3; ++y)
        {
            // ここでサンプリング座標（texcoord）を計算する
            float32_t2 texcoord = input.texcoord + kIndex3x3[x][y] * uvStepSize;
            // テクスチャから色を取り出す
            float32_t3 fetchColor = gTexture.Sample(gSampler, texcoord).rgb;
            // 取り出した色に kernel3x3[x][y] を掛けて output.color.rgb に足す
            output.color.rgb += fetchColor * kernel3x3[x][y];
        }
    }

    // ⑤【正規化】畳み込み後の値を重みの合計（weight）で割る（rcpを掛ける）
    output.color.rgb *= rcp(weight);

    return output;
}


