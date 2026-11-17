struct GrassFieldConstants
{
    float32_t4x4 viewProj;
    float32_t3   baseColorRoot;
    float32_t    sssIntensity;
    float32_t3   baseColorTip;
    float32_t    maxDrawDistance;
    float32_t3   dryTipColor;
    float32_t    specularIntensity;
    float32_t3   fogColor;
    float32_t    fogDensity;
};
ConstantBuffer<GrassFieldConstants> gField : register(b0);

struct Camera
{
    float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

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

struct GrassInstance
{
    float32_t3 position;
    float32_t  height;
    float32_t  width;
    float32_t  yaw;
    float32_t  bend;
    float32_t  colorVariation;
};
StructuredBuffer<GrassInstance> gInstances : register(t0);

struct VSInput
{
    float32_t3 localPos : POSITION0;
    float32_t3 normal   : NORMAL0;
    float32_t  heightT  : TEXCOORD0;
    uint32_t   instId   : SV_InstanceID;
};

struct VSOutput
{
    float32_t4 position  : SV_POSITION;
    float32_t3 worldPos  : TEXCOORD0;
    float32_t3 normal    : NORMAL0;
    float32_t  heightT   : TEXCOORD1;
    float32_t  colorVar  : TEXCOORD2;
    float32_t  gustWave  : TEXCOORD3;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    GrassInstance inst = gInstances[input.instId];

    // カメラ距離による滑らかなLODスケール減衰（遠景ポップ防止）
    float32_t distToCam = length(gCamera.worldPosition - inst.position);
    float32_t distScale = 1.0f - smoothstep(gField.maxDrawDistance * 0.72f, gField.maxDrawDistance, distToCam);

    float32_t bladeH = inst.height * distScale;
    float32_t bladeW = inst.width * lerp(1.0f, 1.45f, 1.0f - distScale); // 遠景ではやや太くして密度感を維持
    float32_t t = input.heightT;
    float32_t bendMask = t * t;

    // ローカル回転 (yaw)
    float32_t c = cos(inst.yaw);
    float32_t s = sin(inst.yaw);

    float32_t3 scaledLocal = float32_t3(
        input.localPos.x * bladeW,
        input.localPos.y * bladeH,
        (input.localPos.z + inst.bend * bendMask) * bladeH);

    float32_t3 rotatedPos = float32_t3(
        scaledLocal.x * c + scaledLocal.z * s,
        scaledLocal.y,
        -scaledLocal.x * s + scaledLocal.z * c);

    float32_t3 rotatedNormal = normalize(float32_t3(
        input.normal.x * c + input.normal.z * s,
        input.normal.y,
        -input.normal.x * s + input.normal.z * c));

    // --- 1. 空間を吹き抜ける風波（Wind Gust）＋個体フラッター ---
    float32_t2 windDir = normalize(gWind.direction.xz + float32_t2(1e-4f, 0.0f));
    float32_t gustCoord = dot(inst.position.xz, windDir) * gWind.gustFrequency * 6.2831853f - gWind.time * gWind.windSpeed * 1.65f;
    float32_t crossWave = sin(inst.position.x * 0.37f - inst.position.z * 0.41f + gWind.time * 1.2f);
    float32_t gustRaw = sin(gustCoord) * 0.65f + sin(gustCoord * 0.53f + crossWave) * 0.35f;
    float32_t gust01 = gustRaw * 0.5f + 0.5f;

    float32_t flutter = sin(gWind.time * 6.2f + float32_t(input.instId % 97) * 0.73f + inst.position.x * 2.3f)
                      * gWind.turbulence * 0.08f;

    float32_t windBendAmount = (0.08f + gust01 * gWind.gustStrength * 0.55f + flutter) * bladeH * bendMask;
    float32_t2 totalPushXZ = windDir * windBendAmount;

    // --- 2. インタラクティブ押し分け（プレイヤーや風オーブが通った場所の草が倒れる） ---
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float32_t radius = gWind.repulsors[i].w;
        if (radius > 0.01f)
        {
            float32_t3 diff = inst.position - gWind.repulsors[i].xyz;
            float32_t horizDist = length(diff.xz);
            if (horizDist < radius && abs(diff.y) < radius * 1.5f)
            {
                float32_t falloff = 1.0f - horizDist / radius;
                falloff = falloff * falloff;
                float32_t2 pushDir = (horizDist > 1e-3f) ? (diff.xz / horizDist) : windDir;
                totalPushXZ += pushDir * (falloff * gWind.repulsorStrength * bladeH * 0.92f * bendMask);
            }
        }
    }

    // 葉の全長を保つアーク長補正（横に大きくしなるほど穂先が自然に沈む）
    float32_t pushLen = length(totalPushXZ);
    float32_t maxPush = bladeH * 0.88f;
    if (pushLen > maxPush && pushLen > 1e-4f)
    {
        totalPushXZ *= (maxPush / pushLen);
        pushLen = maxPush;
    }
    float32_t arcDrop = (bladeH > 1e-3f) ? (pushLen * pushLen) / (2.15f * bladeH) : 0.0f;

    float32_t3 worldPos = inst.position + rotatedPos;
    worldPos.xz += totalPushXZ;
    worldPos.y = max(inst.position.y + 0.01f * t, worldPos.y - arcDrop);

    // 風で倒れた方向に法線も傾けて、風の波頭が太陽光をキラッと反射するようにする
    float32_t3 worldNormal = normalize(rotatedNormal + float32_t3(totalPushXZ.x, 0.55f * bladeH, totalPushXZ.y) * 0.85f);

    output.position = mul(float4(worldPos, 1.0f), gField.viewProj);
    output.worldPos = worldPos;
    output.normal = worldNormal;
    output.heightT = t;
    output.colorVar = inst.colorVariation;
    output.gustWave = gust01;
    return output;
}
