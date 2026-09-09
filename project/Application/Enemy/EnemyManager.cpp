#define NOMINMAX
#include "EnemyManager.h"

#include "Application/GameObject/SlimeManager.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
    const char* kTypeNames[] = { "Slime", "FlowerClover", "FlowerLotus", "FlowerSunward" };
}

EnemyManager::~EnemyManager()
{
    Finalize();
}

void EnemyManager::Initialize(Object3dCom* object3dCom, Camera* camera)
{
    object3dCom_ = object3dCom;
    camera_ = camera;

    enemies_.clear();
    bullets_.clear();

    // 敵まわりの当たり判定はすべて EnemyCollision で自前解決する。
    // エンジン側の押し出しは全部切っておく:
    //   - Player / Minion とは「強さ比較」で結果が変わるので、単純な押し出しでは足りない
    //   - Obstacle（地面の MeshCollider）に押されると敵の座標が勝手に書き換わり、
    //     こちらの地面追従と喧嘩して最終的に島の外へ押し出され、奈落へ落ちていた
    if (auto* cm = CollisionManager::GetInstance())
    {
        cm->SetCollisionFilter(CollisionAttribute::Player, CollisionAttribute::Enemy, false);
        cm->SetCollisionFilter(CollisionAttribute::Minion, CollisionAttribute::Enemy, false);
        cm->SetCollisionFilter(CollisionAttribute::Bullet, CollisionAttribute::Enemy, false);
        cm->SetCollisionFilter(CollisionAttribute::Obstacle, CollisionAttribute::Enemy, false);
    }
}

void EnemyManager::Finalize()
{
    // Initialize() で切ったフィルタを戻しておく。
    // CollisionManager はシーンより長生きするシングルトンなので、
    // 切りっぱなしにすると次のシーンに影響が残る
    if (auto* cm = CollisionManager::GetInstance())
    {
        cm->SetCollisionFilter(CollisionAttribute::Player, CollisionAttribute::Enemy, true);
        cm->SetCollisionFilter(CollisionAttribute::Minion, CollisionAttribute::Enemy, true);
        cm->SetCollisionFilter(CollisionAttribute::Bullet, CollisionAttribute::Enemy, true);
        cm->SetCollisionFilter(CollisionAttribute::Obstacle, CollisionAttribute::Enemy, true);
    }

    for (auto& e : enemies_)
    {
        if (e) e->Finalize();
    }
    enemies_.clear();
    bullets_.clear();
    bulletModels_.clear();
    object3dCom_ = nullptr;
    camera_ = nullptr;
}

int EnemyManager::RollStrength(EnemyType type)
{
    const MobEnemyConfig& c = GetMobEnemyConfig(type);
    int lo = (std::max)(1, c.strengthMin);
    int hi = (std::max)(lo, c.strengthMax);
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng_);
}

MobEnemy* EnemyManager::Spawn(EnemyType type, const Vector3& stageLocalPos, int strength)
{
    if (!object3dCom_) return nullptr;

    int s = (strength >= 1) ? strength : RollStrength(type);

    auto enemy = std::make_unique<MobEnemy>(type);
    if (scaleFunc_)
    {
        enemy->SetScaleFromStrength(scaleFunc_);
    }
    enemy->Initialize(object3dCom_, camera_, stageLocalPos, s);

    MobEnemy* raw = enemy.get();
    enemies_.push_back(std::move(enemy));
    return raw;
}

void EnemyManager::SpawnDebugSet(const Vector3& center)
{
    // 4種類を円状に並べる。強さはランダム
    Spawn(EnemyType::Slime, { center.x - 6.0f, 0.0f, center.z + 6.0f });
    Spawn(EnemyType::Slime, { center.x + 6.0f, 0.0f, center.z + 7.0f });
    Spawn(EnemyType::FlowerClover, { center.x - 3.0f, 0.0f, center.z + 10.0f });
    Spawn(EnemyType::FlowerClover, { center.x + 3.5f, 0.0f, center.z + 11.0f });
    Spawn(EnemyType::FlowerLotus, { center.x - 8.0f, 0.0f, center.z + 12.0f });
    Spawn(EnemyType::FlowerSunward, { center.x + 8.0f, 0.0f, center.z + 13.0f });
}

