#include "ParticleEmitter.h"
#include "ParticleManager.h"

ParticleEmitter::ParticleEmitter(const std::string& groupName,
    const Vector3& position,
    uint32_t emitCount,
    float emitInterval)
{
    groupName_ = groupName;
    position_ = position;
    emitCount_ = emitCount;
    emitInterval_ = emitInterval;
}

void ParticleEmitter::Update(float deltaTime)
{
    // ● 時刻を進める
    emitTimer_ += deltaTime;

    // ● 発生頻度(=emitInterval_)より大きいなら発生
    //    ParticleManager::GetInstance()->Emit(name, position, count);
    while (emitTimer_ >= emitInterval_)
    {
        ParticleManager::GetInstance()->Emit(groupName_, position_, emitCount_);
        // ● 余剰に過ぎた時間も加味して頻度計算する（持ち越し）
        emitTimer_ -= emitInterval_;
    }
}

void ParticleEmitter::Emit()
{
    // エミッタの設定値に従って、ParticleManagerのEmitを呼び出す
    ParticleManager::GetInstance()->Emit(groupName_, position_, emitCount_);
}