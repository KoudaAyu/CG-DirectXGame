#pragma once

#include <memory>
#include <vector>

#include "Sprite.h"
#include "Baziru3_Engine/Core/Base/Vector.h"

/**
 * @brief 数字アトラスの切り出し設定と見た目
 *
 * 画像は ClearScene と共用（Resources/UI/Clear/digits.png）。
 *   セル 128 x 128 / 4列 x 3行 / 全体 512 x 384
 *   index 0-9 = 数字 / 10 = ":" / 11 = "+"（スコア加算のポップアップ用）
 *
 * @note ClearScene のメモでは index 11 は「予備」となっていた。
 *       スコア加算のポップアップで "+" を使うので、そこに "+" を描いておくこと。
 *       画像が無くても engine が白ダミーに差し替えるので落ちない。
 */
struct NumberDisplayStyle
{
    const char* atlasTexture = "Resources/UI/Clear/digits.png";
    Vector2 cellSize{ 128.0f, 128.0f };
    int atlasColumns = 4;
    int colonCell = 10;      //!< ":" のセル番号
    int signCell = 11;       //!< "+" のセル番号
    float colonWidthScale = 0.5f; //!< ":" だけ画面上の横幅を詰める
    float signWidthScale = 0.7f;  //!< "+" も少し詰める
    Vector2 digitSize{ 44.0f, 56.0f }; //!< 画面上の1桁のサイズ（ピクセル）
    float spacing = 2.0f;              //!< 桁の間隔
    float punchAmount = 0.30f;         //!< 桁が変わった瞬間の弾み量
    float punchDamping = 11.0f;        //!< 弾みの減衰速度
};

/**
 * @brief アトラス切り出し方式の数値表示ウィジェット
 *
 * ClearScene の数値表示（1枚のアトラスから桁ごとに切り出して、
 * 桁が変わった瞬間に縦伸び／横縮みで弾ませる）を、
 * ゲームシーンからも使えるように切り出したもの。
 *
 * ClearScene 側は動作実績があるのでそのまま残してある。
 * あちらを載せ替えたくなったらこのクラスに寄せられる。
 *
 * @note Sprite の PSO はデプス無効なので、描いた順に手前へ重なる。
 */
class NumberDisplay
{
public:
    /// @brief 表示のしかた
    enum class Mode
    {
        Integer,     //!< ただの整数（ゼロ埋めの有無は SetZeroPadding）
        TimeMMSS,    //!< MM:SS（値は「秒」で渡す）
        SignedPopup, //!< "+123" のような符号つき。中央そろえ向き
    };

    NumberDisplay() = default;
    ~NumberDisplay();

    NumberDisplay(const NumberDisplay&) = delete;
    NumberDisplay& operator=(const NumberDisplay&) = delete;

    /**
     * @brief 初期化
     * @param cellCapacity 用意するスプライトの数（表示しうる最大の「セル数」）
     *                     TimeMMSS なら 5、6桁のスコアなら 6、"+" 付きなら桁数 + 1
     * @param style 見た目の設定
     */
    void Initialize(int cellCapacity, const NumberDisplayStyle& style);
    void Finalize();

    void SetMode(Mode mode) { mode_ = mode; }
    Mode GetMode() const { return mode_; }

    /// @brief 上位の 0 も描くか（HUD のカウンタは true、ポップアップは false）
    void SetZeroPadding(bool on) { zeroPad_ = on; }

    /// @brief true なら Update() に渡す座標が「中央」、false なら「右端」の基準になる
    void SetCenterAlign(bool on) { centerAlign_ = on; }

    void SetTarget(int value) { target_ = value; }
    int GetTarget() const { return target_; }

    /// @brief 演出なしで一気に目標値へ合わせる
    void SnapToTarget() { shown_ = static_cast<float>(target_); }

    /**
     * @brief パラパラの速さ
     * @param catchUpRate 残差に掛かる追いつき係数（大きいほど速い）
     * @param minStepPerSecond 1秒あたりの最低増分。残差が小さくてもこの速さは出る
     */
    void SetRollSpeed(float catchUpRate, float minStepPerSecond)
    {
        catchUp_ = catchUpRate;
        minStep_ = minStepPerSecond;
    }

    /**
     * @brief 更新（スプライトの座標・サイズ・UV をここで全部決める）
     * @param deltaTime デルタタイム
     * @param anchor 右端（centerAlign_ が true なら中央）の画面座標
     * @param alpha 不透明度
     * @param scale 全体に掛かる倍率。ポップアップの縮小などに使う
     * @return このフレームに桁の絵が変わったか（カウンタ音のトリガに使える）
     */
    bool Update(float deltaTime, const Vector2& anchor, float alpha,
                const Vector2& scale = { 1.0f, 1.0f });

    void Draw(ID3D12GraphicsCommandList* commandList) const;

    /// @brief まだ目標値へ向かって回っている最中か
    bool IsRolling() const { return static_cast<int>(shown_ + 0.5f) != target_; }

    int GetShownValue() const { return static_cast<int>(shown_ + 0.0001f); }

    /// @brief 直近の Update() で組んだ、実際に描いている部分の横幅
    float GetLayoutWidth() const { return layoutWidth_; }

    NumberDisplayStyle& GetStyle() { return style_; }
    const NumberDisplayStyle& GetStyle() const { return style_; }

private:
    /// @brief 1桁分。アトラスのどのセルを出すかだけを持つ
    struct CellSprite
    {
        std::unique_ptr<Sprite> sprite;
        int cell = -1;       //!< 今フレーム出すセル番号
        int shownCell = -1;  //!< 前フレームのセル番号（変化を検出して弾ませる）
        float punch = 0.0f;  //!< 桁が変わった瞬間の弾み。1.0 から減衰する
        bool visible = false;
    };

    /// @brief 表示するセル番号を左から詰める。戻り値は使ったセル数
    int BuildCells(int* out, int capacity) const;

    /// @brief セル番号 -> 切り出し左上座標
    Vector2 CellLeftTop(int cell) const;

    /// @brief セル番号 -> 画面上の横幅倍率（":" と "+" だけ細い）
    float CellWidthScale(int cell) const;

private:
    NumberDisplayStyle style_{};
    Mode mode_ = Mode::Integer;
    bool zeroPad_ = true;
    bool centerAlign_ = false;

    std::vector<CellSprite> cells_;

    int target_ = 0;
    float shown_ = 0.0f;
    float catchUp_ = 6.0f;
    float minStep_ = 40.0f;
    float layoutWidth_ = 0.0f;
};
