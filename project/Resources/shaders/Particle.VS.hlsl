#include "Resources/shaders/Particle.hlsli"

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

StructuredBuffer<Particle> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

struct VertecShederInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertecShederInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    
    Particle particle = gParticles[instanceId];
    
    // 生きていないパーティクル（アルファが0またはスケールが0）はクリップ空間外へ飛ばして即時カリング（PS負荷を完全ゼロ化）
    if (particle.color.a <= 0.001f || particle.scale.x <= 0.0001f)
    {
        output.position = float32_t4(0.0f, 0.0f, 2.0f, 1.0f);
        output.texcoord = float32_t2(0.0f, 0.0f);
        output.color = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
        return output;
    }
    
    // ビルボード行列をもとにワールド行列を作成
    float32_t4x4 worldMatrix = gPerView.billboardMatrix;
    
    // インスタンスIDと経過時間から各パーティクル固有の自転角度を計算
    float32_t seed = frac(sin(float32_t(instanceId) * 12.9898f + 0.123f) * 43758.5453f);
    float32_t rotSpeed = (frac(sin(float32_t(instanceId) * 78.233f + 4.567f) * 43758.5453f) - 0.5f) * 1.5f;
    float32_t angle = seed * 6.2831853f + rotSpeed * particle.currentTime;
    float32_t cosA = cos(angle);
    float32_t sinA = sin(angle);
    
    // クアッドのローカル頂点をビルボード平面上で回転
    float32_t2 rotatedPos;
    rotatedPos.x = input.position.x * cosA - input.position.y * sinA;
    rotatedPos.y = input.position.x * sinA + input.position.y * cosA;
    float32_t4 localPos = float32_t4(rotatedPos, input.position.z, 1.0f);
    
    // スケールを適用
    worldMatrix[0] *= particle.scale.x;
    worldMatrix[1] *= particle.scale.y;
    worldMatrix[2] *= particle.scale.z;
    
    // 位置を適用
    worldMatrix[3].xyz = particle.translate;
    
    // 行列の乗算と座標変換
    output.position = mul(localPos, mul(worldMatrix, gPerView.viewProjection));
    output.texcoord = input.texcoord;
    output.color = particle.color;
    
    return output;
}
