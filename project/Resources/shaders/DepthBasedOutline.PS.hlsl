#include "Resources/shaders/CopyImage.hlsli"
#include "CopyImage.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

Texture2D<float32_t> gDepthTexture : register(t1);
SamplerState gSamplerPoint : register(s1);

struct ProjectionMatrix {
    float32_t4x4 projectionInverse;
};
ConstantBuffer<ProjectionMatrix> gMaterial : register(b1);

static const float32_t2 kIndex3x3[3][3] = {
    { float32_t2(-1.0f, -1.0f), float32_t2(0.0f, -1.0f), float32_t2(1.0f, -1.0f) },
    { float32_t2(-1.0f,  0.0f), float32_t2(0.0f,  0.0f), float32_t2(1.0f,  0.0f) },
    { float32_t2(-1.0f,  1.0f), float32_t2(0.0f,  1.0f), float32_t2(1.0f,  1.0f) },
};

static const float32_t kPrewittHorizontalKernel[3][3] = {
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
};

static const float32_t kPrewittVerticalKernel[3][3] = {
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f },
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    uint width;
    uint height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp((float32_t)width), rcp((float32_t)height));

    float32_t centerNdc = gDepthTexture.Sample(gSamplerPoint, input.texcoord).r;
    if (centerNdc >= 0.9999f)
    {
        PixelShaderOutput output;
        output.color = gTexture.Sample(gSampler, input.texcoord);
        return output;
    }

    float32_t4 centerViewSpace = mul(float32_t4(0.0f, 0.0f, centerNdc, 1.0f), gMaterial.projectionInverse);
    float32_t centerViewZ = centerViewSpace.z * rcp(centerViewSpace.w);

    float32_t2 difference = float32_t2(0.0f, 0.0f);

    [unroll]
    for (int x = 0; x < 3; ++x) {
        [unroll]
        for (int y = 0; y < 3; ++y) {
            float32_t2 texcoord = input.texcoord + kIndex3x3[x][y] * uvStepSize;
            
            float32_t ndcDepth = gDepthTexture.Sample(gSamplerPoint, texcoord).r;
            if (ndcDepth >= 0.9999f) {
                ndcDepth = min(centerNdc + 0.03f, 0.999f);
            }
            float32_t4 viewSpace = mul(float32_t4(0.0f, 0.0f, ndcDepth, 1.0f), gMaterial.projectionInverse);
            float32_t viewZ = viewSpace.z * rcp(viewSpace.w);

            difference.x += viewZ * kPrewittHorizontalKernel[x][y];
            difference.y += viewZ * kPrewittVerticalKernel[x][y];
        }
    }

    float32_t diffMag = length(difference);
    float32_t relDiff = diffMag / max(centerViewZ * 0.12f + 0.35f, 0.1f);

    // 繊細で滑らかなセル調エッジ（アンチエイリアスが効いた自然な輪郭）
    float32_t edge = smoothstep(0.12f, 0.38f, relDiff);

    // 最大濃度を35%程度に抑え、黒く塗りつぶさず元のテクスチャ色を自然に落とし込む
    float32_t outlineOpacity = edge * 0.35f;

    float32_t3 sceneColor = gTexture.Sample(gSampler, input.texcoord).rgb;

    PixelShaderOutput output;
    output.color.rgb = lerp(sceneColor, sceneColor * 0.25f, outlineOpacity);
    output.color.a = 1.0f;

    return output;
}
