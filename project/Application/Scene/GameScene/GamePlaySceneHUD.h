#pragma once

#include <memory>
#include <vector>

#include <d3d12.h>

#include "Sprite.h"
#include "Baziru3_Engine/Core/Base/Vector.h"
#include "Application/UI/NumberDisplay.h"

class Camera;
class Slime;
class SlimeManager;
class EnemyManager;

/**
 * @brief ゲームプレイシーンの 2D UI（HUD）
 *
 * 仮想解像度 1280x720 基準。画面の上のほうに
 *   1段目: SCORE [数値]  TIME [数値]  COIN [数値]
 *   2段目: LIFE  ○○○○○…（残機のぶんだけ小さいスライムを並べる）
 * を並べる。実行中は ImGui の "Game HUD" から座標を詰められる。
 *
 * 数値は ClearScene と同じアトラス（Resources/UI/Clear/digits.png）を
 * NumberDisplay 経由で切り出して使う。桁が変わった瞬間に弾む挙動もそのまま。
 *
 * ほかに:
 *   - 敵・プレイヤー・ミニオンの頭の上に「強さ」の数字（ワールド座標 → 画面座標）
 *   - 敵を倒したときのスコア加算ぶんが、プレイヤーの頭の上に浮かんでから消える
 *
 * @note Sprite の PSO はデプス無効なので、3D の後に描けば必ず手前に来る。
 */
class GamePlaySceneHud
{
public:
    /// @brief 毎フレーム流し込む状態
    struct FrameInput
    {
        Camera* camera = nullptr;
        Slime* player = nullptr;              //!< 群れの代表（SlimeManager::GetLeader()）
        SlimeManager* slimeManager = nullptr; //!< スライム群全体（頭上の数字用）
        EnemyManager* enemyManager = nullptr;

        int score = 0;
        float elapsedSeconds = 0.0f;
        int coin = 0;
        int life = 0;              //!< 残機 ＝ 全スライムのサイズ合計
        bool showHeadNumbers = true;
    };

    GamePlaySceneHud() = default;
    ~GamePlaySceneHud();

    GamePlaySceneHud(const GamePlaySceneHud&) = delete;
    GamePlaySceneHud& operator=(const GamePlaySceneHud&) = delete;

    void Initialize();
    void Finalize();

    void Update(float deltaTime, const FrameInput& input);
    void Draw(ID3D12GraphicsCommandList* commandList);
    void DrawImGui();

    /// @brief スコア加算ぶんを、指定のワールド座標から浮かび上がらせる
    void PushScorePopup(int amount, const Vector3& worldPosition);

    /// @brief 残機が減った瞬間に1回だけ true（SE のトリガ用）
    bool TakeLifeLostEvent();

    /// @brief スコア／コインのカウンタの桁が動いた瞬間に1回だけ true（SE のトリガ用）
    bool TakeCounterTickEvent();

    /**
     * @brief ワールド座標を仮想解像度 1280x720 の画面座標へ変換する
     * @return カメラの後ろなら false（画面外として捨てる）
     * @note 行列は行ベクトル規約（v * M）。engine 全体がこの規約
     */
    static bool WorldToScreen(const Camera& camera, const Vector3& world, Vector2& outScreen);

private:
    /// @brief 残機アイコン1個
    struct LifeIcon
    {
        std::unique_ptr<Sprite> sprite;
        float anim = 0.0f;    //!< 0 = 居ない / 1 = 出きっている
        bool isAlive = false; //!< 今フレーム、この枠に残機が居るか
        float phase = 0.0f;   //!< ふわふわの位相（個体差）
    };

    /// @brief 頭の上の強さの数字。プールから借りて使う
    struct HeadNumber
    {
        std::unique_ptr<NumberDisplay> number;
        bool used = false;
    };

    /// @brief スコア加算のポップアップ
    struct ScorePopup
    {
        std::unique_ptr<NumberDisplay> number;
        bool isActive = false;
        float age = 0.0f;
        Vector3 worldPosition{};
        Vector2 screenPosition{};
    };

    void UpdateCounters(float deltaTime);
    void UpdateLife(float deltaTime, int life);
    void UpdateHeadNumbers(const FrameInput& input);
    void UpdateScorePopups(float deltaTime, const FrameInput& input);

    /// @brief 空いている頭数字を借りて、その場に配置する
    void PlaceHeadNumber(const Camera& camera, const Vector3& worldPosition, float headOffsetY,
                         int value, const Vector4& tint);

    void ReleaseUnusedHeadNumbers();

