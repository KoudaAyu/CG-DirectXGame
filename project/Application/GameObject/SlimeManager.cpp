#include "SlimeManager.h"
#include "Application/GameObject/SlimeCollision.h"
#include "Application/GameObject/SlimePhysics.h"
#include "Baziru3_Engine/Core/Base/KeyInput.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
}

void SlimeManager::Initialize(Object3dCom* object3dCom, Camera* camera) {
    object3dCom_ = object3dCom;
    camera_ = camera;
    slimes_.clear();
}

Slime* SlimeManager::SpawnSlime(const Vector3& pos, int size) {
    auto slime = std::make_unique<Slime>();
    slime->Initialize(object3dCom_, camera_, pos, size);
    slimes_.push_back(std::move(slime));
    return slimes_.back().get();
}

void SlimeManager::SpawnSlimes(const Vector3& basePos, int count, int sizePerSlime) {
    for (int i = 0; i < count; ++i) {
        float offsetX = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f;
        float offsetZ = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f;
        Vector3 pos = { basePos.x + offsetX, basePos.y + 0.2f, basePos.z + offsetZ };
        SpawnSlime(pos, sizePerSlime);
    }
}

void SlimeManager::Clear() {
    slimes_.clear();
}

void SlimeManager::TriggerStageBounce(const Vector2& stageTilt, const Vector2& pivot, float bouncePower) {
    for (auto& slime : slimes_) {
        if (!slime || !slime->IsActive()) continue;

        Vector3 pos = slime->GetPosition();
        bool hasGround = false;
        Vector3 groundNormal{ 0.0f, 1.0f, 0.0f };
        float groundY = SlimePhysics::CalculateGroundedCenterYEx(
            pos.x, pos.z, pos.y, stageTilt, slime->GetRadius(), &hasGround, &groundNormal, pivot, slime->IsGrounded());

        // 接地しているか、床面至近（0.35m以内）のスライムのみがステージの突き上げを受ける
        if (slime->IsGrounded() || (hasGround && (pos.y - groundY) < 0.35f && (pos.y - groundY) >= -0.1f)) {
            slime->BounceFromStage(groundNormal, bouncePower);
        }
    }
}

void SlimeManager::TriggerJump() {
    Vector3 center;
    float spread;
    GetGroupCenterAndSpread(center, spread);
    TriggerStageBounce({ 0.0f, 0.0f }, { center.x, center.z }, 13.5f);
}

void SlimeManager::TriggerSplit() {
    // 分裂で新しく生まれるスライムを一旦溜めておき、ループ後にまとめて追加する
    std::vector<std::unique_ptr<Slime>> newSlimes;

    for (auto& slime : slimes_) {
        if (!slime || !slime->IsActive()) continue;

        int currentSize = slime->GetSize();

        if (currentSize > 1) {
            Vector3 sPos = slime->GetPosition();
            int spawnCount = currentSize - 1; // 親以外の分裂数

            // 内包していた子スライムの参照をクリア（もう使わない）
            slime->ClearAbsorbedChildren();

            // 親スライム自身をサイズ1に戻す
            slime->SetSize(1);
            slime->SetMergeCooldown(0.40f);
            Vector3 jumpVel = { 0.0f, splitUpPower_ * 0.6f, 0.0f };
            slime->Launch(jumpVel);

            // (currentSize - 1) 個の新しいサイズ1スライムを親の位置から放射状に発射
            float angleStep = (2.0f * kPi) / static_cast<float>(spawnCount);
            for (int i = 0; i < spawnCount; ++i) {
                auto newSlime = std::make_unique<Slime>();
                newSlime->Initialize(object3dCom_, camera_, sPos, 1);
                newSlime->SetMergeCooldown(0.40f);

                float angle = angleStep * i + ((std::rand() % 100) / 100.0f - 0.5f) * 0.35f;
                float popSpeed = splitPopPower_ + ((std::rand() % 100) / 100.0f - 0.5f) * (splitPopPower_ * 0.25f);
                float upSpeed = splitUpPower_ + ((std::rand() % 100) / 100.0f - 0.5f) * (splitUpPower_ * 0.25f);

                Vector3 launchVel = {
                    std::sin(angle) * popSpeed,
                    upSpeed,
                    std::cos(angle) * popSpeed
                };
                newSlime->Launch(launchVel);

                newSlimes.push_back(std::move(newSlime));
            }
        } else {
            // もともとサイズ1の単独スライムは、周囲の分裂の波紋に合わせてその場で小さくホップ
            slime->SetMergeCooldown(0.40f);
            Vector3 jumpVel = { 0.0f, splitUpPower_ * 0.4f, 0.0f };
            slime->Launch(jumpVel);
        }
    }

    // 新しく生まれたスライムをリストに追加
    for (auto& ns : newSlimes) {
        slimes_.push_back(std::move(ns));
    }
}

