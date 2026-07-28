struct Particle {
    float32_t3 translate;
    float32_t3 scale;
    float32_t lifeTime;
    float32_t3 velocity;
    float32_t currentTime;
    float32_t4 color;
};

struct EmitterData {
    float32_t3 translate;    // 位置
    float32_t radius;        // 発生半径
    uint32_t count;          // 射出数
    float32_t frequency;     // 発生頻度
    float32_t frequencyTime; // 使用頻度
    uint32_t emit;           // 射出許可
    uint32_t emitterType;    // 0: Point, 1: Box, 2: Sphere, 3: Cone
    float32_t initialSpeed;  // 初速
    float32_t3 boxSize;      // Box領域サイズ
    float32_t coneAngle;     // Cone照射角度 (ラジアン)
    float32_t3 direction;    // Direction / Cone向き
    float32_t particleLifeTime; // パーティクル寿命
    float32_t4 particleColor;   // パーティクルカラー
};

struct PerFrame {
    float32_t time;
    float32_t deltaTime;
};

static const uint32_t kMaxParticles = 1024;

ConstantBuffer<EmitterData> gEmitter : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

// 乱数生成用関数
float32_t3 rand3dTo3d(float32_t3 value) {
    return frac(sin(float32_t3(
        dot(value, float32_t3(12.9898, 78.233, 45.164)),
        dot(value, float32_t3(39.346, 11.135, 83.155)),
        dot(value, float32_t3(73.156, 52.234, 91.127))
    )) * 43758.5453);
}

float32_t rand3dTo1d(float32_t3 value) {
    return frac(sin(dot(value, float32_t3(12.9898, 78.233, 45.164))) * 43758.5453);
}

// 乱数生成クラス
class RandomGenerator
{
    float32_t3 seed;
    float32_t3 Generate3d() {
        seed = rand3dTo3d(seed);
        return seed;
    }
    float32_t Generate1d() {
        float32_t result = rand3dTo1d(seed);
        seed.x = result;
        return result;
    }
};

[numthreads(32, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    if (gEmitter.emit != 0 && DTid.x < gEmitter.count) {
        RandomGenerator generator;
        generator.seed = float32_t3(DTid.x, gPerFrame.time, DTid.x * 0.137f + gPerFrame.time * 12.9898f);

        int32_t freeListIndex;
        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

        if (0 <= freeListIndex && freeListIndex < (int32_t)kMaxParticles) {
            uint32_t particleIndex = gFreeList[freeListIndex];

            // 1. スケール
            float32_t3 randScale = generator.Generate3d();
            gParticles[particleIndex].scale = 0.05f + randScale * 0.08f;

            // 2. 座標と速度の計算 (Emitter 形状タイプに応じた幾何計算)
            float32_t3 emitPos = gEmitter.translate;
            float32_t3 emitVel = float32_t3(0.0f, 1.0f, 0.0f);
            float32_t speed = gEmitter.initialSpeed > 0.0f ? gEmitter.initialSpeed : 1.0f;

            if (gEmitter.emitterType == 0) // Point (点発生)
            {
                emitPos = gEmitter.translate;
                float32_t3 randVel = generator.Generate3d() * 2.0f - 1.0f;
                emitVel = normalize(randVel);
            }
            else if (gEmitter.emitterType == 1) // Box (直方体発生)
            {
                float32_t3 randBox = generator.Generate3d() - 0.5f;
                emitPos = gEmitter.translate + randBox * gEmitter.boxSize;
                emitVel = float32_t3(0.0f, 1.0f, 0.0f);
            }
            else if (gEmitter.emitterType == 2) // Sphere (球状発生)
            {
                float32_t3 randDir = generator.Generate3d() * 2.0f - 1.0f;
                float32_t3 dir = normalize(length(randDir) > 0.001f ? randDir : float32_t3(0, 1, 0));
                float32_t dist = generator.Generate1d() * gEmitter.radius;
                emitPos = gEmitter.translate + dir * dist;
                emitVel = dir;
            }
            else if (gEmitter.emitterType == 3) // Cone (円錐射出)
            {
                emitPos = gEmitter.translate;
                float32_t3 baseDir = length(gEmitter.direction) > 0.001f ? normalize(gEmitter.direction) : float32_t3(0, 1, 0);
                float32_t randAngle = generator.Generate1d() * gEmitter.coneAngle;
                float32_t randRot = generator.Generate1d() * 6.2831853f;

                float32_t3 perp = abs(baseDir.y) < 0.99f ? float32_t3(0, 1, 0) : float32_t3(1, 0, 0);
                float32_t3 right = normalize(cross(perp, baseDir));
                float32_t3 up = cross(baseDir, right);

                emitVel = normalize(baseDir * cos(randAngle) + (right * cos(randRot) + up * sin(randRot)) * sin(randAngle));
            }

            gParticles[particleIndex].translate = emitPos;
            gParticles[particleIndex].velocity = emitVel * speed;

            // 3. 色
            if (gEmitter.particleColor.a > 0.0f) {
                gParticles[particleIndex].color = gEmitter.particleColor;
            } else {
                float32_t randG = generator.Generate1d();
                gParticles[particleIndex].color = float32_t4(1.0f, 0.5f + randG * 0.4f, 0.15f, 1.0f);
            }

            // 4. 寿命
            gParticles[particleIndex].currentTime = 0.0f;
            gParticles[particleIndex].lifeTime = gEmitter.particleLifeTime > 0.0f ? gEmitter.particleLifeTime : (0.5f + generator.Generate1d() * 1.0f);
        } else {
            InterlockedAdd(gFreeListIndex[0], 1);
        }
    }
}
