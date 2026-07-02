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

ConstantBuffer<PerView> gPerView : register(b0);
RWStructuredBuffer<Particle> gParticles : register(u0);

float rand(float3 co) {
    return frac(sin(dot(co, float3(12.9898, 78.233, 45.164))) * 43758.5453);
}

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;
    if(particleIndex < gPerView.maxParticles)
    {
        Particle p = gParticles[particleIndex];
        
        p.currentTime += gPerView.deltaTime;
        if (p.currentTime >= p.lifeTime)
        {
            // Respawn particle
            p.currentTime = 0.0f;
            
            float seed = float(particleIndex) + gPerView.time;
            float r1 = rand(float3(seed, 1.0f, 2.0f));
            float r2 = rand(float3(seed, 3.0f, 4.0f));
            float r3 = rand(float3(seed, 5.0f, 6.0f));
            float r4 = rand(float3(seed, 7.0f, 8.0f));
            
            // Random start position around center
            p.translate = float32_t3(r1 * 0.4f - 0.2f, r2 * 0.4f - 0.2f, r3 * 0.4f - 0.2f);
            
            // Random velocity
            p.velocity = float32_t3(r2 * 2.0f - 1.0f, 1.0f + r3 * 2.0f, r1 * 2.0f - 1.0f);
            
            // Random life time
            p.lifeTime = 1.0f + r4 * 2.0f;
            p.scale = float32_t3(0.5f, 0.5f, 0.5f);
            p.color = float32_t4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        else
        {
            // Move particle
            p.translate += p.velocity * gPerView.deltaTime;
            // Fade out alpha
            p.color.a = 1.0f - (p.currentTime / p.lifeTime);
        }
        
        gParticles[particleIndex] = p;
    }
}
