#pragma once

#include <string>
#include <vector>

#include "Baziru3_Engine/Core/Base/Vector.h"
#include "Application/Enemy/EnemyBase.h"

/**
 * @brief 配置可否の判定
 *
 * 判定材料は「その XZ の真下に歩ける床があるか」と「その床の頭上が空いているか」だけ。
 * 地形データ側に進入禁止領域の定義が無いので、これ以上のことは分からない。
 *
 *   床なし        -> 島の外（NoGround）
 *   床あり／頭上空 -> 配置できる（Ok）
 *   床あり／頭上詰 -> オーバーハングや上下段の隙間の中（Obstructed）
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
        Obstructed,  //!< 頭上が詰まっている（オーバーハングの下）
    };

    /**
     * @brief その XZ に置けるかを判定する
     * @param x ワールド（＝傾き0のときのステージローカル）X
     * @param z ワールド Z
     * @param outY 置ける場合、その床のY座標が入る（nullptr 可）
     */
    Result Test(float x, float z, float* outY = nullptr);

    /// @brief 判定結果のラベル（ImGui 表示用。ImGui のフォントに日本語が無いので ASCII）
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

/// @brief 配置された成長キューブ（食べると残機が増えるやつ）1個分
struct StageGrowthCubeEntry
{
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    float size = 0.85f; //!< 見た目の直径めやす
};

/**
 * @brief 配置された地形メッシュ1枚分
 *
 * 地形は「Resources/10days の中の obj を、好きな位置・向き・大きさで何枚でも置く」形。
 * 以前は GamePlayScene::InitializeScene() にハードコードされていたが、
 * 配置エディタで編集できるよう JSON へ移した。
 *
 * @note 位置は XZ だけをエディタで動かす（Y は手で書けば効く）。
 *       回転は Y 軸のみ。スケールは一様。地形メッシュ同士の衝突は考慮しない。
 */
struct StageTerrainEntry
{
    std::string mesh;         //!< Resources/10days 配下の obj ファイル名（例 "startLand.obj"）
    std::string texture;      //!< 空なら mtl 指定 → それも無ければ既定のテクスチャ
    Vector3 position{ 0.0f, 0.0f, 0.0f }; //!< ワールド座標（傾き0のとき）
    float rotationY = 0.0f;   //!< Y 軸まわりの回転 (rad)
    float scale = 0.25f;      //!< 一様スケール
    bool bossTrigger = false; //!< true のメッシュに踏み入れるとボス戦が始まる
};

/// @brief ボスの配置
struct StageBossEntry
{
    bool enabled = false;                 //!< false ならボスを出さない
    Vector3 position{ 0.0f, 0.0f, 0.0f }; //!< ステージローカル座標
    int hp = 100;                         //!< 初期HP（＝強さ）
};

/**
 * @brief ステージの配置データ（地形・プレイヤー初期位置・敵・コイン・成長キューブ・ボス）
 *
 * 配置エディタが書き出し、GamePlayScene の初期化が読み込む。
 * 座標は全部「ステージローカル＝傾き0のときのワールド座標」で持つ。
 * Y は参考値で、実行時は地形へのレイキャストで上書きされる（地形自身を除く）。
 */
class StageLayout
{
public:
    /// @brief 既定の保存先。Resources/stage_layout.json は別プロジェクトの遺物なので名前を分けてある
    static const char* kDefaultPath;

    /// @brief 地形メッシュを探すディレクトリ
    static const char* kTerrainDirectory;

    std::vector<StageTerrainEntry> terrain;
    Vector3 playerStart{ 0.0f, 0.0f, 0.0f };
    std::vector<StageEnemyEntry> enemies;
    std::vector<StageCoinEntry> coins;
    std::vector<StageGrowthCubeEntry> growthCubes;
    StageBossEntry boss;

    void Clear();
    bool IsEmpty() const { return enemies.empty() && coins.empty() && growthCubes.empty() && !boss.enabled; }

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
     * @brief いま登録されている地形に対して、この配置データが有効かを調べる
     * @param[out] outValidRatio 敵・コイン・キューブのうち「島の上に乗っている」割合 (0..1)
     * @return 半分以上が島の上にあれば true。配置が空の場合も true
     * @note 地形を差し替えると、前の地形で作った JSON の座標は島の外へ出てしまう。
     *       そのまま適用すると全部奈落に落ちるので、読み込み側で弾くために使う
     *
     * @note **地形そのものは対象外**。地形を StageTerrain へ適用したあとに呼ぶこと
     */
    bool IsCompatibleWithCurrentTerrain(float* outValidRatio = nullptr) const;

    /**
     * @brief 地形を実際にレイキャストして、当たり障りのない配置を作る
     *
     * JSON が見つからない／地形と噛み合わないときに使う。座標をハードコードすると
     * 地形差し替えで全部宙に浮くので、置ける場所を自分で探す。
     *
     * @note SlimePhysics に地形メッシュが登録された後に呼ぶこと。
     *       地形が未登録なら原点だけを返す。**terrain は空のまま**（呼び出し側が持っている）
     */
    static StageLayout MakeFallback(int enemyCount = 6, int coinCount = 12, int growthCubeCount = 4);

    /**
     * @brief 地形の既定配置（JSON に terrain が無かったときのフォールバック）
     *
     * 以前 GamePlayScene::InitializeScene() にハードコードされていた
     * 「startLand + Land1 + toLandRoad + roadCell×6（45.563m 刻み）」をそのまま再現する。
     */
    static std::vector<StageTerrainEntry> MakeDefaultTerrain();

    static const char* TypeToName(EnemyType type);
    static bool NameToType(const std::string& name, EnemyType& outType);
};
