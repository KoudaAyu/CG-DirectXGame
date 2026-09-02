#pragma once

#include "Vector.h"
#include "Matrix4x4.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Baziru3_Engine/Framework/Collision/SphereCollider.h"
#include "Application/Player/PikminPlayer.h" // SlimeParamsCPU
#include <memory>

class Object3dCom;
class Camera;

enum class MinionState {
    Rolling,    // ステージ傾斜による自由転がり中
    Merging,    // プレイヤー（巨大スライム）へ合体・吸引中
    Thrown,     // プレイヤーから投げられて放物線飛行中
    Carrying,   // オブジェクト運搬中
    Idle        // その場で待機中
};

enum class MinionType {
    Red,    // 赤（攻撃力高 / 火炎耐性）
    Yellow, // 黄（高く飛ぶ / 電気耐性）
    Blue    // 青（水中OK）
};

/**
 * @brief 個々のミニオン（小スライム）クラス
 */
class Minion {
public:
    Minion();
    ~Minion();

    void Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& spawnPos, MinionType type = MinionType::Red);
    void Update(float deltaTime, const Vector2& stageTilt = { 0.0f, 0.0f });
    void Draw(const RenderContext& ctx);

    // --- ステート制御 ---
    void SetState(MinionState state) { state_ = state; }
    MinionState GetState() const { return state_; }

    const Vector3& GetPosition() const { return position_; }
    void SetPosition(const Vector3& pos);

    void Launch(const Vector3& velocity);
    void AttractTo(const Vector3& attractCenter, float attractSpeed = 25.0f);

    bool IsActive() const { return isActive_; }
    void SetActive(bool active);

    MinionType GetType() const { return type_; }
    float GetRadius() const { return radius_; }

    // 反発ベクトル加算（重なり防止用）
    void AddRepulsion(const Vector3& pushVector) { position_ += pushVector; }

    // スライムパラメータの公開（共有調整用）
    SlimeParamsCPU& GetSlimeParams() { return slimeParams_; }

    // コライダーの取得
    SphereCollider* GetCollider() const { return collider_.get(); }

    // 衝突時の弾性リアクション
    void OnCollision(const CollisionInfo& info);

private:
    void DrawSlime(const RenderContext& ctx);

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<Object3d> object3d_;
    Object3d::ModelData modelData_;
    uint32_t textureIndex_ = 0;

    Vector3 position_{ 0.0f, 0.0f, 0.0f };
    Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
    Vector3 rotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 scale_{ 0.4f, 0.4f, 0.4f };

    MinionState state_ = MinionState::Rolling;
    MinionType type_ = MinionType::Red;

    bool isActive_ = true;
    float radius_ = 0.3f;
    float tiltAccel_ = 35.0f;
    float friction_ = 2.8f;
    float gravity_ = -24.0f;
    float groundY_ = 0.24f;

    // スライム固有
    SlimeParamsCPU slimeParams_;
    float totalTime_ = 0.0f;
    float bounceTimer_ = 0.0f;
    Vector3 prevVelocity_{ 0.0f, 0.0f, 0.0f };

    // 当たり判定
    std::unique_ptr<SphereCollider> collider_;
};
