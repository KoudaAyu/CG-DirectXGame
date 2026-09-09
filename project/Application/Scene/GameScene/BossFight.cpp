#define NOMINMAX
#include "Application/Scene/GameScene/BossFight.h"

#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Application/Enemy/EnemyCollision.h"
#include "Application/GameObject/SlimeManager.h"
#include "Application/GameObject/Slime.h"
#include "Application/GameObject/StageTerrain.h"
#include "Application/Scene/GameScene/GamePlaySceneFX.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
    constexpr float kPi = 3.14159265358979323846f;

    /// @brief 0..1 を滑らかに（両端で速度0）
    float SmoothStep01(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
}

BossFight::~BossFight()
{
    Finalize();
}

void BossFight::Initialize(const SceneRefs& refs)
{
    refs_ = refs;

    hpBar_.Initialize();

    phase_ = Phase::Idle;
    phaseTimer_ = 0.0f;
    cameraBlend_ = 0.0f;
    cameraTargetValid_ = false;
    clearRequested_ = false;
    pendingSelfDestructs_ = 0;

    bullets_.clear();
    bulletModelReady_ = false;
}

void BossFight::Finalize()
{
    DestroyBoss();
    bullets_.clear();
    hpBar_.Finalize();
    refs_ = SceneRefs{};
}

// ===================================================================
// 配置
// ===================================================================

void BossFight::SetLayout(const StageBossEntry& entry)
{
    enabled_ = entry.enabled;
    placedPosition_ = entry.position;
    placedHp_ = (entry.hp > 0) ? entry.hp : 100;

    Restart();
}

void BossFight::WriteLayout(StageBossEntry& out) const
{
    out.enabled = enabled_;
    out.position = placedPosition_;
    out.hp = placedHp_;
}

void BossFight::Restart()
{
    DestroyBoss();

    for (auto& b : bullets_)
    {
        if (b) b->Kill();
    }

    phase_ = Phase::Idle;
    phaseTimer_ = 0.0f;
    cameraBlend_ = 0.0f;
    cameraTargetValid_ = false;
    clearRequested_ = false;
    pendingSelfDestructs_ = 0;

    hpBar_.Hide();

    if (enabled_)
    {
        SpawnBoss();
    }
}

void BossFight::SetStageLocalPosition(const Vector3& p)
{
    placedPosition_ = p;
    if (boss_)
    {
        boss_->SetStageLocalPosition(p);
        boss_->RequestGroundSnap();
    }
}

void BossFight::SetEnabled(bool enabled)
{
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    Restart();
}

void BossFight::SetMaxHp(int hp)
{
    placedHp_ = (std::max)(1, hp);
    if (boss_) boss_->ResetHp(placedHp_);
}

void BossFight::SpawnBoss()
{
    if (!refs_.object3dCom) return;

    BossConfig& config = GetBossConfig();
    config.maxHp = placedHp_;

    boss_ = std::make_unique<Boss>();
    boss_->Initialize(refs_.object3dCom, refs_.camera, placedPosition_, placedHp_);
    boss_->ResetHp(placedHp_);

    // トリガーが引かれるまでは撃たない。その場に立って睨んでいるだけ
    boss_->SetShootEnabled(false);
}

void BossFight::DestroyBoss()
{
    if (boss_)
    {
        boss_->Finalize();
        boss_.reset();
    }
}

bool BossFight::HasVisibleBoss() const
{
    return boss_ != nullptr && !boss_->IsDead();
}

Vector3 BossFight::GetWorldPosition() const
{
    if (boss_) return boss_->GetPosition();
    return placedPosition_;
}

float BossFight::GetPickRadius() const
{
    if (boss_) return (std::max)(1.5f, boss_->GetVisualRadius());
    return (std::max)(1.5f, GetBossConfig().hitRadiusRatio * GetBossConfig().modelScale);
}

// ===================================================================
// イベント
// ===================================================================

void BossFight::NotifySelfDestruct(const Vector3& position)
{
    ++pendingSelfDestructs_;
    lastSelfDestructPos_ = position;
}

