#include "Object3d.hlsli"

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

struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float radius;
    float decay;
    float2 padding;
};

struct LightGroup
{
    DirectionalLight directionalLight;
    PointLight pointLight;
};
ConstantBuffer<LightGroup> gLightGroup : register(b1);

Texture2D<float32_t4> gTexture : register(t3);
TextureCube<float32_t4> gEnvironmentMap : register(t4);
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

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 uv4 = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float2 uv = uv4.xy;
    float4 textureColor = gTexture.Sample(gSample, uv);
    output.color = gMaterial.color * textureColor;

    if (textureColor.a <= 0.1f || output.color.a == 0.0f)
    {
        discard;
    }

    if (gMaterial.enableLighting != 0)
    {
        float32_t3 N = normalize(input.normal);
        float32_t3 V = normalize(gCamera.worldPosition - input.worldPosition);

        // =========================================================
        // 1. 平行光源 (Directional Light) の計算
        // =========================================================
        float32_t3 L_dir = normalize(-gLightGroup.directionalLight.direction);
        float32_t NdotL_dir = max(dot(N, L_dir), 0.0f);

        float32_t dirSpecularPow = 0.0f;
        if (gMaterial.specularModel == 0)
        {
            // Blinn-Phong
            float32_t3 H = normalize(L_dir + V);
            float32_t NdotH = max(dot(N, H), 0.0f);
            dirSpecularPow = pow(NdotH, gMaterial.shininess);
        }
        else
        {
            // Phong
            float32_t3 R = reflect(-L_dir, N);
            float32_t RdotV = max(dot(R, V), 0.0f);
            dirSpecularPow = pow(RdotV, gMaterial.shininess);
        }

        // diffuse (Half-Lambert)
        float32_t dirDiffuseFactor = pow(NdotL_dir * 0.5f + 0.5f, 2.0f);
        float32_t3 dirDiffuse = gMaterial.color.rgb * textureColor.rgb * gLightGroup.directionalLight.color.rgb * dirDiffuseFactor * gLightGroup.directionalLight.intensity;
        float32_t3 dirSpecular = gLightGroup.directionalLight.color.rgb * gLightGroup.directionalLight.intensity * dirSpecularPow * saturate(NdotL_dir);

        // =========================================================
        // 2. 点光源 (Point Light) の計算
        // =========================================================
        float32_t3 pointDiffuse = float32_t3(0.0f, 0.0f, 0.0f);
        float32_t3 pointSpecular = float32_t3(0.0f, 0.0f, 0.0f);

        if (gLightGroup.pointLight.intensity > 0.0f && gLightGroup.pointLight.radius > 0.0f)
        {
            float32_t3 pointVec = gLightGroup.pointLight.position - input.worldPosition;
            float32_t distance = length(pointVec);

            if (distance < gLightGroup.pointLight.radius)
            {
                float32_t3 L_point = normalize(pointVec);
                float32_t NdotL_point = max(dot(N, L_point), 0.0f);

                // スムーズな距離減衰
                float32_t factor = saturate(1.0f - distance / gLightGroup.pointLight.radius);
                float32_t attenuation = pow(factor, gLightGroup.pointLight.decay);

                // 点光源の拡散反射 (Lambert)
                pointDiffuse = gMaterial.color.rgb * textureColor.rgb * gLightGroup.pointLight.color.rgb * NdotL_point * gLightGroup.pointLight.intensity * attenuation;

                // 点光源の鏡面反射 (Blinn-Phong / Phong)
                float32_t pointSpecPow = 0.0f;
                if (gMaterial.specularModel == 0)
                {
                    float32_t3 H_point = normalize(L_point + V);
                    float32_t NdotH_point = max(dot(N, H_point), 0.0f);
                    pointSpecPow = pow(NdotH_point, gMaterial.shininess);
                }
                else
                {
                    float32_t3 R_point = reflect(-L_point, N);
                    float32_t RdotV_point = max(dot(R_point, V), 0.0f);
                    pointSpecPow = pow(RdotV_point, gMaterial.shininess);
                }
                pointSpecular = gLightGroup.pointLight.color.rgb * gLightGroup.pointLight.intensity * pointSpecPow * saturate(NdotL_point) * attenuation;
            }
        }

        // --- Environment Mapping ---
        float32_t3 R_env = reflect(-V, N);
        float32_t3 reflectionColor = gEnvironmentMap.Sample(gSample, R_env).rgb * 0.3f;

        output.color.rgb = dirDiffuse + dirSpecular + pointDiffuse + pointSpecular + reflectionColor;
    }

    return output;
}
