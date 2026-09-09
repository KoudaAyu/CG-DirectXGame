#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Baziru3_Engine/Core/Base/Vector.h"
#include "Baziru3_Engine/Core/Base/RenderContext.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Baziru3_Engine/Framework/Collision/MeshCollider.h"
#include "Application/Editor/StageLayout.h"

class Object3dCom;
class Camera;

/**
 * @brief ステージの地形メッシュ群
 *
 * 以前は GamePlayScene::InitializeScene() が
 * 「startLand / Land1 / toLandRoad / roadCell×6」をハードコードで読み込んでいた。
 * 配置エディタから地形を編集できるようにするため、そこを丸ごとこのクラスへ移した。
 *
 * やること:
 *   - Resources/10days 配下の obj を列挙（カタログ）
 *   - StageTerrainEntry の配列を受け取って、その通りにパーツを生成・登録
 *   - 毎フレームのステージ傾斜（ピボット回転）の適用
 *   - パーツの追加・削除・移動・Y回転・スケール変更（配置エディタ用）
 *   - 「いまどのパーツの上に立っているか」の判定（ボス戦トリガー用）
 *
 * @note パーツ1枚につき Object3d と MeshCollider を1つずつ持ち、
 *       CollisionManager と SlimePhysics の両方へ登録する。
 *       SlimePhysics 側は登録順のインデックスで参照されるので、
 *       追加・削除のたびに全部を登録し直している（RefreshRegistration）。
 *
 * @note 位置は XZ だけをエディタで動かす仕様。回転は Y 軸のみ、スケールは一様。
 *       地形メッシュ同士の衝突は考慮しない（重ねて置ける）。
 */
class StageTerrain
{
public:
    /// @brief 地形パーツ1枚
    struct Part
    {
        // --- 配置データ（JSON と1対1）---
        std::string mesh;
        std::string texture;
        Vector3 position{ 0.0f, 0.0f, 0.0f }; //!< 傾き0のときのワールド座標
        float rotationY = 0.0f;
        float scale = 0.25f;
        bool bossTrigger = false;

        // --- 実体 ---
        std::unique_ptr<Object3d> object;
        std::unique_ptr<MeshCollider> collider;
        Object3d::ModelData modelData;
        uint32_t textureIndex = 0;

        /// @brief SlimePhysics での登録番号（GroundLayer::meshIndex と突き合わせる）
        int groundMeshIndex = -1;

        /// @brief モデルローカルの AABB（読み込み時に1回だけ算出）
        Vector3 localMin{ 0.0f, 0.0f, 0.0f };
        Vector3 localMax{ 0.0f, 0.0f, 0.0f };

        /// @brief 傾き0のときのワールド AABB（位置・回転・スケールを反映）
        Vector3 worldMin{ 0.0f, 0.0f, 0.0f };
        Vector3 worldMax{ 0.0f, 0.0f, 0.0f };

        /// @brief ワールド AABB の XZ 中心。エディタでの掴みどころ（ハンドル）に使う
        Vector3 HandlePosition() const
        {
            return { (worldMin.x + worldMax.x) * 0.5f,
                     (worldMin.y + worldMax.y) * 0.5f,
                     (worldMin.z + worldMax.z) * 0.5f };
        }

        /// @brief ハンドルの当たり半径（XZ の大きさから決める）
        float HandleRadius() const;
    };

public:
    StageTerrain() = default;
    ~StageTerrain();

    StageTerrain(const StageTerrain&) = delete;
    StageTerrain& operator=(const StageTerrain&) = delete;

    void Initialize(Object3dCom* object3dCom, Camera* camera);
    void Finalize();

    /// @brief Resources/10days 配下にある obj のファイル名一覧（propeller は障害物なので除く）
    /// @note 初回呼び出し時にディレクトリを走査してキャッシュする
    static const std::vector<std::string>& GetMeshCatalog();

    /// @brief カタログを取り直す（フォルダに obj を足したあと用）
    static void RefreshMeshCatalog();

    // --- 配置データの適用・吸い出し ---

    /// @brief 配置データの通りにパーツを作り直す
    void ApplyLayout(const std::vector<StageTerrainEntry>& entries);

    /// @brief 現在のパーツを配置データへ書き出す
    void WriteLayout(std::vector<StageTerrainEntry>& out) const;

    // --- 編集 ---

