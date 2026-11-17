struct BotanicalDrawConstants
{
    float32_t4x4 world;
    float32_t4x4 viewProj;
    float32_t4x4 worldInvTranspose;
    float32_t4   baseTint;
    float32_t    waterLevel;
    float32_t    wetShoreHeight;
    float32_t    mossIntensity;
    float32_t    sssIntensity;
    float32_t3   fogColor;
    float32_t    fogDensity;
};
ConstantBuffer<BotanicalDrawConstants> gDraw : register(b0);

struct WindForGPU
{
    float32_t3 direction;
    float32_t  time;
    float32_t  windSpeed;
    float32_t  gustStrength;
    float32_t  gustFrequency;
    float32_t  turbulence;
    float32_t4 repulsors[4];
    float32_t  repulsorStrength;
    float32_t3 padding;
};
ConstantBuffer<WindForGPU> gWind : register(b3);

struct VSInput
{
    float32_t3 position : POSITION0;
    float32_t3 normal   : NORMAL0;
    float32_t2 uv       : TEXCOORD0;
    float32_t4 color    : COLOR0;
    float32_t4 params   : TEXCOORD1; // x: windWeight, y: sss, z: moss, w: ao
};

struct VSOutput
{
    float32_t4 position  : SV_POSITION;
    float32_t3 worldPos  : TEXCOORD0;
    float32_t3 normal    : NORMAL0;
    float32_t2 uv        : TEXCOORD1;
    float32_t4 color     : COLOR0;
    float32_t4 params    : TEXCOORD2;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float32_t3 worldPos = mul(float4(input.position, 1.0f), gDraw.world).xyz;
    float32_t3 worldNormal = normalize(mul(input.normal, (float32_t3x3)gDraw.worldInvTranspose));

    float32_t windW = input.params.x;
    if (windW > 0.001f)
    {
        float32_t2 windDirXZ = normalize(gWind.direction.xz + float32_t2(1e-4f, 0.0f));
        float32_t phase = dot(worldPos.xz, windDirXZ) * gWind.gustFrequency * 6.2831853f - gWind.time * gWind.windSpeed * 1.4f;
        float32_t gust = sin(phase) * 0.6f + sin(phase * 0.47f + 1.3f) * 0.4f;
        float32_t swayAmp = (0.12f + (gust * 0.5f + 0.5f) * gWind.gustStrength * 0.36f) * windW;

        // 葉先の高周波フラッター（ざわめき）
        float32_t flutter = sin(gWind.time * 6.8f + worldPos.x * 4.7f + worldPos.y * 6.1f + worldPos.z * 3.9f)
                          * gWind.turbulence * 0.085f * saturate(windW * 1.3f);

        worldPos.xz += windDirXZ * swayAmp;
        worldPos.x += flutter;
        worldPos.z -= flutter * 0.7f;
        worldPos.y -= swayAmp * swayAmp * 0.28f;
    }

    output.position = mul(float4(worldPos, 1.0f), gDraw.viewProj);
    output.worldPos = worldPos;
    output.normal = worldNormal;
    output.uv = input.uv;
    output.color = input.color;
    output.params = input.params;
    return output;
}
