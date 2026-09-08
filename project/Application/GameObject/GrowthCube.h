#pragma once

#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Baziru3_Engine/Core/Camera/Camera.h"
#include "Baziru3_Engine/Core/Base/RenderContext.h"
#include "Baziru3_Engine/Core/Base/Vector.h"
#include <memory>

class Object3dCom;
class SlimeManager;
class Slime;

/**
 * @brief プリミティブ頂点から生成される成長キューブアイテム
 * 接触したスライムを+1サイズ巨大化させ、キューブ自身も拡大・吸い込まれて消滅します。
 */
class GrowthCube
{
public:
    enum class State
    {
        Active,      // 浮遊・自転中（取得可能）
        Collecting,  // 取得演出中（拡大・吸い込み）
        Inactive     // リスポーン待機中
    };

public:
    GrowthCube() = default;
    ~GrowthCube() = default;

    /**
     * @brief プリミティブ立方体メッシュ（24頂点・36インデックス）をプロシージャルに生成
     * @param size 立方体の一辺の長さ
     */
    static Object3d::ModelData GeneratePrimitiveCube(float size = 1.0f);

    /**
     * @brief 初期化
     * @param object3dCom 描画コンポーネント
     * @param camera カメラ
     * @param basePos ステージ上の配置基準座標
     * @param size キューブサイズ（一辺）
     */
    void Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& basePos, float size = 0.8f);

    /**
     * @brief 毎フレーム更新（浮遊、自転、ステージ傾斜追従、スライム衝突判定、取得演出）
     */
    void Update(float deltaTime, const Vector2& stageTilt, const Vector2& pivot, SlimeManager* slimeManager);

    /**
     * @brief 描画
     */
    void Draw(const RenderContext& ctx);

    /**
     * @brief アイテムを再出現させる
     */
    void Respawn();

    State GetState() const { return state_; }
    bool IsActive() const { return state_ == State::Active; }

    const Vector3& GetBasePosition() const { return basePosition_; }
    void SetBasePosition(const Vector3& pos) { basePosition_ = pos; }

    const Vector3& GetWorldPosition() const { return currentWorldPos_; }

    float GetBaseSize() const { return baseSize_; }
    void SetBaseSize(float size);

    bool IsAutoRespawn() const { return autoRespawn_; }
    void SetAutoRespawn(bool enable) { autoRespawn_ = enable; }

private:
    void CheckSlimeCollision(SlimeManager* slimeManager);

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<Object3d> object3d_;
    Object3d::ModelData modelData_;
    uint32_t textureIndex_ = 0;

    Vector3 basePosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 currentWorldPos_{ 0.0f, 0.0f, 0.0f };
    Vector3 currentScale_{ 1.0f, 1.0f, 1.0f };
    Vector3 currentRotation_{ 0.0f, 0.0f, 0.0f };

    float baseSize_ = 0.8f;
    float currentAngle_ = 0.0f;
    float hoverTimer_ = 0.0f;

    State state_ = State::Active;
    float collectTimer_ = 0.0f;
    float collectDuration_ = 0.45f;
    Vector3 collectStartPos_{ 0.0f, 0.0f, 0.0f };
    Slime* collectedBySlime_ = nullptr;

    bool autoRespawn_ = true;
    float respawnTimer_ = 0.0f;
    float respawnCooldown_ = 10.0f;
};