// ===================================================================
// トリガー
// ===================================================================

bool BossFight::CheckTrigger(const Vector2& pivot) const
{
    if (debugForceTrigger_) return true;
    if (!refs_.terrain) return false;

    // pivot はスライム重心の XZ ＝ ステージ傾斜の回転中心なので、
    // そのまま「傾き0のときのワールド座標」として使える
    return const_cast<StageTerrain*>(refs_.terrain)->IsBossTriggerAt(pivot.x, pivot.y);
}

// ===================================================================
// 更新
// ===================================================================

Vector3 BossFight::CalcStageNormal(const Vector2& stageTilt)
{
    // SlimePhysics.cpp / EnemyManager.cpp が使っているのと同じ式
    Vector3 n{ -std::sin(stageTilt.y),
               std::cos(stageTilt.x) * std::cos(stageTilt.y),
               -std::sin(stageTilt.x) };
    float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    return (len > 1e-5f) ? n * (1.0f / len) : Vector3{ 0.0f, 1.0f, 0.0f };
}

void BossFight::EnterPhase(Phase next)
{
    phase_ = next;
    phaseTimer_ = 0.0f;
}

void BossFight::BeginDeath()
{
    if (phase_ == Phase::Dying || phase_ == Phase::Explode || phase_ == Phase::Finished) return;

    EnterPhase(Phase::Dying);

    if (boss_)
    {
        boss_->SetShootEnabled(false);
        boss_->SetFrozen(true);
        boss_->SetHp(0);
    }

    // 撃ち残しの弾は消す。死んだあとに当たると気持ち悪い
    for (auto& b : bullets_)
    {
        if (b) b->Kill();
    }
}

