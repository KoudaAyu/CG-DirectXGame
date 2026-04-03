static const int SHADING_MODEL_PHONG = 0;
static const int SHADING_MODEL_BLINN_PHONG = 1;
static const float HALF_LAMBERT_EXPONENT = 2.0f;

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t3 padding;
    float32_t4x4 uvTransform;
    float32_t shininess;
    int32_t shadingModel;
    float32_t alphaThreshold;
    float32_t specularIntensity;
};

float CalculateSpecularPow(Material material, float32_t3 normal, float32_t3 lightDirection, float32_t3 toEye)
{
    if (material.shadingModel == SHADING_MODEL_PHONG)
    {
        float32_t3 reflectedLight = reflect(-lightDirection, normal);
        return pow(saturate(dot(reflectedLight, toEye)), material.shininess);
    }

    float32_t3 halfVector = normalize(lightDirection + toEye);
    return pow(saturate(dot(normal, halfVector)), material.shininess);
}
