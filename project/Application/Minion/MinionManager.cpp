#include "MinionManager.h"
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
        minion->SetFollowSpeed(followSpeed_);
        minions_.push_back(std::move(minion));
    }
}

void MinionManager::SetFollowSpeed(float speed) {
    followSpeed_ = speed;
    for (auto& m : minions_) {
        if (m) {
            m->SetFollowSpeed(speed);
        }
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

void MinionManager::CalculateFormationSlots(const Vector3& playerPos, float playerYaw) {
    int slotIndex = 0;
    int ring = 1;
    int slotsInRing = 4;
    int ringSlotCounter = 0;

    for (auto& minion : minions_) {
        if (!minion || !minion->IsActive()) continue;
        if (minion->GetState() != MinionState::Following) continue;

        float ringRadius = slotBaseRadius_ + (ring - 1) * slotRowSpacing_;
        
        float angleStep = (kPi * 0.7f) / static_cast<float>(slotsInRing);
        float baseAngle = (playerYaw + kPi) - (kPi * 0.35f);
        float angle = baseAngle + angleStep * (ringSlotCounter + 0.5f);

        Vector3 slotPos;
        slotPos.x = playerPos.x + std::sin(angle) * ringRadius;
        slotPos.y = playerPos.y;
        slotPos.z = playerPos.z + std::cos(angle) * ringRadius;

        minion->SetTargetPosition(slotPos);

        ringSlotCounter++;
        if (ringSlotCounter >= slotsInRing) {
            ring++;
            slotsInRing += 3;
            ringSlotCounter = 0;
        }
        slotIndex++;
    }
}

void MinionManager::ResolveSeparation() {
    size_t count = minions_.size();
    for (size_t i = 0; i < count; ++i) {
        if (!minions_[i] || !minions_[i]->IsActive()) continue;
        if (minions_[i]->GetState() != MinionState::Following) continue;

        Vector3 posA = minions_[i]->GetPosition();
        for (size_t j = i + 1; j < count; ++j) {
            if (!minions_[j] || !minions_[j]->IsActive()) continue;
            if (minions_[j]->GetState() != MinionState::Following) continue;

            Vector3 posB = minions_[j]->GetPosition();
            Vector3 diff = posA - posB;
            diff.y = 0.0f;
            float distSq = diff.x * diff.x + diff.z * diff.z;

            float minDist = separationRadius_;
            if (distSq < minDist * minDist && distSq > 0.0001f) {
                float dist = std::sqrt(distSq);
                float overlap = 0.5f * (minDist - dist);
                float factor = overlap / dist;
                Vector3 push = { diff.x * factor, diff.y * factor, diff.z * factor };
                minions_[i]->AddRepulsion(push);
                minions_[j]->AddRepulsion({ -push.x, -push.y, -push.z });
            }
        }
    }
}

void MinionManager::TriggerMerge(const Vector3& playerPos) {
    isMergedState_ = true;
    isAllMerged_ = false;
    for (auto& minion : minions_) {
        if (minion && minion->IsActive()) {
            minion->AttractTo(playerPos, 28.0f);
        }
    }
}

void MinionManager::TriggerSplit(const Vector3& playerPos) {
    isMergedState_ = false;
    isAllMerged_ = false;
    int totalMinions = (std::max)(1, static_cast<int>(minions_.size()));
    float angleStep = (2.0f * kPi) / static_cast<float>(totalMinions);
    
    int index = 0;
    for (auto& minion : minions_) {
        if (minion) {
            minion->SetActive(true);
            minion->SetPosition(playerPos);
            minion->SetFollowSpeed(followSpeed_);
            
            float angle = angleStep * index + ((std::rand() % 100) / 100.0f - 0.5f) * 0.3f;
            float popSpeed = 6.0f + (std::rand() % 100) / 25.0f; // 6.0f ~ 10.0f
            float upSpeed = 6.0f + (std::rand() % 100) / 25.0f;  // 6.0f ~ 10.0f

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

bool MinionManager::ThrowMinion(const Vector3& launchPos, const Vector3& forwardDir, float throwPower, float upPower) {
    if (isMergedState_) return false;

    Vector3 vel = {
        forwardDir.x * throwPower,
        upPower,
        forwardDir.z * throwPower
    };
    return ThrowMinionWithVelocity(launchPos, vel);
}

bool MinionManager::ThrowMinionWithVelocity(const Vector3& launchPos, const Vector3& velocity) {
    if (isMergedState_) return false;

    for (auto& minion : minions_) {
        if (minion && minion->IsActive() && minion->GetState() == MinionState::Following) {
            minion->SetPosition(launchPos);
            minion->Launch(velocity);
            return true;
        }
    }
    return false;
}

void MinionManager::Whistle(const Vector3& whistlePos, float radius) {
    float rSq = radius * radius;
    for (auto& minion : minions_) {
        if (minion && minion->IsActive()) {
            Vector3 diff = minion->GetPosition() - whistlePos;
            diff.y = 0.0f;
            if (diff.x * diff.x + diff.z * diff.z <= rSq) {
                minion->SetState(MinionState::Following);
            }
        }
    }
}

void MinionManager::Update(float deltaTime, const Vector3& playerPos, float playerYaw, bool isMerged, float playerRadius) {
    if (isMergedState_ != isMerged) {
        if (isMerged) {
            TriggerMerge(playerPos);
        } else {
            TriggerSplit(playerPos);
        }
    }

    if (!isMergedState_) {
        CalculateFormationSlots(playerPos, playerYaw);
        ResolveSeparation();
    } else {
        bool allInactive = true;
        float absorbRadius = (std::max)(0.6f, playerRadius * 0.7f);
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

    for (auto& minion : minions_) {
        if (minion) {
            minion->Update(deltaTime);
        }
    }
}

void MinionManager::Draw(const RenderContext& ctx) {
    for (auto& minion : minions_) {
        if (minion) {
            minion->Draw(ctx);
        }
    }
}