BossFight::FrameResult BossFight::Update(const FrameInput& input)
{
    FrameResult result;

    // 自爆イベントは必ず引き取る（溜まったまま次のフェーズへ持ち越さない）
    const int selfDestructs = pendingSelfDestructs_;
    pendingSelfDestructs_ = 0;

    if (!enabled_ || input.editorMode)
    {
        hpBar_.SetRatio(boss_ ? boss_->GetHpRatio() : 1.0f);
        hpBar_.Update(input.deltaTime);
        cameraBlend_ = 0.0f;

        // エディタ中でもボスは地面に吸着させておきたいので、凍結して Update だけ回す
        if (boss_ && input.editorMode)
        {
            boss_->SetFrozen(true);
            EnemyUpdateContext ctx;
            ctx.deltaTime = input.deltaTime;
            ctx.stageTilt = { 0.0f, 0.0f };
            ctx.pivot = input.pivot;
            ctx.playerPos = { input.pivot.x, 0.0f, input.pivot.y };
            ctx.playerStrength = 1;
            boss_->Update(ctx);
        }
        return result;
    }

    phaseTimer_ += input.deltaTime;

    Slime* leader = refs_.slimeManager ? refs_.slimeManager->GetLeader() : nullptr;
    const Vector3 playerPos = leader ? leader->GetPosition()
                                     : Vector3{ input.pivot.x, 0.0f, input.pivot.y };

    // ------------------------------------------------------------------
    // フェーズ進行
    // ------------------------------------------------------------------
    switch (phase_)
    {
    case Phase::Idle:
        if (boss_ && CheckTrigger(input.pivot))
        {
            EnterPhase(Phase::FocusIn);
            hpBar_.SetRatio(boss_->GetHpRatio());
            hpBar_.Show();

            // TODO(SE): ボス戦の開始音（ジングル）をここで鳴らす
        }
        break;

    case Phase::FocusIn:
    {
        // この間はボスもプレイヤーも動けない
        result.freezeSlimes = true;
        if (boss_)
        {
            boss_->SetFrozen(true);
            boss_->SetShootEnabled(false);
        }

        const float total = (std::max)(0.2f, focusInSeconds_);
        const float t = std::clamp(phaseTimer_ / total, 0.0f, 1.0f);

        // 寄る -> 見せる -> 戻る
        const float inEnd = std::clamp(focusInRatio_, 0.05f, 0.9f);
        const float holdEnd = std::clamp(inEnd + focusHoldRatio_, inEnd, 0.98f);

        if (t < inEnd)
        {
            cameraBlend_ = SmoothStep01(t / inEnd);
        }
        else if (t < holdEnd)
        {
            cameraBlend_ = 1.0f;
        }
        else
        {
            cameraBlend_ = 1.0f - SmoothStep01((t - holdEnd) / (std::max)(0.02f, 1.0f - holdEnd));
        }

        if (phaseTimer_ >= total)
        {
            cameraBlend_ = 0.0f;
            EnterPhase(Phase::Battle);
            if (boss_)
            {
                boss_->SetFrozen(false);
                boss_->SetShootEnabled(true);
            }
        }
        break;
    }

    case Phase::Battle:
        cameraBlend_ = 0.0f;
        if (boss_)
        {
            boss_->SetFrozen(false);
            boss_->SetShootEnabled(true);

            // --- 自爆でダメージ ---
            if (selfDestructs > 0)
            {
                const int damage = GetBossConfig().selfDestructDamage * selfDestructs;
                if (boss_->ApplyDamage(damage))
                {
                    // TODO(SE): ボスの最後の一撃（HPが尽きた瞬間）の音をここで鳴らす
                    result.scoreGain = input.playerLife * input.playerLife * input.playerLife;
                    result.scorePopupAt = boss_->GetHeadPosition();
                    BeginDeath();
                }
                else
                {
                    // TODO(SE): ボスの被弾音をここで鳴らす
                    if (refs_.fx)
                    {
                        refs_.fx->EmitBossHit(boss_->GetHeadPosition(), boss_->GetVisualRadius());
                    }
                }
            }
        }
        break;

    case Phase::Dying:
    {
        result.freezeSlimes = true;

        const float total = (std::max)(0.3f, deathSeconds_);
        const float t = std::clamp(phaseTimer_ / total, 0.0f, 1.0f);

        const float blendIn = std::clamp(deathBlendInRatio_, 0.02f, 0.9f);
        cameraBlend_ = (t < blendIn) ? SmoothStep01(t / blendIn) : 1.0f;

        // 震え・オーラ・カメラシェイクをだんだん激しく（後半で一気に上がるよう二乗）
        const float ramp = t * t;
        if (boss_)
        {
            boss_->SetAuraIntensity(1.0f + (deathAuraMax_ - 1.0f) * ramp);
            boss_->SetDeathShake(deathBodyShakeMax_ * ramp);
        }
        // trauma は毎フレーム減衰するので、毎フレーム足し直す
        result.cameraShake = deathShakeMax_ * ramp * input.deltaTime * 6.0f;

        if (phaseTimer_ >= total)
        {
            // --- 大爆発 ---
            if (refs_.fx && boss_)
            {
                refs_.fx->EmitBossExplosion(boss_->GetHeadPosition(), boss_->GetVisualRadius());
            }
            result.cameraShake += explodeShake_;

            // TODO(SE): ボスの爆散音をここで鳴らす

            DestroyBoss();
            hpBar_.SetRatio(0.0f);
            hpBar_.Hide();
            EnterPhase(Phase::Explode);
        }
        break;
    }

    case Phase::Explode:
    {
        result.freezeSlimes = true;

        const float total = (std::max)(0.2f, explodeSeconds_);
        const float t = std::clamp(phaseTimer_ / total, 0.0f, 1.0f);

        // 爆心を映したまま、後半でゆっくり通常カメラへ戻す
        cameraBlend_ = (t < 0.5f) ? 1.0f : 1.0f - SmoothStep01((t - 0.5f) * 2.0f);

        if (phaseTimer_ >= total)
        {
            EnterPhase(Phase::Finished);
        }
        break;
    }

    case Phase::Finished:
        cameraBlend_ = 0.0f;
        if (!clearRequested_)
        {
            clearRequested_ = true;
            result.requestClear = true;
        }
        break;
    }

    // ------------------------------------------------------------------
    // ボス本体の更新
    // ------------------------------------------------------------------
    if (boss_)
    {
        EnemyUpdateContext ctx;
        ctx.deltaTime = input.deltaTime;
        ctx.stageTilt = input.stageTilt;
        ctx.pivot = input.pivot;
        ctx.playerPos = playerPos;
        ctx.playerStrength = leader ? leader->GetSize() : 1;

        boss_->Update(ctx);

        // 発射要求を回収して弾にする
        Boss::ShootBurst burst;
        if (boss_->TakeShootBurst(burst) && burst.fire)
        {
            // TODO(SE): ボスの全方向弾の発射音をここで鳴らす
            //           burst.origin が発射位置、burst.directions が向きの配列
            const float forward = GetBossConfig().muzzleForward;
            for (const Vector3& dir : burst.directions)
            {
                const Vector3 origin = { burst.origin.x + dir.x * forward,
                                         burst.origin.y,
                                         burst.origin.z + dir.z * forward };
                FireBullet(origin, dir);
            }
        }
    }

    // ------------------------------------------------------------------
    // 弾の更新と衝突
    // ------------------------------------------------------------------
    for (auto& b : bullets_)
    {
        if (b) b->Update(input.deltaTime);
    }

    if (enableCollision_ && refs_.slimeManager && phase_ == Phase::Battle)
    {
        ResolveBulletCollisions(leader);

        if (ResolveSlimeCollisions(refs_.slimeManager, input.stageTilt, input.pivot, result))
        {
            // スコアの増分は「残機（＝全スライムのサイズ合計）の三乗」。
            // 倒し方（自爆で HP を削りきる / 体当たり）によらず同じ
            result.scoreGain = input.playerLife * input.playerLife * input.playerLife;
            if (boss_) result.scorePopupAt = boss_->GetHeadPosition();
            BeginDeath();
        }
    }

    // ------------------------------------------------------------------
    // カメラ目標と HP バー
    // ------------------------------------------------------------------
    cameraTargetValid_ = false;
    if (boss_ && cameraBlend_ > 0.0f)
    {
        // ボスの「正面」＝ボスが向いている方向。そこにカメラを置いて振り返らせる
        const float yaw = boss_->GetYaw();
        const Vector3 forward = { std::sin(yaw), 0.0f, std::cos(yaw) };

        const float radius = boss_->GetVisualRadius();
        const Vector3 lookAt = { boss_->GetPosition().x,
                                 boss_->GetPosition().y + radius * 2.0f * focusLookHeightRatio_,
                                 boss_->GetPosition().z };

        cameraTargetPos_ = { lookAt.x + forward.x * focusDistance_,
                             lookAt.y + focusHeight_,
                             lookAt.z + forward.z * focusDistance_ };

        const Vector3 toTarget = lookAt - cameraTargetPos_;
        const float horiz = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
        cameraTargetRot_ = { std::atan2(-toTarget.y, (std::max)(0.1f, horiz)),
                             std::atan2(toTarget.x, toTarget.z),
                             0.0f };
        cameraTargetValid_ = true;
    }
    else if (cameraBlend_ > 0.0f)
    {
        // 爆発後（ボスは既に消えている）は最後の位置を映したまま
        cameraTargetValid_ = true;
    }

    if (boss_) hpBar_.SetRatio(boss_->GetHpRatio());
    hpBar_.Update(input.deltaTime);

    // オーラは FX 側が毎フレーム撒く
    if (refs_.fx && boss_)
    {
        refs_.fx->UpdateBoss(input.deltaTime, boss_.get());
    }

    return result;
}

