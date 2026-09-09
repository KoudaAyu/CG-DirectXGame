#pragma once

#include <algorithm>
#include <cstdlib>
#include <memory>

#include "Sprite.h"
#include "Baziru3_Engine/Core/Base/Vector.h"

/**
 * @brief UI の文字に「ずらした濃い色の同じ文字」を重ねるための小道具
 *
 * UI の文字画像が白いままだと、明るい地形やスカイボックスの上で背景に溶けて読めない。
 * そこで **同じ画像をもう1枚、少し左上へずらして濃紺で重ねる**。
 *
 * 既定（inFront = true）では重ねたほうが**手前**に来るので、
 *   - 文字の本体は濃紺になって読める
 *   - 元の白が右下のふちにだけ残って、ハイライトのように見える
 * という2色刷りっぽい見た目になる。
 *
 * `inFront = false` にすると、ただの影として後ろに敷くだけになる
 * （その場合はオフセットを右下＝正の値にするのが自然）。
 *
 * ヘッダオンリーなので .cpp は無い。使う側は
 *   1. スプライトを1枚作るごとに Create() でもう1枚作る
 *   2. その1枚ぶんの色を MakeColor() で決めておく（ここでランダムが入る）
 *   3. 元のスプライトを配置し終わったら Sync() で写す
 *   4. Draw の順番だけ inFront で振り分ける
 * の4つを守ればいい。
 *
 * @note Sprite の PSO はデプス無効なので、**後に描いたものが必ず手前**に来る。
 */
struct UiTextShadowStyle
{
    bool enabled = true;

    /// @brief 元の文字からのずらし量（ピクセル）。負で左上
    Vector2 offset{ -4.0f, -4.0f };

    /// @brief めっちゃ深い青
    Vector4 color{ 0.045f, 0.075f, 0.30f, 1.0f };

    /// @brief RGB に乗せるランダム幅（±）。0 でランダム無し
    float colorJitter = 0.05f;

    /// @brief 青みだけ余分に振る量。青の濃さがバラけて単調にならない
    float blueJitter = 0.10f;

    /// @brief true: 元の文字の**手前**に重ねる（本体が濃紺になる）/ false: 後ろに敷くただの影
    bool inFront = true;

    /// @brief 元の文字の不透明度に掛ける係数
    float alphaScale = 1.0f;
};

namespace UiShadow
{
    /// @brief 0..1 の一様乱数。見た目のばらつき用なので rand() で十分
    inline float Random01()
    {
        return static_cast<float>(std::rand() % 1001) / 1000.0f;
    }

    /**
     * @brief この1枚ぶんの色を決める（**生成時に1回だけ**呼ぶこと）
     * @note 毎フレーム呼ぶと色がチカチカするので絶対にやらないこと
     */
    inline Vector4 MakeColor(const UiTextShadowStyle& style)
    {
        auto jitter = [](float base, float amount) {
            const float r = (Random01() - 0.5f) * 2.0f * amount;
            return std::clamp(base + r, 0.0f, 1.0f);
        };

        Vector4 color{};
        color.x = jitter(style.color.x, style.colorJitter);
        color.y = jitter(style.color.y, style.colorJitter);
        color.z = jitter(style.color.z, style.colorJitter + style.blueJitter);
        color.w = style.color.w;
        return color;
    }

    /**
     * @brief 重ね用のスプライトを1枚作る
     * @note 画像が無くても engine が 4x4 の白ダミーを返すので落ちない
     */
    inline std::unique_ptr<Sprite> Create(const char* texturePath, const Vector2& size,
                                          const Vector2& anchorPoint)
    {
        std::unique_ptr<Sprite> sprite = Sprite::Create(texturePath, { 0.0f, 0.0f });
        if (!sprite) return nullptr;

        sprite->SetAnchorPoint(anchorPoint);
        sprite->SetSize(size);
        sprite->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        return sprite;
    }

    /**
     * @brief 元のスプライトの位置・サイズ・回転・不透明度を写して、offset ぶんずらす
     * @param color MakeColor() で決めておいた、この1枚ぶんの色
     * @param scaleX ずらし量に掛ける倍率（縮小表示している数字などで使う）
     * @param scaleY 同上
     * @note **元のスプライトを配置し終わったあと**に呼ぶこと。
     *       アトラスの切り出し（SetTextureLeftTop / SetTextureSize）は
     *       元と同じものを呼び出し側で入れておくこと（こちらからは読めない）
     */
    inline void Sync(Sprite* shadow, const Sprite* source, const UiTextShadowStyle& style,
                     const Vector4& color, float scaleX = 1.0f, float scaleY = 1.0f)
    {
        if (!shadow || !source) return;

        const Vector2 position = source->GetPosition();
        shadow->SetPosition({ position.x + style.offset.x * scaleX,
                              position.y + style.offset.y * scaleY });
        shadow->SetSize(source->GetSize());
        shadow->SetRotation(source->GetRotation());

        Vector4 applied = color;
        applied.w = style.enabled ? (source->GetColor().w * style.alphaScale) : 0.0f;
        shadow->SetColor(applied);
        shadow->Update();
    }

    /// @brief 透明にして畳む（使っていない枠用）
    inline void Hide(Sprite* shadow)
    {
        if (!shadow) return;
        Vector4 color = shadow->GetColor();
        color.w = 0.0f;
        shadow->SetColor(color);
        shadow->Update();
    }
}