    /**
     * @brief パーツを1枚足す（position は **モデル原点** の置き場所）
     * @param mesh Resources/10days 配下の obj ファイル名
     * @param position モデル原点をどこに置くか。JSON の値と1対1
     * @return 追加されたパーツ。読み込みに失敗したら nullptr
     *
     * @warning **この obj 群はローカル原点がメッシュから大きく外れている。**
     *          例: Land1.obj のローカル中心は (-613.9, 7.2, 280.2)。
     *          スケール 0.25 だと、原点を (0,0) に置いた瞬間に
     *          島の実体は (-153.5, +70.0) に現れる（168m 先）。
     *          「クリックしたところに置く」用途では
     *          必ず AddPartCenteredAt() のほうを使うこと
     */
    Part* AddPart(const std::string& mesh, const Vector3& position,
                  float rotationY = 0.0f, float scale = 0.25f,
                  bool bossTrigger = false, const std::string& texture = "");

    /**
     * @brief パーツを1枚足す（**見た目の中心**が centerXZ に来るように置く）
     * @param centerXZ ワールド AABB の XZ 中心を持ってきたい場所（y は無視）
     * @return 追加されたパーツ。読み込みに失敗したら nullptr
     *
     * 配置エディタのクリック配置はこちらを使う。
     * ローカル原点のずれを吸収するので、「クリックしたところに島が出る」
     */
    Part* AddPartCenteredAt(const std::string& mesh, const Vector3& centerXZ,
                            float rotationY = 0.0f, float scale = 0.25f,
                            bool bossTrigger = false, const std::string& texture = "");

    /// @brief 見た目の中心（ワールド AABB の XZ 中心）が centerXZ に来るよう置き直す
    void SetPartCenterXZ(Part* part, float centerX, float centerZ);

    void RemovePart(Part* part);
    void ClearParts();

    void SetPartPositionXZ(Part* part, float x, float z);
    void SetPartRotationY(Part* part, float radian);
    void SetPartScale(Part* part, float scale);
    void SetPartBossTrigger(Part* part, bool on);

    // --- 毎フレーム ---

    /**
     * @brief ステージ傾斜（ピボット回転）と揺らしを全パーツへ反映する
     * @param stageTilt ステージ傾斜 (x: pitch, y: roll)
     * @param pivot 回転中心の XZ（＝スライム重心）
     * @param bounceOffsetY ステージ揺らしの垂直オフセット
     * @param shakeTilt ステージ揺らしの回転成分
     *
     * @note 各パーツの最終的な変換は
     *         v * scale * Ry(rotationY) * R_tilt + (position - pivot) * R_tilt + pivot
     *       になる。R_tilt は GamePlayScene が使っていたのと同じ Rx(pitch) * Rz(-roll)。
     *       Object3d はオイラー角しか持てないので、合成回転行列から角度を逆算している
     */
    void UpdateTransforms(const Vector2& stageTilt, const Vector2& pivot,
                          float bounceOffsetY, const Vector2& shakeTilt);

    /// @brief カメラの最新 ViewProjection に合わせて WVP 定数バッファだけ更新する
    void SyncConstantBuffers();

    // --- 参照 ---

    const std::vector<std::unique_ptr<Part>>& GetParts() const { return parts_; }
    int GetPartCount() const { return static_cast<int>(parts_.size()); }
    Part* GetPart(int index);
    int IndexOf(const Part* part) const;

    /**
     * @brief その XZ の「一番上の床」がどのパーツのものかを返す
     * @return 該当パーツ。床が無ければ nullptr
     * @note ステージ傾斜が 0 でなくても動くが、SlimePhysics の
     *       QueryGroundLayers は傾斜を考慮しないので、
     *       呼ぶ側でステージローカル座標に直しておくこと
     */
    Part* FindPartUnder(float x, float z);

    /// @brief その XZ の真下にボス戦トリガー付きのパーツがあるか
    bool IsBossTriggerAt(float x, float z);

    /// @brief ボス戦トリガーが1枚でも設定されているか
    bool HasBossTrigger() const;

    /// @brief 地形の基本色（草原色）を全パーツへ設定する
    void SetBaseColor(const Vector4& color);
    const Vector4& GetBaseColor() const { return baseColor_; }

private:
    /// @brief パーツを1枚読み込んで実体を作る（登録はしない）
    std::unique_ptr<Part> CreatePart(const std::string& mesh, const Vector3& position,
                                     float rotationY, float scale, bool bossTrigger,
                                     const std::string& texture) const;

    /// @brief パーツの傾き0でのワールド AABB を計算し直す
    static void RecalculateBounds(Part& part);

    /// @brief CollisionManager / SlimePhysics への登録を全部やり直す
    /// @note SlimePhysics のメッシュ番号は登録順なので、パーツを増減したら必ず呼ぶ
    void RefreshRegistration();

    void UnregisterAll();

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;

    std::vector<std::unique_ptr<Part>> parts_;
    Vector4 baseColor_{ 0.55f, 0.85f, 0.50f, 1.0f };
    bool registered_ = false;
};