bool BossFight::GetCameraTarget(Vector3& outPosition, Vector3& outRotation) const
{
    if (!cameraTargetValid_) return false;
    outPosition = cameraTargetPos_;
    outRotation = cameraTargetRot_;
    return true;
}

// ===================================================================
// 弾
// ===================================================================

void BossFight::FireBullet(const Vector3& origin, const Vector3& direction)
{
    if (!refs_.object3dCom) return;

    const BossConfig& config = GetBossConfig();

    if (!bulletModelReady_)
    {
        if (!config.bulletDirectory || !config.bulletFileName) return;
        if (config.bulletDirectory[0] == '\0' || config.bulletFileName[0] == '\0') return;

        bulletModelKey_ = std::string(config.bulletDirectory) + "/" + config.bulletFileName;

        const std::string file = config.bulletFileName;
        const bool isObj = (file.size() >= 4) && (file.compare(file.size() - 4, 4, ".obj") == 0);
        bulletModel_ = isObj ? Object3d::LoadObjFile(config.bulletDirectory, file)
                             : Object3d::LoadModelFile(config.bulletDirectory, file);
        if (bulletModel_.vertices.empty()) return;

        float maxLenSq = 0.0f;
        for (const auto& v : bulletModel_.vertices)
        {
            const float lenSq = v.position.x * v.position.x + v.position.y * v.position.y + v.position.z * v.position.z;
            maxLenSq = (std::max)(maxLenSq, lenSq);
        }
        bulletModel_.boundingRadius = (maxLenSq > 0.0f) ? std::sqrt(maxLenSq) : 1.0f;
        bulletModelReady_ = true;
    }

    // プールから空きを探す
    EnemyBullet* target = nullptr;
    for (auto& b : bullets_)
    {
        if (b && !b->IsAlive())
        {
            target = b.get();
            break;
        }
    }

    if (!target)
    {
        auto bullet = std::make_unique<EnemyBullet>();
        bullet->Setup(refs_.object3dCom, refs_.camera, bulletModel_, bulletModelKey_, config.bulletColor);
        target = bullet.get();
        bullets_.push_back(std::move(bullet));
    }

    target->Fire(origin, direction, config.bulletSpeed, config.bulletLifeTime,
                 config.bulletScale, config.bulletHitRadius);
}

