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

void MinionManager::ResolveSeparation(const Vector3& rotation, const Vector2& stageTilt, const Vector2& pivot) {
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

            // どちらも合体可能なら、弾き飛ばし衝突は行わずマージ処理に委ねる
            if (minions_[i]->CanMerge() && minions_[j]->CanMerge()) continue;

            Vector3 posB = minions_[j]->GetPosition();
            Vector3 scaleB = minions_[j]->GetScale();
            const Vector3& squashB = minions_[j]->GetSlimeParams().squashStretch;


            float impulse = 0.0f;
            // 互いに50%ずつ押し合う (weightA = 0.5, weightB = 0.5)
            if (SlimeCollision::ResolveCollision(posA, scaleA, squashA, 0.5f,
                                                 posB, scaleB, squashB, 0.5f,
                                                 impulse, rotation, rotation, stageNormal)) {
                // 傾斜面上の厳密な接地中心に再クランプ（接地中かつ地面が存在する場合のみ）
                bool hasGroundA = false, hasGroundB = false;
                float gyA = SlimePhysics::CalculateGroundedCenterYEx(posA.x, posA.z, posA.y, stageTilt, 0.22f, &hasGroundA, pivot);
                float gyB = SlimePhysics::CalculateGroundedCenterYEx(posB.x, posB.z, posB.y, stageTilt, 0.22f, &hasGroundB, pivot);
                if (hasGroundA && minions_[i]->GetState() == MinionState::Rolling) posA.y = gyA;
                if (hasGroundB && minions_[j]->GetState() == MinionState::Rolling) posB.y = gyB;

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

void MinionManager::ResolvePlayerSeparation(const Vector3& playerPos, const Vector3& playerVelocity, const Vector3& playerScale, const Vector3& playerSquash, const Vector3& playerRotation, const Vector2& stageTilt, const Vector2& pivot) {
    Matrix4x4 rotMat = Multiply(MakeRotateXMatrix(playerRotation.x),
                                Multiply(MakeRotateYMatrix(playerRotation.y), MakeRotateZMatrix(playerRotation.z)));
    Vector3 stageNormal = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };

    Vector3 pPos = playerPos;
    for (auto& minion : minions_) {
        if (!minion || !minion->IsActive()) continue;
        // 合体可能なミニオンはプレイヤーに吸着合体するため押し出さない（吹き飛ばし防止）
        if (minion->CanMerge()) continue;
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
            bool hasGM = false;
            float gyM = SlimePhysics::CalculateGroundedCenterYEx(mPos.x, mPos.z, mPos.y, stageTilt, 0.22f, &hasGM, pivot);
            if (hasGM && minion->GetState() == MinionState::Rolling) mPos.y = gyM;
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

void MinionManager::TriggerSplit(const Vector3& playerPos, int splitCount) {
    (void)splitCount;
    isMergedState_ = false;
    isAllMerged_ = false;
    absorbedCount_ = 0;

    int totalMinions = static_cast<int>(minions_.size());
    if (totalMinions == 0) return;

    float angleStep = (2.0f * kPi) / static_cast<float>(totalMinions);

    for (size_t index = 0; index < minions_.size(); ++index) {
        auto& minion = minions_[index];
        if (!minion) continue;

        bool wasInactive = !minion->IsActive();
        minion->SetSize(1);
        minion->SetActive(true);
        // 分裂直後の即時再合体を防止する適度なクールダウン（0.40秒）
        minion->SetMergeCooldown(0.40f);

        Vector3 mPos = minion->GetPosition();
        float distToPlayerSq = (mPos.x - playerPos.x) * (mPos.x - playerPos.x) + (mPos.z - playerPos.z) * (mPos.z - playerPos.z);

        // プレイヤーに吸収されていたスライム、または近傍にいたスライムはプレイヤー中心から放射状にパァン！と弾き出す
        if (wasInactive || distToPlayerSq < 2.5f * 2.5f) {
            minion->SetPosition(playerPos);

            float angle = angleStep * index + ((std::rand() % 100) / 100.0f - 0.5f) * 0.35f;
            float popSpeed = splitPopPower_ + ((std::rand() % 100) / 100.0f - 0.5f) * (splitPopPower_ * 0.25f);
            float upSpeed = splitUpPower_ + ((std::rand() % 100) / 100.0f - 0.5f) * (splitUpPower_ * 0.25f);

            Vector3 launchVel = {
                std::sin(angle) * popSpeed,
                upSpeed,
                std::cos(angle) * popSpeed
            };
            minion->Launch(launchVel);
        } else {
            // もともと離れた位置にいた仲間スライムは、その場で上方向に小さくポヨンとホップ
            Vector3 jumpVel = { 0.0f, splitUpPower_ * 0.6f, 0.0f };
            minion->Launch(jumpVel);
        }
    }
}


void MinionManager::SetAllAbsorbed(bool absorbed) {
    for (auto& minion : minions_) {
        if (minion) {
            minion->SetActive(!absorbed);
            minion->SetSize(1);
        }
    }
    absorbedCount_ = absorbed ? static_cast<int>(minions_.size()) : 0;
    isAllMerged_ = absorbed;
    isMergedState_ = absorbed;
}

void MinionManager::SetInitialAbsorbedCount(int absorbedCount) {
    absorbedCount_ = absorbedCount;
    int total = static_cast<int>(minions_.size());
    for (int i = 0; i < total; ++i) {
        if (minions_[i]) {
            minions_[i]->SetSize(1);
            // absorbedCount 体は吸収中（非アクティブ）、残りはアクティブ
            minions_[i]->SetActive(i >= absorbedCount);
        }
    }
    isMergedState_ = (absorbedCount > 0);
    isAllMerged_ = (absorbedCount >= total);
}

MinionManager::MergeResult MinionManager::CheckAndResolveMerge(const Vector3& playerPos, float playerScale, int playerSize) {
    MergeResult result;

    // 1. プレイヤーとミニオンの接触合体判定（水平面XZ距離で判定）
    float playerRadius = playerScale * 0.78f;

    for (auto& minion : minions_) {
        if (!minion || !minion->CanMerge()) continue;

        Vector3 mPos = minion->GetPosition();
        float dx = mPos.x - playerPos.x;
        float dz = mPos.z - playerPos.z;
        float distSq = dx * dx + dz * dz;

        // スライムの扁平変形や接近吸い寄せを考慮し、十分な余裕マージン(+0.50m)を付与
        float mergeDist = playerRadius + minion->GetRadius() + 0.50f;
        if (distSq <= mergeDist * mergeDist) {
            // プレイヤーに合体！
            int mSize = minion->GetSize();
            absorbedCount_ += mSize;
            result.newlyMergedCount += mSize;
            minion->SetActive(false);
        }
    }

    // 2. ミニオン同士の接触合体判定（小ロコロコ同士が接触したら 1+1=2、2+1=3... と合体成長）
    size_t count = minions_.size();
    for (size_t i = 0; i < count; ++i) {
        if (!minions_[i] || !minions_[i]->CanMerge()) continue;

        for (size_t j = i + 1; j < count; ++j) {
            if (!minions_[j] || !minions_[j]->CanMerge()) continue;

            Vector3 posA = minions_[i]->GetPosition();
            Vector3 posB = minions_[j]->GetPosition();
            float dx = posA.x - posB.x;
            float dz = posA.z - posB.z;
            float distSq = dx * dx + dz * dz;

            float mergeDist = minions_[i]->GetRadius() + minions_[j]->GetRadius() + 0.50f;
            if (distSq <= mergeDist * mergeDist) {
                int combinedSize = minions_[i]->GetSize() + minions_[j]->GetSize();
                Vector3 mergeCenter = { (posA.x + posB.x) * 0.5f, (posA.y + posB.y) * 0.5f, (posA.z + posB.z) * 0.5f };

                if (playerSize <= 1 && !result.playerPromoted) {
                    // プレイヤーが分裂後の最小サイズ(1)のとき、最初に触れ合った仲間同士の接触点に
                    // プレイヤー本体を昇格・合体出現させる（ロコロコ本家の完全対等システム）
                    result.playerPromoted = true;
                    result.promotedPos = mergeCenter;
                    result.promotedSize = combinedSize;
                    absorbedCount_ += (combinedSize - 1);
                    result.newlyMergedCount += (combinedSize - 1);

                    minions_[i]->SetActive(false);
                    minions_[j]->SetActive(false);
                } else {
                    // すでにプレイヤーがある程度育っている場合は、仲間iに仲間jを合体させてサイズ成長
                    minions_[i]->SetPosition(mergeCenter);
                    minions_[i]->SetSize(combinedSize);
                    minions_[i]->GetSlimeParams().impulseStrength = 0.50f; // ポヨン！と合体弾性
                    minions_[j]->SetActive(false);
                }
            }
        }
    }

    // 合体状態フラグの更新
    bool allInactive = true;
    for (const auto& m : minions_) {
        if (m && m->IsActive()) {
            allInactive = false;
            break;
        }
    }
    isAllMerged_ = allInactive;
    isMergedState_ = (absorbedCount_ > 0);

    return result;
}



void MinionManager::GetGroupCenterAndSpread(const Vector3& playerPos, Vector3& outCenter, float& outSpread) const {
    Vector3 sumPos = playerPos;
    int count = 1; // プレイヤー自身も含める

    // プレイヤーから極端に遠くへ吹っ飛んだミニオンが重心を異常に引っ張らないよう保護（半径16m以内を優先）
    const float kMaxInfluenceRadiusSq = 16.0f * 16.0f;

    for (const auto& minion : minions_) {
        if (minion && minion->IsActive()) {
            Vector3 mPos = minion->GetPosition();
            float dx = mPos.x - playerPos.x;
            float dz = mPos.z - playerPos.z;
            float distSq = dx * dx + dz * dz;

            if (distSq <= kMaxInfluenceRadiusSq) {
                sumPos += mPos;
                count++;
            } else {
                // 遠方のミニオンは影響半径の境界位置で重心に寄与させる
                float factor = 16.0f / std::sqrt(distSq);
                sumPos.x += playerPos.x + dx * factor;
                sumPos.y += playerPos.y;
                sumPos.z += playerPos.z + dz * factor;
                count++;
            }
        }
    }

    outCenter = { sumPos.x / count, playerPos.y, sumPos.z / count };

    // 重心からの最大距離（広がり幅）を計算（最大14mでクランプ）
    float maxDistSq = 0.0f;
    Vector3 diffPlayer = playerPos - outCenter;
    maxDistSq = (std::max)(maxDistSq, diffPlayer.x * diffPlayer.x + diffPlayer.z * diffPlayer.z);

    for (const auto& minion : minions_) {
        if (minion && minion->IsActive()) {
            Vector3 diff = minion->GetPosition() - outCenter;
            float distSq = diff.x * diff.x + diff.z * diff.z;
            if (distSq > maxDistSq) {
                maxDistSq = distSq;
            }
        }
    }

    outSpread = (std::min)(14.0f, std::sqrt(maxDistSq));
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

MinionManager::MergeResult MinionManager::Update(float deltaTime, const Vector3& playerPos, bool isMerged, float playerScale, const Vector2& stageTilt, const Vector3& playerSquash, const Vector3& playerVelocity, int playerSize) {
    // 接触による自然合体判定の実行
    MergeResult mergeResult = CheckAndResolveMerge(playerPos, playerScale, playerSize);

    Vector2 pivot = { playerPos.x, playerPos.z };

    // 1. 各ミニオンの物理挙動更新（重力加速・移動・変形パラメータ計算）
    for (auto& minion : minions_) {
        if (minion) {
            minion->Update(deltaTime, stageTilt, pivot);
        }
    }

    // 2. 移動完了後の確定座標・変形状態に基づく多重球分離（衝突解消）
    // 通常時・合体巨大化時を問わず常に実行し、小型ミニオンへのめり込みを100%防止
    Vector3 playerScaleVec = { playerScale, playerScale, playerScale };
    Vector3 rot = { stageTilt.x, 0.0f, -stageTilt.y };

    // 2パスのリラクゼーションで多頭密集時の押し出し・めり込みを完全解消
    for (int iter = 0; iter < 2; ++iter) {
        ResolvePlayerSeparation(playerPos, playerVelocity, playerScaleVec, playerSquash, rot, stageTilt, pivot);
        ResolveSeparation(rot, stageTilt, pivot);
    }

    return mergeResult;
}

int MinionManager::GetMaxMinionSize() const {
    int maxS = 0;
    for (const auto& minion : minions_) {
        if (minion && minion->IsActive()) {
            maxS = (std::max)(maxS, minion->GetSize());
        }
    }
    return maxS;
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

