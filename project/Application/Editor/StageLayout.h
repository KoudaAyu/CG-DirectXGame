#pragma once

#include <string>
#include <vector>

#include "Baziru3_Engine/Core/Base/Vector.h"
#include "Application/Enemy/EnemyBase.h"

/**
 * @brief 配置可否の判定
 *
 * 判定材料は「その XZ の真下に歩ける床が何枚あるか」だけ。
 * 地形データ側に進入禁止領域の定義が無いので、これ以上のことは分からない。
 *
 *   0枚 -> 島の外（NoGround）
 *   1枚 -> プレイ area。ここだけ配置できる（Ok）
 *   2枚以上 -> 上下段が重なっている＝遮蔽物の下。ややこしいので配置禁止（Obstructed）
 *
 * @note 必ずステージ傾斜が 0 の状態（＝配置モード中）で呼ぶこと。
 *       傾いていると床の高さが変わって判定がぶれる。
 */
namespace StagePlacement
{
    enum class Result
    {
        Ok,          //!< 配置できる
        NoGround,    //!< 床が無い（島の外）
        Obstructed,  //!< 上下段が重なっている（遮蔽物の下）
    };

    /**
     * @brief その XZ に置けるかを判定する
     * @param x ワールド（＝傾き0のときのステージローカル）X
     * @param z ワールド Z
     * @param outY 置ける場合、その床のY座標が入る（nullptr 可）
     */
    Result Test(float x, float z, float* outY = nullptr);

    /// @brief 判定結果の日本語ラベル（ImGui 表示用）
    const char* ResultLabel(Result r);
}

/// @brief 配置された敵1体分
struct StageEnemyEntry
{
    EnemyType type = EnemyType::Slime;
    Vector3 position{ 0.0f, 0.0f, 0.0f }; //!< ステージローカル座標（傾き0のときのワールド座標）
    int strength = -1;                    //!< -1 ならスポーン時にランダム
};

/// @brief 配置されたコイン1枚分
struct StageCoinEntry
{
    Vector3 position{ 0.0f, 0.0f, 0.0f };
};

/**
 * @brief ステージの配置データ（プレイヤー初期位置・敵・コイン）
 *
 * 配置エディタが書き出し、GamePlayScene の初期化が読み込む。
 * 座標は全部「ステージローカル＝傾き0のときのワールド座標」で持つ。
 * Y は参考値で、実行時は地形へのレイキャストで上書きされる。
 */
class StageLayout
{
public:
    /// @brief 既定の保存先。Resources/stage_layout.json は別プロジェクトの遺物なので名前を分けてある
    static const char* kDefaultPath;

    Vector3 playerStart{ 0.0f, 0.0f, 0.0f };
    std::vector<StageEnemyEntry> enemies;
    std::vector<StageCoinEntry> coins;

    void Clear();
    bool IsEmpty() const { return enemies.empty() && coins.empty(); }

    /**
     * @brief JSON から読み込む
     * @return 読めた場合 true。ファイルが無い・壊れている場合は false（中身は変更しない）
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief JSON へ書き出す
     * @return 書けた場合 true
     */
    bool SaveToFile(const std::string& path) const;

    /**
     * @brief 地形を実際にレイキャストして、当たり障りのないフォールバック配置を作る
     *
     * JSON が見つからないときに使う。座標をハードコードすると地形差し替えで
     * 全部宙に浮く（または下段に落ちる）ので、置ける場所を自分で探す。
     *
     * @param enemyCount 置きたい敵の数
     * @param coinCount 置きたいコインの数
     * @note SlimePhysics に地形メッシュが登録された後に呼ぶこと。
     *       地形が未登録なら原点だけを返す
     */
    static StageLayout MakeFallback(int enemyCount = 6, int coinCount = 12);

    static const char* TypeToName(EnemyType type);
    static bool NameToType(const std::string& name, EnemyType& outType);
};