void BossFight::ResolveBulletCollisions(Slime* target)
{
    if (!target) return;

    EnemyCollision::SlimeBody slime;
    slime.position = target->GetPosition();
    slime.scale = target->GetScale();
    slime.squashStretch = target->GetSlimeParams().squashStretch;
    slime.baseRadius = 1.0f;
    slime.strength = target->GetSize();

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

    if (hitCount <= 0) return;

    // TODO(SE): ボスの弾がプレイヤーに当たったときの音をここで鳴らす

    const float len = std::sqrt(pushSum.x * pushSum.x + pushSum.z * pushSum.z);
    const Vector3 dir = (len > 1e-4f) ? Vector3{ pushSum.x / len, 0.0f, pushSum.z / len }
                                      : Vector3{ 0.0f, 0.0f, 1.0f };

    // 雑魚の弾と同じ暫定仕様: ノックバックのみ。塊のサイズは減らない
    Vector3 velocity = target->GetVelocity();
    velocity.x = dir.x * bulletKnockback_;
    velocity.z = dir.z * bulletKnockback_;
    velocity.y = (std::max)(velocity.y, bulletKnockback_ * 0.22f);
    target->SetVelocity(velocity);

    auto& params = target->GetSlimeParams();
    params.impulseStrength = (std::max)(params.impulseStrength, 0.5f);
    params.squashStretch = { 0.26f, -0.22f, 0.26f };
}

// ===================================================================
// ボス本体との接触
// ===================================================================