void SlimeManager::CheckAndResolveMerge() {
    size_t count = slimes_.size();
    for (size_t i = 0; i < count; ++i) {
        if (!slimes_[i] || !slimes_[i]->CanMerge()) continue;

        for (size_t j = i + 1; j < count; ++j) {
            if (!slimes_[j] || !slimes_[j]->CanMerge()) continue;

            Vector3 posA = slimes_[i]->GetPosition();
            Vector3 posB = slimes_[j]->GetPosition();
            float dx = posA.x - posB.x;
            float dy = posA.y - posB.y;
            float dz = posA.z - posB.z;
            float distSq = dx * dx + dz * dz;

            float mergeDist = (std::max)(mergeThreshold_, slimes_[i]->GetRadius() + slimes_[j]->GetRadius() + 0.50f);
            if (distSq <= mergeDist * mergeDist && std::abs(dy) <= mergeDist) {
                // スライムiにスライムjが合体！
                int combinedSize = slimes_[i]->GetSize() + slimes_[j]->GetSize();

                // 合体位置: 大きい方に寄せる（半々だとめり込みやすいため）
                float totalSize = static_cast<float>(slimes_[i]->GetSize() + slimes_[j]->GetSize());
                float weightA = static_cast<float>(slimes_[i]->GetSize()) / totalSize;
                float weightB = static_cast<float>(slimes_[j]->GetSize()) / totalSize;
                Vector3 mergeCenter = {
                    posA.x * weightA + posB.x * weightB,
                    (std::max)(posA.y, posB.y), // Y は高い方を採用（めり込み防止）
                    posA.z * weightA + posB.z * weightB
                };

                // slimes_[j] を非アクティブに
                slimes_[j]->ClearAbsorbedChildren();
                slimes_[j]->SetActive(false);
                slimes_[j]->SetSize(1);

                // slimes_[i] にサイズを集約
                slimes_[i]->SetSize(combinedSize);
                slimes_[i]->SetPosition(mergeCenter);

                // 合体後の地面補正: 新しいスケールでの接地高さを再計算して沈み込みを防止
                float newGroundY = mergeCenter.y; // フォールバック
                bool hasGround = false;
                float groundOffset = slimes_[i]->GetScale().x * 0.75f;
                float calcGroundY = SlimePhysics::CalculateGroundedCenterYEx(
                    mergeCenter.x, mergeCenter.z, mergeCenter.y,
                    { 0.0f, 0.0f }, groundOffset, &hasGround, { mergeCenter.x, mergeCenter.z }, false);
                if (hasGround && calcGroundY > mergeCenter.y) {
                    mergeCenter.y = calcGroundY;
                    slimes_[i]->SetPosition(mergeCenter);
                }

                slimes_[i]->GetSlimeParams().impulseStrength = 0.50f; // ポヨン！と合体弾性
            }
        }
    }
}

void SlimeManager::ResolveSeparation(const Vector3& rotation, const Vector2& stageTilt, const Vector2& pivot) {
    Matrix4x4 rotMat = Multiply(MakeRotateXMatrix(rotation.x),
                                Multiply(MakeRotateYMatrix(rotation.y), MakeRotateZMatrix(rotation.z)));
    Vector3 stageNormal = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };

    size_t count = slimes_.size();
    for (size_t i = 0; i < count; ++i) {
        if (!slimes_[i] || !slimes_[i]->IsActive()) continue;

        for (size_t j = i + 1; j < count; ++j) {
            if (!slimes_[j] || !slimes_[j]->IsActive()) continue;

            Vector3 posA = slimes_[i]->GetPosition();
            Vector3 posB = slimes_[j]->GetPosition();
            Vector3 scaleA = slimes_[i]->GetScale();
            Vector3 scaleB = slimes_[j]->GetScale();
            const Vector3& squashA = slimes_[i]->GetSlimeParams().squashStretch;
            const Vector3& squashB = slimes_[j]->GetSlimeParams().squashStretch;

            float impulse = 0.0f;
            if (SlimeCollision::ResolveCollision(posA, scaleA, squashA, 0.5f,
                                                 posB, scaleB, squashB, 0.5f,
                                                 impulse, rotation, rotation, stageNormal)) {
                bool hasGroundA = false, hasGroundB = false;
                float gyA = SlimePhysics::CalculateGroundedCenterYEx(posA.x, posA.z, posA.y, stageTilt, 0.22f, &hasGroundA, pivot, slimes_[i]->IsGrounded());
                float gyB = SlimePhysics::CalculateGroundedCenterYEx(posB.x, posB.z, posB.y, stageTilt, 0.22f, &hasGroundB, pivot, slimes_[j]->IsGrounded());
                if (hasGroundA && slimes_[i]->GetState() == SlimeState::Rolling) posA.y = gyA;
                if (hasGroundB && slimes_[j]->GetState() == SlimeState::Rolling) posB.y = gyB;

                slimes_[i]->SetPosition(posA);
                slimes_[j]->SetPosition(posB);

                Vector3 diff = posB - posA;
                float diffLenSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
                if (diffLenSq > 1e-6f) {
                    Vector3 norm = diff * (1.0f / std::sqrt(diffLenSq));
                    Vector3 velA = slimes_[i]->GetVelocity();
                    Vector3 velB = slimes_[j]->GetVelocity();
                    float closingSpeed = (velB.x - velA.x) * norm.x + (velB.y - velA.y) * norm.y + (velB.z - velA.z) * norm.z;
                    if (closingSpeed < 0.0f) {
                        Vector3 relImpulse = norm * (closingSpeed * 0.5f);
                        slimes_[i]->SetVelocity(velA + relImpulse);
                        slimes_[j]->SetVelocity(velB - relImpulse);
                    }
                }

                if (impulse > 0.05f) {
                    slimes_[i]->GetSlimeParams().impulseStrength = (std::max)(slimes_[i]->GetSlimeParams().impulseStrength, impulse * 0.4f);
                    slimes_[j]->GetSlimeParams().impulseStrength = (std::max)(slimes_[j]->GetSlimeParams().impulseStrength, impulse * 0.4f);
                }
            }
        }
    }
}

