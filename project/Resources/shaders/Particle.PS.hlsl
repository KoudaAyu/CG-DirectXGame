#include "Resources/shaders/Particle.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    int32_t specularModel; // 0: Blinn-Phong, 1: Phong
    float32_t2 padding; // 16バイトアライメントのためのパディング
    float4x4 uvTransform;
    float32_t shininess;
    float32_t3 padding2;
};

ConstantBuffer<Material> gMaterial : register(b0);

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float32_t intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float4> gTexture : register(t3);

SamplerState gSample : register(s0);


struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};


PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t4 uv4 = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSample, uv4.xy);
    
    // 1. 擬似球状法線ライティングによる3次元ボリューメトリック陰影付け
    float32_t2 pCoord = (input.texcoord - 0.5f) * 2.0f;
    float32_t rSq = dot(pCoord, pCoord);
    float32_t shade = 1.0f;
    float32_t scatter = 0.0f;
    
    if (rSq < 1.0f)
    {
        float32_t3 sphereNorm = float32_t3(pCoord.x, pCoord.y, sqrt(1.0f - rSq));
        // 斜め上空（太陽光）からの光線ベクトル（ビュー空間シミュレーション）
        float32_t3 sunDir = normalize(float32_t3(0.28f, 0.88f, 0.38f));
        float32_t nDotL = dot(sphereNorm, sunDir);
        
        // A. ハーフランバート陰影（光の当たる上面のハイライトと、底面の自然な陰影）
        float32_t halfLambert = saturate(nDotL * 0.5f + 0.5f);
        shade = lerp(0.58f, 1.22f, halfLambert * halfLambert);
        
        // B. 煙特有の前方多重散乱（Subsurface / Forward Scattering: 逆光透過光）
        // 光の反対側を向く薄いエッジが、光を透過してふんわりと明るく透ける
        float32_t backScatter = saturate(-nDotL);
        scatter = pow(backScatter, 2.5f) * 0.28f * (1.0f - textureColor.a * 0.6f);
    }
    
    // 2. 煙の内部密度による自己遮蔽（濃い煙核の陰影と微小な繊維の立体感）
    float32_t densityOcclusion = lerp(0.78f, 1.08f, textureColor.a);
    float32_t3 litColor = (textureColor.rgb * shade * densityOcclusion) + float32_t3(scatter, scatter * 1.1f, scatter * 0.9f);
    output.color.rgb = gMaterial.color.rgb * litColor * input.color.rgb;
    
    // 3. 外周フェード（テクスチャの有機的な輪郭を活かしつつエッジを自然にブレンド）
    float32_t radialFade = saturate(1.05f - rSq);
    output.color.a = gMaterial.color.a * (textureColor.a * radialFade) * input.color.a;
    
    if(output.color.a <= 0.002f)
    {
        discard;
    }
    
    return output;
}
