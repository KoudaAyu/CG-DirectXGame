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

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    if (gEmitter.emit != 0) { // 射出許可が出たので射出
        RandomGenerator generator;
        // シードの初期化
        generator.seed = (DTid + gPerFrame.time + 1.234f) * (gPerFrame.time + 0.567f);

        for (uint32_t countIndex = 0; countIndex < gEmitter.count; ++countIndex) {
            int32_t freeListIndex;
            // FreeListのIndexを1つ前に設定し、現在のIndexを取得する
            InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

            // 最大数よりも少なければ射出可能
            if (0 <= freeListIndex && freeListIndex < kMaxParticles) {
                uint32_t particleIndex = gFreeList[freeListIndex];

                // 1. スケール（ノズル出口サイズ 0.08〜0.12m：手のひら大の煙の塊）
                float32_t randScale = generator.Generate1d();
                float32_t initScale = 0.08f + randScale * 0.04f;
                gParticles[particleIndex].scale = float32_t3(initScale, initScale, initScale);

                // 2. 座標 (ノズル噴出口中心付近の微細なオフセット)
                float32_t3 randPos = generator.Generate3d();
                float32_t3 dir = normalize(randPos * 2.0f - 1.0f);
                float32_t dist = generator.Generate1d() * gEmitter.radius;
                gParticles[particleIndex].translate = gEmitter.translate + dir * dist;

                // 3. 色（化学発煙筒特有の鮮烈なエメラルド〜ライムグリーン、微細な色相揺らぎ）
                float32_t hueVariation = (generator.Generate1d() - 0.5f) * 0.06f;
                float32_t3 baseColor = float32_t3(0.12f + hueVariation * 0.5f, 1.0f, 0.35f + hueVariation);
                float32_t brightness = 0.92f + generator.Generate1d() * 0.22f;
                gParticles[particleIndex].color.rgb = saturate(baseColor * brightness);
                gParticles[particleIndex].color.a = 0.96f;

                // 4. その他のパラメータ（ヘリの進入高度まで届く十分な寿命 3.8〜5.0秒）
                gParticles[particleIndex].currentTime = 0.0f;
                gParticles[particleIndex].lifeTime = 3.8f + generator.Generate1d() * 1.2f;

                // 5. スモークグレネードの斜め上噴出＆風下ドリフト（ヘリパイロットを誘導する煙の帯）
                // 360度の方位角
                float32_t azimuth = generator.Generate1d() * 6.2831853f;
                float32_t elevRand = generator.Generate1d();
                float32_t elevation;
                float32_t speed;
                
                if (elevRand < 0.75f)
                {
                    // 75%: 上空へ勢いよく立ち昇る誘導煙柱（45〜65度）
                    elevation = 0.78f + generator.Generate1d() * 0.35f;
                    speed = 1.8f + generator.Generate1d() * 0.90f; // 1.8〜2.7 m/s
                }
                else
                {
                    // 25%: 低空〜中空を漂う外縁煙（25〜45度）
                    elevation = 0.45f + generator.Generate1d() * 0.35f;
                    speed = 1.2f + generator.Generate1d() * 0.70f;
                }

                float32_t cosElev = cos(elevation);
                // パイロットに風向を示す環境風（+X: 0.55, +Z: 0.65）に吹かれて斜めに美しくたなびく
                float32_t velX = cos(azimuth) * cosElev * speed * 0.40f + 0.55f;
                float32_t velY = sin(elevation) * speed + 0.50f; // 力強い上空立ち昇り
                float32_t velZ = sin(azimuth) * cosElev * speed * 0.40f + 0.65f;
                gParticles[particleIndex].velocity = float32_t3(velX, velY, velZ);
            } else {
                // 発生させられなかったので、減らしてしまった分をもとに戻す
                InterlockedAdd(gFreeListIndex[0], 1);
                break;
            }
        }
    }
}
