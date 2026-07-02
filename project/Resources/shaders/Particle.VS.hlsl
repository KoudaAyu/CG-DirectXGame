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
    
    // ビルボード行列をもとにワールド行列を作成
    float32_t4x4 worldMatrix = gPerView.billboardMatrix;
    
    // スケールを適用
    worldMatrix[0] *= particle.scale.x;
    worldMatrix[1] *= particle.scale.y;
    worldMatrix[2] *= particle.scale.z;
    
    // 位置を適用
    worldMatrix[3].xyz = particle.translate;
    
    // 行列の乗算と座標変換
    output.position = mul(input.position, mul(worldMatrix, gPerView.viewProjection));
    output.texcoord = input.texcoord;
    output.color = particle.color;
    
    return output;
}
