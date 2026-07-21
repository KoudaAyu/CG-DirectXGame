struct Particle {
    float32_t3 translate;
    float32_t3 scale;
    float32_t lifeTime;
    float32_t3 velocity;
    float32_t currentTime;
    float32_t4 color;
};

struct EmitterSphere {
    float32_t3 translate; //位置
    float32_t radius; //射出範囲
    uint32_t count; //射出数
    float32_t frequency; //発生頻度
    float32_t frequencyTime; //使用頻度
    uint32_t emit; //許可
};

struct PerFrame {
    float32_t time;
    float32_t deltaTime;
};

static const uint32_t kMaxParticles = 1024;

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeCounter : register(u1);

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

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    if (gEmitter.emit != 0) { // 射出許可が出たので射出
        RandomGenerator generator;
        // シードの初期化
        generator.seed = (DTid + gPerFrame.time) * gPerFrame.time;

        for (uint32_t countIndex = 0; countIndex < gEmitter.count; ++countIndex) {
            int32_t particleIndex;
            // アトミックに加算して空きスロットインデックスを取得 (スライド2枚目)
            InterlockedAdd(gFreeCounter[0], 1, particleIndex);

            // 最大数よりも少なければ射出可能
            if (particleIndex < kMaxParticles) {
                // 1. スケール
                float32_t3 randScale = generator.Generate3d();
                gParticles[particleIndex].scale = 0.1f + randScale * 0.2f;

                // 2. 座標 (Emitter の位置 + 半径以内のランダムな球状オフセット)
                float32_t3 randPos = generator.Generate3d();
                float32_t3 dir = normalize(randPos * 2.0f - 1.0f);
                float32_t dist = generator.Generate1d() * gEmitter.radius;
                gParticles[particleIndex].translate = gEmitter.translate + dir * dist;

                // 3. 色
                gParticles[particleIndex].color.rgb = generator.Generate3d();
                gParticles[particleIndex].color.a = 1.0f;

                // 4. その他のパラメータ
                gParticles[particleIndex].currentTime = 0.0f;
                gParticles[particleIndex].lifeTime = 1.0f + generator.Generate1d() * 2.0f; // 寿命を 1.0〜3.0 秒に設定

                // 速度ベクトルもランダム（上方向をベースに周囲へ拡散）
                float32_t3 randVel = generator.Generate3d();
                float32_t3 velDir = normalize(float32_t3(randVel.x * 2.0f - 1.0f, 1.0f + randVel.y * 2.0f, randVel.z * 2.0f - 1.0f));
                gParticles[particleIndex].velocity = velDir * (1.0f + generator.Generate1d() * 2.0f); // 速度 1.0〜3.0
            }
        }
    }
}
