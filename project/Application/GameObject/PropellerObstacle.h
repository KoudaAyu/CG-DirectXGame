#pragma once
#include <memory>
#include <vector>
#include <string>
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Baziru3_Engine/Core/Camera/Camera.h"
#include "Baziru3_Engine/Core/Base/RenderContext.h"
#include "Baziru3_Engine/Core/Base/Vector.h"
#include "Baziru3_Engine/Framework/Collision/BoxCollider.h"
#include "Baziru3_Engine/Framework/Collision/SphereCollider.h"

class Object3dCom;

/**
 * @brief 自転するプロペラ障害物ギミック (PropellerObstacle)
 * モデルの頂点群から羽根の枚数（2枚/3枚/4枚...）とサイズを自動検出し、
 * ピッタリの OBB (BoxCollider) と中心ハブ (SphereCollider) を自動生成します。
 */
class PropellerObstacle
{
public:
    struct Wing
    {
        std::unique_ptr<BoxCollider> collider;
        float baseAngle = 0.0f;                     // 羽根のローカル基準角 (rad: atan2(-z, x))
        Vector3 rotationEuler{ 0.0f, 0.0f, 0.0f };  // コライダー追従用オイラー角
        float length = 3.0f;                        // 羽根の長さ (ワールド寸法)
        float thickness = 0.28f;                    // 羽根の厚み (ワールド寸法)
        float width = 0.28f;                        // 羽根の幅 (ワールド寸法)
        float centerDistU = 1.76f;                  // ハブ中心から羽根OBB中心までの動径距離 (ワールド寸法)
        float centerY = 0.63f;                      // 羽根OBBの高さ中心 (ワールド寸法)
    };

    PropellerObstacle() = default;
    ~PropellerObstacle();

    /**
     * @brief 初期化（モデルの頂点からコライダーを自動解析・生成）
     * @param object3dCom 描画コンポーネント
     * @param camera カメラ
     * @param basePosition 配置位置 (ステージ上の基本XZ座標)
     * @param scale スケール (デフォルト {1.5f, 1.5f, 1.5f})
     * @param spinSpeed 自転速度 (rad/s, 正で時計回り/負で反時計回り)
     * @param modelDirectory モデルのディレクトリパス
     * @param modelFilename モデルのファイル名
     */
    void Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& basePosition,
                    const Vector3& scale = { 1.5f, 1.5f, 1.5f }, float spinSpeed = 2.5f,
                    const std::string& modelDirectory = "Resources/10days",
                    const std::string& modelFilename = "propeller.obj");

    /**
     * @brief 毎フレーム更新処理
     * @param deltaTime フレーム時間
     * @param stageTilt ステージの傾斜角 (X: ピッチ, Y: ロール)
     */
    void Update(float deltaTime, const Vector2& stageTilt);

    /**
     * @brief 描画処理
     * @param ctx レンダーコンテキスト
     */
    void Draw(const RenderContext& ctx);

    /**
     * @brief 終了・解放処理（コライダーの登録解除）
     */
    void Finalize();

    // ゲッター・セッター
    const Vector3& GetPosition() const { return currentWorldPos_; }
    const Vector3& GetScale() const { return scale_; }
    float GetSpinSpeed() const { return spinSpeed_; }
    void SetSpinSpeed(float speed) { spinSpeed_ = speed; }
    void SetColor(const Vector4& color);

    int GetDetectedWingCount() const { return detectedWingCount_; }
    float GetDetectedRadius() const { return detectedRadius_; }
    float GetDetectedWingLength() const { return detectedWingLen_; }
    float GetDetectedWingThickness() const { return detectedWingThick_; }
    float GetDetectedWingWidth() const { return detectedWingWidth_; }
    float GetDetectedWingCenterY() const { return detectedWingCenterY_; }
    float GetDetectedHubRadius() const { return detectedHubRadius_; }

    const std::vector<std::unique_ptr<Wing>>& GetWings() const { return wings_; }

private:
    /**
     * @brief モデル頂点群から羽根の枚数・寸法を自動解析してコライダーを構築
     */
    void AutoDetectAndBuildColliders();

private:
    std::unique_ptr<Object3d> object3d_;
    Object3dCom* object3dCom_ = nullptr;

    Vector3 basePosition_{ 0.0f, 0.0f, 0.0f };      // ステージ上の基本配置座標
    Vector3 currentWorldPos_{ 0.0f, 0.0f, 0.0f };   // 傾斜適用後のワールド座標
    Vector3 scale_{ 1.5f, 1.5f, 1.5f };
    float currentAngle_ = 0.0f;                      // 現在の自転角度 (rad)
    float spinSpeed_ = 2.5f;                         // 自転速度 (rad/s)

    // 自動検出されたプロペラ精密寸法 (モデル空間寸法)
    float detectedRadius_ = 1.0f;
    float detectedHubRadius_ = 0.24f;
    float detectedHubCenterY_ = 0.26f;
    float detectedWingLen_ = 3.0f;
    float detectedWingThick_ = 0.19f;
    float detectedWingWidth_ = 0.19f;
    float detectedWingCenterY_ = 0.42f;
    int detectedWingCount_ = 4;

    // 動的に自動生成される羽根コライダー群 (ヒープ固定でポインタ参照の安全性を保証)
    std::vector<std::unique_ptr<Wing>> wings_;

    bool isInitialized_ = false;
};
