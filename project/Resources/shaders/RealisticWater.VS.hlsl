struct RealisticWaterConstants
{
    float32_t4x4 world;
    float32_t4x4 viewProj;
    float32_t4x4 invViewProj;
    float32_t3   shallowColor;
    float32_t    time;
    float32_t3   deepColor;
    float32_t    waveHeight;
    float32_t3   absorptionCoeff;
    float32_t    flowSpeed;
    float32_t2   flowDirection;
    float32_t    refractionScale;
    float32_t    causticsIntensity;
    float32_t    foamWidth;
    float32_t    foamIntensity;
    float32_t    nearZ;
    float32_t    farZ;
    float32_t3   fogColor;
    float32_t    fogDensity;
};
ConstantBuffer<RealisticWaterConstants> gWater : register(b0);

struct VSInput
{
    float32_t3 position : POSITION0;
    float32_t2 uv       : TEXCOORD0;
};

struct VSOutput
{
    float32_t4 position  : SV_POSITION;
    float32_t3 worldPos  : TEXCOORD0;
    float32_t3 normal    : NORMAL0;
    float32_t4 clipPos   : TEXCOORD1;
    float32_t  crest     : TEXCOORD2;
};

void AccumulateGerstnerWave(
    float32_t2 baseXZ,
    float32_t2 dir,
    float32_t steepness,
    float32_t wavelength,
    float32_t ampScale,
    float32_t speedScale,
    inout float32_t3 offset,
    inout float32_t3 tangent,
    inout float32_t3 binormal,
    inout float32_t crestAcc)
{
    float32_t k = 6.2831853f / max(wavelength, 0.1f);
    float32_t c = sqrt(9.8f / k) * speedScale;
    float32_t2 d = normalize(dir);
    float32_t a = gWater.waveHeight * ampScale;
    float32_t q = saturate(steepness) / max(k * a * 4.0f, 1e-3f);
    q = min(q, 0.22f);

    float32_t f = k * (dot(d, baseXZ) - c * gWater.time * max(0.25f, gWater.flowSpeed));
    float32_t sinF = sin(f);
    float32_t cosF = cos(f);

    offset.x += d.x * (q * a * cosF);
    offset.y += a * sinF;
    offset.z += d.y * (q * a * cosF);

    tangent  += float32_t3(-d.x * d.x * q * k * a * sinF, d.x * k * a * cosF, -d.x * d.y * q * k * a * sinF);
    binormal += float32_t3(-d.x * d.y * q * k * a * sinF, d.y * k * a * cosF, -d.y * d.y * q * k * a * sinF);
    crestAcc += sinF * ampScale;
}

VSOutput main(VSInput input)
{
    VSOutput output;

    float32_t3 baseWorld = mul(float4(input.position, 1.0f), gWater.world).xyz;
    float32_t2 flow = normalize(gWater.flowDirection + float32_t2(1e-4f, 0.0f));
    float32_t2 crossFlow = float32_t2(-flow.y, flow.x);

    float32_t3 disp = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t3 tangent = float32_t3(1.0f, 0.0f, 0.0f);
    float32_t3 binormal = float32_t3(0.0f, 0.0f, 1.0f);
    float32_t crest = 0.0f;

    if (gWater.waveHeight > 1e-4f)
    {
        AccumulateGerstnerWave(baseWorld.xz, flow + crossFlow * 0.25f, 0.45f, 4.8f, 0.50f, 0.85f, disp, tangent, binormal, crest);
        AccumulateGerstnerWave(baseWorld.xz, flow - crossFlow * 0.42f, 0.40f, 2.9f, 0.32f, 1.05f, disp, tangent, binormal, crest);
        AccumulateGerstnerWave(baseWorld.xz, crossFlow * 0.8f + flow * 0.5f, 0.35f, 1.65f, 0.18f, 1.25f, disp, tangent, binormal, crest);
        AccumulateGerstnerWave(baseWorld.xz, -crossFlow * 0.6f + flow * 0.8f, 0.30f, 0.95f, 0.10f, 1.40f, disp, tangent, binormal, crest);
    }

    float32_t3 worldPos = baseWorld + disp;
    float32_t3 normal = normalize(cross(binormal, tangent));
    float32_t4 clipPos = mul(float4(worldPos, 1.0f), gWater.viewProj);

    output.position = clipPos;
    output.worldPos = worldPos;
    output.normal = normal;
    output.clipPos = clipPos;
    output.crest = saturate(crest * 0.5f + 0.5f);
    return output;
}