    static std::unique_ptr<Sprite> MakeSprite(const char* texturePath, const Vector2& size);
    static void ApplySprite(Sprite* sprite, const Vector2& center, const Vector2& size, float alpha);

private:
    /// @brief ラベル1枚（白い文字 ＋ 少しずらして重ねる濃紺の同じ文字）
    struct LabelSprite
    {
        std::unique_ptr<Sprite> base;
        std::unique_ptr<Sprite> shadow;
        Vector4 shadowColor{ 0.0f, 0.0f, 0.0f, 1.0f }; //!< 生成時に1回だけ決める
    };

    /// @brief ラベルを1組作る
    static LabelSprite MakeLabel(const char* texturePath, const Vector2& size,
                                 const UiTextShadowStyle& shadowStyle);

    /// @brief ラベルを配置して、重ねるほうも写す
    void ApplyLabel(LabelSprite& label, const Vector2& center, const Vector2& size, float alpha);

private:
    // --- 1段目: SCORE / TIME / COIN ---
    LabelSprite labelScore_;
    LabelSprite labelTime_;
    LabelSprite labelCoin_;
    NumberDisplay scoreNumber_;
    NumberDisplay timeNumber_;
    NumberDisplay coinNumber_;

    // --- 2段目: LIFE ---
    LabelSprite labelLife_;
    std::vector<LifeIcon> lifeIcons_;
    int shownLifeCount_ = 0;

    // --- 頭の上の強さ ---
    std::vector<HeadNumber> headNumbers_;
    int headCursor_ = 0;

    // --- スコア加算のポップアップ ---
    std::vector<ScorePopup> scorePopups_;

    // --- SE 用の1フレームイベント ---
    bool lifeLostEvent_ = false;
    bool counterTickEvent_ = false;

public:
    /// @brief 白い文字が背景に溶けないように重ねる濃紺の設定（ImGui の "Game HUD" から触れる）
    /// @note ラベルと数値で別々に持っている。数値のほうは各 NumberDisplay の style に入る
    UiTextShadowStyle labelShadow_{};
    UiTextShadowStyle numberShadow_{};

    // ===============================================================
    // レイアウト（1280x720 基準）。ImGui の "Game HUD" から調整できる
    // 添付のラフ（SCORE / TIME / COIN が上段、LIFE が2段目）に合わせた初期値
    // ===============================================================
    bool showHud_ = true;

    Vector2 labelScorePos_{ 88.0f, 40.0f };
    Vector2 labelTimePos_{ 474.0f, 40.0f };
    Vector2 labelCoinPos_{ 769.0f, 40.0f };
    Vector2 labelSize_{ 150.0f, 44.0f };

    /// 数値は「右端の桁の中心」で置く
    Vector2 scoreValuePos_{ 382.0f, 40.0f };
    Vector2 timeValuePos_{ 678.0f, 40.0f };
    Vector2 coinValuePos_{ 972.0f, 40.0f };
    Vector2 digitSize_{ 40.0f, 52.0f };
    float digitSpacing_ = 3.0f;

    Vector2 labelLifePos_{ 88.0f, 88.0f };
    Vector2 labelLifeSize_{ 130.0f, 44.0f };
    Vector2 lifeOrigin_{ 182.0f, 88.0f }; //!< 1個目のスライムの中心
    float lifeSpacing_ = 40.0f;
    Vector2 lifeIconSize_{ 40.0f, 32.0f };
    float lifePopSeconds_ = 0.22f;   //!< にゅっと出るまでの時間
    float lifeHideSeconds_ = 0.18f;  //!< にゅっと消えるまでの時間
    float lifeBobAmplitude_ = 2.5f;
    float lifeBobSpeed_ = 3.0f;
    int lifeMaxIcons_ = 24;          //!< 並べる上限（これを超えたら以降は描かない）

    // 頭の上の強さ
    bool showHeadNumbers_ = true;
    Vector2 headDigitSize_{ 26.0f, 32.0f };
    float headDigitSpacing_ = 0.0f;
    float headOffsetPlayer_ = 1.55f;  //!< 見た目半径の何倍ぶん上に出すか
    float headOffsetMinion_ = 2.0f;
    float headOffsetEnemy_ = 1.25f;   //!< 敵はヒットボックス上端からの倍率
    float headAlpha_ = 0.95f;

    // スコア加算のポップアップ
    float popupRiseSpeed_ = 62.0f;   //!< 画面座標での上昇速度 (px/s)
    float popupLife_ = 1.1f;
    Vector2 popupDigitSize_{ 34.0f, 44.0f };
    float popupOffsetY_ = 2.2f;      //!< プレイヤーの見た目半径の何倍ぶん上から出すか

    // カウンタのパラパラ
    float scoreRollCatchUp_ = 5.0f;
    float scoreRollMinStep_ = 140.0f;
    float coinRollCatchUp_ = 8.0f;
    float coinRollMinStep_ = 6.0f;
};
