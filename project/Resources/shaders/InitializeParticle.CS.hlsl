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

float rand(float3 co) {
    return frac(sin(dot(co, float3(12.9898, 78.233, 45.164))) * 43758.5453);
}

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;
    if(particleIndex < kMaxParticles)
    {
        gParticles[particleIndex] = (Particle)0;
        
        float seed = float(particleIndex);
        float r1 = rand(float3(seed, 1.0f, 2.0f));
        float r2 = rand(float3(seed, 3.0f, 4.0f));
        float r3 = rand(float3(seed, 5.0f, 6.0f));
        float r4 = rand(float3(seed, 7.0f, 8.0f));
        
        // Random velocity between -1.0 and 1.0
        gParticles[particleIndex].velocity = float32_t3(r1 * 2.0f - 1.0f, r2 * 2.0f - 1.0f, r3 * 2.0f - 1.0f);
        
        // Random lifetime between 1.0 and 3.0 seconds
        gParticles[particleIndex].lifeTime = 1.0f + r4 * 2.0f;
        gParticles[particleIndex].currentTime = 0.0f;
        gParticles[particleIndex].scale = float32_t3(0.5f, 0.5f, 0.5f);
        gParticles[particleIndex].color = float32_t4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}
