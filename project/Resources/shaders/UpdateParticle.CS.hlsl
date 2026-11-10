struct Particle {
    float32_t3 translate;
    float32_t lifeTime;
    float32_t3 scale;
    float32_t currentTime;
    float32_t3 velocity;
    float32_t padding;
    float32_t4 color;
};

struct PerView {
    float32_t4x4 viewProjection;
    float32_t4x4 billboardMatrix;
    float32_t deltaTime;
    float32_t time;
    uint32_t maxParticles;
    float32_t padding;
};

static const uint32_t kMaxParticles = 1024;

ConstantBuffer<PerView> gPerView : register(b0);
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;
    if (particleIndex < gPerView.maxParticles)

    {
        // alphaが0のparticleは死んでいるとみなして更新しない
        if (gParticles[particleIndex].color.a > 0.0f)
        {
            float32_t dt = gPerView.deltaTime;
            gParticles[particleIndex].currentTime += dt;
            float32_t progress = gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime;

            // 1. 空気抵抗（垂直上昇を自然に風速へ漸近させ、上空で風下に大きくたなびかせる）：
            float32_t horizDrag = max(0.0f, 1.0f - 0.22f * dt);
            float32_t vertDrag  = max(0.0f, 1.0f - 0.38f * dt);
            gParticles[particleIndex].velocity.x *= horizDrag;
            gParticles[particleIndex].velocity.z *= horizDrag;
            gParticles[particleIndex].velocity.y *= vertDrag;

            // 2. 熱気浮力（ヘリ進入高度 4.5〜6.5m まで力強く立ち昇る）：
            float32_t buoyancy = lerp(1.25f, 0.08f, saturate(progress * 1.5f));
            gParticles[particleIndex].velocity.y += buoyancy * dt;

            // 3. 戦場の環境風によるたなびき（ヘリパイロットに風向を知らせる誘導風帯）：
            float32_t windFactor = saturate(progress * 1.8f);
            float32_t windX = 0.55f * windFactor;
            float32_t windZ = 0.65f * windFactor;
            gParticles[particleIndex].velocity.x += windX * dt;
            gParticles[particleIndex].velocity.z += windZ * dt;

            // 4. 高周波3D乱流渦（Multi-Frequency Micro Turbulence）：
            // 主乱流＋細かなマイクロ渦の2重合成により、きめ細やかな煙の流れを実現
            float32_t3 pos1 = gParticles[particleIndex].translate * 1.8f;
            float32_t3 pos2 = gParticles[particleIndex].translate * 3.6f;
            float32_t timeVal = gPerView.time * 1.1f;
            
            float32_t3 curl;
            curl.x = sin(pos1.y * 2.2f + timeVal) * cos(pos1.z * 1.8f + 1.2f)
                   + sin(pos2.y * 3.1f + timeVal * 1.5f) * cos(pos2.z * 2.7f) * 0.45f;
            curl.y = sin(pos1.z * 2.0f + timeVal * 0.9f) * cos(pos1.x * 2.4f + 2.3f)
                   + sin(pos2.z * 2.8f + timeVal * 1.3f) * cos(pos2.x * 3.3f) * 0.45f;
            curl.z = sin(pos1.x * 1.7f + timeVal * 1.2f) * cos(pos1.y * 1.9f + 0.7f)
                   + sin(pos2.x * 2.5f + timeVal * 1.4f) * cos(pos2.y * 2.9f) * 0.45f;

            // 乱流の強さ
            float32_t turbIntensity = lerp(0.18f, 0.55f, saturate(progress * 1.6f));

            // 座標更新
            gParticles[particleIndex].translate += (gParticles[particleIndex].velocity + curl * turbIntensity) * dt;

            // スモークグレネードの地面這い（Ground Interaction / 地面接地で水平へ滑る）：
            // 地面（Y < 0.04m）に当たった煙は地面下に潜らず、地面上を低く水平に広がる
            if (gParticles[particleIndex].translate.y < 0.04f)
            {
                gParticles[particleIndex].translate.y = 0.04f;
                gParticles[particleIndex].velocity.y = abs(gParticles[particleIndex].velocity.y) * 0.15f;
            }

            // 5. スモークグレネードの上空膨張成長（大煙塊・中パフ・外縁ウィスプの3階層スケール）：
            float32_t tierRand = frac(sin(float32_t(particleIndex) * 91.127f) * 43758.5453f);
            float32_t particleMaxScale;
            float32_t initScale;
            float32_t baseAlpha;
            if (tierRand < 0.50f)
            {
                // 主煙塊（50%）：大きく濃密な主煙柱（ヘリ上空からも明瞭に視認できるボリューム）
                initScale = 0.11f;
                particleMaxScale = 0.88f + frac(sin(float32_t(particleIndex) * 45.164f) * 43758.5453f) * 0.20f;
                baseAlpha = 0.98f;
            }
            else if (tierRand < 0.82f)
            {
                // サブパフ（32%）：中型のモコモコした凹凸パフ
                initScale = 0.08f;
                particleMaxScale = 0.68f + frac(sin(float32_t(particleIndex) * 45.164f) * 43758.5453f) * 0.14f;
                baseAlpha = 0.90f;
            }
            else
            {
                // 外縁ウィスプ（18%）：風下へ長くたなびくベール
                initScale = 0.06f;
                particleMaxScale = 0.48f + frac(sin(float32_t(particleIndex) * 45.164f) * 43758.5453f) * 0.10f;
                baseAlpha = 0.70f;
            }

            float32_t growthCurve = pow(saturate(progress), 0.55f);
            float32_t currentScale = lerp(initScale, particleMaxScale, growthCurve);
            gParticles[particleIndex].scale = float32_t3(currentScale, currentScale, currentScale);

            // 6. 有機的なミリタリータクティカルグリーンの色シフト：
            // ノズルコア：発光感のある高輝度ライムグリーン
            // 煙柱中部：濃密な戦術ミリタリーグリーン
            // 拡散上空：大気に溶け込む深みのあるフォレストスモークグリーン
            float32_t3 coreGreen    = float32_t3(0.12f, 1.00f, 0.36f);
            float32_t3 midGreen     = float32_t3(0.08f, 0.90f, 0.28f);
            float32_t3 diffuseGreen = float32_t3(0.04f, 0.68f, 0.20f);

            float32_t3 finalColor;
            if (progress < 0.25f)
            {
                finalColor = lerp(coreGreen, midGreen, progress / 0.25f);
            }
            else
            {
                finalColor = lerp(midGreen, diffuseGreen, (progress - 0.25f) / 0.75f);
            }
            gParticles[particleIndex].color.rgb = finalColor;

            // 7. スモークグレネードの高密度遮蔽アルファフェード：
            // 展開中は濃密な煙幕を持続（視界遮断）、寿命末期（progress > 0.70）で滑らかに消散
            float32_t fadeProgress = saturate((progress - 0.70f) / 0.30f);
            float32_t alpha = lerp(baseAlpha, 0.0f, pow(fadeProgress, 1.4f));
            gParticles[particleIndex].color.a = alpha;

            // 寿命終了またはalphaが0になったので、ここはFreeとする
            if (gParticles[particleIndex].color.a <= 0.01f || progress >= 1.0f)
            {
                gParticles[particleIndex].color.a = 0.0f;
                // スケールに0を入れておいてVertexShader出力で棄却されるようにする
                gParticles[particleIndex].scale = float32_t3(0.0f, 0.0f, 0.0f);
                int32_t freeListIndex;
                InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
                // 最新のFreeListIndexの場所に死んだParticleのIndexを設定する
                if ((freeListIndex + 1) < kMaxParticles)
                {
                    gFreeList[freeListIndex + 1] = particleIndex;
                }
                else
                {
                    // 安全策
                    InterlockedAdd(gFreeListIndex[0], -1);
                }
            }
        }
    }
}
