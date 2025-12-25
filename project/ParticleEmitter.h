#pragma once
#include "Vector.h"
#include <string>

/// <summary>
/// パーティクル発生器
/// </summary>
class ParticleEmitter
{
public:
    // コンストラクタ
    ParticleEmitter(const std::string& groupName,   // 対応するパーティクルグループ名
        const Vector3& position,        // 発生位置
        uint32_t emitCount,             // 1回あたりの発生数
        float emitInterval);            // 発生間隔(秒)

    // デストラクタ
    ~ParticleEmitter() = default;

    // 更新処理：経過時間(dt)を受け取り、必要ならEmitを呼ぶ
    void Update(float deltaTime);
    // パーティクル発生（設定値に従ってEmitを呼ぶ）
    void Emit();
    void SetPosition(const Vector3& p) { position_ = p; }

private:
    std::string groupName_; // 対応するパーティクルグループ名
    Vector3 position_;      // 発生位置
    uint32_t emitCount_;    // 1回のEmitで生成する数
    float emitInterval_;    // 発生間隔(秒)
    float emitTimer_ = 0.0f; // 発生までの経過時間

};