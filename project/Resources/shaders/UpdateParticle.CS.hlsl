struct Particle {
    float32_t3 translate;
    float32_t3 scale;
    float32_t lifeTime;
    float32_t3 velocity;
    float32_t currentTime;
    float32_t4 color;
};

struct PerView {
    float32_t4x4 viewProjection;
    float32_t4x4 billboardMatrix;
    float32_t deltaTime;
    float32_t time;
    float32_t2 padding;
};

struct GPUFieldData {
    float32_t3 translate; // 力場の中心座標
    float32_t radius;     // 影響領域の半径
    uint32_t fieldType;   // 0: None, 1: Attractor(引き寄せ), 2: Vortex(渦), 3: Wind(風), 4: Drag(抵抗)
    float32_t strength;   // 強度
    float32_t3 direction; // Windの方向
    float32_t padding[2];
};

static const uint32_t kMaxParticles = 1024;

ConstantBuffer<PerView> gPerView : register(b0);
ConstantBuffer<GPUFieldData> gField : register(b1);
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;
    if (particleIndex < kMaxParticles)
    {
        // alphaが0のparticleは死んでいるとみなして更新しない
        if (gParticles[particleIndex].color.a != 0.0f)
        {
            // GPU Field 物理演算 (Attractor / Vortex / Wind / Drag)
            if (gField.fieldType != 0)
            {
                float32_t3 diff = gField.translate - gParticles[particleIndex].translate;
                float32_t dist = length(diff);
                if (dist < gField.radius && dist > 0.0001f)
                {
                    float32_t factor = 1.0f - (dist / gField.radius);
                    if (gField.fieldType == 1) // Attractor (引き寄せ)
                    {
                        float32_t3 dir = diff / dist;
                        gParticles[particleIndex].velocity += dir * (gField.strength * factor * gPerView.deltaTime);
                    }
                    else if (gField.fieldType == 2) // Vortex (渦運動)
                    {
                        float32_t3 up = float32_t3(0, 1, 0);
                        float32_t3 dir = diff / dist;
                        float32_t3 vortexDir = normalize(cross(up, dir));
                        gParticles[particleIndex].velocity += vortexDir * (gField.strength * factor * gPerView.deltaTime);
                    }
                    else if (gField.fieldType == 3) // Wind (風)
                    {
                        float32_t3 windDir = length(gField.direction) > 0.001f ? normalize(gField.direction) : float32_t3(1, 0, 0);
                        gParticles[particleIndex].velocity += windDir * (gField.strength * factor * gPerView.deltaTime);
                    }
                    else if (gField.fieldType == 4) // Drag (抵抗/減衰)
                    {
                        gParticles[particleIndex].velocity *= max(0.0f, 1.0f - gField.strength * gPerView.deltaTime);
                    }
                }
            }

            gParticles[particleIndex].translate += gParticles[particleIndex].velocity * gPerView.deltaTime;
            gParticles[particleIndex].currentTime += gPerView.deltaTime;
            float32_t alpha = 1.0f - (gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
            gParticles[particleIndex].color.a = saturate(alpha);

            // alphaが0になったので、ここはFreeとする
            if (gParticles[particleIndex].color.a == 0.0f)
            {
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
