#include "Resources/shaders/Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    int32_t specularModel;
    float32_t reflectionFactor;
    float32_t fresnelF0;
    float32_t4x4 uvTransform;
    float32_t shininess;
    float32_t3 padding2;
};
ConstantBuffer<Material> gMaterial : register(b0);

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct Camera
{
    float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

struct WaterParams
{
    float32_t time;
    float32_t flowSpeed;
    float32_t waveScale;
    float32_t padding;
};
ConstantBuffer<WaterParams> gWaterParams : register(b3);

Texture2D<float32_t4> gTexture : register(t3);
SamplerState gSample : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float time = gWaterParams.time;
    float flowSpeed = gWaterParams.flowSpeed;

    // --- 遠景エイリアシング(チカチカ)防止のためのカメラ距離減衰ファクター ---
    float distToCam = length(gCamera.worldPosition - input.worldPosition);
    float distanceFade = saturate(1.0f - (distToCam - 15.0f) / 45.0f); // 15m以内で1.0, 60mで0.0へ平滑化

    // --- 1. カーブに沿った多層流速ベクトルの計算 ---
    float2 flowUV1 = input.texcoord * float2(3.0f, 4.0f) + float2(0.0f, time * flowSpeed * 0.18f);
    float2 flowUV2 = input.texcoord * float2(6.0f, 8.0f) + float2(sin(time * 0.9f) * 0.08f, time * flowSpeed * 0.28f);

    // 波の複雑なうねりと合成 (遠景では波の強さをマイルドにしてチカチカを防止)
    float wave1 = sin(flowUV1.x * 12.0f + flowUV1.y * 18.0f + time * 3.5f) * 0.5f + 0.5f;
    float wave2 = cos(flowUV2.x * 22.0f - flowUV2.y * 28.0f + time * 4.5f) * 0.5f + 0.5f;
    float combinedWave = (wave1 + wave2) * 0.5f;

    // 法線の動的歪み (距離に応じて歪みを減衰)
    float3 N = normalize(input.normal);
    N.x += (wave1 - 0.5f) * 0.38f * distanceFade;
    N.z += (wave2 - 0.5f) * 0.38f * distanceFade;
    N = normalize(N);

    // 視線ベクトルとライトベクトル
    float3 L = normalize(-gDirectionalLight.direction);
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);
    float3 H = normalize(L + V);

    // フレネル反射 (斜めから見ると空が映り、真上から見ると川底が透ける)
    float NdotV = max(dot(N, V), 0.0f);
    float fresnel = pow(1.0f - NdotV, 3.0f);
    fresnel = saturate(0.18f + fresnel * 0.82f);

    // 水色グラデーション (深い川と浅瀬のグラデーション)
    float3 deepWaterColor = float3(0.03f, 0.24f, 0.48f);  // 深い川の色
    float3 shallowWaterColor = float3(0.16f, 0.62f, 0.82f); // 浅瀬のエメラルド色
    float3 waterBase = lerp(shallowWaterColor, deepWaterColor, NdotV);

    // --- 2. 水際の白い泡ライン (Shore Foam Effects: 遠景では滑らかにスムーズ化) ---
    float distFromEdge = min(input.texcoord.x, 1.0f - input.texcoord.x);
    float foamZone = smoothstep(0.18f, 0.01f, distFromEdge);
    
    // 遠くでは高周波ノイズを減衰させてチカチカを防止
    float lowFreqNoise = sin(input.texcoord.y * 12.0f + time * 3.0f) * 0.5f + 0.5f;
    float highFreqNoise = sin(input.texcoord.y * 50.0f + time * 6.0f) * cos(input.texcoord.x * 30.0f - time * 4.0f) * 0.5f + 0.5f;
    float foamNoise = lerp(lowFreqNoise, highFreqNoise, distanceFade);
    
    float foamMask = pow(foamNoise, 2.0f) * foamZone * (0.4f + distanceFade * 0.6f);
    float3 foamColor = float3(0.92f, 0.96f, 1.0f);

    // ディレクショナルライティング
    float NdotL = max(dot(N, L), 0.0f);
    float3 diffuse = waterBase * gDirectionalLight.color.rgb * (NdotL * 0.5f + 0.5f);

    // 太陽光の鏡面反射 (距離に応じて眩しさを滑らかに維持)
    float NdotH = max(dot(N, H), 0.0f);
    float specularPow = pow(NdotH, 128.0f);
    float3 specular = gDirectionalLight.color.rgb * specularPow * gDirectionalLight.intensity * (1.5f + distanceFade * 1.3f);

    // 空の映り込み (Sky Reflection)
    float3 skyColor = float3(0.42f, 0.72f, 1.0f);
    float3 finalColor = lerp(diffuse, skyColor, fresnel * 0.45f) + specular;

    // 泡カラーのブレンド合成
    finalColor = lerp(finalColor, foamColor, foamMask * 0.85f);

    output.color.rgb = finalColor;
    output.color.a = saturate(0.82f + foamMask * 0.18f);

    return output;
}
