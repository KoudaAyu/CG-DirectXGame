#include "MinionManager.h"
#include "Application/GameObject/SlimeCollision.h"
#include "Application/GameObject/SlimePhysics.h"
#include "Matrix4x4.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
}

void MinionManager::Initialize(Object3dCom* object3dCom, Camera* camera) {
    object3dCom_ = object3dCom;
    camera_ = camera;
    minions_.clear();
}

void MinionManager::SpawnMinion(const Vector3& spawnPos, int count, MinionType type) {
    for (int i = 0; i < count; ++i) {
        auto minion = std::make_unique<Minion>();
        float offsetX = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f;
        float offsetZ = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f;
        Vector3 pos = { spawnPos.x + offsetX, spawnPos.y + 0.2f, spawnPos.z + offsetZ };
        minion->Initialize(object3dCom_, camera_, pos, type);
        minions_.push_back(std::move(minion));
    }
}

void MinionManager::ClearMinions() {
    minions_.clear();
}

void MinionManager::SetMinionCount(int targetCount, const Vector3& playerPos) {
    int current = static_cast<int>(minions_.size());
    if (targetCount > current) {
        SpawnMinion(playerPos, targetCount - current);
    } else if (targetCount < current) {
        minions_.resize(targetCount);
    }
}

int MinionManager::GetActiveCount() const {
    int count = 0;
    for (const auto& m : minions_) {
        if (m && m->IsActive() && m->GetState() != MinionState::Merging) {
            count++;
        }
    }
    return count;
}

int MinionManager::GetReadyCount(const Vector3& playerPos, float maxPickupRadius) const {
    if (isMergedState_) return 0;
    int count = 0;
    float maxDistSq = maxPickupRadius * maxPickupRadius;
    for (const auto& m : minions_) {
        if (m && m->IsActive() && m->GetState() == MinionState::Rolling) {
            Vector3 diff = m->GetPosition() - playerPos;
            diff.y = 0.0f;
            if (diff.x * diff.x + diff.z * diff.z <= maxDistSq) {
                count++;
            }
        }
    }
    return count;
}

int MinionManager::GetMergedCount() const {
    if (!isMergedState_) return 0;
    int count = 0;
    for (const auto& m : minions_) {
        if (m && !m->IsActive()) {
            count++;
        }
    }
    return count;
}

void MinionManager::ResolveSeparation(const Vector3& rotation, const Vector2& stageTilt) {
    size_t count = minions_.size();
    if (count < 2) return;

    Matrix4x4 rotMat = Multiply(MakeRotateXMatrix(rotation.x),
                                Multiply(MakeRotateYMatrix(rotation.y), MakeRotateZMatrix(rotation.z)));
    Vector3 stageNormal = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };

    for (size_t i = 0; i < count; ++i) {
        if (!minions_[i] || !minions_[i]->IsActive()) continue;
        if (minions_[i]->GetState() != MinionState::Rolling) continue;

        Vector3 posA = minions_[i]->GetPosition();
        Vector3 scaleA = minions_[i]->GetScale();
        const Vector3& squashA = minions_[i]->GetSlimeParams().squashStretch;

        for (size_t j = i + 1; j < count; ++j) {
            if (!minions_[j] || !minions_[j]->IsActive()) continue;
            if (minions_[j]->GetState() != MinionState::Rolling) continue;

            Vector3 posB = minions_[j]->GetPosition();
            Vector3 scaleB = minions_[j]->GetScale();
            const Vector3& squashB = minions_[j]->GetSlimeParams().squashStretch;

            float impulse = 0.0f;
            // 互いに50%ずつ押し合う (weightA = 0.5, weightB = 0.5)
            if (SlimeCollision::ResolveCollision(posA, scaleA, squashA, 0.5f,
                                                 posB, scaleB, squashB, 0.5f,
                                                 impulse, rotation, rotation, stageNormal)) {
                // 傾斜面上の厳密な接地中心に再クランプ
                posA.y = SlimePhysics::CalculateGroundedCenterY(posA.x, posA.z, stageTilt, 0.22f);
                posB.y = SlimePhysics::CalculateGroundedCenterY(posB.x, posB.z, stageTilt, 0.22f);

                minions_[i]->SetPosition(posA);
                minions_[j]->SetPosition(posB);

                // 相互の接近速度成分を除去（接触状態での無駄なめり込み加速を防止）
                Vector3 diff = posB - posA;
                float diffLenSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
                if (diffLenSq > 1e-6f) {
                    Vector3 norm = diff * (1.0f / std::sqrt(diffLenSq));
                    Vector3 velA = minions_[i]->GetVelocity();
                    Vector3 velB = minions_[j]->GetVelocity();
                    float closingSpeed = (velB.x - velA.x) * norm.x + (velB.y - velA.y) * norm.y + (velB.z - velA.z) * norm.z;
                    if (closingSpeed < 0.0f) {
                        Vector3 relImpulse = norm * (closingSpeed * 0.5f);
                        minions_[i]->SetVelocity(velA + relImpulse);
                        minions_[j]->SetVelocity(velB - relImpulse);
                    }
                }

                if (impulse > 0.05f) {
                    minions_[i]->GetSlimeParams().impulseStrength = (std::max)(minions_[i]->GetSlimeParams().impulseStrength, impulse * 0.4f);
                    minions_[j]->GetSlimeParams().impulseStrength = (std::max)(minions_[j]->GetSlimeParams().impulseStrength, impulse * 0.4f);
                }
            }
        }
    }
}

