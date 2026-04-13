#include "Resources/shaders/Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    int32_t specularModel; // 0: Blinn-Phong, 1: Phong
    float32_t2 padding;
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

Texture2D<float32_t4> gTexture : register(t3);
SamplerState gSample : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

struct Camera
{
    float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
};

ConstantBuffer<PointLight> gPointLight : register(b3);

struct SpotLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float32_t3 direction;
    float outerCos;
    float innerCos;
    float32_t3 padding;
};

ConstantBuffer<SpotLight> gSpotLight : register(b4);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t4 uv4 = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t2 uv = uv4.xy;
    float32_t4 textureColor = gTexture.Sample(gSample, uv);
    output.color = gMaterial.color * textureColor;

    if (textureColor.a <= 0.1f || output.color.a == 0.0f)
    {
        discard;
    }

    if (gMaterial.enableLighting != 0)
    {
        float3 N = normalize(input.normal);
        float3 V = normalize(gCamera.worldPosition - input.worldPosition);
        float3 totalDiffuse = 0;
        float3 totalSpecular = 0;

    // directional
    {
            float3 L = normalize(-gDirectionalLight.direction);
            float NdotL = max(dot(N, L), 0.0f);
            float diffuseFactor = pow(NdotL * 0.5f + 0.5f, 2.0f);
        // directional diffuse/specular (既存ロジック)
            float3 diffuseD = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * diffuseFactor * gDirectionalLight.intensity;
            float specPowD = 0;
            if (gMaterial.specularModel == 0)
            {
                float3 H = normalize(L + V);
                specPowD = pow(max(dot(N, H), 0.0f), gMaterial.shininess);
            }
            else
            {
                float3 R = reflect(-L, N);
                specPowD = pow(max(dot(R, V), 0.0f), gMaterial.shininess);
            }
            float3 specD = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specPowD * saturate(NdotL);
            totalDiffuse += diffuseD;
            totalSpecular += specD;
        }

    // point light（単一の point が既にあるならこれを使う）
    {
            float3 toLight = gPointLight.position - input.worldPosition;
            float dist = max(length(toLight), 1e-4f);
            float3 Lp = normalize(toLight); // surface -> light
            float NdotLp = max(dot(N, Lp), 0.0f);
        // 距離減衰の例（逆二乗）。必要なら係数を Material/Light に追加して調整
            float attenP = gPointLight.intensity / (dist * dist);
            float diffuseFactorP = pow(NdotLp * 0.5f + 0.5f, 2.0f);
            float3 diffuseP = gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * diffuseFactorP * attenP;
            float specPowP = 0;
            if (gMaterial.specularModel == 0)
            {
                float3 Hp = normalize(Lp + V);
                specPowP = pow(max(dot(N, Hp), 0.0f), gMaterial.shininess);
            }
            else
            {
                float3 Rp = reflect(-Lp, N);
                specPowP = pow(max(dot(Rp, V), 0.0f), gMaterial.shininess);
            }
            float3 specP = gPointLight.color.rgb * attenP * specPowP * saturate(NdotLp);
            totalDiffuse += diffuseP;
            totalSpecular += specP;
        }

    // spot light（新規）
    {
        // gSpotLight.direction はライトの向いている方向（ライト座標系で前向きベクトル）と仮定
            float3 toLight = gSpotLight.position - input.worldPosition;
            float dist = max(length(toLight), 1e-4f);
            float3 Ls = normalize(toLight); // surface -> light
      
            float cosAngle = dot(-Ls, normalize(gSpotLight.direction));
            
        // outer/inner はコサイン値で渡すのが高速（またはラジアン→cosを渡す）
            float spotFactor = 0.0f;
            if (cosAngle > gSpotLight.outerCos)
            {
            // smoothstep between outer and inner
                spotFactor = saturate((cosAngle - gSpotLight.outerCos) / (gSpotLight.innerCos - gSpotLight.outerCos));
            }
        // atten に距離減衰とスポット係数を掛ける
            float attenS = gSpotLight.intensity * spotFactor / (dist * dist);

            float NdotLs = max(dot(N, Ls), 0.0f);
            float diffuseFactorS = pow(NdotLs * 0.5f + 0.5f, 2.0f);
            float3 diffuseS = gMaterial.color.rgb * textureColor.rgb * gSpotLight.color.rgb * diffuseFactorS * attenS;

            float specPowS = 0;
            if (gMaterial.specularModel == 0)
            {
                float3 Hs = normalize(Ls + V);
                specPowS = pow(max(dot(N, Hs), 0.0f), gMaterial.shininess);
            }
            else
            {
                float3 Rs = reflect(-Ls, N);
                specPowS = pow(max(dot(Rs, V), 0.0f), gMaterial.shininess);
            }
            float3 specS = gSpotLight.color.rgb * attenS * specPowS * saturate(NdotLs);

            totalDiffuse += diffuseS;
            totalSpecular += specS;
        }

        output.color.rgb = totalDiffuse + totalSpecular;
    }

    return output;
}