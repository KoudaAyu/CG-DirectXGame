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
    
    float32_t3 reflectLight = reflect(gDirectionalLight.direction,normalize(input.normal));



    if (textureColor.a <= 0.1 || output.color.a == 0.0)
    {
        discard;
    }
    
if (gMaterial.enableLighting != 0)
{
    // 1. 光の向きを反転させて反射ベクトルを計算
    float32_t3 reflectLight = reflect(normalize(gDirectionalLight.direction), normalize(input.normal));
    float RdotE = dot(reflectLight, toEye);
    
    // 2. 鏡面反射の強さ
    float specularPow = pow(saturate(RdotE), gMaterial.shininess);

    // 3. ハーフランバート
    float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
    
    // 4. 拡散反射
    float32_t3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
    
    // 5. 鏡面反射 (saturate(NdotL)を掛けて裏側の漏れを防ぐ)
    float32_t3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * saturate(NdotL);

    // 6. 合体！
    output.color.rgb = diffuse + specular;
}
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    
  
    return output;
}