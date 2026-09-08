#pragma once
#include <memory>
#include <vector>
#include <string>
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Baziru3_Engine/Core/Camera/Camera.h"
#include "Baziru3_Engine/Core/Base/RenderContext.h"
#include "Baziru3_Engine/Core/Base/Vector.h"
#include "Baziru3_Engine/Framework/Collision/MeshCollider.h"

class Object3dCom;

/**
 * @brief 自転するプロペラ障害物ギミック (PropellerObstacle)
 * エンジン標準の MeshCollider を活用し、プロペラ 3D モデルのポリゴンそのものによる
 * 完全精密なメッシュ衝突判定・押し出し・自転弾き飛ばしを実現します。
 */
class PropellerObstacle
{
public:
    PropellerObstacle() = default;
    ~PropellerObstacle();

    /**
     * @brief 初期化（モデルの読み込みと MeshCollider の生成・登録）
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
     * @param pivot 傾斜の回転中心 (自機水平座標)
     */
    void Update(float deltaTime, const Vector2& stageTilt, const Vector2& pivot = { 0.0f, 0.0f });

    /**
     * @brief 描画処理
     * @param ctx レンダーコンテキスト
     */
    void Draw(const RenderContext& ctx);

    /**
     * @brief 終了・解放処理（コライダーの登録解除）
     */
    void Finalize();

    /**
     * @brief スライムとの精密メッシュ衝突判定および押し出し・自転反発解決
     * @param slimePos スライムのワールド座標（更新される）
     * @param slimeVel スライムの速度（更新される）
     * @param slimeRadius スライムの有効半径
     * @param isMerged 巨大化合体状態かどうか
     * @param slimeSquash スライムの変形パラメータ (出力)
     * @param outImpulse 衝撃強度 (出力)
     * @return 衝突が発生した場合 true
     */
    bool ResolveSlimeCollision(Vector3& slimePos, Vector3& slimeVel, float slimeRadius,
                               bool isMerged, Vector3& slimeSquash, float& outImpulse);

    // ゲッター・セッター
    const Vector3& GetPosition() const { return currentWorldPos_; }
    const Vector3& GetScale() const { return scale_; }
    float GetSpinSpeed() const { return spinSpeed_; }
    void SetSpinSpeed(float speed) { spinSpeed_ = speed; }
    void SetColor(const Vector4& color);

    MeshCollider* GetMeshCollider() const { return meshCollider_.get(); }

    int GetDetectedWingCount() const { return detectedWingCount_; }
    float GetDetectedRadius() const { return detectedRadius_; }
    float GetDetectedWingLength() const { return detectedWingLen_; }
    float GetDetectedWingThickness() const { return detectedWingThick_; }
    float GetDetectedWingWidth() const { return detectedWingWidth_; }
    float GetDetectedWingCenterY() const { return detectedWingCenterY_; }
    float GetDetectedHubRadius() const { return detectedHubRadius_; }

private:
    /**
     * @brief モデル頂点群から寸法情報を解析し MeshCollider を構築
     */
    void BuildMeshCollider();

private:
    std::unique_ptr<Object3d> object3d_;
    std::unique_ptr<MeshCollider> meshCollider_;
    Object3dCom* object3dCom_ = nullptr;

    Vector3 basePosition_{ 0.0f, 0.0f, 0.0f };      // ステージ上の基本配置座標
    Vector3 currentWorldPos_{ 0.0f, 0.0f, 0.0f };   // 傾斜適用後のワールド座標
    Vector3 scale_{ 1.5f, 1.5f, 1.5f };
    float currentAngle_ = 0.0f;                      // 現在の自転角度 (rad)
    float spinSpeed_ = 2.5f;                         // 自転速度 (rad/s)
    Matrix4x4 currentWorldMatrix_{};                 // 現在のワールド変換行列
    Matrix4x4 currentInvWorldMatrix_{};              // ワールド変換の逆行列

    // 自動検出されたプロペラ寸法 (情報表示用)
    float detectedRadius_ = 1.0f;
    float detectedHubRadius_ = 0.24f;
    float detectedHubCenterY_ = 0.26f;
    float detectedWingLen_ = 3.0f;
    float detectedWingThick_ = 0.19f;
    float detectedWingWidth_ = 0.19f;
    float detectedWingCenterY_ = 0.42f;
    int detectedWingCount_ = 4;

    bool isInitialized_ = false;
};

