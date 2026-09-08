#pragma once

#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "Application/Enemy/MobEnemy.h"
#include "Application/Enemy/EnemyBullet.h"

class Object3dCom;
class Camera;
class PikminPlayer;
class MinionManager;

/**
 * @brief モブ敵と敵弾をまとめて管理する
 *
 * やること:
 *   - スポーン（強さはランダム。範囲は種類ごとの設定から）
 *   - 更新・描画・撃破された個体の除去
 *   - 敵の発射要求を拾って弾を生成し、プールで使い回す
 *   - プレイヤーの塊との衝突解決（強弱で「倒す」か「跳ね飛ばす」かが変わる）
 *   - ImGui のデバッグパネル
 */
class EnemyManager
{
public:
    EnemyManager() = default;
    ~EnemyManager();

    void Initialize(Object3dCom* object3dCom, Camera* camera);
    void Finalize();

    /**
     * @brief 敵をスポーンする
     * @param type 種類
     * @param stageLocalPos 配置座標（ステージが水平なときのワールド座標）
     * @param strength 強さ。-1 なら種類ごとの範囲からランダム
     * @return 生成された敵（所有権はマネージャ側。失敗時 nullptr）
     */
    MobEnemy* Spawn(EnemyType type, const Vector3& stageLocalPos, int strength = -1);

    /// @brief デバッグ用。適当な配置で一通りの種類を出す
    void SpawnDebugSet(const Vector3& center);

    void ClearAll();

    /**
     * @brief 更新
     * @param deltaTime デルタタイム
     * @param stageTilt ステージ傾斜
     * @param player プレイヤー（衝突解決のため位置・速度が書き換わる）
     * @param minionManager 小スライム群（省略可。渡すと小スライムも強さ判定で敵と戦う）
     */
    void Update(float deltaTime, const Vector2& stageTilt, PikminPlayer* player,
                MinionManager* minionManager = nullptr);

    void Draw(const RenderContext& ctx);

    /// @brief ImGui デバッグパネル（USE_IMGUI 無効時は何もしない）
    void DrawImGui();

    // --- 強さ -> 見た目スケール ---
    /// @brief 変換関数を差し替える。既存の個体にも即座に反映される
    void SetScaleFromStrength(EnemyBase::ScaleFromStrengthFunc func);

    // --- パラメータ ---
    void SetBounceSpeed(float s) { bounceSpeed_ = s; }
    float GetBounceSpeed() const { return bounceSpeed_; }
    void SetBulletKnockback(float s) { bulletKnockback_ = s; }
    float GetBulletKnockback() const { return bulletKnockback_; }

    /// @brief 自爆の爆風半径 = base + perSize * (分裂前サイズ - 1)
    void SetSelfDestructRadius(float base, float perSize) { selfDestructBaseRadius_ = base; selfDestructPerSize_ = perSize; }

    int GetAliveCount() const;
    int GetActiveBulletCount() const;
    const std::vector<std::unique_ptr<MobEnemy>>& GetEnemies() const { return enemies_; }

private:
    /// @brief 弾を1発撃つ（プールから使い回す）
    void FireBullet(const MobEnemyConfig& config, const MobEnemy::ShootRequest& request);

    /// @brief 弾モデルを読み込んでキャッシュする
    const Object3d::ModelData* GetOrLoadBulletModel(const MobEnemyConfig& config, std::string& outKey);

    /// @brief プレイヤーの塊 vs 全敵
    void ResolvePlayerCollisions(PikminPlayer* player, const Vector2& stageTilt, const Vector2& pivot);

    /// @brief 敵弾 vs プレイヤーの塊
    void ResolveBulletCollisions(PikminPlayer* player);

    /// @brief 小スライム（ミニオン）vs 全敵。判定ルールはプレイヤー本体と同じ
    void ResolveMinionCollisions(MinionManager* minionManager, const Vector2& stageTilt, const Vector2& pivot);

    /// @brief プレイヤーの自爆で、爆風内の敵を強さ問わず倒す
    void ResolveSelfDestruct(PikminPlayer* player);

    /// @brief 現在のステージ傾斜での床面法線
    static Vector3 CalcStageNormal(const Vector2& stageTilt);

    int RollStrength(EnemyType type);

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;

    std::vector<std::unique_ptr<MobEnemy>> enemies_;
    std::vector<std::unique_ptr<EnemyBullet>> bullets_;
    std::unordered_map<std::string, Object3d::ModelData> bulletModels_;

    EnemyBase::ScaleFromStrengthFunc scaleFunc_; //!< 空なら EnemyBase の既定

    std::mt19937 rng_{ std::random_device{}() };

    float bounceSpeed_ = 13.0f;        //!< 強い敵にぶつかったときの吹っ飛び初速
    float bulletKnockback_ = 9.0f;     //!< 被弾時のノックバック初速
    float minionBounceSpeed_ = 9.0f;   //!< 小スライムが強い敵に弾かれるときの初速
    float selfDestructBaseRadius_ = 4.0f; //!< 自爆の爆風半径（サイズ1のとき）
    float selfDestructPerSize_ = 0.8f;    //!< 分裂前サイズ1つあたりの爆風半径の伸び
    float lastSelfDestructRadius_ = 0.0f; //!< ImGui 表示用
    int lastSelfDestructKills_ = 0;       //!< ImGui 表示用
    bool enableCollision_ = true;   //!< デバッグ用に判定を止められるように

    // ImGui 用
    int imguiSpawnType_ = 0;
    int imguiSpawnStrength_ = -1;
    float imguiSpawnDistance_ = 6.0f;
    int imguiConfigType_ = 0;
    int imguiScalePreset_ = 0;
};