void EnemyManager::ClearAll()
{
    for (auto& e : enemies_)
    {
        if (e) e->Finalize();
    }
    enemies_.clear();

    for (auto& b : bullets_)
    {
        if (b) b->Kill();
    }
}

void EnemyManager::Remove(MobEnemy* enemy)
{
    if (!enemy) return;

    auto it = std::find_if(enemies_.begin(), enemies_.end(),
                           [enemy](const std::unique_ptr<MobEnemy>& e) { return e.get() == enemy; });
    if (it == enemies_.end()) return;

    (*it)->Finalize();
    enemies_.erase(it);
}

MobEnemy* EnemyManager::ReplaceType(MobEnemy* enemy, EnemyType newType)
{
    if (!enemy) return nullptr;
    if (enemy->GetType() == newType) return enemy;

    // 種類ごとにモデルもアニメーションも違うので、同じ座標・同じ強さで作り直す
    Vector3 pos = enemy->GetStageLocalPosition();
    int strength = enemy->GetStrength();
    bool wasFrozen = enemy->IsFrozen();

    Remove(enemy);

    MobEnemy* replaced = Spawn(newType, pos, strength);
    if (replaced) replaced->SetFrozen(wasFrozen);
    return replaced;
}

void EnemyManager::SetEditorMode(bool on)
{
    editorMode_ = on;

    for (auto& e : enemies_)
    {
        if (e) e->SetFrozen(on);
    }

    if (on)
    {
        // 飛んでいる弾は置き去りにせず消しておく
        for (auto& b : bullets_)
        {
            if (b) b->Kill();
        }
    }
}

void EnemyManager::SetScaleFromStrength(EnemyBase::ScaleFromStrengthFunc func)
{
    scaleFunc_ = std::move(func);

    // 既存の個体にも反映（nullptr を渡すと EnemyBase の既定関数に戻る）
    for (auto& e : enemies_)
    {
        if (e) e->SetScaleFromStrength(scaleFunc_);
    }
}

const Object3d::ModelData* EnemyManager::GetOrLoadBulletModel(const MobEnemyConfig& config, std::string& outKey)
{
    if (!config.bulletDirectory || !config.bulletFileName) return nullptr;
    if (config.bulletDirectory[0] == '\0' || config.bulletFileName[0] == '\0') return nullptr;

    outKey = std::string(config.bulletDirectory) + "/" + config.bulletFileName;

    auto it = bulletModels_.find(outKey);
    if (it != bulletModels_.end())
    {
        return &it->second;
    }

    std::string file = config.bulletFileName;
    bool isObj = (file.size() >= 4) && (file.compare(file.size() - 4, 4, ".obj") == 0);
    Object3d::ModelData data = isObj ? Object3d::LoadObjFile(config.bulletDirectory, file)
                                     : Object3d::LoadModelFile(config.bulletDirectory, file);
    if (data.vertices.empty())
    {
        return nullptr;
    }

    float maxLenSq = 0.0f;
    for (const auto& v : data.vertices)
    {
        float lenSq = v.position.x * v.position.x + v.position.y * v.position.y + v.position.z * v.position.z;
        maxLenSq = (std::max)(maxLenSq, lenSq);
    }
    data.boundingRadius = (maxLenSq > 0.0f) ? std::sqrt(maxLenSq) : 1.0f;

    auto inserted = bulletModels_.emplace(outKey, std::move(data));
    return &inserted.first->second;
}

void EnemyManager::FireBullet(const MobEnemyConfig& config, const MobEnemy::ShootRequest& request)
{
    // TODO(SE): 敵の弾の発射音をここで鳴らす
    //           request.origin が発射位置、request.direction が向き

    std::string key;
    const Object3d::ModelData* model = GetOrLoadBulletModel(config, key);
    if (!model) return;

    // プールから同じモデルの空きを探す
    EnemyBullet* target = nullptr;
    for (auto& b : bullets_)
    {
        if (b && !b->IsAlive() && b->GetModelKey() == key)
        {
            target = b.get();
            break;
        }
    }

    if (!target)
    {
        auto bullet = std::make_unique<EnemyBullet>();
        bullet->Setup(object3dCom_, camera_, *model, key, config.bulletColor);
        target = bullet.get();
        bullets_.push_back(std::move(bullet));
    }

    target->Fire(request.origin, request.direction,
                 config.bulletSpeed, config.bulletLifeTime,
                 config.bulletScale, config.bulletHitRadius);
}

