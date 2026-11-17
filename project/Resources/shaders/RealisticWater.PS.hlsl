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

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float32_t  intensity;
};

struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float32_t  intensity;
    float32_t  radius;
    float32_t  decay;
    float32_t2 padding;
};

struct LightGroup
{
    DirectionalLight directionalLight;
    PointLight       pointLight;
};
ConstantBuffer<LightGroup> gLightGroup : register(b1);

struct Camera
{
    float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

Texture2D<float32_t4>   gRefractionColor : register(t0);
Texture2D<float32_t>    gRefractionDepth : register(t1);
TextureCube<float32_t4> gEnvironmentMap  : register(t2);

SamplerState gLinearClamp : register(s0);
SamplerState gPointClamp  : register(s1);
SamplerState gLinearWrap  : register(s2);

struct PSInput
{
    float32_t4 position : SV_POSITION;
    float32_t3 worldPos : TEXCOORD0;
    float32_t3 normal   : NORMAL0;
    float32_t4 clipPos  : TEXCOORD1;
    float32_t  crest    : TEXCOORD2;
};

float32_t LinearizeDepth(float32_t ndcDepth)
{
    float32_t n = gWater.nearZ;
    float32_t f = gWater.farZ;
    return (n * f) / max(f - ndcDepth * (f - n), 1e-5f);
}

float32_t3 ReconstructWorldPos(float32_t2 uv, float32_t ndcDepth)
{
    float32_t4 ndc = float32_t4(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f, ndcDepth, 1.0f);
    float32_t4 worldH = mul(ndc, gWater.invViewProj);
    return worldH.xyz / max(abs(worldH.w), 1e-6f);
}

float32_t Hash21(float32_t2 p)
{
    p = frac(p * float32_t2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float32_t Noise2(float32_t2 p)
{
    float32_t2 i = floor(p);
    float32_t2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float32_t a = Hash21(i);
    float32_t b = Hash21(i + float32_t2(1.0f, 0.0f));
    float32_t c = Hash21(i + float32_t2(0.0f, 1.0f));
    float32_t d = Hash21(i + float32_t2(1.0f, 1.0f));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

// 水底に投影される2層干渉コースティクス模様
float32_t EvaluateCaustics(float32_t2 p, float32_t t)
{
    float32_t2 uv1 = p * 1.45f + float32_t2(t * 0.35f, -t * 0.28f);
    float32_t2 uv2 = float32_t2(p.x * 0.8f - p.y * 0.6f, p.x * 0.6f + p.y * 0.8f) * 1.85f - float32_t2(t * 0.24f, t * 0.41f);

    float32_t w1 = abs(sin(uv1.x * 3.1f + sin(uv1.y * 2.7f + t)) + cos(uv1.y * 3.4f - cos(uv1.x * 2.3f - t))) * 0.5f;
    float32_t w2 = abs(sin(uv2.x * 3.5f - cos(uv2.y * 2.9f - t)) + cos(uv2.y * 3.1f + sin(uv2.x * 2.6f + t))) * 0.5f;

    float32_t net1 = pow(saturate(1.0f - w1), 4.5f);
    float32_t net2 = pow(saturate(1.0f - w2), 4.5f);
    return saturate((net1 + net2) * 1.6f);
}

// 多重スクロールさざ波（Capillary Ripples）によるプロシージャル法線勾配
float32_t2 SampleRippleSlope(float32_t2 worldXZ, float32_t2 flowDir, float32_t t, float32_t distFade)
{
    float32_t2 scrollA = worldXZ * 1.35f - flowDir * (t * 0.85f);
    float32_t2 scrollB = worldXZ * 2.90f - flowDir * (t * 1.35f) + float32_t2(3.7f, -2.1f);
    float32_t2 scrollC = worldXZ * 6.20f + float32_t2(flowDir.y, -flowDir.x) * (t * 0.65f);

    float32_t eps = 0.08f;
    float32_t hC = Noise2(scrollA) * 0.55f + Noise2(scrollB) * 0.30f + Noise2(scrollC) * 0.15f * distFade;
    float32_t hX = Noise2(scrollA + float32_t2(eps, 0.0f)) * 0.55f + Noise2(scrollB + float32_t2(eps * 2.1f, 0.0f)) * 0.30f + Noise2(scrollC + float32_t2(eps * 4.5f, 0.0f)) * 0.15f * distFade;
    float32_t hZ = Noise2(scrollA + float32_t2(0.0f, eps)) * 0.55f + Noise2(scrollB + float32_t2(0.0f, eps * 2.1f)) * 0.30f + Noise2(scrollC + float32_t2(0.0f, eps * 4.5f)) * 0.15f * distFade;

    return float32_t2(hC - hX, hC - hZ) * (2.8f * (0.25f + 0.75f * distFade));
}

float32_t4 main(PSInput input) : SV_TARGET0
{
    float32_t3 toCam = gCamera.worldPosition - input.worldPos;
    float32_t distToCam = length(toCam);
    float32_t3 V = toCam / max(distToCam, 1e-4f);
    float32_t3 L = normalize(-gLightGroup.directionalLight.direction);
    float32_t3 lightColor = gLightGroup.directionalLight.color.rgb * gLightGroup.directionalLight.intensity;

    // 遠景エイリアシング抑制フェード
    float32_t detailFade = 1.0f - smoothstep(14.0f, 65.0f, distToCam);

    // --- 1. Gerstner法線 + 多層さざ波マイクロ法線の合成 ---
    float32_t2 flowDir = normalize(gWater.flowDirection + float32_t2(1e-4f, 0.0f));
    float32_t t = gWater.time * max(0.15f, gWater.flowSpeed);
    float32_t2 rippleSlope = SampleRippleSlope(input.worldPos.xz, flowDir, t, detailFade);

    float32_t3 N = normalize(input.normal + float32_t3(rippleSlope.x * 0.42f, 0.0f, rippleSlope.y * 0.42f));

    // --- 2. スクリーン空間UVと線形水深の取得 ---
    float32_t invW = 1.0f / max(input.clipPos.w, 1e-5f);
    float32_t2 ndcXY = input.clipPos.xy * invW;
    float32_t2 screenUV = clamp(float32_t2(ndcXY.x * 0.5f + 0.5f, -ndcXY.y * 0.5f + 0.5f), 0.001f, 0.999f);
    float32_t waterNdcZ = saturate(input.clipPos.z * invW);
    float32_t waterLinearZ = LinearizeDepth(waterNdcZ);

    float32_t rawSceneNdcZ = gRefractionDepth.SampleLevel(gPointClamp, screenUV, 0);
    float32_t rawSceneLinearZ = LinearizeDepth(rawSceneNdcZ);
    float32_t baseViewDepthDiff = max(0.0f, rawSceneLinearZ - waterLinearZ);

    // --- 3. 前景にじみ防止付きの水中屈折 (Screen-Space Refraction with Foreground Rejection) ---
    float32_t2 refractOffset = N.xz * gWater.refractionScale * saturate(baseViewDepthDiff * 1.8f);
    // 画面端でのクランプ折り返し防止
    float32_t edgeGuard = smoothstep(0.0f, 0.06f, min(min(screenUV.x, 1.0f - screenUV.x), min(screenUV.y, 1.0f - screenUV.y)));
    float32_t2 refractedUV = clamp(screenUV + refractOffset * edgeGuard, 0.001f, 0.999f);

    float32_t refractedNdcZ = gRefractionDepth.SampleLevel(gPointClamp, refractedUV, 0);
    float32_t refractedLinearZ = LinearizeDepth(refractedNdcZ);

    // もし歪ませた先が水面より手前の物体（水上に出ている岩や草）だった場合は歪みをキャンセル
    if (refractedLinearZ < waterLinearZ - 0.005f)
    {
        refractedUV = screenUV;
        refractedNdcZ = rawSceneNdcZ;
        refractedLinearZ = rawSceneLinearZ;
    }

    float32_t3 sceneColor = gRefractionColor.SampleLevel(gLinearClamp, refractedUV, 0).rgb;
    float32_t3 underwaterWorldPos = ReconstructWorldPos(refractedUV, refractedNdcZ);

    // 水面から川底までの実際の光学距離と鉛直水深
    float32_t opticalPath = max(0.0f, refractedLinearZ - waterLinearZ);
    float32_t verticalDepth = max(0.0f, input.worldPos.y - underwaterWorldPos.y);
    float32_t effectiveDepth = min(opticalPath, verticalDepth * 1.65f + 0.02f);

    // --- 4. 水底へのプロシージャル太陽コースティクス（集光模様）投影 ---
    if (gWater.causticsIntensity > 0.001f && effectiveDepth > 0.01f)
    {
        float32_t2 sunProjXZ = underwaterWorldPos.xz - L.xz * (underwaterWorldPos.y / max(abs(L.y), 0.25f));
        float32_t causticPattern = EvaluateCaustics(sunProjXZ, t * 1.15f);
        float32_t causticDepthMask = smoothstep(0.02f, 0.22f, verticalDepth) * exp(-verticalDepth * 0.85f);
        sceneColor += lightColor * causticPattern * causticDepthMask * gWater.causticsIntensity * 0.55f;
    }

    // --- 5. Beer-Lambert の法則による波長別光吸収（赤が速く消え、浅瀬は透明→エメラルド→深紺へ遷移） ---
    float32_t3 transmittance = exp(-gWater.absorptionCoeff * effectiveDepth);
    float32_t depthTintFactor = 1.0f - exp(-effectiveDepth * 0.95f);
    float32_t3 volumeScatterColor = lerp(gWater.shallowColor, gWater.deepColor, depthTintFactor)
                                  * (0.32f + 0.68f * saturate(L.y * 0.7f + 0.3f))
                                  * gLightGroup.directionalLight.color.rgb;

    float32_t3 underwaterResult = sceneColor * transmittance + volumeScatterColor * (1.0f - transmittance);

    // --- 6. 波頭の逆光サブサーフェス・スキャタリング（エメラルド透過光） ---
    float32_t waveBacklight = pow(saturate(dot(V, -L)), 3.5f) * input.crest * saturate(1.0f - V.y);
    underwaterResult += gWater.shallowColor * lightColor * waveBacklight * 0.45f;

    // --- 7. フレネル反射（Skybox キューブマップ + 大気グラデーション + GGX太陽スペキュラきらめき） ---
    float32_t NdotV = saturate(dot(N, V));
    float32_t fresnel = 0.02f + 0.98f * pow(1.0f - NdotV, 5.0f);

    float32_t3 R = reflect(-V, N);
    R.y = max(R.y, 0.01f); // 水面下への反射ベクトル潜り込み防止
    R = normalize(R);
    float32_t3 envSample = gEnvironmentMap.Sample(gLinearWrap, R).rgb;

    // 太陽色と調和する大気スカイグラデーションをブレンドして自然な空映りを作る
    float32_t3 zenithSky = float32_t3(0.22f, 0.48f, 0.85f) * gLightGroup.directionalLight.color.rgb;
    float32_t3 horizonSky = gWater.fogColor;
    float32_t3 proceduralSky = lerp(horizonSky, zenithSky, pow(saturate(R.y), 0.65f));
    float32_t3 reflectionColor = lerp(envSample, proceduralSky, 0.48f);

    // 太陽のきらめき（GGX Microfacet Specular + 広いグレア）
    float32_t3 H = normalize(L + V);
    float32_t NdotH = saturate(dot(N, H));
    float32_t NdotL = saturate(dot(N, L));
    float32_t roughness = lerp(0.055f, 0.14f, 1.0f - detailFade);
    float32_t a2 = roughness * roughness * roughness * roughness;
    float32_t denom = (NdotH * NdotH * (a2 - 1.0f) + 1.0f);
    float32_t D = a2 / max(3.14159265f * denom * denom, 1e-5f);
    float32_t3 sunGlitter = lightColor * min(D * 0.22f, 12.0f) * NdotL;

    float32_t3 surfaceColor = lerp(underwaterResult, reflectionColor, saturate(fresnel * 0.92f)) + sunGlitter;

    // --- 8. 深度交差による岸辺・水中岩まわりの自動白泡 (Depth-Intersection Shore & Obstacle Foam) ---
    if (gWater.foamWidth > 0.001f && gWater.foamIntensity > 0.001f)
    {
        // 非歪み深度から正確な水際距離を取得（岩・橋脚・砂浜すべてに自動対応！）
        float32_t3 unrefractedWorld = ReconstructWorldPos(screenUV, rawSceneNdcZ);
        float32_t shoreVerticalDepth = max(0.0f, input.worldPos.y - unrefractedWorld.y);

        float32_t shoreBand = 1.0f - smoothstep(0.0f, gWater.foamWidth, shoreVerticalDepth);
        float32_t2 foamUV = input.worldPos.xz * 2.6f - flowDir * (t * 0.9f);
        float32_t foamCell = Noise2(foamUV) * 0.6f + Noise2(foamUV * 2.7f + float32_t2(t * 0.4f, -t * 0.3f)) * 0.4f;

        // 水際のさざ波リング＋有機的な泡レース模様
        float32_t shoreRipples = sin(shoreVerticalDepth / max(gWater.foamWidth, 0.05f) * 18.0f - gWater.time * 3.8f) * 0.5f + 0.5f;
        float32_t contactEdge = 1.0f - smoothstep(0.0f, gWater.foamWidth * 0.22f, shoreVerticalDepth);
        float32_t foamMask = saturate(contactEdge * 0.95f + shoreBand * smoothstep(0.36f, 0.78f, foamCell * 0.7f + shoreRipples * 0.45f));

        // 波が高いときの白波（Whitecaps）も加算
        float32_t whitecap = smoothstep(0.76f, 0.98f, input.crest) * saturate(gWater.waveHeight * 6.0f) * foamCell;
        foamMask = saturate((foamMask + whitecap * 0.55f) * gWater.foamIntensity);

        float32_t3 litFoamColor = float32_t3(0.94f, 0.97f, 1.0f) * (float32_t3(0.38f, 0.46f, 0.56f) + lightColor * 0.68f);
        surfaceColor = lerp(surfaceColor, litFoamColor, foamMask);
    }

    // --- 9. 最浅部（水深 0〜2cm）のマイクロエッジソフトブレンド ---
    float32_t ultraShallowFade = smoothstep(0.0f, 0.025f, baseViewDepthDiff);
    float32_t3 rawGround = gRefractionColor.SampleLevel(gLinearClamp, screenUV, 0).rgb;
    surfaceColor = lerp(rawGround, surfaceColor, ultraShallowFade);

    // --- 10. 大気遠近フォグ ---
    float32_t fogFactor = 1.0f - exp(-max(0.0f, distToCam - 12.0f) * gWater.fogDensity);
    surfaceColor = lerp(surfaceColor, gWater.fogColor, saturate(fogFactor));

    return float32_t4(surfaceColor, 1.0f);
}
