#define NOMINMAX
#include "Application/UI/NumberDisplay.h"

#include <algorithm>
#include <cmath>

namespace
{
    /// @brief 指数補間でなめらかに目標値へ寄せる（ClearScene と同じ式）
    float Approach(float current, float target, float rate, float deltaTime)
    {
        const float t = 1.0f - std::exp(-rate * deltaTime);
        return current + (target - current) * t;
    }
}

NumberDisplay::~NumberDisplay()
{
    Finalize();
}

void NumberDisplay::Initialize(int cellCapacity, const NumberDisplayStyle& style)
{
    Finalize();

    style_ = style;
    cellCapacity = (std::max)(1, cellCapacity);
    cells_.resize(static_cast<size_t>(cellCapacity));

    for (CellSprite& cell : cells_)
    {
        cell.cell = -1;
        cell.shownCell = -1;
        cell.punch = 0.0f;
        cell.visible = false;

        cell.sprite = Sprite::Create(style_.atlasTexture, { 0.0f, 0.0f });
        if (!cell.sprite) continue;

        cell.sprite->SetAnchorPoint({ 0.5f, 0.5f });
        cell.sprite->SetSize(style_.digitSize);
        cell.sprite->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });

        // 切り出しサイズはここで一度決めれば十分。
        // 左上座標だけを毎フレーム差し替えて数字を切り替える
        // （Sprite::Update() が textureLeftTop_ / textureSize_ から UV を計算し直す）
        cell.sprite->SetTextureSize(style_.cellSize);
    }
}

void NumberDisplay::Finalize()
{
    for (CellSprite& cell : cells_)
    {
        if (cell.sprite)
        {
            cell.sprite->Finalize();
            cell.sprite.reset();
        }
    }
    cells_.clear();
}

Vector2 NumberDisplay::CellLeftTop(int cell) const
{
    const int columns = (std::max)(1, style_.atlasColumns);
    const int column = cell % columns;
    const int row = cell / columns;
    return { static_cast<float>(column) * style_.cellSize.x,
             static_cast<float>(row) * style_.cellSize.y };
}

float NumberDisplay::CellWidthScale(int cell) const
{
    if (cell == style_.colonCell) return style_.colonWidthScale;
    if (cell == style_.signCell) return style_.signWidthScale;
    return 1.0f;
}

int NumberDisplay::BuildCells(int* out, int capacity) const
{
    if (capacity <= 0) return 0;

    int value = static_cast<int>(shown_);
    if (value < 0) value = 0;

    if (mode_ == Mode::TimeMMSS)
    {
        // MM:SS。桁の並びは固定（分の十/一 → ":" → 秒の十/一）
        int minutes = value / 60;
        int seconds = value % 60;
        if (minutes > 99)
        {
            minutes = 99;
            seconds = 59;
        }
        const int fixedCells[5] = { minutes / 10, minutes % 10, style_.colonCell,
                                    seconds / 10, seconds % 10 };
        const int count = (std::min)(capacity, 5);
        for (int i = 0; i < count; ++i) out[i] = fixedCells[i];
        return count;
    }

    // --- 数字部分を右から取り出す ---
    int digits[16] = {};
    int digitCount = 0;
    int work = value;
    do
    {
        if (digitCount >= 16) break;
        digits[digitCount++] = work % 10;
        work /= 10;
    } while (work > 0);

    const bool wantsSign = (mode_ == Mode::SignedPopup);
    const int signSlots = wantsSign ? 1 : 0;

    int shownDigits = digitCount;
    if (zeroPad_)
    {
        // 上位の 0 も描く。用意したセル数いっぱいまで使う
        shownDigits = (std::max)(digitCount, capacity - signSlots);
    }
    shownDigits = (std::min)(shownDigits, capacity - signSlots);
    shownDigits = (std::max)(1, shownDigits);

    int index = 0;
    if (wantsSign && signSlots > 0) out[index++] = style_.signCell;

    for (int i = shownDigits - 1; i >= 0; --i)
    {
        const int digit = (i < digitCount) ? digits[i] : 0;
        out[index++] = digit;
    }
    return index;
}

