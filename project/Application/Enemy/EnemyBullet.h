#pragma once

#include <memory>
#include <string>

#include "Baziru3_Engine/Core/Base/Vector.h"
#include "Baziru3_Engine/Core/Base/RenderContext.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"

class Object3dCom;
class Camera;

/**
 * @brief 敵が撃つ弾
 *
 * Application/GameObject/Bullet は sphere.obj 決め打ちなので、
 * 花のモデル付き弾（Lotus_Bullet / Sunward_Bullet）用に別で用意した。
 *
 * EnemyManager がプールして使い回す前提:
 *   Setup() でモデルを1回だけ持たせる -> Fire() で撃つたびに初期化
 */
class EnemyBullet
{
public:
    EnemyBullet() = default;
    ~EnemyBullet() = default;

    /**
     * @brief モデルを持たせる（1個につき1回だけ呼ぶ）
     * @param object3dCom 3D描画コンポーネント
     * @param camera カメラ
     * @param modelData 読み込み済みモデルデータ（EnemyManager がキャッシュしたもの）
     * @param modelKey どのモデルで作られたかの識別子。使い回し時の照合に使う
     * @param color 弾の色
     */
    void Setup(Object3dCom* object3dCom, Camera* camera,
               const Object3d::ModelData& modelData, const std::string& modelKey,
               const Vector4& color);

    /**
     * @brief 発射（プールから取り出して再利用するときもこれを呼ぶ）
     * @param startPos 発射位置
     * @param direction 進行方向（正規化されていなくてよい）
     * @param speed 速度 (m/s)
     * @param lifeTime 寿命 (秒)
     * @param scale 見た目スケール
     * @param hitRadius 当たり判定半径
     */
    void Fire(const Vector3& startPos, const Vector3& direction,
              float speed, float lifeTime, float scale, float hitRadius);

    void Update(float deltaTime);
    void Draw(const RenderContext& ctx);

    bool IsAlive() const { return isAlive_; }
    void Kill() { isAlive_ = false; }

    const Vector3& GetPosition() const { return position_; }
    float GetHitRadius() const { return hitRadius_; }
    const std::string& GetModelKey() const { return modelKey_; }
    bool IsReady() const { return object3d_ != nullptr; }

private:
    std::unique_ptr<Object3d> object3d_;
    std::string modelKey_;

    Vector3 position_{ 0.0f, 0.0f, 0.0f };
    Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
    Vector3 rotation_{ 0.0f, 0.0f, 0.0f };

    float scale_ = 0.3f;
    float hitRadius_ = 0.25f;
    float lifeTime_ = 2.5f;
    float age_ = 0.0f;
    bool isAlive_ = false;
};