Vector3 EnemyManager::CalcStageNormal(const Vector2& stageTilt)
{
    // SlimePhysics.cpp が既定法線として使っているのと同じ式
    Vector3 n{ -std::sin(stageTilt.y),
               std::cos(stageTilt.x) * std::cos(stageTilt.y),
               -std::sin(stageTilt.x) };
    float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    return (len > 1e-5f) ? n * (1.0f / len) : Vector3{ 0.0f, 1.0f, 0.0f };
}

void EnemyManager::Update(float deltaTime, const Vector2& stageTilt, SlimeManager* slimeManager)
{
    // 演出・SE 用のイベントは1フレームぶんだけ持つ
    defeatEvents_.clear();
    hitEvents_.clear();

    // 自爆イベントは必ず毎フレーム引き取る。
    // ここより下でリターンすると、古い座標のまま次フレームに爆発してしまう
    // （エディタ中はスライムの更新自体を止めているので発生しないが、
    //   イベントが溜まったまま Play に戻ると古い座標で爆発するので引き取りは続ける）
    if (slimeManager)
    {
        SlimeManager::SelfDestructEvent discarded;
        if (editorMode_)
        {
            slimeManager->TakeSelfDestructEvent(discarded);
        }
        else
        {
            ResolveSelfDestruct(slimeManager);
        }
    }

    if (!object3dCom_) return;

    // 敵が「どこを狙い、どのくらいの強さと比べるか」の基準は群れの代表（一番大きい個体）
    Slime* leader = slimeManager ? slimeManager->GetLeader() : nullptr;

    Vector3 playerPos = leader ? leader->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
    Vector2 pivot{ playerPos.x, playerPos.z };

    EnemyUpdateContext ctx;
    ctx.deltaTime = deltaTime;
    ctx.stageTilt = stageTilt;
    ctx.pivot = pivot;
    ctx.playerPos = playerPos;
    ctx.playerStrength = leader ? leader->GetSize() : 1;

    // 1. 敵の更新と発射要求の回収
    for (auto& e : enemies_)
    {
        if (!e || e->IsDead()) continue;

        e->Update(ctx);

        MobEnemy::ShootRequest req;
        if (e->TakeShootRequest(req) && !editorMode_)
        {
            FireBullet(e->GetConfig(), req);
        }
    }

    // 2. 弾の更新
    for (auto& b : bullets_)
    {
        if (b) b->Update(deltaTime);
    }

    // 3. 衝突解決（エディタ中は押し出しも撃破もしない）
    if (enableCollision_ && slimeManager && !editorMode_)
    {
        ResolveSlimeCollisions(slimeManager, stageTilt, pivot);
        ResolveBulletCollisions(leader);
    }

    // 4. 死体の掃除
    enemies_.erase(
        std::remove_if(enemies_.begin(), enemies_.end(),
                       [](const std::unique_ptr<MobEnemy>& e) { return !e || e->IsDead(); }),
        enemies_.end());
}