void MinionManager::ResolvePlayerSeparation(const Vector3& playerPos, const Vector3& playerVelocity, const Vector3& playerScale, const Vector3& playerSquash, const Vector3& playerRotation, const Vector2& stageTilt) {
    Matrix4x4 rotMat = Multiply(MakeRotateXMatrix(playerRotation.x),
                                Multiply(MakeRotateYMatrix(playerRotation.y), MakeRotateZMatrix(playerRotation.z)));
    Vector3 stageNormal = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };

    Vector3 pPos = playerPos;
    for (auto& minion : minions_) {
        if (!minion || !minion->IsActive()) continue;
        // 合体吸引中（Merging）のミニオンはプレイヤーに吸い込まれるため押し出さない
        // 転がり中（Rolling）のミニオンのみを衝突押し出しの対象とする（合体巨大化時も確実に押し出す）
        if (minion->GetState() != MinionState::Rolling) continue;

        Vector3 mPos = minion->GetPosition();
        Vector3 mScale = minion->GetScale();
        const Vector3& mSquash = minion->GetSlimeParams().squashStretch;
        Vector3 mRot = minion->GetRotation();

        float impulse = 0.0f;
        // プレイヤーは不動 (weight=0.0)、ミニオンが100%外側へ押し出される (weight=1.0)
        if (SlimeCollision::ResolveCollision(pPos, playerScale, playerSquash, 0.0f,
                                             mPos, mScale, mSquash, 1.0f,
                                             impulse, playerRotation, mRot, stageNormal)) {
            mPos.y = SlimePhysics::CalculateGroundedCenterY(mPos.x, mPos.z, stageTilt, 0.22f);
            minion->SetPosition(mPos);

            // プレイヤーからの押し出し速度をミニオンに伝達（巨大プレイヤーの突進・接触による弾き飛ばし）
            Vector3 mVel = minion->GetVelocity();
            Vector3 diff = mPos - pPos;
            float diffLenSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
            if (diffLenSq > 1e-6f) {
                Vector3 away = diff * (1.0f / std::sqrt(diffLenSq));
                Vector3 relVel = mVel - playerVelocity;
                float closingSpeed = (relVel.x * away.x + relVel.y * away.y + relVel.z * away.z);
                if (closingSpeed < 0.0f) {
                    // 相対接近速度を相殺し、巨大プレイヤーの押し出し速度＋衝撃反発を付与
                    mVel += away * (-closingSpeed + impulse * 2.5f);
                    minion->SetVelocity(mVel);
                }
            }

            if (impulse > 0.05f) {
                minion->GetSlimeParams().impulseStrength = (std::max)(minion->GetSlimeParams().impulseStrength, impulse * 0.6f);
            }
        }
    }
}


void MinionManager::TriggerMerge(const Vector3& playerPos, float mergeRadius) {
    isMergedState_ = true;
    isAllMerged_ = false;
    float mergeRadiusSq = mergeRadius * mergeRadius;
    for (auto& minion : minions_) {
        if (minion && minion->IsActive() && minion->GetState() == MinionState::Rolling) {
            Vector3 diff = minion->GetPosition() - playerPos;
            diff.y = 0.0f; // 水平距離で判定
            if (diff.x * diff.x + diff.z * diff.z <= mergeRadiusSq) {
                minion->AttractTo(playerPos, 28.0f);
            }
        }
    }
}