bool BossFight::ResolveSlimeCollisions(SlimeManager* slimeManager, const Vector2& stageTilt,
                                       const Vector2& pivot, FrameResult& result)
{
    (void)pivot;
    if (!slimeManager || !boss_ || boss_->IsDead()) return false;

    const Vector3 planeNormal = CalcStageNormal(stageTilt);
    const Slime* leader = slimeManager->GetLeader();

    bool defeated = false;

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
        slime.baseRadius = 1.0f;
        slime.strength = target->GetSize();

        Vector3 velocity = target->GetVelocity();

        // ボスの強さ ＝ 現在HP。プレイヤーの塊サイズのほうが大きくなったら
        // EnemyDefeated が返ってきて、そこで初めて体当たりで倒せる
        auto hit = EnemyCollision::ResolvePlayerVsEnemy(slime, velocity, boss_->MakeHitBody(),
                                                        isLeader ? bounceSpeed_ : minionBounceSpeed_,
                                                        planeNormal);
        if (!hit.hit) continue;

        auto& params = target->GetSlimeParams();
        params.impulseStrength = (std::max)(params.impulseStrength, hit.impulse);

        switch (hit.outcome)
        {
        case EnemyCollision::HitOutcome::EnemyDefeated:
            // プレイヤーのほうが強くなった。体当たりでボスが死ぬ。
            // スコアの計算は呼び出し元（Update）がやる
            defeated = true;
            params.squashStretch = { 0.22f, -0.18f, 0.22f };
            break;

        case EnemyCollision::HitOutcome::PlayerBounced:
        case EnemyCollision::HitOutcome::Standoff:
            // ボスのほうが強い。弾き返される
            target->SetPosition(slime.position);
            if (!isLeader && target->GetState() != SlimeState::Thrown)
            {
                target->Launch(velocity);
                target->GetSlimeParams().squashStretch = { 0.30f, -0.24f, 0.30f };
            }
            else
            {
                target->SetVelocity(velocity);
                params.squashStretch = { 0.34f, -0.28f, 0.34f };
            }

            if (refs_.fx)
            {
                refs_.fx->EmitEnemyHitSplash(slime.position, params.baseColor);
            }

            // TODO(SE): ボスに弾かれたときの衝突音をここで鳴らす
            break;

        default:
            break;
        }

        if (defeated) break;
    }

    return defeated;
}

// ===================================================================
// 描画
// ===================================================================

void BossFight::Draw(const RenderContext& ctx)
{
    if (boss_ && !boss_->IsDead())
    {
        boss_->DrawShadow(ctx);
        boss_->Draw(ctx);
    }

    for (auto& b : bullets_)
    {
        if (b && b->IsAlive()) b->Draw(ctx);
    }
}

void BossFight::DrawHud(ID3D12GraphicsCommandList* commandList)
{
    hpBar_.Draw(commandList);
}

