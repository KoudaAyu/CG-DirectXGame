#include "Resources/shaders/Object3d.hlsli"
#include "Resources/shaders/Material.hlsli"

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
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSample, transformedUV.xy);
    output.color = gMaterial.color * textureColor;

    float32_t3 normal = normalize(input.normal);
    float32_t3 lightDirection = normalize(-gDirectionalLight.direction);
    float NdotL = dot(normal, lightDirection);

    if (textureColor.a <= gMaterial.alphaThreshold || output.color.a <= gMaterial.alphaThreshold)
    {
        discard;
    }
    
if (gMaterial.enableLighting != 0)
{
    float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
    float specularPow = CalculateSpecularPow(gMaterial, normal, lightDirection, toEye);
    
    float cos = pow(NdotL * 0.5f + 0.5f, HALF_LAMBERT_EXPONENT);

    float32_t3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
    
    float32_t3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * saturate(NdotL) * gMaterial.specularIntensity;

    output.color.rgb = diffuse + specular;
}
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    
  
    return output;
}