void MinionManager::TriggerSplit(const Vector3& playerPos) {
    isMergedState_ = false;
    isAllMerged_ = false;

    // 合体によって吸収されていた（非アクティブな）ミニオンのみを対象にカウント
    int absorbedCount = 0;
    for (auto& minion : minions_) {
        if (minion && !minion->IsActive()) {
            absorbedCount++;
        }
    }

    if (absorbedCount <= 0) return;

    float angleStep = (2.0f * kPi) / static_cast<float>(absorbedCount);
    
    int index = 0;
    for (auto& minion : minions_) {
        if (minion && !minion->IsActive()) {
            minion->SetActive(true);
            minion->SetPosition(playerPos);
            
            float angle = angleStep * index + ((std::rand() % 100) / 100.0f - 0.5f) * 0.3f;
            float popSpeed = splitPopPower_ + ((std::rand() % 100) / 100.0f - 0.5f) * (splitPopPower_ * 0.3f);
            float upSpeed = splitUpPower_ + ((std::rand() % 100) / 100.0f - 0.5f) * (splitUpPower_ * 0.3f);

            Vector3 launchVel = {
                std::sin(angle) * popSpeed,
                upSpeed,
                std::cos(angle) * popSpeed
            };
            minion->Launch(launchVel);
            index++;
        }
    }
}

bool MinionManager::ThrowMinionWithVelocity(const Vector3& launchPos, const Vector3& velocity) {
    if (isMergedState_) return false;

    // 手元付近（半径3.5m以内）にいるRolling中ミニオンの中から、最も投擲位置に近いものを選択
    Minion* bestMinion = nullptr;
    float bestDistSq = 3.5f * 3.5f; // 投擲可能範囲の最大距離の2乗

    for (auto& minion : minions_) {
        if (!minion || !minion->IsActive()) continue;
        if (minion->GetState() != MinionState::Rolling) continue;

        Vector3 diff = minion->GetPosition() - launchPos;
        diff.y = 0.0f; // 水平距離で判定
        float distSq = diff.x * diff.x + diff.z * diff.z;

        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestMinion = minion.get();
        }
    }

    if (bestMinion) {
        bestMinion->SetPosition(launchPos);
        bestMinion->Launch(velocity);
        return true;
    }

    return false;
}

void MinionManager::Update(float deltaTime, const Vector3& playerPos, bool isMerged, float playerScale, const Vector2& stageTilt, const Vector3& playerSquash, const Vector3& playerVelocity) {
    if (isMergedState_ != isMerged) {
        if (isMerged) {
            TriggerMerge(playerPos, mergePickupRadius_);
        } else {
            TriggerSplit(playerPos);
        }
    }

    if (isMergedState_) {
        bool allInactive = true;
        float absorbRadius = (std::max)(0.6f, playerScale * 0.7f);
        float absorbRadiusSq = absorbRadius * absorbRadius;

        for (auto& minion : minions_) {
            if (minion && minion->IsActive() && minion->GetState() == MinionState::Merging) {
                Vector3 diff = minion->GetPosition() - playerPos;
                float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
                if (distSq < absorbRadiusSq) {
                    minion->SetActive(false);
                } else {
                    allInactive = false;
                    minion->AttractTo(playerPos, 28.0f);
                }
            }
        }
        isAllMerged_ = allInactive;
    }

    // 1. 各ミニオンの物理挙動更新（重力加速・移動・変形パラメータ計算）
    for (auto& minion : minions_) {
        if (minion) {
            minion->Update(deltaTime, stageTilt);
        }
    }

    // 2. 移動完了後の確定座標・変形状態に基づく多重球分離（衝突解消）
    // 通常時・合体巨大化時を問わず常に実行し、小型ミニオンへのめり込みを100%防止
    Vector3 playerScaleVec = { playerScale, playerScale, playerScale };
    Vector3 rot = { stageTilt.x, 0.0f, -stageTilt.y };

    // 2パスのリラクゼーションで多頭密集時の押し出し・めり込みを完全解消
    for (int iter = 0; iter < 2; ++iter) {
        ResolvePlayerSeparation(playerPos, playerVelocity, playerScaleVec, playerSquash, rot, stageTilt);
        ResolveSeparation(rot, stageTilt);
    }
}

void MinionManager::Draw(const RenderContext& ctx) {
    for (auto& minion : minions_) {
        if (minion) {
            minion->Draw(ctx);
        }
    }
}

void MinionManager::DrawDebug(Camera* camera) {
#ifdef _DEBUG
    if (!camera) return;
    for (const auto& minion : minions_) {
        if (minion && minion->IsActive() && minion->GetState() != MinionState::Merging) {
            auto shape = SlimeCollision::BuildMultiSphere(minion->GetPosition(), minion->GetScale(), minion->GetSlimeParams().squashStretch, minion->GetRotation());
            uint32_t color = 0xFF00FF7F; // 緑
            if (minion->GetType() == MinionType::Red) color = 0xFF4040FF;         // 赤
            else if (minion->GetType() == MinionType::Yellow) color = 0xFF00FFFF; // 黄
            else if (minion->GetType() == MinionType::Blue) color = 0xFFFF8000;   // 青
            SlimeCollision::DrawDebugMultiSphere(shape, camera, color);
        }
    }
#endif
}