void EnemyManager::ResolveSlimeCollisions(SlimeManager* slimeManager, const Vector2& stageTilt, const Vector2& pivot)
{
    if (!slimeManager) return;

    // 判定ルールは全個体で共通。強さは Slime::GetSize()（含まれる最小単位スライムの数）。
    // 旧実装ではプレイヤー本体とミニオンで関数が分かれていたが、中身の違いは
    //   (1) 跳ね返り係数（bounceSpeed_ / minionBounceSpeed_）
    //   (2) 弾かれたときに Launch して放物線で飛ぶか、その場で押し戻すだけか
    //   (3) 演出イベントの isPlayer フラグ
    // の3点だけだったので、代表個体（一番大きいもの）かどうかで振り分けている。
    // 大きい塊が敵に当たって空を飛ぶと画が破綻するので、代表は Launch しない
    const Vector3 planeNormal = CalcStageNormal(stageTilt);
    const Slime* leader = slimeManager->GetLeader();

    for (const auto& slimePtr : slimeManager->GetSlimes())
    {
        Slime* target = slimePtr.get();
        if (!target || !target->IsActive()) continue;

        // 吸い込まれている最中の個体は判定から外す（位置が補間で飛ぶため）
        if (target->GetState() == SlimeState::Merging) continue;

        const bool isLeader = (target == leader);

        EnemyCollision::SlimeBody slime;
        slime.position = target->GetPosition();
        slime.scale = target->GetScale();
        slime.squashStretch = target->GetSlimeParams().squashStretch;
        slime.baseRadius = 1.0f; // SlimeCollision と同じ規約（見た目半径 = scale * 0.78）
        slime.strength = target->GetSize();

        Vector3 velocity = target->GetVelocity();
        bool changed = false;
        bool bounced = false;

        for (auto& e : enemies_)
        {
            if (!e || e->IsDead()) continue;

            auto result = EnemyCollision::ResolvePlayerVsEnemy(slime, velocity, e->MakeHitBody(),
                                                               isLeader ? bounceSpeed_ : minionBounceSpeed_,
                                                               planeNormal);
            if (!result.hit) continue;

            auto& params = target->GetSlimeParams();
            params.impulseStrength = (std::max)(params.impulseStrength, result.impulse);

            // 演出用。衝突点はスライム中心と敵のヒットボックス中心の中点で近似する
            const Vector3 enemyCenter = e->GetHitCenter();
            const Vector3 contact = { (slime.position.x + enemyCenter.x) * 0.5f,
                                      (slime.position.y + enemyCenter.y) * 0.5f,
                                      (slime.position.z + enemyCenter.z) * 0.5f };

            switch (result.outcome)
            {
            case EnemyCollision::HitOutcome::EnemyDefeated:
                // スライムのほうが強い。押し戻さずに突き抜けて倒す
                defeatEvents_.push_back({ enemyCenter, e->GetStrength() });
                e->Defeat();
                if (isLeader)
                {
                    params.squashStretch = { 0.18f, -0.14f, 0.18f };
                }
                else
                {
                    params.squashStretch = { 0.16f, -0.12f, 0.16f };
                }
                break;

            case EnemyCollision::HitOutcome::PlayerBounced:
                // 敵のほうが強い。跳ね飛ばされる
                hitEvents_.push_back({ contact, isLeader });
                if (isLeader)
                {
                    params.squashStretch = { 0.32f, -0.26f, 0.32f };
                }
                changed = true;
                bounced = true;
                break;

            case EnemyCollision::HitOutcome::Standoff:
                // 同じ強さ。押し合うだけ
                hitEvents_.push_back({ contact, isLeader });
                changed = true;
                break;

            default:
                break;
            }

            // 押される側の敵は、跳ね返し／押し合いどちらでも同じだけ動かす
            if (result.outcome != EnemyCollision::HitOutcome::EnemyDefeated &&
                (result.enemyPush.x != 0.0f || result.enemyPush.z != 0.0f))
            {
                e->ApplyPush(result.enemyPush, stageTilt, pivot);
            }
        }

        if (changed)
        {
            target->SetPosition(slime.position);

            // 小さい個体は弾かれたときだけ放物線で飛ばす。
            // 接触が続いているあいだ毎フレーム Launch すると、いつまでも着地できないので
            // まだ飛んでいない個体だけにする
            if (!isLeader && bounced && target->GetState() != SlimeState::Thrown)
            {
                // Launch() は Thrown 状態にして放物線を描かせる。弾かれた感じが出る
                target->Launch(velocity);
                // Launch() が squashStretch を上書きするので、演出はこの後に掛ける
                target->GetSlimeParams().squashStretch = { 0.28f, -0.22f, 0.28f };
            }
            else
            {
                target->SetVelocity(velocity);
            }
        }
    }
}

