struct Particle {
    float32_t3 translate;
    float32_t lifeTime;
    float32_t3 scale;
    float32_t currentTime;
    float32_t3 velocity;
    float32_t padding;
    float32_t4 color;
};

static const uint32_t kMaxParticles = 10240;

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;
    if(particleIndex < kMaxParticles)
    {
        gParticles[particleIndex] = (Particle)0;
        gFreeList[particleIndex] = particleIndex;
    }
    
    // スレッド 0 で Index が末尾 (kMaxParticles - 1) を指すように初期化
    if (particleIndex == 0)
    {
        gFreeListIndex[0] = kMaxParticles - 1;
    }
}