void SlimeManager::Update(float deltaTime, KeyInput* keyInput, const Vector2& stageTilt) {
    if (keyInput) {
        // Fキー: 合体リクエスト
        if (keyInput->TriggerKey(DIK_F)) {
            RequestMerge();
        }
        // Eキー: 全員分裂
        if (keyInput->TriggerKey(DIK_E)) {
            TriggerSplit();
        }
    }

    // 合体判定
    if (mergeRequested_) {
        CheckAndResolveMerge();
        mergeRequested_ = false;
    }

    // 重心ピボット
    Vector3 center;
    float spread;
    GetGroupCenterAndSpread(center, spread);
    Vector2 pivot = { center.x, center.z };

    // 各スライムの物理更新
    for (auto& slime : slimes_) {
        if (slime) {
            slime->Update(deltaTime, stageTilt, pivot);
        }
    }

    // スライム同士の衝突分離（2パス）
    Vector3 rot = { stageTilt.x, 0.0f, -stageTilt.y };
    for (int iter = 0; iter < 2; ++iter) {
        ResolveSeparation(rot, stageTilt, pivot);
    }
}

void SlimeManager::GetGroupCenterAndSpread(Vector3& outCenter, float& outSpread) const {
    Vector3 sumPos = { 0.0f, 0.0f, 0.0f };
    int totalWeight = 0;

    for (const auto& slime : slimes_) {
        if (slime && slime->IsActive()) {
            // レベル3のスライムは同座標にレベル1スライムが3つあるのと同じ重み（質量加重平均）
            int weight = (std::max)(1, slime->GetSize());
            Vector3 pos = slime->GetPosition();
            sumPos += pos * static_cast<float>(weight);
            totalWeight += weight;
        }
    }

    if (totalWeight == 0) {
        outCenter = { 0.0f, 0.0f, 0.0f };
        outSpread = 1.0f;
        return;
    }

    outCenter = { sumPos.x / totalWeight, sumPos.y / totalWeight, sumPos.z / totalWeight };

    float maxDistSq = 0.0f;
    for (const auto& slime : slimes_) {
        if (slime && slime->IsActive()) {
            Vector3 diff = slime->GetPosition() - outCenter;
            float distSq = diff.x * diff.x + diff.z * diff.z;
            if (distSq > maxDistSq) {
                maxDistSq = distSq;
            }
        }
    }

    outSpread = (std::min)(14.0f, (std::max)(1.0f, std::sqrt(maxDistSq)));
}

int SlimeManager::GetActiveCount() const {
    int count = 0;
    for (const auto& s : slimes_) {
        if (s && s->IsActive()) count++;
    }
    return count;
}

int SlimeManager::GetTotalCount() const {
    return static_cast<int>(slimes_.size());
}

int SlimeManager::GetMaxSlimeSize() const {
    int maxS = 0;
    for (const auto& s : slimes_) {
        if (s && s->IsActive()) {
            maxS = (std::max)(maxS, s->GetSize());
        }
    }
    return maxS;
}

int SlimeManager::GetTotalSize() const {
    int total = 0;
    for (const auto& s : slimes_) {
        if (s && s->IsActive()) {
            total += s->GetSize();
        }
    }
    return total;
}

void SlimeManager::Draw(const RenderContext& ctx) {
    for (auto& slime : slimes_) {
        if (slime) {
            slime->Draw(ctx);
        }
    }
}

void SlimeManager::DrawDebug(Camera* cam) {
    for (auto& slime : slimes_) {
        if (slime) {
            slime->DrawDebug(cam);
        }
    }
}
