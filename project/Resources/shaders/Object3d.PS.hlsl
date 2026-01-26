#include "Resources/shaders/Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t3 padding;
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
    float radius;
    float decay;
    float padding[2];
};

ConstantBuffer<PointLight> gPointLight : register(b3);

struct SpotLight
{
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t3 direction;
    float32_t distance;
    float32_t decay;
    float32_t cosAngle;
  
};

ConstantBuffer<SpotLight> gSpotLight : register(b4);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float3 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSample, transformedUV.xy);
    output.color = gMaterial.color * textureColor;
    
    //hals lambert
    float NdotL = dot(normalize(input.normal), normalize(-gDirectionalLight.direction));
    //float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);

    float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);



    if (textureColor.a <= 0.1 || output.color.a == 0.0)
    {
        discard;
    }
    
if (gMaterial.enableLighting != 0)
{
    float32_t3 halfVector = normalize(-gDirectionalLight.direction + toEye);
    float NDotH = dot(normalize(input.normal), halfVector);
    float specularPow = pow(saturate(NDotH), gMaterial.shininess);
    
    // 3. ハーフランバート
    float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);

    
    // 4. 拡散反射
    float32_t3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
    
    // 5. 鏡面反射 (saturate(NdotL)を掛けて裏側の漏れを防ぐ)
    float32_t3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * saturate(NdotL);

  
        
    //ポイントライト
    float32_t distance = length(gPointLight.position - input.worldPosition);
    float32_t factor = pow(saturate(-distance / gPointLight.radius + 1.0), gPointLight.decay);
    float32_t3 positionDirection = normalize(input.worldPosition - gPointLight.position);
    gPointLight.color.rgb * gPointLight.intensity * factor;
        
    float NdotPointLight = dot(normalize(input.normal), normalize(gPointLight.position - input.worldPosition));
    float specularPowPointLight = pow(saturate(NdotPointLight), gMaterial.shininess);
    //ハーフランバート
    float cosPointLight = pow(NdotPointLight * 0.5f + 0.5f, 2.0f);
        
    //拡散反射
    float32_t3 diffusePointLight = gMaterial.color * textureColor.rgb * gPointLight.color.rgb * cosPointLight * gPointLight.intensity;
    
    //鏡面反射
    float32_t3 specularPointLight = gPointLight.color.rgb * gPointLight.intensity * specularPointLight * saturate(NdotPointLight);
        
        float32_t3 spotLightDirectionOnSurface = normalize(input.worldPosition - gSpotLight.position);
        float32_t cosAngle = dot(spotLightDirectionOnSurface, gSpotLight.direction);
        float32_t falloffFactor = saturate((cosAngle - gSpotLight.cosAngle) / (1.0f - gSpotLight.cosAngle));

// 距離減衰 (PointLightと同様の計算式を適用)
        float32_t distanceSpot = length(gSpotLight.position - input.worldPosition);
        float32_t attenuationFactorSpot = pow(saturate(-distanceSpot / gSpotLight.distance + 1.0f), gSpotLight.decay);

// スポットライトによる拡散反射 (入射光の向きを考慮)
        float32_t3 L_spot = normalize(gSpotLight.position - input.worldPosition);
        float NdotL_spot = dot(normalize(input.normal), L_spot);
        float cosSpot = pow(NdotL_spot * 0.5f + 0.5f, 2.0f); // ハーフランバート

        float32_t3 diffuseSpotLight = gMaterial.color.rgb * textureColor.rgb * gSpotLight.color.rgb * cosSpot * gSpotLight.intensity * attenuationFactorSpot * falloffFactor;

// --- 最終合成の修正 ---
        output.color.rgb = diffuse + specular + diffusePointLight + specularPowPointLight + diffuseSpotLight;
        
      
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    
  
    return output;
}