bool NumberDisplay::Update(float deltaTime, const Vector2& anchor, float alpha,
                           const Vector2& scale)
{
    // --- パラパラとカウントを進める ---
    const float targetF = static_cast<float>(target_);
    const float diff = targetF - shown_;
    if (std::abs(diff) <= 0.5f)
    {
        shown_ = targetF;
    }
    else
    {
        float step = (std::max)(minStep_, std::abs(diff) * catchUp_) * deltaTime;
        step = (std::min)(step, std::abs(diff));
        shown_ += (diff > 0.0f) ? step : -step;
    }

    // --- 出すセルを決める ---
    const int capacity = static_cast<int>(cells_.size());
    int wanted[32] = {};
    const int wantedCount = BuildCells(wanted, (std::min)(capacity, 32));

    for (int i = 0; i < capacity; ++i)
    {
        cells_[static_cast<size_t>(i)].visible = (i < wantedCount);
        cells_[static_cast<size_t>(i)].cell = (i < wantedCount) ? wanted[i] : -1;
    }

    // --- 全体の横幅を先に測る（中央そろえと右そろえの両方に要る）---
    const float digitW = style_.digitSize.x * scale.x;
    const float digitH = style_.digitSize.y * scale.y;
    const float spacing = style_.spacing * scale.x;

    float totalWidth = 0.0f;
    for (int i = 0; i < wantedCount; ++i)
    {
        totalWidth += digitW * CellWidthScale(wanted[i]);
        if (i > 0) totalWidth += spacing;
    }
    layoutWidth_ = totalWidth;

    // 右端の座標を決める。centerAlign_ なら anchor が中央
    const float rightEdge = centerAlign_ ? (anchor.x + totalWidth * 0.5f)
                                         : (anchor.x + digitW * 0.5f);

    // --- 右から順に並べる ---
    bool cellChanged = false;
    float cursorRight = rightEdge;

    for (int i = wantedCount - 1; i >= 0; --i)
    {
        CellSprite& cell = cells_[static_cast<size_t>(i)];

        const float width = digitW * CellWidthScale(cell.cell);
        const float centerX = cursorRight - width * 0.5f;
        cursorRight -= width + spacing;

        // 桁が変わった瞬間に弾ませる
        if (cell.cell != cell.shownCell)
        {
            if (cell.shownCell >= 0)
            {
                cell.punch = 1.0f;
                cellChanged = true;
            }
            cell.shownCell = cell.cell;

            if (cell.sprite)
            {
                cell.sprite->SetTextureLeftTop(CellLeftTop(cell.cell));
            }
        }
        cell.punch = Approach(cell.punch, 0.0f, style_.punchDamping, deltaTime);

        if (!cell.sprite) continue;

        // 縦に伸びて横が縮む＝跳ねた感じ
        const float scaleY = 1.0f + style_.punchAmount * cell.punch;
        const float scaleX = 1.0f - style_.punchAmount * cell.punch * 0.55f;

        cell.sprite->SetPosition({ centerX, anchor.y });
        cell.sprite->SetSize({ width * scaleX, digitH * scaleY });
        cell.sprite->SetRotation(0.0f);

        Vector4 color = cell.sprite->GetColor();
        color.w = std::clamp(alpha, 0.0f, 1.0f);
        cell.sprite->SetColor(color);
        cell.sprite->Update();
    }

    // 使わなかったスプライトは透明にして畳んでおく
    for (int i = wantedCount; i < capacity; ++i)
    {
        CellSprite& cell = cells_[static_cast<size_t>(i)];
        cell.shownCell = -1;
        cell.punch = 0.0f;
        if (!cell.sprite) continue;

        Vector4 color = cell.sprite->GetColor();
        color.w = 0.0f;
        cell.sprite->SetColor(color);
        cell.sprite->Update();
    }

    return cellChanged;
}

void NumberDisplay::Draw(ID3D12GraphicsCommandList* commandList) const
{
    for (const CellSprite& cell : cells_)
    {
        if (cell.visible && cell.sprite)
        {
            cell.sprite->Draw(commandList);
        }
    }
}
