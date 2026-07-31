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

static const uint32_t kMaxParticles = 10240;

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
        if (gParticles[particleIndex].color.a != 0.0f)
        {
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