void EnemyManager::ResolveSelfDestruct(SlimeManager* slimeManager)
{
    if (!slimeManager) return;

    SlimeManager::SelfDestructEvent ev;
    if (!slimeManager->TakeSelfDestructEvent(ev)) return;

    // 分裂前の塊が大きいほど爆風も広い
    float radius = selfDestructBaseRadius_
                 + selfDestructPerSize_ * static_cast<float>((std::max)(1, ev.sizeBefore) - 1);
    float radiusSq = radius * radius;

    int kills = 0;
    for (auto& e : enemies_)
    {
        if (!e || e->IsDead()) continue;

        Vector3 center = e->GetHitCenter();
        float dx = center.x - ev.position.x;
        float dz = center.z - ev.position.z;
        float dy = center.y - ev.position.y;

        // 水平は爆風半径、縦はゆるめに見る（背の高い花も巻き込む）
        if (dx * dx + dz * dz > radiusSq) continue;
        if (std::abs(dy) > radius + 2.0f) continue;

        // 自爆は強さ問わず倒せる
        defeatEvents_.push_back({ center, e->GetStrength() });
        e->Defeat();
        ++kills;
    }

    lastSelfDestructRadius_ = radius;
    lastSelfDestructKills_ = kills;
}

void EnemyManager::ResolveBulletCollisions(Slime* target)
{
    // 代表スライムが居ない（全員落下中など）ときは何もしない
    if (!target) return;

    EnemyCollision::SlimeBody slime;
    slime.position = target->GetPosition();
    slime.scale = target->GetScale();
    slime.squashStretch = target->GetSlimeParams().squashStretch;
    slime.baseRadius = 1.0f;
    slime.strength = target->GetSize();

    // 同じフレームに複数当たっても、ノックバックは合成して1回だけ適用する
    Vector3 pushSum{ 0.0f, 0.0f, 0.0f };
    int hitCount = 0;

    for (auto& b : bullets_)
    {
        if (!b || !b->IsAlive()) continue;

        Vector3 pushDir{ 0.0f, 0.0f, 0.0f };
        if (!EnemyCollision::CheckBulletVsSlime(b->GetPosition(), b->GetHitRadius(), slime, pushDir))
        {
            continue;
        }

        pushSum.x += pushDir.x;
        pushSum.z += pushDir.z;
        ++hitCount;

        b->Kill();
    }

    if (hitCount > 0)
    {
        float len = std::sqrt(pushSum.x * pushSum.x + pushSum.z * pushSum.z);
        Vector3 dir = (len > 1e-4f) ? Vector3{ pushSum.x / len, 0.0f, pushSum.z / len }
                                    : Vector3{ 0.0f, 0.0f, 1.0f };

        // 暫定仕様: ノックバックのみ。塊のサイズは減らない
        Vector3 velocity = target->GetVelocity();
        velocity.x = dir.x * bulletKnockback_;
        velocity.z = dir.z * bulletKnockback_;
        velocity.y = (std::max)(velocity.y, bulletKnockback_ * 0.22f);
        target->SetVelocity(velocity);

        auto& params = target->GetSlimeParams();
        params.impulseStrength = (std::max)(params.impulseStrength, 0.5f);
        params.squashStretch = { 0.28f, -0.22f, 0.28f };
    }
}

void EnemyManager::Draw(const RenderContext& ctx)
{
    for (auto& e : enemies_)
    {
        if (e) e->Draw(ctx);
    }
    for (auto& b : bullets_)
    {
        if (b) b->Draw(ctx);
    }
}

int EnemyManager::GetAliveCount() const
{
    int count = 0;
    for (const auto& e : enemies_)
    {
        if (e && !e->IsDead()) ++count;
    }
    return count;
}

int EnemyManager::GetActiveBulletCount() const
{
    int count = 0;
    for (const auto& b : bullets_)
    {
        if (b && b->IsAlive()) ++count;
    }
    return count;
}

