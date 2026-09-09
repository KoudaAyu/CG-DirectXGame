#pragma once

#include "Vector.h"
#include "Matrix4x4.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Baziru3_Engine/Framework/Collision/MeshCollider.h"
#include "Application/GameObject/SlimePhysics.h"
#include "Application/GameObject/SlimeMesh.h"
#include <vector>
#include <memory>

class Object3dCom;
class Camera;

enum class SlimeState {
    Rolling,    // ステージ傾斜による自由転がり中
    Merging,    // 合体・吸引中
    Thrown,     // 空中放物線飛行中
    Idle        // 待機中
};

/**
 * @brief 統合スライムクラス（旧PikminPlayerとMinionを統合）
 */
class Slime {
public:
    Slime();
    ~Slime();

    void Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& startPos, int initialSize = 1);
    void Update(float deltaTime, const Vector2& stageTilt = { 0.0f, 0.0f }, const Vector2& pivot = { 0.0f, 0.0f });
    void Draw(const RenderContext& ctx);
    void DrawShadow(const RenderContext& ctx);
    void DrawXRay(const RenderContext& ctx, ID3D12PipelineState* xRayPSO);
    void DrawDebug(Camera* camera);

    // 物理アクション
    void Jump(float jumpVelocity = 13.0f);
    void BounceFromStage(const Vector3& groundNormal, float bouncePower = 13.5f);
    void Launch(const Vector3& velocity);
    void AttractTo(const Vector3& targetPos, float speed = 25.0f);

    // 衝突
    void OnCollision(const CollisionInfo& info);

    // 座標・速度・回転・スケール
    const Vector3& GetPosition() const { return position_; }
    void SetPosition(const Vector3& pos);

    const Vector3& GetVelocity() const { return velocity_; }
    void SetVelocity(const Vector3& vel) { velocity_ = vel; }

    const Vector3& GetRotation() const { return rotation_; }
    void SetRotation(const Vector3& rot) { rotation_ = rot; }

    const Vector3& GetScale() const { return scale_; }
    float GetRadius() const { return radius_; }
    float GetGroundY() const { return groundY_; }
    float GetEffectiveOffset() const { return groundY_ * (1.0f + ceilingSquash_); }

    // 大きさ（ロコロコサイズ: 1〜10）
    int GetSize() const { return size_; }
    void SetSize(int s);

    // アクティブ状態
    bool IsActive() const { return isActive_; }
    void SetActive(bool active);

    // 接地状態
    bool IsGrounded() const { return isGrounded_; }

    // スポーン初期位置
    const Vector3& GetSpawnPosition() const { return spawnPos_; }
    void SetSpawnPosition(const Vector3& pos) { spawnPos_ = pos; }

    // ステート制御
    SlimeState GetState() const { return state_; }
    void SetState(SlimeState state) { state_ = state; }

    // スライム変形パラメータ
    SlimeParamsCPU& GetSlimeParams() { return slimeParams_; }
    const SlimeParamsCPU& GetSlimeParams() const { return slimeParams_; }

    // 合体制御
    bool CanMerge() const { return isActive_ && mergeCooldown_ <= 0.0f; }
    void SetMergeCooldown(float cd) { mergeCooldown_ = cd; }
    float GetMergeCooldown() const { return mergeCooldown_; }

    // 狭い隙間・天井変形制御
    float GetCeilingSquash() const { return ceilingSquash_; }
    void SetCeilingSquash(float s) { ceilingSquash_ = s; }

    // タイトル画面専用の「大きい青い例外」
    void SetTitleException(bool isTitle);
    bool IsTitleException() const { return isTitleException_; }

    // 互換性ヘルパー
    bool IsMerged() const { return size_ > 1; }
    void ToggleMerge() {}
    float GetCurrentScale() const { return scale_.x; }
    float GetTiltAccel() const { return tiltAccel_; }
    void SetTiltAccel(float a) { tiltAccel_ = a; }

    /**
     * @brief 転がりの速さに掛かる倍率（1.0 が既定）
     * @note ステージ傾斜による加速度と、斜面すべりの両方に掛かる。
     *       終端速度は「加速度 / 摩擦」で決まるので、
     *       0.67 を入れるとそのまま **移動速度が 0.67 倍** になる。
     *       空中の放物線（Launch / Thrown）には掛からないので、
     *       分裂で弾け飛ぶ勢いは変わらない。
     *
     *       セットしているのは SlimeManager::Update()。
     *       群れの代表（＝プレイヤー本体）だけ 1.0、それ以外を遅くしている
     */
    float GetSpeedScale() const { return speedScale_; }
    void SetSpeedScale(float s) { speedScale_ = (s < 0.0f) ? 0.0f : s; }
    float GetFriction() const { return SlimePhysics::GetFriction(); }
    void SetFriction(float f) { SlimePhysics::SetFriction(f); }

    // コライダー取得
    MeshCollider* GetCollider() const { return meshCollider_.get(); }
    Object3d* GetModel() const { return object3d_.get(); }

private:
    void UpdatePhysics(float deltaTime, const Vector2& stageTilt, const Vector2& pivot);
    void DrawSlime(const RenderContext& ctx);
    float CalculateScaleBySize(int s) const;

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<Object3d> object3d_;
    Object3d::ModelData modelData_;
    uint32_t textureIndex_ = 0;

    Vector3 position_{ 0.0f, 0.0f, 0.0f };
    Vector3 spawnPos_{ 0.0f, 0.4f, 0.0f };
    Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
    Vector3 rotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 scale_{ 0.4f, 0.4f, 0.4f };

    SlimeState state_ = SlimeState::Rolling;
    int size_ = 1;

    bool isActive_ = true;
    bool isGrounded_ = false;
    float radius_ = 0.3f;
    float groundY_ = 0.30f;
    float tiltAccel_ = 35.0f;
    float speedScale_ = 1.0f;   //!< 転がりの速さの倍率。SlimeManager が毎フレーム入れる
    float currentMergedScale_ = 0.4f;

    float mergeCooldown_ = 0.0f;
    float obstacleCooldown_ = 0.0f;
    float bounceTimer_ = 0.0f;
    float totalTime_ = 0.0f;

    SlimeParamsCPU slimeParams_;

    std::unique_ptr<MeshCollider> meshCollider_;

    Vector3 prevVelocity_{ 0.0f, 0.0f, 0.0f }; // スクワッシュ変形用の前フレーム速度
    bool isTitleException_ = false;
    float ceilingSquash_ = 0.0f; // 狭い隙間・天井による平べった変形率

    std::unique_ptr<CharacterShadow> shadow_;
    bool shadowDrawnThisFrame_ = false;
};
