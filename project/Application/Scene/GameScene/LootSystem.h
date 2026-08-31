#pragma once
#include <vector>
#include <string>
#include <memory>
#include "Vector.h"

class Player;
class AppParticleManager;

/// <summary>
/// 探索可能な物資の種類
/// </summary>
enum class LootType
{
    Medkit,    // 救急キット (IFAK)
    AmmoBox,   // 予備弾薬箱 (7.62x39mm AP)
    GoldDuck,  // 金の鴨像 (高額換金レリック)
    Roubles    // 現金 (ルーブル紙幣)
};

/// <summary>
/// フィールド上に配置または敵からドロップする物資プロップ構造体
/// </summary>
struct LootableProp
{
    Vector3 position      = { 0.0f, 0.0f, 0.0f }; // 3Dワールド座標
    std::string name      = "LOOT CRATE";         // アイテム名称
    LootType type         = LootType::Roubles;    // 物資種別
    int value             = 0;                    // 換金価値（ルーブル）または補給量
    bool isLooted         = false;                // 回収済みフラグ
    float searchTimer     = 0.0f;                 // 探索進捗タイマー (秒)
    float maxSearchTime   = 1.2f;                 // 探索完了に必要な長押し秒数
};

/// <summary>
/// 浮遊ダメージ/物資獲得テキスト情報
/// </summary>
struct FloatingText
{
    Vector3 position;     // 発生3D座標
    std::string text;     // 表示テキスト
    Vector4 color;        // 文字カラー
    float lifeTime;       // 残り生存秒数
    float maxLifeTime;    // 最大生存秒数
    bool isCritical;      // クリティカル演出フラグ
};

/// <summary>
/// 物資探索（Looting）＆ ドロップ管理サブシステム
/// ステージ上の木箱、敵撃破時の死体漁り、[E]長押し探索進行、インベントリ加算を一元管理します。
/// </summary>
class LootSystem
{
public:
    // === 定数定義 (Constants) ===
    static constexpr float kInteractRadius        = 2.2f;  // [E]キーで探索可能なインタラクト距離 (m)
    static constexpr float kDefaultSearchDuration = 1.2f;  // 通常木箱の探索所要時間 (秒)
    static constexpr float kCorpseSearchDuration  = 0.8f;  // 死体漁りの探索所要時間 (秒)
    static constexpr int   kAmmoBoxSupplyCount    = 30;    // 弾薬箱1個あたりの補給弾薬数 (30発)
    static constexpr int   kGoldDuckValue         = 50000; // 金の鴨像の売却額 ($50,000)
    static constexpr int   kCashMinRoubles        = 8000;  // 現金ドロップの最低額 ($8,000)
    static constexpr int   kCashMaxRoubles        = 25000; // 現金ドロップの最高額 ($25,000)

public:
    /// <summary>
    /// 初期化（ステージ上の初期木箱・物資クレートの配置）
    /// </summary>
    void Initialize();

    /// <summary>
    /// 全物資リストのリセット（再出撃時）
    /// </summary>
    void Reset();

    /// <summary>
    /// 敵撃破時に死体ロケーションへ戦利品クレートを動的生成
    /// </summary>
    /// <param name="position">敵の死亡位置</param>
    /// <param name="enemyName">撃破された敵の名称</param>
    void SpawnCorpseLoot(const Vector3& position, const std::string& enemyName);

    /// <summary>
    /// 毎フレームの更新（プレイヤー接近判定、[E]長押し探索進行、アイテム取得処理）
    /// </summary>
    void Update(float deltaTime, Player* player, AppParticleManager* particleMgr, std::vector<FloatingText>& floatingTexts);

    /// <summary>
    /// 全物資プロップリストの参照を取得
    /// </summary>
    const std::vector<LootableProp>& GetProps() const { return props_; }
    std::vector<LootableProp>& GetProps() { return props_; }

private:
    std::vector<LootableProp> props_;
};