void BossFight::DrawImGui()
{
#ifdef USE_IMGUI
    // ImGui のフォントに日本語グリフが無いので、ラベルは全部 ASCII で書くこと
    if (ImGui::CollapsingHeader("Boss Fight"))
    {
        const char* phaseName = "?";
        switch (phase_)
        {
        case Phase::Idle:     phaseName = "Idle (waiting for trigger)"; break;
        case Phase::FocusIn:  phaseName = "FocusIn"; break;
        case Phase::Battle:   phaseName = "Battle"; break;
        case Phase::Dying:    phaseName = "Dying"; break;
        case Phase::Explode:  phaseName = "Explode"; break;
        case Phase::Finished: phaseName = "Finished"; break;
        }

        ImGui::Text("Phase : %s  (%.2f s)", phaseName, phaseTimer_);
        ImGui::Text("Enabled: %s / Boss: %s", enabled_ ? "yes" : "no", boss_ ? "alive" : "none");
        if (boss_)
        {
            ImGui::Text("HP    : %d / %d  (%.0f%%)", boss_->GetHp(), boss_->GetMaxHp(),
                        boss_->GetHpRatio() * 100.0f);
            ImGui::Text("Aura  : x%.2f", boss_->GetAuraIntensity());
        }
        ImGui::Text("Camera blend: %.2f / Bullets: %d", cameraBlend_,
                    static_cast<int>(bullets_.size()));

        ImGui::SeparatorText("Debug");
        ImGui::Checkbox("Force trigger", &debugForceTrigger_);
        if (ImGui::Button("Start fight now"))
        {
            if (boss_ && phase_ == Phase::Idle)
            {
                EnterPhase(Phase::FocusIn);
                hpBar_.SetRatio(boss_->GetHpRatio());
                hpBar_.Show();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Kill boss"))
        {
            BeginDeath();
        }
        ImGui::SameLine();
        if (ImGui::Button("Restart"))
        {
            Restart();
        }

        if (boss_)
        {
            int hp = boss_->GetHp();
            if (ImGui::SliderInt("HP##boss", &hp, 0, boss_->GetMaxHp()))
            {
                boss_->SetHp(hp);
            }
        }

        ImGui::SeparatorText("Intro focus");
        ImGui::DragFloat("Focus Seconds", &focusInSeconds_, 0.05f, 0.5f, 12.0f);
        ImGui::DragFloat("Move-in Ratio", &focusInRatio_, 0.01f, 0.05f, 0.9f);
        ImGui::DragFloat("Hold Ratio", &focusHoldRatio_, 0.01f, 0.0f, 0.9f);
        ImGui::DragFloat("Distance (m)", &focusDistance_, 0.1f, 2.0f, 40.0f);
        ImGui::DragFloat("Height (m)", &focusHeight_, 0.1f, -5.0f, 20.0f);
        ImGui::DragFloat("Look Height Ratio", &focusLookHeightRatio_, 0.01f, 0.0f, 2.0f);

        ImGui::SeparatorText("Death");
        ImGui::DragFloat("Death Seconds", &deathSeconds_, 0.05f, 0.5f, 12.0f);
        ImGui::DragFloat("Blend-in Ratio", &deathBlendInRatio_, 0.01f, 0.02f, 0.9f);
        ImGui::DragFloat("Shake Max", &deathShakeMax_, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("Aura Max", &deathAuraMax_, 0.05f, 1.0f, 16.0f);
        ImGui::DragFloat("Body Shake Max", &deathBodyShakeMax_, 0.005f, 0.0f, 0.6f);
        ImGui::DragFloat("Explode Seconds", &explodeSeconds_, 0.05f, 0.2f, 10.0f);
        ImGui::DragFloat("Explode Shake", &explodeShake_, 0.01f, 0.0f, 2.0f);

        ImGui::SeparatorText("Battle");
        ImGui::Checkbox("Enable collision", &enableCollision_);
        ImGui::DragFloat("Bullet Knockback", &bulletKnockback_, 0.1f, 0.0f, 40.0f);
        ImGui::DragFloat("Bounce (leader)", &bounceSpeed_, 0.1f, 0.0f, 40.0f);
        ImGui::DragFloat("Bounce (small)", &minionBounceSpeed_, 0.1f, 0.0f, 40.0f);

        ImGui::SeparatorText("Boss config");
        BossConfig& c = GetBossConfig();
        ImGui::DragInt("Max HP", &c.maxHp, 1.0f, 1, 9999);
        ImGui::DragInt("Self Destruct Damage", &c.selfDestructDamage, 0.2f, 1, 100);
        ImGui::DragFloat("Model Scale", &c.modelScale, 0.01f, 0.1f, 8.0f);
        ImGui::DragFloat("Hit Radius Ratio", &c.hitRadiusRatio, 0.02f, 0.1f, 6.0f);
        ImGui::DragFloat("Hit Offset Ratio", &c.hitOffsetRatio, 0.02f, 0.0f, 6.0f);
        ImGui::DragInt("Bullet Ways", &c.bulletWays, 0.2f, 3, 64);
        ImGui::DragFloat("Shoot Interval", &c.shootInterval, 0.02f, 0.2f, 10.0f);
        ImGui::DragFloat("Bullet Speed", &c.bulletSpeed, 0.1f, 1.0f, 40.0f);
        ImGui::DragFloat("Volley Spin (rad)", &c.spinPerVolley, 0.01f, -1.0f, 1.0f);
        if (ImGui::Button("Apply model settings to boss") && boss_)
        {
            boss_->RefreshFromSpec();
        }

        ImGui::SeparatorText("Placement");
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", placedPosition_.x, placedPosition_.y, placedPosition_.z);
        ImGui::Text("Placed HP: %d", placedHp_);
    }

    hpBar_.DrawImGui();
#endif
}