void EnemyManager::DrawImGui()
{
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("Enemy")) return;

    ImGui::Text("Alive: %d   Bullets: %d / %d pooled   SkinnedObj pool: %d",
                GetAliveCount(), GetActiveBulletCount(), static_cast<int>(bullets_.size()),
                GetSkinnedObject3dPoolTotal());
    ImGui::Checkbox("Enable Collision", &enableCollision_);
    ImGui::DragFloat("Bounce Speed", &bounceSpeed_, 0.1f, 0.0f, 40.0f);
    ImGui::DragFloat("Small Slime Bounce Speed", &minionBounceSpeed_, 0.1f, 0.0f, 40.0f);
    ImGui::DragFloat("Bullet Knockback", &bulletKnockback_, 0.1f, 0.0f, 40.0f);

    ImGui::SeparatorText("Self Destruct (E key)");
    ImGui::DragFloat("Blast Radius (size 1)", &selfDestructBaseRadius_, 0.05f, 0.0f, 40.0f);
    ImGui::DragFloat("Blast Radius / size", &selfDestructPerSize_, 0.05f, 0.0f, 10.0f);
    ImGui::Text("Last blast: r=%.2f  kills=%d", lastSelfDestructRadius_, lastSelfDestructKills_);

    ImGui::SeparatorText("Spawn");
    ImGui::Combo("Type##spawn", &imguiSpawnType_, kTypeNames, IM_ARRAYSIZE(kTypeNames));
    ImGui::DragInt("Strength (-1 = random)", &imguiSpawnStrength_, 0.1f, -1, 20);
    ImGui::DragFloat("Distance", &imguiSpawnDistance_, 0.1f, 1.0f, 40.0f);
    if (ImGui::Button("Spawn In Front", ImVec2(160, 28)))
    {
        // プレイヤーの手前ではなくワールド原点基準の適当な位置に出す
        // （厳密な配置は後で相談する前提の仮スポーン）
        Vector3 pos{ 0.0f, 0.0f, imguiSpawnDistance_ };
        Spawn(static_cast<EnemyType>(imguiSpawnType_), pos, imguiSpawnStrength_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Spawn Debug Set", ImVec2(160, 28)))
    {
        SpawnDebugSet({ 0.0f, 0.0f, 0.0f });
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All", ImVec2(100, 28)))
    {
        ClearAll();
    }

    ImGui::SeparatorText("Strength -> Scale");
    const char* presets[] = { "Default (curve)", "Linear (0.6 + 0.15n)", "Flat (1.0)" };
    if (ImGui::Combo("Preset", &imguiScalePreset_, presets, IM_ARRAYSIZE(presets)))
    {
        switch (imguiScalePreset_)
        {
        case 0: SetScaleFromStrength(nullptr); break;
        case 1: SetScaleFromStrength([](int s) { return 0.60f + 0.15f * static_cast<float>(s - 1); }); break;
        case 2: SetScaleFromStrength([](int) { return 1.0f; }); break;
        default: break;
        }
    }
    {
        // 今の変換でどんなスケールになるかのプレビュー
        char buf[192] = {};
        size_t offset = 0;
        for (int s = 1; s <= 6; ++s)
        {
            if (offset + 1 >= sizeof(buf)) break;
            float v = scaleFunc_ ? scaleFunc_(s) : EnemyBase::GetDefaultScaleFromStrengthFunc()(s);
            int written = snprintf(buf + offset, sizeof(buf) - offset, "%d:%.2f  ", s, v);
            if (written <= 0) break;
            offset += static_cast<size_t>(written);
        }
        ImGui::TextUnformatted(buf);
    }

    ImGui::SeparatorText("Type Settings");
    ImGui::Combo("Type##config", &imguiConfigType_, kTypeNames, IM_ARRAYSIZE(kTypeNames));
    {
        MobEnemyConfig& c = GetMobEnemyConfig(static_cast<EnemyType>(imguiConfigType_));
        ImGui::Text("%s  (%s / %s)", c.typeName,
                    c.canMove ? "move" : "fixed",
                    c.canShoot ? "shoot" : "no shot");

        ImGui::DragFloat("Model Scale", &c.modelScale, 0.01f, 0.05f, 5.0f);
        ImGui::DragFloat("Ground Offset", &c.groundOffsetRatio, 0.01f, -2.0f, 2.0f);
        ImGui::DragFloat("Hit Radius", &c.hitRadiusRatio, 0.01f, 0.05f, 3.0f);
        ImGui::DragFloat("Hit Offset Y", &c.hitOffsetRatio, 0.01f, 0.0f, 3.0f);
        ImGui::DragFloat3("Hit Half (AABB)", &c.hitHalfRatio.x, 0.01f, 0.05f, 3.0f);

        int shape = (c.hitShape == EnemyCollision::HitShape::AABB) ? 1 : 0;
        const char* shapes[] = { "Sphere", "AABB" };
        if (ImGui::Combo("Hit Shape", &shape, shapes, IM_ARRAYSIZE(shapes)))
        {
            c.hitShape = (shape == 1) ? EnemyCollision::HitShape::AABB : EnemyCollision::HitShape::Sphere;
        }

        ImGui::DragFloat("Move Speed", &c.moveSpeed, 0.05f, 0.0f, 20.0f);
        ImGui::DragFloat("Chase Range", &c.chaseRange, 0.1f, 0.0f, 60.0f);
        ImGui::DragFloat("Lose Range", &c.loseRange, 0.1f, 0.0f, 80.0f);
        ImGui::DragFloat("Keep Distance", &c.keepDistance, 0.05f, 0.0f, 20.0f);

        if (c.canShoot)
        {
            ImGui::DragFloat("Shoot Range", &c.shootRange, 0.1f, 0.0f, 60.0f);
            ImGui::DragFloat("Shoot Interval", &c.shootInterval, 0.05f, 0.1f, 15.0f);
            ImGui::DragFloat("Bullet Speed", &c.bulletSpeed, 0.1f, 0.5f, 60.0f);
            ImGui::DragFloat("Bullet Scale", &c.bulletScale, 0.01f, 0.02f, 3.0f);
            ImGui::DragFloat("Bullet Hit Radius", &c.bulletHitRadius, 0.01f, 0.02f, 3.0f);
            ImGui::DragFloat("Muzzle Height", &c.muzzleHeightRatio, 0.01f, 0.0f, 4.0f);
        }

        ImGui::DragInt("Strength Min", &c.strengthMin, 0.1f, 1, 30);
        ImGui::DragInt("Strength Max", &c.strengthMax, 0.1f, 1, 30);

        ImGui::Checkbox("Use Animation", &c.useAnimation);
        ImGui::DragFloat("Anim Speed", &c.animSpeed, 0.01f, 0.05f, 5.0f);
        ImGui::TextDisabled("clips: idle=%s / walk=%s / attack=%s / alert=%s",
                            c.clipIdle, c.clipWalk, c.clipAttack, c.clipAlert);
        ImGui::TextDisabled("Use Animation の切り替えは次にスポーンする個体から効きます");

        if (ImGui::Button("Apply To Existing", ImVec2(200, 26)))
        {
            // modelScale や当たり判定比率を変えたあと、既存個体に反映させる
            for (auto& e : enemies_)
            {
                if (e && e->GetType() == static_cast<EnemyType>(imguiConfigType_))
                {
                    e->RefreshFromSpec();
                }
            }
        }
        ImGui::TextDisabled("Model Scale / Hit ratio はこのボタンで既存個体に反映されます");
    }

    ImGui::SeparatorText("Instances");
    if (ImGui::BeginChild("EnemyList", ImVec2(0, 120), true))
    {
        for (size_t i = 0; i < enemies_.size(); ++i)
        {
            MobEnemy* e = enemies_[i].get();
            if (!e) continue;
            const Vector3& p = e->GetPosition();
            ImGui::Text("[%2zu] %-14s STR %2d  scale %.2f  pos(%.1f, %.1f, %.1f) %s %s",
                        i, e->GetTypeName(), e->GetStrength(), e->GetScale().x,
                        p.x, p.y, p.z,
                        e->IsChasing() ? "<chase>" : "",
                        e->IsAnimated() ? e->GetAnimator().GetCurrentClipName() : "[static]");
        }
    }
    ImGui::EndChild();
#endif
}
