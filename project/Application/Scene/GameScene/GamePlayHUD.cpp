#include "GamePlayHUD.h"
#include "GamePlayHUDLayout.h"
#include "TextureManager.h"
#include "Camera.h"
#include "../../Player/Player.h"
#include "../../Enemy/Enemy.h"
#include "../../Enemy/MovingEnemy.h"
#include "Obstacle.h"
#include "Target.h"
#include "RaidStats.h"
#include "TutorialSign.h"
#include "Application/Config/GameConfig.h"
#include "Baziru3_Engine/Framework/AI/NavMesh.h"
#include <Windows.h>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <iterator>
#include <numbers>

#include "Application/Config/AppDebugConfig.h"
#include "Application/UI/TacticalUICanvas.h"
#include "Application/Transition/StingerTransition.h"
#include "Application/Transition/DeathTransition.h"

namespace
{
    using GameConfig::Math::kPi;
    using GameConfig::Math::kTwoPi;

    struct HudViewport
    {
        TacticalDrawList* dl = nullptr;
        float screenW        = 0.0f;
        float screenH        = 0.0f;
        float uiScale        = 1.0f;
    };

    const char* AsUtf8(const char8_t* text) noexcept { return reinterpret_cast<const char*>(text); }

    float SmoothStep01(float x) noexcept
    {
        const float t = (std::clamp)(x, 0.0f, 1.0f);
        return t * t * (HudLayout::Common::kSmoothStepA - HudLayout::Common::kSmoothStepB * t);
    }

    bool AcquireHudViewport(
        HudViewport& out,
        float minScale = HudLayout::Common::kStandardScaleMin,
        float maxScale = HudLayout::Common::kStandardScaleMax)
    {
        TacticalUICanvas* ui = TacticalUICanvas::GetInstance();
        if (!ui) return false;
        const UIVec2 disp = ui->GetDisplaySize();
        TacticalDrawList* dl = ui->GetForegroundDrawList();
        if (!dl || disp.x <= 0.0f || disp.y <= 0.0f) return false;
        out.dl      = dl;
        out.screenW = disp.x;
        out.screenH = disp.y;
        out.uiScale = (std::clamp)(disp.x / HudLayout::Common::kReferenceScreenW, minScale, maxScale);
        return true;
    }

    UITextureID ResolveHudTexture(uint32_t texIndex)
    {
        if (texIndex == UINT32_MAX || texIndex == TextureManager::kInvalidTextureIndex) return static_cast<UITextureID>(0);
        return static_cast<UITextureID>(TextureManager::GetInstance()->GetSrvHandleGPU(texIndex).ptr);
    }

    void FormatHudCommas(int value, char* outBuf, size_t bufSize)
    {
        if (!outBuf || bufSize == 0) return;
        char raw[HudLayout::Common::kSmallBufSize];
        const bool neg = (value < 0);
        const unsigned int absVal = neg ? static_cast<unsigned int>(-value) : static_cast<unsigned int>(value);
        std::snprintf(raw, sizeof(raw), "%u", absVal);
        const int len = static_cast<int>(std::strlen(raw));

        char temp[HudLayout::Common::kMedBufSize];
        int outIdx = 0;
        if (neg) temp[outIdx++] = '-';
        for (int i = 0; i < len && outIdx < HudLayout::Common::kCommaMaxWriteIdx; ++i)
        {
            if (i > 0 && ((len - i) % HudLayout::Common::kCommaGroupDigits == 0)) temp[outIdx++] = ',';
            temp[outIdx++] = raw[i];
        }
        temp[outIdx] = '\0';
        std::snprintf(outBuf, bufSize, "%s", temp);
    }

    float SnapFontPx(float fontSize)
    {
        return std::round((std::clamp)(fontSize, HudLayout::Common::kFontMinPx, HudLayout::Common::kFontMaxPx));
    }

    UIVec2 MeasureSharpText(UIFontRole role, float fontSize, const char* text)
    {
        if (!text || !text[0]) return UIVec2(0.0f, 0.0f);
        TacticalUICanvas* ui = TacticalUICanvas::GetInstance();
        return ui ? ui->CalcTextSize(text, nullptr, SnapFontPx(fontSize), role) : UIVec2(0.0f, 0.0f);
    }

    void DrawShadowedText(
        TacticalDrawList* dl, UIFontRole role, float fontSize, UIVec2 pos, uint32_t col,
        const char* text, float shadowAlphaScale = HudLayout::Common::kDefaultShadowAlpha)
    {
        if (!dl || !text || !text[0]) return;
        const uint32_t alphaByte = (col >> UI_COL32_A_SHIFT) & HudPalette::kChannelMask;
        if (alphaByte == 0) return;
        TacticalUICanvas* ui = TacticalUICanvas::GetInstance();
        const TacticalFont* font = ui ? ui->GetFont(role) : nullptr;
        if (!font) return;

        const float pxSize = SnapFontPx(fontSize);
        const float sx     = std::round(pos.x);
        const float sy     = std::round(pos.y);
        if (shadowAlphaScale > HudLayout::Common::kMinShadowAlpha)
        {
            const int shA   = (std::clamp)(static_cast<int>(static_cast<float>(alphaByte) * shadowAlphaScale), 0, HudPalette::kMaxAlphaInt);
            const float off = HudLayout::Common::kShadowOffsetPx;
            dl->AddText(font, pxSize, UIVec2(sx + off, sy + off), UI_COL32(0, 0, 0, shA), text);
        }
        dl->AddText(font, pxSize, UIVec2(sx, sy), col, text);
    }

    void DrawShadowedTextAligned(
        TacticalDrawList* dl, UIFontRole role, float fontSize, float anchorX, float centerY,
        uint32_t col, const char* text, HudTextAlign align = HudTextAlign::Left,
        float shadowAlphaScale = HudLayout::Common::kDefaultShadowAlpha)
    {
        if (!dl || !text || !text[0]) return;
        const UIVec2 sz = MeasureSharpText(role, fontSize, text);
        float px = anchorX;
        if (align == HudTextAlign::Center) px -= sz.x * 0.5f;
        else if (align == HudTextAlign::Right) px -= sz.x;
        DrawShadowedText(dl, role, fontSize, UIVec2(px, centerY - sz.y * 0.5f), col, text, shadowAlphaScale);
    }

    float MeasureHudAtlasDigits(const char* text, float height)
    {
        return MeasureSharpText(UIFontRole::SubBoldJP, height * HudLayout::Common::kAtlasDigitFontMul, text).x;
    }

    void DrawHudAtlasDigits(
        TacticalDrawList* dl, UITextureID /*digitsTex*/, const char* text,
        float anchorX, float centerY, float height, uint32_t col, HudTextAlign align = HudTextAlign::Left)
    {
        DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, height * HudLayout::Common::kAtlasDigitFontMul,
            anchorX, centerY, col, text, align, HudLayout::Common::kAtlasDigitShadowAlpha);
    }

    void DrawHudAtlasSprite(
        TacticalDrawList* dl, UITextureID tex, float atlasW, float atlasH,
        const AtlasSpriteRect& rect, UIVec2 dMin, UIVec2 dMax, uint32_t tint = UI_COL32_WHITE)
    {
        if (!dl || !tex || ((tint >> UI_COL32_A_SHIFT) & HudPalette::kChannelMask) == 0) return;
        dl->AddImage(tex, dMin, dMax, UIVec2(rect.sx0 / atlasW, rect.sy0 / atlasH), UIVec2(rect.sx1 / atlasW, rect.sy1 / atlasH), tint);
    }

    void DrawSlantedPlate(
        TacticalDrawList* dl, float x, float y, float w, float h, float slant,
        uint32_t fillCol, uint32_t borderCol = 0, float borderThick = 1.0f)
    {
        if (!dl || w <= HudLayout::Common::kMinPlateSize || h <= HudLayout::Common::kMinPlateSize) return;
        const UIVec2 pts[HudLayout::Common::kQuadVertCount] = {
            UIVec2(x + slant, y), UIVec2(x + w + slant, y), UIVec2(x + w, y + h), UIVec2(x, y + h)
        };
        if (((fillCol >> UI_COL32_A_SHIFT) & HudPalette::kChannelMask) > 0)
            dl->AddConvexPolyFilled(pts, HudLayout::Common::kQuadVertCount, fillCol);
        if (((borderCol >> UI_COL32_A_SHIFT) & HudPalette::kChannelMask) > 0)
            dl->AddPolyline(pts, HudLayout::Common::kQuadVertCount, borderCol, UIDrawFlags_Closed, borderThick);
    }

    void DrawFramedPanel(TacticalDrawList* dl, float x, float y, float w, float h, float slant, const SlantedPanelStyle& style)
    {
        DrawSlantedPlate(dl, x + style.shadowOffsetX, y + style.shadowOffsetY, w, h, slant, style.shadowCol);
        DrawSlantedPlate(dl, x, y, w, h, slant, style.fillCol, style.borderCol, style.borderThick);
    }

    void DrawProgressBar(
        TacticalDrawList* dl, float x, float y, float w, float h, float slant,
        float ratio, uint32_t bgCol, uint32_t fillCol, uint32_t borderCol = 0, float borderThick = 1.0f,
        float lagRatio = 0.0f, uint32_t lagCol = 0, float minFillRatio = HudLayout::StatusPanel::kMinFillRatio)
    {
        DrawSlantedPlate(dl, x, y, w, h, slant, bgCol, borderCol, borderThick);
        if (lagRatio > ratio + minFillRatio && ((lagCol >> UI_COL32_A_SHIFT) & HudPalette::kChannelMask) > 0)
            DrawSlantedPlate(dl, x, y, w * lagRatio, h, slant, lagCol);
        if (ratio > minFillRatio)
            DrawSlantedPlate(dl, x, y, w * ratio, h, slant, fillCol);
    }

    uint32_t ScaleHudAlpha(uint32_t col, float alphaScale) noexcept
    {
        const uint32_t a = (col >> UI_COL32_A_SHIFT) & HudPalette::kChannelMask;
        const uint32_t sa = static_cast<uint32_t>((std::clamp)(static_cast<int>(static_cast<float>(a) * alphaScale), 0, HudPalette::kMaxAlphaInt));
        return (col & HudPalette::kRgbMask) | (sa << UI_COL32_A_SHIFT);
    }

    void DrawKeycapBadge(
        TacticalDrawList* dl, float x, float y, float w, float h, float rounding,
        uint32_t bgCol, uint32_t bdCol, float bdThick, float fontSize, uint32_t textCol, const char* keyText, float textCenterY)
    {
        dl->AddRectFilled(UIVec2(x, y), UIVec2(x + w, y + h), bgCol, rounding);
        dl->AddRect(UIVec2(x, y), UIVec2(x + w, y + h), bdCol, rounding, UIDrawFlags_None, bdThick);
        DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, fontSize, x + w * 0.5f, textCenterY, textCol, keyText, HudTextAlign::Center);
    }

    void DrawKeyPromptPill(
        TacticalDrawList* dl, float centerX, float topY, float pillH, float slant, float uiScale,
        const char* keyLabel, const char* actionText, float keyFontPx, float textFontPx,
        uint32_t pillBg, uint32_t pillBd, uint32_t keyBg, uint32_t keyBd, uint32_t keyTx, uint32_t textCol,
        float minClampX, float maxClampX, float alphaScale = 1.0f)
    {
        if (!dl || !actionText || !actionText[0] || alphaScale <= 0.01f) return;
        namespace RT = HudLayout::Reticle;
        const float padX   = RT::kPromptPadX * uiScale;
        const UIVec2 actSz = MeasureSharpText(UIFontRole::SubBoldJP, textFontPx, actionText);
        const bool hasKey  = (keyLabel && keyLabel[0]);
        const float keyW   = hasKey ? (MeasureSharpText(UIFontRole::SubBoldJP, keyFontPx, keyLabel).x + RT::kPromptKeyPadW * uiScale) : 0.0f;
        const float keyGap = hasKey ? (RT::kPromptKeyGapX * uiScale) : 0.0f;
        const float pillW  = padX * 2.0f + keyW + keyGap + actSz.x;
        const float safeMaxX = (std::max)(minClampX, maxClampX - pillW);
        const float px = std::round((std::clamp)(centerX - pillW * 0.5f, minClampX, safeMaxX));
        const float py = std::round(topY);
        const float midY = py + pillH * 0.5f;

        const uint32_t shCol  = ScaleHudAlpha(HudPalette::kPanelShadow140, alphaScale);
        const uint32_t bgColS = ScaleHudAlpha(pillBg, alphaScale);
        const uint32_t bdColS = ScaleHudAlpha(pillBd, alphaScale);
        DrawSlantedPlate(dl, px + 1.5f, py + 2.0f, pillW, pillH, slant, shCol);
        DrawSlantedPlate(dl, px, py, pillW, pillH, slant, bgColS, bdColS, 1.35f);
        DrawSlantedPlate(dl, px, py, 3.4f * uiScale, pillH, slant, bdColS);

        float textStartX = px + padX + 2.0f * uiScale;
        if (hasKey)
        {
            const float kh = (std::max)(12.0f, pillH - RT::kPromptKeyHTrim * uiScale);
            const float ky = py + (pillH - kh) * 0.5f;
            DrawKeycapBadge(dl, textStartX, ky, keyW, kh, 2.5f,
                ScaleHudAlpha(keyBg, alphaScale), ScaleHudAlpha(keyBd, alphaScale), 1.15f,
                keyFontPx, ScaleHudAlpha(keyTx, alphaScale), keyLabel, midY);
            textStartX += keyW + keyGap;
        }
        DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, textFontPx, textStartX, midY, ScaleHudAlpha(textCol, alphaScale), actionText, HudTextAlign::Left);
    }

    void DrawAccentedTag(
        TacticalDrawList* dl, float x, float y, float w, float h, float slant,
        float shadowOffX, float shadowOffY, uint32_t shadowCol,
        uint32_t bgCol, uint32_t borderCol, float borderThick,
        float stripeW, float fontSize, float textShiftX, uint32_t textCol, const char* text,
        uint32_t stripeCol = 0)
    {
        DrawSlantedPlate(dl, x + shadowOffX, y + shadowOffY, w, h, slant, shadowCol);
        DrawSlantedPlate(dl, x, y, w, h, slant, bgCol, borderCol, borderThick);
        if (stripeW > 0.0f) DrawSlantedPlate(dl, x, y, stripeW, h, slant, stripeCol ? stripeCol : borderCol);
        DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, fontSize, x + w * 0.5f + textShiftX, y + h * 0.5f, textCol, text, HudTextAlign::Center);
    }

    void DrawDiamondPip(TacticalDrawList* dl, float cx, float cy, float rad, uint32_t fillCol, uint32_t outlineCol = 0)
    {
        if (!dl || rad <= HudLayout::Common::kMinPipRadius) return;
        const UIVec2 pts[HudLayout::Common::kQuadVertCount] = {
            UIVec2(cx, cy - rad), UIVec2(cx + rad, cy), UIVec2(cx, cy + rad), UIVec2(cx - rad, cy)
        };
        if (((fillCol >> UI_COL32_A_SHIFT) & HudPalette::kChannelMask) > 0)
            dl->AddConvexPolyFilled(pts, HudLayout::Common::kQuadVertCount, fillCol);
        if (((outlineCol >> UI_COL32_A_SHIFT) & HudPalette::kChannelMask) > 0)
            dl->AddPolyline(pts, HudLayout::Common::kQuadVertCount, outlineCol, UIDrawFlags_Closed, HudLayout::Common::kDiamondOutlineThick);
    }

    void DrawPipBadgePlate(
        TacticalDrawList* dl, float x, float y, float w, float h, float slant,
        uint32_t bgCol, uint32_t bdCol, float bdThick,
        float pipOffX, float pipRad, uint32_t pipCol,
        float fontPx, float textOffX, uint32_t textCol, const char* text)
    {
        const float midY = y + h * 0.5f;
        DrawSlantedPlate(dl, x, y, w, h, slant, bgCol, bdCol, bdThick);
        DrawDiamondPip(dl, x + pipOffX, midY, pipRad, pipCol);
        DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, fontPx, x + textOffX, midY, textCol, text, HudTextAlign::Left);
    }

    void DrawCornerBrackets(
        TacticalDrawList* dl, UIVec2 center, float offset, float signedArm,
        uint32_t shadowCol, float shadowThick, uint32_t fgCol, float fgThick)
    {
        constexpr int kCornerSigns[] = { -1, 1 };
        for (int sx : kCornerSigns)
        {
            for (int sy : kCornerSigns)
            {
                const UIVec2 corner(center.x + offset * static_cast<float>(sx), center.y + offset * static_cast<float>(sy));
                const UIVec2 hEnd(corner.x + signedArm * static_cast<float>(sx), corner.y);
                const UIVec2 vEnd(corner.x, corner.y + signedArm * static_cast<float>(sy));
                dl->AddLine(corner, hEnd, shadowCol, shadowThick);
                dl->AddLine(corner, vEnd, shadowCol, shadowThick);
                dl->AddLine(corner, hEnd, fgCol, fgThick);
                dl->AddLine(corner, vEnd, fgCol, fgThick);
            }
        }
    }

    void DrawCheckmarkBadge(TacticalDrawList* dl, float cx, float cy, float rad, bool completed, uint32_t activeCol)
    {
        if (!dl) return;
        namespace C = HudLayout::Common;
        if (completed)
        {
            dl->AddCircleFilled(UIVec2(cx, cy), rad, HudPalette::kCheckDoneFill, C::kCheckCircleSegs);
            dl->AddCircle(UIVec2(cx, cy), rad, HudPalette::kCheckDoneBorder, C::kCheckCircleSegs, C::kCheckCircleThick);
            const UIVec2 p0(cx + rad * C::kCheckP0X, cy + rad * C::kCheckP0Y);
            const UIVec2 p1(cx + rad * C::kCheckP1X, cy + rad * C::kCheckP1Y);
            const UIVec2 p2(cx + rad * C::kCheckP2X, cy + rad * C::kCheckP2Y);
            dl->AddLine(p0, p1, HudPalette::kCheckStroke, C::kCheckStrokeThick);
            dl->AddLine(p1, p2, HudPalette::kCheckStroke, C::kCheckStrokeThick);
        }
        else
        {
            DrawDiamondPip(dl, cx, cy, rad * C::kCheckOuterPipMul, HudPalette::kCheckPendingFill, activeCol);
            DrawDiamondPip(dl, cx, cy, rad * C::kCheckInnerPipMul, activeCol);
        }
    }

    const char* GetSectorNameJP(const Vector3& pos)
    {
        namespace SB = HudLayout::SectorBounds;
        if (pos.z < SB::kLandingPadMaxZ && std::abs(pos.x) < SB::kLandingPadHalfX) return AsUtf8(u8"着陸ヘリパッド");
        if (pos.z >= SB::kBridgeMinZ && pos.z <= SB::kBridgeMaxZ)
        {
            return (pos.x < 0.0f) ? AsUtf8(u8"西の橋") : AsUtf8(u8"東の橋");
        }
        if (pos.z < SB::kSouthMaxZ)
        {
            if (pos.x >= SB::kSmuggleDockMinX && pos.z >= SB::kSmuggleDockMinZ) return AsUtf8(u8"東：密輸ドック");
            if (pos.x >= SB::kShootingRangeMinX) return AsUtf8(u8"東：射撃訓練場");
            if (pos.x <= SB::kCombatYardMaxX) return AsUtf8(u8"西：実戦訓練場");
            return AsUtf8(u8"南：中央回廊");
        }
        if (pos.x < SB::kNorthWestMaxX)
        {
            if (pos.z > SB::kVaultMinZ) return AsUtf8(u8"北西：秘宝庫");
            return AsUtf8(u8"北西：巡回広場");
        }
        if (pos.z > SB::kExtractPadMinZ) return AsUtf8(u8"脱出ヘリパッド");
        return AsUtf8(u8"北東：司令部");
    }

    struct ActiveObjectiveInfo
    {
        Vector3 worldPos        = { 0.0f, 0.0f, 0.0f };
        int activeTargetIndex   = -1;
        int destroyedTargets    = 0;
        int totalTargets        = 0;
        bool hasLootedAny       = false;
        bool isLootObjective    = false;
        bool isExtractObjective = false;
        const char* phaseTag    = "STEP 1 / 5";
        const char* markerLabel = "OBJECTIVE";
        const char* actionLine1 = "";
        const char* actionLine2 = "";
        uint32_t accentCol      = HudPalette::kCyanAccent;
    };

    void ApplyObjectivePhase(ActiveObjectiveInfo& info, const ObjectivePhaseSpec& spec)
    {
        info.phaseTag    = spec.phaseTag;
        info.markerLabel = AsUtf8(spec.markerLabel);
        info.actionLine1 = AsUtf8(spec.actionLine1);
        info.actionLine2 = AsUtf8(spec.actionLine2);
        info.accentCol   = spec.accentCol;
    }

    ActiveObjectiveInfo ResolveActiveObjective(const GamePlayHUDContext& ctx, int prevChosenIdx = -1)
    {
        namespace Obj = HudLayout::Objective;
        ActiveObjectiveInfo info{};
        const Vector3 pPos = ctx.player ? ctx.player->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };

        const LootableProp* starterCrate = nullptr;
        if (ctx.lootProps)
        {
            for (const auto& prop : *ctx.lootProps)
            {
                if (prop.isLooted)
                {
                    info.hasLootedAny = true;
                }
                else if (!starterCrate && prop.position.x > Obj::kStarterCrateMinX && prop.position.z < Obj::kStarterCrateMaxZ)
                {
                    starterCrate = &prop;
                }
            }
            if (!starterCrate && !ctx.lootProps->empty() && !(*ctx.lootProps)[0].isLooted)
            {
                starterCrate = &(*ctx.lootProps)[0];
            }
        }

        if (ctx.targets)
        {
            info.totalTargets = static_cast<int>(ctx.targets->size());
            for (const auto& t : *ctx.targets)
            {
                if (t && t->IsDead()) info.destroyedTargets++;
            }
        }

        // 1. 全標的破壊完了後：北東の緑スモーク脱出ヘリパッドへ誘導
        if (ctx.isReadyToExtract)
        {
            info.isExtractObjective = true;
            info.worldPos = ctx.extractionGoalPos;
            ApplyObjectivePhase(info, ctx.isPlayerInExtractionZone ? Obj::kPhaseExtractInZone : Obj::kPhaseExtractReady);
            return info;
        }

        // 2. 開始直後（未回収 ＆ 標的未破壊）：東ウイング入口の初期弾薬箱で [E] 物資回収を案内
        if (!info.hasLootedAny && info.destroyedTargets == 0 && starterCrate &&
            pPos.z < Obj::kStarterGuideMaxZ && pPos.x > Obj::kStarterGuideMinX)
        {
            info.isLootObjective = true;
            info.worldPos = starterCrate->position;
            ApplyObjectivePhase(info, Obj::kPhaseStarterLoot);
            return info;
        }

        // 3. 残存標的の選択（推奨チュートリアル順または現在地から最適な標的へ誘導）
        int chosenIdx = -1;
        if (ctx.targets && !ctx.targets->empty())
        {
            auto isAlive = [&](int idx) -> bool {
                return idx >= 0 && idx < static_cast<int>(ctx.targets->size()) &&
                       (*ctx.targets)[idx] && !(*ctx.targets)[idx]->IsDead();
            };

            if (isAlive(Obj::kTargetIdxEastRange) &&
                (info.destroyedTargets == 0 || (pPos.z < Obj::kTarget0ZoneMaxZ && pPos.x >= Obj::kTarget0ZoneMinX)))
            {
                chosenIdx = Obj::kTargetIdxEastRange; // 東：射撃訓練場
            }
            else if (isAlive(Obj::kTargetIdxWestYard) && pPos.z < Obj::kTarget1ZoneMaxZ)
            {
                chosenIdx = Obj::kTargetIdxWestYard; // 西：実戦訓練ヤード
            }
            else
            {
                float bestScore = Obj::kMaxSearchDistSq;
                for (int i = 0; i < static_cast<int>(ctx.targets->size()); ++i)
                {
                    if (!isAlive(i)) continue;
                    const Vector3 tp = (*ctx.targets)[i]->GetPosition();
                    const float dx = tp.x - pPos.x;
                    const float dz = tp.z - pPos.z;
                    const float dist = std::sqrt(dx * dx + dz * dz);
                    // 等距離ライン付近での標的[3]⇔[4]の高速チラつきを防ぐヒステリシス補正
                    const float score = (i == prevChosenIdx) ? (dist - 3.0f) : dist;
                    if (score < bestScore)
                    {
                        bestScore = score;
                        chosenIdx = i;
                    }
                }
            }

            if (chosenIdx >= 0)
            {
                info.activeTargetIndex = chosenIdx;
                info.worldPos = (*ctx.targets)[chosenIdx]->GetPosition();
            }
        }

        if (chosenIdx == Obj::kTargetIdxEastRange)
        {
            ApplyObjectivePhase(info, Obj::kPhaseTarget1);
        }
        else if (chosenIdx == Obj::kTargetIdxWestYard)
        {
            ApplyObjectivePhase(info, Obj::kPhaseTarget2);
        }
        else if (pPos.z < Obj::kBridgePlayerMaxZ && info.worldPos.z > Obj::kBridgeTargetMinZ)
        {
            ApplyObjectivePhase(info, (chosenIdx == Obj::kTargetIdxNorthWest) ? Obj::kPhaseBridgeTo3 : Obj::kPhaseBridgeTo4);
        }
        else if (chosenIdx == Obj::kTargetIdxNorthWest)
        {
            ApplyObjectivePhase(info, Obj::kPhaseTarget3);
        }
        else
        {
            ApplyObjectivePhase(info, Obj::kPhaseTarget4);
        }

        return info;
    }

    bool ProjectWorldToScreen(const Vector3& pos3D, const Matrix4x4& vp, float screenW, float screenH, Vector2& outPos)
    {
        namespace C = HudLayout::Common;
        const float w = pos3D.x * vp.m[C::kMatX][C::kMatW] + pos3D.y * vp.m[C::kMatY][C::kMatW] +
                        pos3D.z * vp.m[C::kMatZ][C::kMatW] + vp.m[C::kMatW][C::kMatW];
        if (w <= 0.0f) return false;
        const float x = (pos3D.x * vp.m[C::kMatX][C::kMatX] + pos3D.y * vp.m[C::kMatY][C::kMatX] +
                         pos3D.z * vp.m[C::kMatZ][C::kMatX] + vp.m[C::kMatW][C::kMatX]) / w;
        const float y = (pos3D.x * vp.m[C::kMatX][C::kMatY] + pos3D.y * vp.m[C::kMatY][C::kMatY] +
                         pos3D.z * vp.m[C::kMatZ][C::kMatY] + vp.m[C::kMatW][C::kMatY]) / w;
        outPos.x = (x + 1.0f) * 0.5f * screenW;
        outPos.y = (1.0f - y) * 0.5f * screenH;
        return true;
    }
}

void GamePlayHUD::Initialize()
{
    TextureManager* tm       = TextureManager::GetInstance();
    medkitTextureIndex_      = tm->Load("Resources/item_medkit.jpg");
    ammoTextureIndex_        = tm->Load("Resources/item_ammo.jpg");
    goldDuckTextureIndex_    = tm->Load("Resources/item_gold_duck.jpg");
    roublesTextureIndex_     = tm->Load("Resources/item_roubles.jpg");
    cineAtlasTextureIndex_   = tm->Load("Resources/gameplay_cine_atlas.png");
    hudAtlasTextureIndex_    = tm->Load("Resources/gameplay_hud_atlas.png");
    clearDigitsTextureIndex_ = tm->Load("Resources/clear_digits_atlas.png");
    displayedPlayerHpLag_    = 1.0f;
    cachedGuidePath_.clear();
    smoothGuidePath_.clear();
    scratchGuidePath_.clear();
    cachedGuidePath_.reserve(HudLayout::Common::kGuidePathReserve);
    smoothGuidePath_.reserve(HudLayout::Common::kSmoothPathReserve);
    scratchGuidePath_.reserve(HudLayout::Common::kSmoothPathReserve);
    cachedGuideGoal_         = { 0.0f, 0.0f, 0.0f };
    guidePathRefreshTimer_   = 0.0f;
    smoothedOffscreenDir_    = { 0.0f, -1.0f };
    hasOffscreenDir_         = false;
    lastActiveTargetIdx_     = -1;
}

bool GamePlayHUD::Project3DTo2D(const Vector3& pos3D, const Matrix4x4& vp, float screenW, float screenH, Vector2& outPos)
{
    return ProjectWorldToScreen(pos3D, vp, screenW, screenH, outPos);
}

void GamePlayHUD::Draw(const GamePlayHUDContext& ctx, float deltaTime)
{
    // トランジション実行中は全HUDを完全に非表示
    if (StingerTransition::GetInstance() && StingerTransition::GetInstance()->IsTransitioning()) return;
    if (DeathTransition::GetInstance() && DeathTransition::GetInstance()->IsTransitioning()) return;

    const UIVec2 ioDisplaySize = TacticalUICanvas::GetInstance()->GetDisplaySize();
    const float screenW = ioDisplaySize.x;
    const float screenH = ioDisplaySize.y;
    if (screenW <= 0.0f || screenH <= 0.0f) return;

    // 生還クライマックス演出中は戦闘HUDを退避させ、シネマティック脱出レイヤーのみを描画
    if (ctx.isGameCleared)
    {
        DrawExtractionSequenceHUD(ctx, deltaTime);
        return;
    }

    // 各種タクティカルHUDレイヤーの順番描画
    laserLockedOnTarget_ = false;
    DrawLowHpRedVignetteEffect(ctx);
    DrawNavigationGuideHUD(ctx, deltaTime);
    DrawEnemyTacticalHUD(ctx);
    DrawLaserSight(ctx);
    DrawDamageIndicator(ctx);
    DrawLootingHUD(ctx);
    DrawFloatingTexts(ctx);
    DrawVisionConesAndGizmos(ctx);
    DrawExtractionSequenceHUD(ctx, deltaTime);
    DrawMissionObjectiveHUD(ctx);
    DrawQuestTrackerHUD(ctx);
    DrawTutorialSignHUD(ctx);
    DrawPlayerVitalsHUD(ctx, deltaTime);
    DrawControlsGuideHUD(ctx);
    DrawPlayerAmmoHUD(ctx, deltaTime);
    DrawTacticalMapHUD(ctx);
    DrawDeathSequenceHUD(ctx);
    DrawPerformanceTrackerUI(ctx, deltaTime);
    DrawTacticalReticle(ctx);

    // シーン入場時のスムーズなフェードイン演出 (0.75秒)
    if (ctx.sceneEntranceFadeTimer > 0.0f)
    {
        TacticalDrawList* drawList = TacticalUICanvas::GetInstance()->GetForegroundDrawList();
        if (drawList)
        {
            const float alpha = (std::clamp)(ctx.sceneEntranceFadeTimer / HudTiming::kSceneEntranceFadeSec, 0.0f, 1.0f);
            const int aVal = static_cast<int>(HudPalette::kMaxAlphaFloat * alpha);
            drawList->AddRectFilled(
                UIVec2(0.0f, 0.0f), UIVec2(screenW, screenH),
                UI_COL32(HudPalette::kEntranceFadeR, HudPalette::kEntranceFadeG, HudPalette::kEntranceFadeB, aVal));
        }
    }
}

// =============================================================================
// 作戦目標 & レイドタイマー タクティカルヘッダー HUD (画面上部中央)
// =============================================================================

void GamePlayHUD::DrawMissionObjectiveHUD(const GamePlayHUDContext& ctx)
{
    if (!ctx.player || ctx.player->IsDead() || ctx.isDeathSequenceActive) return;

    HudViewport vp{};
    if (!AcquireHudViewport(vp)) return;
    TacticalDrawList* dl = vp.dl;
    const float uiScale  = vp.uiScale;
    namespace MH = HudLayout::MissionHeader;

    int totalTargets = 0, destroyedCount = 0;
    if (ctx.targets)
    {
        totalTargets = static_cast<int>(ctx.targets->size());
        for (const auto& t : *ctx.targets)
            if (t && t->IsDead()) destroyedCount++;
    }

    const Vector3 diff          = ctx.extractionGoalPos - ctx.player->GetPosition();
    const float distToExtract   = std::sqrt(diff.x * diff.x + diff.z * diff.z);
    const float remTime         = RaidStats::GetInstance().GetRemainingTime();
    const int remMin            = (std::max)(0, static_cast<int>(remTime) / HudTiming::kSecondsPerMinute);
    const int remSec            = (std::max)(0, static_cast<int>(remTime) % HudTiming::kSecondsPerMinute);
    const float timeNow         = static_cast<float>(TacticalUICanvas::GetInstance()->GetTime());

    // スリム・高視認タクティカルレイドヘッダー (456 x 38：1行ボールド文字＋高コントラスト暗色プレートで潰れゼロ)
    const float hudW  = MH::kWidth * uiScale;
    const float hudH  = MH::kHeight * uiScale;
    const float slant = MH::kSlant * uiScale;
    const float hudX  = std::round((vp.screenW - hudW) * 0.5f);
    const float hudY  = std::round(MH::kTopY * uiScale);
    const float midY  = hudY + hudH * 0.5f;

    uint32_t accentCol = ctx.isReadyToExtract ? HudPalette::kEmeraldReady : HudPalette::kGoldAccent;
    if (remTime < HudTiming::kUrgentTimeSec)
        accentCol = (std::sin(timeNow * HudTiming::kUrgentBlinkSpeed) > 0.0f) ? MH::kUrgentBlinkHi : MH::kUrgentBlinkLo;
    else if (remTime < HudTiming::kWarnTimeSec && !ctx.isReadyToExtract)
        accentCol = MH::kWarnAccent;

    // 背面シャドウ ＆ 高コントラスト暗色ガンメタル傾斜プレート（背景の川や白波が透けて文字が潰れるのを防止）
    DrawFramedPanel(dl, hudX, hudY, hudW, hudH, slant, MH::kPanelStyle);
    // 上端タクティカルアクセントライン
    dl->AddLine(UIVec2(hudX + slant, hudY), UIVec2(hudX + hudW + slant, hudY), accentCol, MH::kTopAccentThick * uiScale);

    // 3コンパートメント区切り線 (残り時間 | 標的 | 脱出状況)
    const float div1X = hudX + MH::kDiv1OffsetX * uiScale;
    const float div2X = hudX + MH::kDiv2OffsetX * uiScale;
    for (float divX : { div1X, div2X })
    {
        dl->AddLine(UIVec2(divX + slant * MH::kDivTopSlantMul, hudY + MH::kDivPadY * uiScale),
                    UIVec2(divX + slant * MH::kDivBotSlantMul, hudY + hudH - MH::kDivPadY * uiScale), MH::kDividerCol, MH::kDivThick);
    }

    // 1. 左セクション：残り時間 + 04:50
    const float lblFont = MH::kLabelFont * uiScale;
    DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, lblFont, hudX + MH::kLeftLabelPadX * uiScale, midY, MH::kTimeLabelCol, AsUtf8(u8"残り時間"), HudTextAlign::Left);

    char timeBuf[HudLayout::Common::kShortBufSize];
    std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", remMin, remSec);
    const uint32_t timeCol = (remTime < HudTiming::kUrgentTimeSec) ? MH::kTimeUrgentCol
                           : ((remTime < HudTiming::kWarnTimeSec) ? MH::kTimeWarnCol : HudPalette::kTextIceWhite);
    DrawShadowedTextAligned(dl, UIFontRole::TitleBoldJP, MH::kTimerFont * uiScale, div1X - MH::kTimerRightPadX * uiScale, midY, timeCol, timeBuf, HudTextAlign::Right);

    // 2. 中央セクション：標的 + ダイヤピップ + 0/4
    DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, lblFont, div1X + MH::kMidLabelPadX * uiScale, midY, MH::kTargetLabelCol, AsUtf8(u8"標的"), HudTextAlign::Left);

    const int pipCount     = (std::clamp)(totalTargets, 0, MH::kMaxTargetPips);
    const float pipStartX  = div1X + MH::kPipStartX * uiScale;
    const float pipSpacing = MH::kPipSpacing * uiScale;
    for (int i = 0; i < pipCount; ++i)
    {
        const float px = pipStartX + static_cast<float>(i) * pipSpacing;
        if (i < destroyedCount)
        {
            DrawDiamondPip(dl, px, midY, MH::kPipDoneRadius * uiScale, MH::kPipDoneFill, MH::kPipDoneOutline);
        }
        else
        {
            DrawDiamondPip(dl, px, midY, MH::kPipOpenOuterRad * uiScale, MH::kPipOpenFill, MH::kPipOpenOutline);
            DrawDiamondPip(dl, px, midY, MH::kPipOpenInnerRad * uiScale, MH::kPipOpenCore);
        }
    }

    char tgtBuf[HudLayout::Common::kShortBufSize];
    std::snprintf(tgtBuf, sizeof(tgtBuf), "%d/%d", destroyedCount, totalTargets);
    DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, MH::kTargetCountFont * uiScale, div2X - MH::kTargetRightPadX * uiScale, midY,
        ctx.isReadyToExtract ? MH::kTargetReadyCol : MH::kTargetActiveCol, tgtBuf, HudTextAlign::Right);

    // 3. 右セクション：脱出ロック / 脱出開放 + 距離
    DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, lblFont, div2X + MH::kRightLabelPadX * uiScale, midY,
        ctx.isReadyToExtract ? MH::kLzOpenLabelCol : MH::kLzLockLabelCol,
        ctx.isReadyToExtract ? AsUtf8(u8"脱出開放") : AsUtf8(u8"脱出ロック"), HudTextAlign::Left);

    char distBuf[HudLayout::Common::kShortBufSize];
    std::snprintf(distBuf, sizeof(distBuf), "%.0fm", distToExtract);
    DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, MH::kDistFont * uiScale, hudX + hudW - MH::kDistRightPadX * uiScale, midY,
        ctx.isReadyToExtract ? MH::kLzOpenDistCol : MH::kLzLockDistCol, distBuf, HudTextAlign::Right);

    // 4. [F7] 自動デモプレイ稼働バナー ＆ [RMB] バレットタイム表示（上部ヘッダー直下）
    float subBannerY = hudY + hudH + 7.0f * uiScale;
    if (ctx.isAutoDemoPlay)
    {
        const bool isClearPass = ((ctx.autoDemoCycleIndex & 1) != 0);
        const float pulse = 0.5f + 0.5f * std::sin(timeNow * 4.5f);
        const char* mainTitle = AsUtf8(u8"DEMO PLAY  //  自動デモプレイ稼働中");
        const char* subTitle  = isClearPass
            ? AsUtf8(u8"【2/2周目：クリア実演】全標的制圧〜ヘリ脱出   |   [F7] キーで自分でプレイする")
            : AsUtf8(u8"【1/2周目：敗北・救難演出】続いて2周目でクリア実演   |   [F7] キーで自分でプレイする");

        const float mainFont = 19.0f * uiScale;
        const float subFont  = 12.5f * uiScale;
        const UIVec2 mSz     = MeasureSharpText(UIFontRole::TitleBoldJP, mainFont, mainTitle);
        const UIVec2 sSz     = MeasureSharpText(UIFontRole::SubBoldJP, subFont, subTitle);
        const float dW       = (std::max)(mSz.x + 56.0f * uiScale, sSz.x + 36.0f * uiScale);
        const float dH       = 48.0f * uiScale;
        const float dSlant   = 8.0f * uiScale;
        const float dX       = std::round((vp.screenW - dW) * 0.5f);
        const uint32_t bdCol = isClearPass
            ? UI_COL32(70, 245, 185, static_cast<int>(200.0f + 55.0f * pulse))
            : UI_COL32(255, 195, 75, static_cast<int>(200.0f + 55.0f * pulse));
        const uint32_t recCol = UI_COL32(255, 75, 65, static_cast<int>(150.0f + 105.0f * pulse));

        DrawSlantedPlate(dl, dX + 2.5f, subBannerY + 3.0f, dW, dH, dSlant, HudPalette::kPanelShadow140);
        DrawSlantedPlate(dl, dX, subBannerY, dW, dH, dSlant, UI_COL32(8, 16, 26, 242), bdCol, 1.8f);
        dl->AddLine(UIVec2(dX + dSlant, subBannerY), UIVec2(dX + dW + dSlant, subBannerY), bdCol, 2.6f * uiScale);

        // 点滅インジケータードット ＋ 大見出し「DEMO PLAY // 自動デモプレイ稼働中」
        const float row1Y = subBannerY + 15.5f * uiScale;
        const float row2Y = subBannerY + 35.5f * uiScale;
        dl->AddCircleFilled(UIVec2(vp.screenW * 0.5f - mSz.x * 0.5f - 12.0f * uiScale, row1Y), 5.0f * uiScale, recCol, 14);
        DrawShadowedTextAligned(dl, UIFontRole::TitleBoldJP, mainFont, vp.screenW * 0.5f + 4.0f * uiScale, row1Y,
            UI_COL32(255, 242, 165, 255), mainTitle, HudTextAlign::Center);
        DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, subFont, vp.screenW * 0.5f, row2Y,
            UI_COL32(225, 244, 255, 250), subTitle, HudTextAlign::Center);

        subBannerY += dH + 6.0f * uiScale;
    }
    if (ctx.isBulletTimeActive)
    {
        const char* btTxt = AsUtf8(u8"BULLET TIME // 0.25x スロー鑑賞中");
        const float bFont = 12.5f * uiScale;
        const UIVec2 bSz  = MeasureSharpText(UIFontRole::SubBoldJP, bFont, btTxt);
        const float bW    = bSz.x + 28.0f * uiScale;
        const float bH    = 22.0f * uiScale;
        const float bX    = std::round((vp.screenW - bW) * 0.5f);
        DrawAccentedTag(dl, bX, subBannerY, bW, bH, 5.0f * uiScale,
            2.0f, 2.0f, HudPalette::kPanelShadow120,
            UI_COL32(14, 28, 42, 226), UI_COL32(255, 210, 85, 235), 1.3f,
            4.0f * uiScale, bFont, 12.0f * uiScale, UI_COL32(255, 236, 165, 255), btTxt, UI_COL32(80, 230, 255, 255));
    }
}

// =============================================================================
// オペレーター耐久値・回避スタミナ・遮蔽ステルス HUD (画面左下)
// =============================================================================

void GamePlayHUD::DrawPlayerVitalsHUD(const GamePlayHUDContext& ctx, float deltaTime)
{
    if (!ctx.player || ctx.player->IsDead() || ctx.isDeathSequenceActive) return;

    HudViewport vp{};
    if (!AcquireHudViewport(vp)) return;
    TacticalDrawList* dl = vp.dl;
    const float uiScale  = vp.uiScale;
    namespace SP = HudLayout::StatusPanel;

    const UITextureID digitsTex = ResolveHudTexture(clearDigitsTextureIndex_);
    const float hpRatio   = (std::clamp)(ctx.player->GetHPRatio(), 0.0f, 1.0f);
    const float stamRatio = (std::clamp)(ctx.player->GetStaminaRatio(), 0.0f, 1.0f);
    const int curHp       = static_cast<int>(std::ceil(ctx.player->GetHP()));
    const int maxHp       = static_cast<int>(std::round(ctx.player->GetMaxHP()));

    // 被弾時の白遅延ダメージゲージ追従
    displayedPlayerHpLag_ = (hpRatio < displayedPlayerHpLag_)
        ? (std::max)(hpRatio, displayedPlayerHpLag_ - HudTiming::kHpLagDecayRate * deltaTime)
        : hpRatio;

    const float panelW  = SP::kWidth * uiScale;
    const float panelH  = SP::kHeight * uiScale;
    const float slant   = SP::kSlant * uiScale;
    const float panelX  = std::round(SP::kMarginX * uiScale);
    const float panelY  = std::round(vp.screenH - panelH - SP::kMarginBottom * uiScale);
    const float timeNow = static_cast<float>(TacticalUICanvas::GetInstance()->GetTime());
    const float pulse   = 0.5f + 0.5f * std::sin(timeNow * HudTiming::kVitalsPulseSpeed);

    // 遮蔽物カバー中はプレート上部にエメラルドの STEALTH ピルバッジを表示
    if (ctx.player->IsInCover())
    {
        const char* stealthTxt = AsUtf8(u8"STEALTH // 遮蔽中");
        const float stFont     = SP::kStealthFont * uiScale;
        const UIVec2 stSz      = MeasureSharpText(UIFontRole::SubBoldJP, stFont, stealthTxt);
        const float badgeH     = SP::kStealthHeight * uiScale;
        DrawPipBadgePlate(dl, panelX + SP::kStealthOffsetX * uiScale, panelY - badgeH - SP::kStealthGapY * uiScale,
            stSz.x + SP::kStealthPadW * uiScale, badgeH, SP::kStealthSlant * uiScale,
            SP::kStealthBg, SP::kStealthBorder, SP::kStealthBorderThick,
            SP::kStealthPipX * uiScale, SP::kStealthPipRad * uiScale, SP::kStealthPipCol,
            stFont, SP::kStealthTextX * uiScale, SP::kStealthTextCol, stealthTxt);
    }

    // ガンメタル装甲プレート ＆ 左端ゴールドストライプ
    DrawFramedPanel(dl, panelX, panelY, panelW, panelH, slant, SP::kPanelStyle);
    DrawSlantedPlate(dl, panelX, panelY, SP::kStripeWidth * uiScale, panelH, slant,
        (hpRatio <= SP::kCritHpRatio) ? HudPalette::kDangerRed : HudPalette::kGoldAccent);

    // 1. HP / 耐久 ラベル + 100/100 数値
    DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, SP::kHpLabelFont * uiScale,
        panelX + SP::kLabelOffsetX * uiScale, panelY + SP::kHpHeaderY * uiScale, SP::kHpLabelCol, AsUtf8(u8"HP / 耐久"), HudTextAlign::Left);

    char hpBuf[HudLayout::Common::kSmallBufSize];
    std::snprintf(hpBuf, sizeof(hpBuf), "%d/%d", curHp, maxHp);
    const uint32_t hpNumCol = (hpRatio <= SP::kCritHpRatio) ? SP::kHpNumCritCol
                            : ((hpRatio <= SP::kWarnHpRatio) ? SP::kHpNumWarnCol : SP::kHpNumFineCol);
    DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, SP::kHpNumFont * uiScale,
        panelX + panelW - SP::kHpNumRightPad * uiScale, panelY + SP::kHpHeaderY * uiScale, hpNumCol, hpBuf, HudTextAlign::Right);

    // HP スラントゲージ（白遅延ダメージバー ＋ メインアーマーバー）
    const float barX = panelX + SP::kBarOffsetX * uiScale;
    const float barY = panelY + SP::kHpBarY * uiScale;
    const float barW = panelW - SP::kBarWidthTrim * uiScale;
    const uint32_t hpFill = (hpRatio <= SP::kCritHpRatio) ? SP::kHpBarCritCol
                          : ((hpRatio <= SP::kWarnHpRatio) ? SP::kHpBarWarnCol : SP::kHpBarFineCol);
    DrawProgressBar(dl, barX, barY, barW, SP::kHpBarH * uiScale, SP::kHpBarSlant * uiScale,
        hpRatio, SP::kHpBarBg, hpFill, SP::kHpBarBorder, SP::kHpBarBorderThick, displayedPlayerHpLag_, SP::kHpBarLagCol);

    // 2. STAMINA / 回避 ラベル + 回避スタミナスラントバー (30%目盛り付き)
    DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, SP::kStamLabelFont * uiScale,
        panelX + SP::kLabelOffsetX * uiScale, panelY + SP::kStamLabelY * uiScale, SP::kStamLabelCol, AsUtf8(u8"STAMINA / 回避"), HudTextAlign::Left);

    const float sBarY  = panelY + SP::kStamBarY * uiScale;
    const float sBarH  = SP::kStamBarH * uiScale;
    const float sSlant = SP::kStamBarSlant * uiScale;
    DrawProgressBar(dl, barX, sBarY, barW, sBarH, sSlant, stamRatio, SP::kStamBarBg,
        (stamRatio < SP::kDodgeStamRatio) ? SP::kStamLowCol : SP::kStamNormalCol);

    const float dodgeTickX = barX + barW * SP::kDodgeStamRatio;
    dl->AddLine(UIVec2(dodgeTickX + sSlant, sBarY - SP::kDodgeTickPadY * uiScale),
                UIVec2(dodgeTickX, sBarY + sBarH + SP::kDodgeTickPadY * uiScale), SP::kDodgeTickCol, SP::kDodgeTickThick);

    // =========================================================================
    // 3. Escape from Duckov / Tarkov スタイル クイックスロット・ベルト ([Q] 救急キット / [C] 銃収納速歩)
    //    バイタル右隣にコンパクトな正方形タクティカルスロットとして集約し、画面下部中央を開放！
    // =========================================================================
    const float slotW  = SP::kSlotWidth * uiScale;
    const float slotH  = SP::kSlotHeight * uiScale;
    const float slotY  = panelY + (panelH - slotH);
    const float slot1X = panelX + panelW + SP::kSlotPanelGap * uiScale;
    const float slot2X = slot1X + slotW + SP::kSlotGap * uiScale;

    auto drawQuickSlotCard = [&](float sx, uint32_t bgCol, uint32_t bdCol, bool active, const char* keyLabel, float risingFillRatio) {
        dl->AddRectFilled(UIVec2(sx + SP::kSlotShadowOffX, slotY + SP::kSlotShadowOffY),
                          UIVec2(sx + slotW + SP::kSlotShadowOffX, slotY + slotH + SP::kSlotShadowOffY),
                          HudPalette::kSlotShadow115, SP::kSlotRounding);
        dl->AddRectFilled(UIVec2(sx, slotY), UIVec2(sx + slotW, slotY + slotH), bgCol, SP::kSlotRounding);
        if (risingFillRatio > SP::kHealMinProg)
        {
            dl->AddRectFilled(UIVec2(sx + SP::kHealFillInset, slotY + slotH - slotH * risingFillRatio),
                              UIVec2(sx + slotW - SP::kHealFillInset, slotY + slotH - SP::kHealFillInset),
                              SP::kMedHealFill, SP::kHealFillRounding);
        }
        dl->AddRect(UIVec2(sx, slotY), UIVec2(sx + slotW, slotY + slotH),
                    bdCol, SP::kSlotRounding, UIDrawFlags_None, active ? SP::kSlotActiveThick : SP::kSlotIdleThick);

        DrawKeycapBadge(dl, sx + SP::kKeycapMin * uiScale, slotY + SP::kKeycapMin * uiScale,
            (SP::kKeycapMaxX - SP::kKeycapMin) * uiScale, (SP::kKeycapMaxY - SP::kKeycapMin) * uiScale,
            SP::kKeycapRounding, SP::kKeycapBg, bdCol, SP::kKeycapBorderThick,
            SP::kKeycapFont * uiScale, SP::kKeycapTextCol, keyLabel, slotY + SP::kKeycapCenterY * uiScale);
    };

    // --- Slot [Q]: MEDKIT 応急処置スロット ---
    {
        const int medkits   = ctx.player->GetMedkitCount();
        const bool healing  = ctx.player->IsHealing();
        const float healPrg = (std::clamp)(ctx.player->GetHealProgress(), 0.0f, 1.0f);
        const bool urgeHeal = (!healing && hpRatio < SP::kUrgeHealHpRatio && medkits > 0);

        const uint32_t sBg = healing ? SP::kMedActiveBg : HudPalette::kGunmetalBg234;
        const uint32_t sBd = healing ? SP::kMedActiveBorder
            : (urgeHeal ? UI_COL32(SP::kMedUrgeR, SP::kMedUrgeG, SP::kMedUrgeB, static_cast<int>(SP::kUrgeAlphaBase + SP::kUrgeAlphaPulse * pulse))
                        : ((medkits > 0) ? SP::kMedReadyBorder : SP::kMedEmptyBorder));

        drawQuickSlotCard(slot1X, sBg, sBd, healing, "Q", healing ? healPrg : 0.0f);

        const UITextureID medTex = ResolveHudTexture(medkitTextureIndex_);
        if (medTex != 0)
        {
            const float icSz = SP::kMedIconSize * uiScale;
            const UIVec2 iMin(slot1X + (slotW - icSz) * 0.5f, slotY + SP::kMedIconTopY * uiScale);
            dl->AddImageRounded(medTex, iMin, UIVec2(iMin.x + icSz, iMin.y + icSz), UIVec2(0.0f, 0.0f), UIVec2(1.0f, 1.0f),
                (medkits > 0 || healing) ? SP::kMedIconActive : SP::kMedIconEmpty, SP::kMedIconRounding);
        }

        // 右下残数 または 治療%
        char qSub[HudLayout::Common::kShortBufSize];
        const float subX = slot1X + slotW - SP::kMedSubRightPad * uiScale;
        const float subY = slotY + slotH - SP::kMedSubBotPad * uiScale;
        if (healing)
        {
            std::snprintf(qSub, sizeof(qSub), "%d%%", static_cast<int>(healPrg * HudLayout::Common::kPercentMul));
            DrawHudAtlasDigits(dl, digitsTex, qSub, subX, subY, SP::kMedPctHeight * uiScale, SP::kMedPctTextCol, HudTextAlign::Right);
        }
        else
        {
            std::snprintf(qSub, sizeof(qSub), "x%d", medkits);
            DrawHudAtlasDigits(dl, digitsTex, qSub, subX, subY, SP::kMedCountHeight * uiScale,
                (medkits > 0) ? SP::kMedHasCountCol : SP::kMedZeroCountCol, HudTextAlign::Right);
        }
    }

    // --- Slot [C]: HOLSTER SPRINT 銃収納ダッシュ切替スロット ---
    {
        const bool holstered = ctx.player->IsWeaponHolstered();
        drawQuickSlotCard(slot2X, holstered ? SP::kHolsterActiveBg : HudPalette::kGunmetalBg234,
                          holstered ? SP::kHolsterActiveBd : SP::kHolsterIdleBd, holstered, "C", 0.0f);

        // 中央ベクターアイコン：斜めスリング銃 ＋ 高速シェブロン `>>`
        const float icCx     = slot2X + slotW * 0.5f;
        const float icCy     = slotY + SP::kHolsterIconCy * uiScale;
        const uint32_t icCol = holstered ? SP::kHolsterIconHi : SP::kHolsterIconIdle;
        dl->AddLine(UIVec2(icCx + SP::kSlingX0 * uiScale, icCy + SP::kSlingY0 * uiScale),
                    UIVec2(icCx + SP::kSlingX1 * uiScale, icCy + SP::kSlingY1 * uiScale), icCol, SP::kSlingThick * uiScale);
        dl->AddLine(UIVec2(icCx + SP::kGripX0 * uiScale, icCy + SP::kGripY0 * uiScale),
                    UIVec2(icCx + SP::kGripX1 * uiScale, icCy + SP::kGripY1 * uiScale), icCol, SP::kGripThick * uiScale);
        for (int ch = 0; ch < SP::kSprintChevronCount; ++ch)
        {
            const float ox = icCx + (SP::kChevronStartX + static_cast<float>(ch) * SP::kChevronStepX) * uiScale;
            const UIVec2 tip(ox + SP::kChevronSpanX * uiScale, icCy + SP::kChevronMidY * uiScale);
            dl->AddLine(UIVec2(ox, icCy + SP::kChevronTopY * uiScale), tip, icCol, SP::kChevronThick * uiScale);
            dl->AddLine(tip, UIVec2(ox, icCy + SP::kChevronBotY * uiScale), icCol, SP::kChevronThick * uiScale);
        }

        DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, SP::kHolsterSubFont * uiScale,
            slot2X + slotW * 0.5f, slotY + slotH - SP::kHolsterSubBotPad * uiScale,
            holstered ? SP::kHolsterTextHi : SP::kHolsterTextIdle,
            holstered ? "+30%" : AsUtf8(u8"速歩"), HudTextAlign::Center);
    }

    // =========================================================================
    // 4. プレイヤー足元のタクティカル・ラジアル進捗リング（リロード文字表記はレティクル直下に一本化して重複を解消）
    // =========================================================================
    if (ctx.camera && !ctx.isFullMapOpen && (ctx.player->IsReloading() || ctx.player->IsHealing()))
    {
        Vector2 feet2D{};
        if (Project3DTo2D(ctx.player->GetPosition() + Vector3{ 0.0f, SP::kFeetOffsetY, 0.0f },
                          ctx.camera->GetViewProjectionMatrix(), vp.screenW, vp.screenH, feet2D))
        {
            const bool isRel   = ctx.player->IsReloading();
            const float prg    = (std::clamp)(isRel ? ctx.player->GetReloadProgress() : ctx.player->GetHealProgress(), 0.0f, 1.0f);
            const float ringR  = SP::kFeetRingRadius * uiScale;
            const float aStart = -kPi * 0.5f;
            const UIVec2 feetCenter(feet2D.x, feet2D.y);

            dl->AddCircle(feetCenter, ringR, SP::kFeetBgCol, SP::kFeetBgSegs, SP::kFeetBgThick * uiScale);
            if (prg > SP::kFeetMinProg)
            {
                const uint32_t rCol = !isRel ? SP::kFeetHealCol
                    : ((prg < HudTiming::kReloadEjectEnd) ? SP::kFeetRelEjectCol
                    : ((prg < HudTiming::kReloadSeatTickRatio) ? SP::kFeetRelInsCol : SP::kFeetRelBoltCol));
                dl->PathArcTo(feetCenter, ringR, aStart, aStart + prg * kTwoPi, SP::kFeetArcSegs);
                dl->PathStroke(rCol, UIDrawFlags_None, SP::kFeetArcThick * uiScale);
            }
        }
    }
}

// =============================================================================
// 脱出シーケンス演出 HUD (3Dヘリパッド境界リング・開放アラート・カウントダウン・離脱完了シネマ)
// =============================================================================

void GamePlayHUD::DrawExtractionSequenceHUD(const GamePlayHUDContext& ctx, float /*deltaTime*/)
{
    if (!ctx.player || ctx.isDeathSequenceActive) return;

    HudViewport vpInfo{};
    if (!AcquireHudViewport(vpInfo, HudLayout::Common::kExtractionScaleMin, HudLayout::Common::kExtractionScaleMax)) return;
    TacticalDrawList* drawList = vpInfo.dl;
    const float screenW = vpInfo.screenW;
    const float screenH = vpInfo.screenH;
    const float uiScale = vpInfo.uiScale;
    namespace EB = HudLayout::ExtractionBanner;

    const UITextureID cineTex   = ResolveHudTexture(cineAtlasTextureIndex_);
    const UITextureID digitsTex = ResolveHudTexture(clearDigitsTextureIndex_);

    auto drawCineSprite = [&](const AtlasSpriteRect& rect, UIVec2 dMin, UIVec2 dMax, uint32_t tint) {
        DrawHudAtlasSprite(drawList, cineTex, EB::kAtlasSize, EB::kAtlasSize, rect, dMin, dMax, tint);
    };

    auto drawRadioSubtitle = [&](const char* subTxt, float padW, float bottomMarginY, uint32_t bgCol, uint32_t bdCol, uint32_t txCol) {
        const float subFont = EB::kSubFont * uiScale;
        const UIVec2 sSz    = MeasureSharpText(UIFontRole::SubBoldJP, subFont, subTxt);
        const float plateW  = sSz.x + padW * uiScale;
        const float plateH  = sSz.y + EB::kSubPadH * uiScale;
        DrawAccentedTag(drawList, (screenW - plateW) * 0.5f, screenH - plateH - bottomMarginY, plateW, plateH,
            EB::kSubSlant * uiScale, 0.0f, 0.0f, 0, bgCol, bdCol, EB::kSubBorderThick, 0.0f, subFont, 0.0f, txCol, subTxt);
    };

    const float timeNow = static_cast<float>(TacticalUICanvas::GetInstance()->GetTime());

    // =========================================================================
    // A. 生還クライマックス演出 (isGameCleared == true: シネマスコープ ＆ 回収完了バナー)
    // =========================================================================
    if (ctx.isGameCleared)
    {
        const float p = (std::clamp)(ctx.clearClimaxProgress, 0.0f, 1.0f);
        if (p < HudTiming::kClimaxFlashEnd)
        {
            const float fAlpha = (1.0f - p / HudTiming::kClimaxFlashEnd) * EB::kFlashMaxAlpha;
            drawList->AddRectFilled(UIVec2(0.0f, 0.0f), UIVec2(screenW, screenH),
                UI_COL32(EB::kFlashR, EB::kFlashG, EB::kFlashB, static_cast<int>(HudPalette::kMaxAlphaFloat * fAlpha)));
        }

        const float barRatio = SmoothStep01(p / HudTiming::kClimaxLetterboxEnd);
        const float curBarH  = screenH * EB::kLetterboxRatio * barRatio;
        if (curBarH > EB::kMinLetterboxPx)
        {
            drawList->AddRectFilled(UIVec2(0.0f, 0.0f), UIVec2(screenW, curBarH), EB::kLetterboxFill);
            drawList->AddRectFilled(UIVec2(0.0f, screenH - curBarH), UIVec2(screenW, screenH), EB::kLetterboxFill);
            drawList->AddLine(UIVec2(0.0f, curBarH), UIVec2(screenW, curBarH),
                UI_COL32(EB::kTopBarLineR, EB::kTopBarLineG, EB::kTopBarLineB, static_cast<int>(EB::kTopBarLineAlpha * barRatio)), EB::kTopBarLineThick);
            drawList->AddLine(UIVec2(0.0f, screenH - curBarH), UIVec2(screenW, screenH - curBarH),
                UI_COL32(EB::kBotBarLineR, EB::kBotBarLineG, EB::kBotBarLineB, static_cast<int>(EB::kBotBarLineAlpha * barRatio)), EB::kBotBarLineThick);
        }

        const float bnrIn = SmoothStep01((p - HudTiming::kClimaxBannerStart) / HudTiming::kClimaxBannerSpan);
        if (bnrIn > EB::kMinAnimVis)
        {
            const float bnrH = EB::kClearBannerH * uiScale;
            const float bnrW = bnrH * (EB::kAtlasSize / EB::kClearBannerSrcH);
            const float bnrX = (screenW - bnrW) * 0.5f;
            const float bnrY = curBarH + EB::kClearBannerTopGap * uiScale - (1.0f - bnrIn) * EB::kClearBannerSlideY * uiScale;
            drawCineSprite(EB::kClearBannerRect, UIVec2(bnrX, bnrY), UIVec2(bnrX + bnrW, bnrY + bnrH),
                UI_COL32(HudPalette::kMaxAlphaInt, HudPalette::kMaxAlphaInt, HudPalette::kMaxAlphaInt, static_cast<int>(HudPalette::kMaxAlphaFloat * bnrIn)));
        }

        const float subIn = SmoothStep01((p - HudTiming::kClimaxSubStart) / HudTiming::kClimaxSubSpan);
        if (subIn > EB::kMinAnimVis)
        {
            drawRadioSubtitle(AsUtf8(u8"[DUSTOFF-1] 回収完了！ しっかり掴まれ、基地へ帰還する！"),
                EB::kClearSubPadW, curBarH + EB::kClearSubBotGap * uiScale,
                UI_COL32(EB::kClearSubBgR, EB::kClearSubBgG, EB::kClearSubBgB, static_cast<int>(EB::kSubBgAlpha * subIn)),
                UI_COL32(EB::kClearSubBdR, EB::kClearSubBdG, EB::kClearSubBdB, static_cast<int>(EB::kSubBorderAlpha * subIn)),
                UI_COL32(EB::kClearSubTxR, EB::kClearSubTxG, EB::kClearSubTxB, static_cast<int>(EB::kClearSubTextAlpha * subIn)));
        }
        return;
    }

    // =========================================================================
    // B. 3Dフィールド投影・脱出ヘリパッド境界リング ＆ ウェイポイントマーカー
    // =========================================================================
    if (ctx.camera && !ctx.player->IsDead())
    {
        const Matrix4x4& vp   = ctx.camera->GetViewProjectionMatrix();
        const Vector3 pPos    = ctx.player->GetPosition();
        const float dx        = ctx.extractionGoalPos.x - pPos.x;
        const float dz        = ctx.extractionGoalPos.z - pPos.z;
        const float dist      = std::sqrt(dx * dx + dz * dz);
        const float padRadius = GameConfig::Environment::kExtractionRadius;
        const float pulse     = 0.5f + 0.5f * std::sin(timeNow * HudTiming::kHelipadPulseSpeed);

        if (dist < EB::kRingMaxDrawDist)
        {
            const float progressRatio = (ctx.isPlayerInExtractionZone && ctx.isReadyToExtract && ctx.extractionMaxTime > 0.0f)
                ? (std::clamp)(1.0f - ctx.extractionTimer / ctx.extractionMaxTime, 0.0f, 1.0f) : 0.0f;
            const int filledSegs = static_cast<int>(progressRatio * static_cast<float>(EB::kRingSegs));
            const float rotOff   = timeNow * HudTiming::kHelipadRingRotSpeed;

            for (int i = 0; i < EB::kRingSegs; ++i)
            {
                if (!ctx.isReadyToExtract && ((i & 1) != 0)) continue;
                const float a0 = (static_cast<float>(i) / static_cast<float>(EB::kRingSegs)) * kTwoPi + rotOff;
                const float a1 = (static_cast<float>(i + 1) / static_cast<float>(EB::kRingSegs)) * kTwoPi + rotOff;
                Vector2 s0{}, s1{};
                if (Project3DTo2D(ctx.extractionGoalPos + Vector3{ std::cos(a0) * padRadius, EB::kRingLiftY, std::sin(a0) * padRadius }, vp, screenW, screenH, s0) &&
                    Project3DTo2D(ctx.extractionGoalPos + Vector3{ std::cos(a1) * padRadius, EB::kRingLiftY, std::sin(a1) * padRadius }, vp, screenW, screenH, s1))
                {
                    uint32_t ringCol = EB::kRingLockedCol;
                    float thick      = EB::kRingLockedThick;
                    if (ctx.isReadyToExtract)
                    {
                        const bool filled = (ctx.isPlayerInExtractionZone && i < filledSegs);
                        ringCol = filled ? EB::kRingFilledCol
                                         : UI_COL32(EB::kRingReadyR, EB::kRingReadyG, EB::kRingReadyB, static_cast<int>(EB::kRingPulseBaseA + EB::kRingPulseAmpA * pulse));
                        thick   = filled ? EB::kRingFilledThick : EB::kRingReadyThick;
                    }
                    drawList->AddLine(UIVec2(s0.x, s0.y), UIVec2(s1.x, s1.y), ringCol, thick);
                }
            }
        }

        if ((ctx.isReadyToExtract && !ctx.isPlayerInExtractionZone) || (!ctx.isReadyToExtract && dist < EB::kLockedMarkerDist))
        {
            const Vector3 marker3D = ctx.extractionGoalPos + Vector3{ 0.0f, EB::kMarkerBaseY + EB::kMarkerBobAmp * std::sin(timeNow * HudTiming::kHelipadMarkerBobSpeed), 0.0f };
            Vector2 m2D{};
            if (Project3DTo2D(marker3D, vp, screenW, screenH, m2D))
            {
                m2D.x = (std::clamp)(m2D.x, EB::kMarkerClampX * uiScale, screenW - EB::kMarkerClampX * uiScale);
                m2D.y = (std::clamp)(m2D.y, EB::kMarkerClampTop * uiScale, screenH - EB::kMarkerClampBot * uiScale);

                char wpBuf[HudLayout::Common::kLabelBufSize];
                if (ctx.isReadyToExtract) std::snprintf(wpBuf, sizeof(wpBuf), AsUtf8(u8"脱出LZ [開放]  %.0fm"), dist);
                else std::snprintf(wpBuf, sizeof(wpBuf), "%s", AsUtf8(u8"脱出LZ [ロック中]"));

                const float wpFont = EB::kWpFont * uiScale;
                const UIVec2 wpSz  = MeasureSharpText(UIFontRole::SubBoldJP, wpFont, wpBuf);
                const float tagW   = wpSz.x + EB::kWpPadW * uiScale;
                const float tagH   = wpSz.y + EB::kWpPadH * uiScale;
                DrawAccentedTag(drawList, m2D.x - tagW * 0.5f, m2D.y - tagH, tagW, tagH, EB::kWpSlant * uiScale,
                    EB::kWpShadowX, EB::kWpShadowY, HudPalette::kPanelShadow125,
                    ctx.isReadyToExtract ? EB::kWpOpenBg : EB::kWpLockBg,
                    ctx.isReadyToExtract ? EB::kWpOpenBd : EB::kWpLockBd, EB::kWpBorderThick,
                    EB::kWpStripeW * uiScale, wpFont, EB::kWpTextShiftX * uiScale,
                    ctx.isReadyToExtract ? EB::kWpOpenTx : EB::kWpLockTx, wpBuf);
            }
        }
    }

    // =========================================================================
    // C. 全標的破壊時：EXTRACTION UNLOCKED バナー ＆ [DUSTOFF-1] 無線通信 (4.2秒間)
    // =========================================================================
    if (ctx.extractionUnlockBannerTimer > 0.0f && !ctx.isPlayerInExtractionZone)
    {
        const float fi    = SmoothStep01((HudTiming::kUnlockBannerTotalSec - ctx.extractionUnlockBannerTimer) / HudTiming::kUnlockBannerFadeInSec);
        const float fo    = SmoothStep01(ctx.extractionUnlockBannerTimer / HudTiming::kUnlockBannerFadeOutSec);
        const float alpha = fi * fo;
        if (alpha > EB::kMinAnimVis)
        {
            const float bnrH = EB::kUnlockBannerH * uiScale;
            const float bnrW = bnrH * (EB::kAtlasSize / EB::kUnlockedBannerSrcH);
            const float bnrX = (screenW - bnrW) * 0.5f;
            const float bnrY = EB::kUnlockBannerY * uiScale - (1.0f - fi) * EB::kUnlockSlideY * uiScale;
            drawCineSprite(EB::kUnlockedBannerRect, UIVec2(bnrX, bnrY), UIVec2(bnrX + bnrW, bnrY + bnrH),
                UI_COL32(HudPalette::kMaxAlphaInt, HudPalette::kMaxAlphaInt, HudPalette::kMaxAlphaInt, static_cast<int>(EB::kUnlockSpriteAlpha * alpha)));

            drawRadioSubtitle(AsUtf8(u8"[DUSTOFF-1] 全標的を破壊完了！ 北東の緑スモークLZへ急行せよ！"),
                EB::kUnlockSubPadW, EB::kUnlockSubBotMargin * uiScale,
                UI_COL32(EB::kUnlockSubBgR, EB::kUnlockSubBgG, EB::kUnlockSubBgB, static_cast<int>(EB::kSubBgAlpha * alpha)),
                UI_COL32(EB::kUnlockSubBdR, EB::kUnlockSubBdG, EB::kUnlockSubBdB, static_cast<int>(EB::kSubBorderAlpha * alpha)),
                UI_COL32(EB::kUnlockSubTxR, EB::kUnlockSubTxG, EB::kUnlockSubTxB, static_cast<int>(EB::kUnlockSubTextAlpha * alpha)));
        }
    }

    // =========================================================================
    // D. 脱出ゾーン滞在時：EXTRACTING タクティカルカウントダウンプログレスバー
    // =========================================================================
    if (ctx.isPlayerInExtractionZone && ctx.isReadyToExtract && ctx.extractionMaxTime > 0.0f)
    {
        const float ratio   = (std::clamp)(1.0f - (ctx.extractionTimer / ctx.extractionMaxTime), 0.0f, 1.0f);
        const float centerX = screenW * 0.5f;
        const float baseY   = EB::kCountdownBaseY * uiScale;
        const float hdrH    = EB::kCountdownHdrH * uiScale;
        const float hdrW    = hdrH * (EB::kAtlasSize / EB::kExtractingHdrSrcH);
        drawCineSprite(EB::kExtractingHeaderRect, UIVec2(centerX - hdrW * 0.5f, baseY), UIVec2(centerX + hdrW * 0.5f, baseY + hdrH),
            UI_COL32(HudPalette::kMaxAlphaInt, HudPalette::kMaxAlphaInt, HudPalette::kMaxAlphaInt, EB::kCountdownHdrAlpha));

        const int numSegs     = EB::kCountdownSegs;
        const float totalBarW = EB::kTotalBarW * uiScale;
        const float segGap    = EB::kSegGap * uiScale;
        const float segW      = (totalBarW - segGap * static_cast<float>(numSegs - 1)) / static_cast<float>(numSegs);
        const float segH      = EB::kSegH * uiScale;
        const float slant     = EB::kSegSlant * uiScale;
        const float barStartX = centerX - totalBarW * 0.5f;
        const float barY      = baseY + hdrH + EB::kBarTopGap * uiScale;

        drawList->AddRectFilled(UIVec2(barStartX - EB::kBackdropPadX * uiScale, barY - EB::kBackdropPadY * uiScale),
                                UIVec2(barStartX + totalBarW + EB::kBackdropPadX * uiScale, barY + segH + EB::kBackdropPadY * uiScale),
                                EB::kBackdropCol, EB::kBackdropRound);

        for (int s = 0; s < numSegs; ++s)
        {
            const float segThreshold  = static_cast<float>(s + 1) / static_cast<float>(numSegs);
            const float prevThreshold = static_cast<float>(s) / static_cast<float>(numSegs);
            uint32_t segCol = EB::kSegEmptyCol;
            if (ratio >= segThreshold)
            {
                segCol = (ratio > EB::kNearFullRatio) ? EB::kSegNearFullCol : EB::kSegActiveCol;
            }
            else if (ratio > prevThreshold)
            {
                const float part = (ratio - prevThreshold) * static_cast<float>(numSegs);
                segCol = UI_COL32(EB::kSegPartR, EB::kSegPartG, EB::kSegPartB, static_cast<int>(EB::kPartBaseAlpha + EB::kPartSpanAlpha * part));
            }
            DrawSlantedPlate(drawList, barStartX + static_cast<float>(s) * (segW + segGap), barY, segW, segH, slant, segCol);
        }

        char timerBuf[HudLayout::Common::kSmallBufSize];
        std::snprintf(timerBuf, sizeof(timerBuf), "%.1fS", (std::max)(0.0f, ctx.extractionTimer));
        DrawHudAtlasDigits(drawList, digitsTex, timerBuf, centerX,
            barY + segH + EB::kTimerOffsetY * uiScale, EB::kTimerDigitH * uiScale,
            (ratio > EB::kNearFullRatio) ? EB::kDigitNearFull : EB::kDigitNormal, HudTextAlign::Center);
    }
}

// =============================================================================
// タクティカル兵装 ＆ 湾曲30連PMAGシースルー断面アニメーション HUD (画面右下)
// =============================================================================

namespace
{
    void DrawPmagCutaway(
        TacticalDrawList* dl, float panelX, float panelY, float panelH, float uiScale,
        int ammo, int maxAmmo, int resAmmo, bool isReloading, float reloadProg)
    {
        namespace AP = HudLayout::AmmoPanel;
        const float magWellX = panelX + AP::kMagWellX * uiScale;
        const float magWellY = panelY + AP::kMagWellY * uiScale;
        const float magW     = AP::kMagW * uiScale;
        const float magH     = AP::kMagH * uiScale;

        dl->PushClipRect(UIVec2(panelX + AP::kClipPad * uiScale, panelY + AP::kClipPad * uiScale),
                         UIVec2(panelX + AP::kClipMaxX * uiScale, panelY + panelH - AP::kClipPad * uiScale), true);

        float magSlideY = 0.0f;
        float magAlpha  = 1.0f;
        int   visAmmo   = ammo;
        if (isReloading)
        {
            if (reloadProg < HudTiming::kReloadEjectEnd)
            {
                const float t = reloadProg / HudTiming::kReloadEjectEnd;
                magSlideY = (t * t) * AP::kSlideDropPx * uiScale;
                magAlpha  = 1.0f - t * AP::kEjectFadeMul;
            }
            else if (reloadProg < HudTiming::kReloadEmptyEnd)
            {
                magAlpha = 0.0f;
            }
            else if (reloadProg < HudTiming::kReloadInsertEnd)
            {
                const float ease = SmoothStep01((reloadProg - HudTiming::kReloadEmptyEnd) / HudTiming::kReloadInsertSpan);
                magSlideY = (1.0f - ease) * AP::kSlideDropPx * uiScale;
                magAlpha  = AP::kInsertBaseAlpha + AP::kInsertSpanAlpha * ease;
                visAmmo   = (std::min)(maxAmmo, ammo + resAmmo);
            }
            else
            {
                visAmmo = (std::min)(maxAmmo, ammo + resAmmo);
            }
        }

        // 固定側のライフル・フレアマグウェル口（上部カラー）
        const UIVec2 collarMin(magWellX - AP::kCollarExtendX * uiScale, magWellY);
        const UIVec2 collarMax(magWellX + magW + AP::kCollarExtendX * uiScale, magWellY + AP::kCollarH * uiScale);
        dl->AddRectFilled(collarMin, collarMax, AP::kCollarFill, AP::kCollarRounding);
        dl->AddRect(collarMin, collarMax, AP::kCollarBorder, AP::kCollarRounding, UIDrawFlags_None, AP::kCollarBorderThick);

        if (magAlpha > AP::kMinVisibleMagAlpha)
        {
            const int aShell  = static_cast<int>(AP::kShellMaxAlpha * magAlpha);
            const float my0   = magWellY + AP::kBodyTopOffset * uiScale + magSlideY;
            const float bodyH = magH - AP::kBodyHeightTrim * uiScale;
            auto curveShiftX  = [&](float tNorm) { return (tNorm * tNorm) * AP::kCurveBendPx * uiScale; }; // PMAG特有の緩やかな前方バナナ湾曲

            const uint32_t shellOuterCol = UI_COL32(AP::kShellOuterR, AP::kShellOuterG, AP::kShellOuterB, aShell);
            const uint32_t shellInnerCol = UI_COL32(AP::kShellInnerR, AP::kShellInnerG, AP::kShellInnerB, aShell);
            const uint32_t shellEdgeCol  = UI_COL32(AP::kShellEdgeR,  AP::kShellEdgeG,  AP::kShellEdgeB,  aShell);
            const float innerPad         = AP::kShellInnerPad * uiScale;

            // 湾曲ポリマー外殻（6分割台形ストリップ）
            for (int s = 0; s < AP::kShellSegs; ++s)
            {
                const float t0 = static_cast<float>(s) / static_cast<float>(AP::kShellSegs);
                const float t1 = static_cast<float>(s + 1) / static_cast<float>(AP::kShellSegs);
                const float y0 = my0 + t0 * bodyH, y1 = my0 + t1 * bodyH;
                const float x0 = magWellX + curveShiftX(t0), x1 = magWellX + curveShiftX(t1);

                const UIVec2 outer[HudLayout::Common::kQuadVertCount] = {
                    UIVec2(x0, y0), UIVec2(x0 + magW, y0), UIVec2(x1 + magW, y1), UIVec2(x1, y1)
                };
                const UIVec2 inner[HudLayout::Common::kQuadVertCount] = {
                    UIVec2(x0 + innerPad, y0), UIVec2(x0 + magW - innerPad, y0),
                    UIVec2(x1 + magW - innerPad, y1), UIVec2(x1 + innerPad, y1)
                };
                dl->AddConvexPolyFilled(outer, HudLayout::Common::kQuadVertCount, shellOuterCol);
                dl->AddConvexPolyFilled(inner, HudLayout::Common::kQuadVertCount, shellInnerCol);
                dl->AddLine(outer[0], outer[HudLayout::Common::kQuadVertCount - 1], shellEdgeCol, AP::kShellEdgeThick);
                dl->AddLine(outer[1], outer[HudLayout::Common::kQuadBottomRightIdx], shellEdgeCol, AP::kShellEdgeThick);
            }

            // 下端フレア形状ラバー・レンジャーフロアプレート
            {
                const float bx = magWellX + curveShiftX(1.0f) - AP::kFloorExtendX * uiScale;
                const float by = my0 + bodyH - AP::kFloorTopTrim * uiScale;
                const UIVec2 fMin(bx, by), fMax(bx + magW + AP::kFloorExtraW * uiScale, by + AP::kFloorH * uiScale);
                dl->AddRectFilled(fMin, fMax, UI_COL32(AP::kFloorFillR, AP::kFloorFillG, AP::kFloorFillB, aShell), AP::kFloorRounding);
                dl->AddRect(fMin, fMax, UI_COL32(AP::kFloorBdR, AP::kFloorBdG, AP::kFloorBdB, aShell), AP::kFloorRounding, UIDrawFlags_None, AP::kFloorBorderThick);
            }

            // 内部15段ダブルスタック 5.56x45mm 真鍮カートリッジ＋銅弾頭 ＆ フォロワースプリング描画
            const int filledRows     = (std::clamp)((visAmmo + 1) * AP::kMaxVisibleRows / maxAmmo, (visAmmo > 0 ? 1 : 0), AP::kMaxVisibleRows);
            const float rowTopPad    = AP::kRowTopPad * uiScale;
            const float slotStep     = (bodyH - AP::kRowSpanTrim * uiScale) / static_cast<float>(AP::kMaxVisibleRows);
            const bool lowWarn       = (!isReloading && visAmmo <= AP::kLowAmmoThreshold);
            const uint32_t brassCol  = lowWarn ? UI_COL32(AP::kBrassWarnR, AP::kBrassWarnG, AP::kBrassWarnB, aShell)
                                               : UI_COL32(AP::kBrassNormR, AP::kBrassNormG, AP::kBrassNormB, aShell);
            const uint32_t copperCol = lowWarn ? UI_COL32(AP::kCopperWarnR, AP::kCopperWarnG, AP::kCopperWarnB, aShell)
                                               : UI_COL32(AP::kCopperNormR, AP::kCopperNormG, AP::kCopperNormB, aShell);
            const float rw           = magW - AP::kRowWidthTrim * uiScale;
            const float rh           = (std::max)(AP::kMinRowH, slotStep - AP::kRowGapH * uiScale);
            const float caseW        = rw * AP::kCaseWidthRatio;

            for (int r = 0; r < filledRows; ++r)
            {
                const float tMid = (static_cast<float>(r) + 0.5f) / static_cast<float>(AP::kMaxVisibleRows);
                const float ry   = my0 + rowTopPad + static_cast<float>(r) * slotStep;
                const float rx   = magWellX + curveShiftX(tMid) + AP::kRowInsetX * uiScale;
                dl->AddRectFilled(UIVec2(rx, ry), UIVec2(rx + caseW, ry + rh), brassCol, AP::kCaseRounding);
                dl->AddTriangleFilled(UIVec2(rx + caseW, ry), UIVec2(rx + rw, ry + rh * 0.5f), UIVec2(rx + caseW, ry + rh), copperCol);
            }

            // アンチチルト・オレンジフォロワー ＆ 伸縮ジグザグ鋼製スプリング
            if (filledRows < AP::kMaxVisibleRows)
            {
                const float folT = static_cast<float>(filledRows) / static_cast<float>(AP::kMaxVisibleRows);
                const float folY = my0 + rowTopPad + static_cast<float>(filledRows) * slotStep;
                const float folX = magWellX + curveShiftX(folT) + AP::kFollowerInsetX * uiScale;
                const float folW = magW - AP::kFollowerWidthTrim * uiScale;
                dl->AddRectFilled(UIVec2(folX, folY), UIVec2(folX + folW, folY + AP::kFollowerH * uiScale),
                                  UI_COL32(AP::kFollowerR, AP::kFollowerG, AP::kFollowerB, aShell), AP::kFollowerRounding);

                const float sprStartY = folY + AP::kSpringTopOff * uiScale;
                const float sprEndY   = my0 + bodyH - AP::kSpringBotTrim * uiScale;
                if (sprEndY > sprStartY + AP::kSpringMinSpan)
                {
                    const uint32_t sprCol = UI_COL32(AP::kSpringR, AP::kSpringG, AP::kSpringB, static_cast<int>(AP::kSpringAlpha * magAlpha));
                    const float insetPx   = AP::kSpringInsetX * uiScale;
                    for (int z = 0; z < AP::kSpringZigs; ++z)
                    {
                        const float zt0 = static_cast<float>(z) / static_cast<float>(AP::kSpringZigs);
                        const float zt1 = static_cast<float>(z + 1) / static_cast<float>(AP::kSpringZigs);
                        const float sy0 = sprStartY + zt0 * (sprEndY - sprStartY);
                        const float sy1 = sprStartY + zt1 * (sprEndY - sprStartY);
                        const float sx0 = magWellX + curveShiftX((sy0 - my0) / bodyH) + (((z & 1) == 0) ? insetPx : (magW - insetPx));
                        const float sx1 = magWellX + curveShiftX((sy1 - my0) / bodyH) + ((((z + 1) & 1) == 0) ? insetPx : (magW - insetPx));
                        dl->AddLine(UIVec2(sx0, sy0), UIVec2(sx1, sy1), sprCol, AP::kSpringThick * uiScale);
                    }
                }
            }
        }
        dl->PopClipRect();
    }
}

void GamePlayHUD::DrawPlayerAmmoHUD(const GamePlayHUDContext& ctx, float /*deltaTime*/)
{
    if (!ctx.player || ctx.player->IsDead() || ctx.isDeathSequenceActive) return;

    HudViewport vp{};
    if (!AcquireHudViewport(vp)) return;
    TacticalDrawList* dl = vp.dl;
    const float uiScale  = vp.uiScale;
    namespace AP = HudLayout::AmmoPanel;

    const UITextureID digitsTex = ResolveHudTexture(clearDigitsTextureIndex_);
    const int ammo         = ctx.player->GetMagazineAmmo();
    const int maxAmmo      = (std::max)(1, ctx.player->GetMaxMagazineAmmo());
    const int resAmmo      = ctx.player->GetReserveAmmo();
    const bool isReloading = ctx.player->IsReloading();
    const float reloadProg = (std::clamp)(ctx.player->GetReloadProgress(), 0.0f, 1.0f);
    const bool isCancelled = (ctx.player->GetReloadCancelledTimer() > 0.0f);
    const bool wasDry      = ctx.player->WasDryReload();

    // 右下専用ウェポンパネル（高コントラスト暗色プレート＋1行ボールド文字で潰れゼロ）
    const float panelW = AP::kWidth * uiScale;
    const float panelH = AP::kHeight * uiScale;
    const float slant  = AP::kSlant * uiScale;
    const float panelX = std::round(vp.screenW - panelW - AP::kMargin * uiScale);
    const float panelY = std::round(vp.screenH - panelH - AP::kMargin * uiScale);

    uint32_t accentCol = HudPalette::kGoldAccent;
    if (isReloading) accentCol = AP::kReloadAccent;
    else if (isCancelled || ammo == 0) accentCol = HudPalette::kDangerRed;
    else if (ammo <= AP::kLowAmmoThreshold) accentCol = AP::kLowAmmoAccent;

    DrawFramedPanel(dl, panelX, panelY, panelW, panelH, slant, AP::kPanelStyle);
    DrawSlantedPlate(dl, panelX + panelW - AP::kStripeWidth * uiScale, panelY, AP::kStripeWidth * uiScale, panelH, slant, accentCol);

    // 1. 左コンパートメント：リアル湾曲30連PMAG シースルーカットアウェイ断面図
    DrawPmagCutaway(dl, panelX, panelY, panelH, uiScale, ammo, maxAmmo, resAmmo, isReloading, reloadProg);

    // 2. 右コンパートメント：M4A1 SOPMODシルエット・残弾デジタル・3段階リロードバー
    const float rightX = panelX + AP::kRightOffsetX * uiScale;
    const float rightW = panelW - AP::kRightWidthTrim * uiScale;
    dl->AddLine(UIVec2(rightX - AP::kDivLeftShift * uiScale, panelY + AP::kDivPadY * uiScale),
                UIVec2(rightX - AP::kDivLeftShift * uiScale, panelY + panelH - AP::kDivPadY * uiScale), AP::kDivCol, AP::kDivThick);

    const float topY  = panelY + AP::kTopOffsetY * uiScale;
    const float topCy = topY + AP::kTopCenterOffY * uiScale;
    const char* alertText = nullptr;
    uint32_t alertCol     = 0;
    if (isReloading)                         { alertText = AsUtf8(u8"RELOADING // 装填中"); alertCol = AP::kReloadTextCol; }
    else if (isCancelled)                    { alertText = AsUtf8(u8"RELOAD CANCELLED");    alertCol = AP::kCancelTextCol; }
    else if (ammo == 0)                      { alertText = AsUtf8(u8"EMPTY // 弾切れ");     alertCol = AP::kEmptyTextCol; }
    else if (ammo <= AP::kLowAmmoThreshold)  { alertText = AsUtf8(u8"LOW AMMO // 残弾少");  alertCol = AP::kLowAmmoTextCol; }

    if (alertText)
    {
        DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, AP::kStatusFont * uiScale,
            rightX + AP::kStatusTextPadX * uiScale, topCy, alertCol, alertText, HudTextAlign::Left);
    }
    else
    {
        // M4A1 SOPMOD ベクターシルエット ＋ 口径タグ
        const float gx = rightX + AP::kGunOriginPadX * uiScale;
        const float gy = topY + AP::kGunOriginPadY * uiScale;
        for (const auto& part : AP::kRifleSilhouetteParts)
        {
            dl->AddRectFilled(UIVec2(gx + part.x0 * uiScale, gy + part.y0 * uiScale),
                              UIVec2(gx + part.x1 * uiScale, gy + part.y1 * uiScale),
                              part.useAccentCol ? AP::kGunOpticCol : AP::kGunBodyCol, part.rounding);
        }
        dl->AddLine(UIVec2(gx + AP::kBarrelStartX * uiScale, gy + AP::kBarrelOffY * uiScale),
                    UIVec2(gx + AP::kBarrelEndX * uiScale, gy + AP::kBarrelOffY * uiScale), AP::kGunBodyCol, AP::kBarrelThick * uiScale);
        DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, AP::kCaliberFont * uiScale, rightX + rightW, topCy, AP::kCaliberCol, "5.56mm", HudTextAlign::Right);
    }

    // 中段：大型装填弾数 "30" + "/30" ＆ 予備弾薬アイコン "+90"
    const float midCy = panelY + AP::kMidCenterY * uiScale;
    uint32_t magCol   = HudPalette::kTextIceWhite;
    if (isReloading) magCol = AP::kReloadTextCol;
    else if (ammo == 0) magCol = AP::kMagZeroCol;
    else if (ammo <= AP::kLowAmmoThreshold) magCol = AP::kMagLowCol;

    char curAmmoBuf[HudLayout::Common::kShortBufSize];
    std::snprintf(curAmmoBuf, sizeof(curAmmoBuf), "%d", ammo);
    const float magFont = AP::kMagAmmoFont * uiScale;
    DrawShadowedTextAligned(dl, UIFontRole::TitleBoldJP, magFont, rightX + AP::kStatusTextPadX * uiScale, midCy, magCol, curAmmoBuf, HudTextAlign::Left);
    const float curW = MeasureSharpText(UIFontRole::TitleBoldJP, magFont, curAmmoBuf).x;

    char capBuf[HudLayout::Common::kShortBufSize];
    std::snprintf(capBuf, sizeof(capBuf), "/%d", maxAmmo);
    DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, AP::kCapAmmoFont * uiScale,
        rightX + AP::kCapGapX * uiScale + curW, midCy + AP::kCapOffY * uiScale, AP::kCapTextCol, capBuf, HudTextAlign::Left);

    // 右側：予備弾薬ボックスアイコン ＋ 予備弾数
    char resBuf[HudLayout::Common::kShortBufSize];
    std::snprintf(resBuf, sizeof(resBuf), "+%d", resAmmo);
    const float resDigitH = AP::kReserveDigitH * uiScale;
    DrawHudAtlasDigits(dl, digitsTex, resBuf, rightX + rightW, midCy + AP::kReserveOffY * uiScale,
        resDigitH, (resAmmo > 0) ? AP::kResHasCol : AP::kResEmptyCol, HudTextAlign::Right);
    const float resW = MeasureHudAtlasDigits(resBuf, resDigitH);

    const UITextureID ammoTex = ResolveHudTexture(ammoTextureIndex_);
    if (ammoTex != 0)
    {
        const float aIc = AP::kAmmoIconSize * uiScale;
        const UIVec2 iMin(rightX + rightW - resW - aIc - AP::kAmmoIconGapX * uiScale, midCy - aIc * 0.5f + AP::kAmmoIconOffY * uiScale);
        dl->AddImageRounded(ammoTex, iMin, UIVec2(iMin.x + aIc, iMin.y + aIc), UIVec2(0.0f, 0.0f), UIVec2(1.0f, 1.0f), AP::kAmmoIconTint, AP::kAmmoIconRound);
    }

    // 下段：リロード中は「3段階メカニカル進捗バー (EJECT | INSERT | BOLT)」、通常時は30連精密ピップバー
    const float botY = panelY + AP::kBotBarY * uiScale;
    const float botH = (isReloading ? AP::kReloadBarH : AP::kPipBarH) * uiScale;
    if (isReloading)
    {
        const uint32_t stgCol = (reloadProg < HudTiming::kReloadEjectEnd) ? AP::kStageEjectCol
                              : ((reloadProg < HudTiming::kReloadSeatTickRatio) ? AP::kStageInsertCol : AP::kStageBoltCol);
        DrawProgressBar(dl, rightX, botY, rightW, botH, AP::kReloadBarSlant * uiScale,
            reloadProg, AP::kReloadBarBg, stgCol, AP::kReloadBarBd, AP::kReloadBarBdThick, 0.0f, 0, 0.0f);

        for (float tickRatio : { HudTiming::kReloadEjectEnd, HudTiming::kReloadSeatTickRatio })
        {
            const float tx = rightX + rightW * tickRatio;
            dl->AddLine(UIVec2(tx + AP::kStageTickSlant * uiScale, botY), UIVec2(tx, botY + botH), AP::kStageTickCol, AP::kStageTickThick);
        }

        const char* stgTxt = (reloadProg < HudTiming::kReloadEjectEnd) ? "MAG OUT"
                           : ((reloadProg < HudTiming::kReloadSeatTickRatio) ? "INSERT" : (wasDry ? "BOLT" : "READY"));
        DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, AP::kStageFont * uiScale,
            rightX + rightW * 0.5f, botY + botH * 0.5f, AP::kStageTextCol, stgTxt, HudTextAlign::Center);
    }
    else
    {
        const int totalPips         = (std::clamp)(maxAmmo, 1, AP::kMaxAmmoPips);
        const float pipGap          = AP::kPipGap * uiScale;
        const float pipW            = (std::max)(AP::kMinPipW, (rightW - pipGap * static_cast<float>(totalPips - 1)) / static_cast<float>(totalPips));
        const uint32_t loadedPipCol = (ammo <= AP::kLowAmmoThreshold) ? AP::kPipLowCol : AP::kPipFullCol;
        for (int p = 0; p < totalPips; ++p)
        {
            DrawSlantedPlate(dl, rightX + static_cast<float>(p) * (pipW + pipGap), botY + AP::kPipPadY * uiScale,
                pipW, botH - AP::kPipTrimH * uiScale, AP::kPipSlant * uiScale, (p < ammo) ? loadedPipCol : AP::kPipEmptyCol);
        }
    }
}

// =============================================================================
// 3D敵タクティカル頭上ゲージ・ステルス視界アーク・射撃予兆レーザー HUD
// =============================================================================

void GamePlayHUD::DrawEnemyTacticalHUD(const GamePlayHUDContext& ctx)
{
    if (!ctx.camera || !ctx.player || !ctx.enemies || ctx.isDeathSequenceActive) return;

    HudViewport vpInfo{};
    if (!AcquireHudViewport(vpInfo)) return;
    TacticalDrawList* dl = vpInfo.dl;
    const float screenW  = vpInfo.screenW;
    const float screenH  = vpInfo.screenH;
    const float uiScale  = vpInfo.uiScale;
    namespace EH = HudLayout::EnemyHud;

    const Matrix4x4& vp     = ctx.camera->GetViewProjectionMatrix();
    const Vector3 playerPos = ctx.player->GetPosition();
    const float timeNow     = static_cast<float>(TacticalUICanvas::GetInstance()->GetTime());

    for (const auto& enemyPtr : *ctx.enemies)
    {
        const Enemy* enemy = enemyPtr.get();
        if (!enemy || enemy->IsDead()) continue;

        const Vector3 ePos = enemy->GetPosition();
        const float dx     = ePos.x - playerPos.x;
        const float dz     = ePos.z - playerPos.z;
        const float dist   = std::sqrt(dx * dx + dz * dz);
        if (dist > EH::kMaxDrawDist) continue;

        const Enemy::AIState state = enemy->GetAIState();
        const float detectMeter    = (std::clamp)(enemy->GetDetectionMeter(), 0.0f, 1.0f);
        const float telegraphProg  = enemy->GetAimTelegraphProgress();

        // 1. 近距離・未発覚時の地面投影ステルス索敵視界アーク (16.5m以内)
        if (state != Enemy::AIState::Chase && dist < EH::kVisionArcMaxDist)
        {
            const float yaw      = enemy->GetYaw();
            const float halfFov  = enemy->GetFieldOfView() * 0.5f;
            const float coneLen  = (std::min)(EH::kVisionMaxConeLen, enemy->GetMaxSightRange() * EH::kVisionRangeRatio);
            const float distFade = (std::clamp)((EH::kVisionArcMaxDist - dist) / EH::kVisionFadeSpan, 0.0f, 1.0f);
            const bool isAlert   = (state == Enemy::AIState::Investigate || detectMeter > EH::kVisionDetectThresh);
            const uint32_t arcCol = isAlert
                ? UI_COL32(EH::kAlertArcR, EH::kAlertArcG, EH::kAlertArcB, static_cast<int>(EH::kAlertArcAlpha * distFade))
                : UI_COL32(EH::kCalmArcR,  EH::kCalmArcG,  EH::kCalmArcB,  static_cast<int>(EH::kCalmArcAlpha * distFade));

            Vector2 origin2D{}, leftEdge2D{}, rightEdge2D{};
            const bool originOk = Project3DTo2D(ePos + Vector3{ 0.0f, EH::kVisionLiftY, 0.0f }, vp, screenW, screenH, origin2D);

            for (int s = 0; s < EH::kVisionArcSegs; ++s)
            {
                const float a0 = yaw - halfFov + (static_cast<float>(s) / static_cast<float>(EH::kVisionArcSegs)) * (halfFov * 2.0f);
                const float a1 = yaw - halfFov + (static_cast<float>(s + 1) / static_cast<float>(EH::kVisionArcSegs)) * (halfFov * 2.0f);
                Vector2 s0{}, s1{};
                if (Project3DTo2D(ePos + Vector3{ std::sin(a0) * coneLen, EH::kVisionLiftY, std::cos(a0) * coneLen }, vp, screenW, screenH, s0) &&
                    Project3DTo2D(ePos + Vector3{ std::sin(a1) * coneLen, EH::kVisionLiftY, std::cos(a1) * coneLen }, vp, screenW, screenH, s1))
                {
                    dl->AddLine(UIVec2(s0.x, s0.y), UIVec2(s1.x, s1.y), arcCol, EH::kVisionArcThick);
                    if (s == 0) leftEdge2D = s0;
                    if (s == EH::kVisionArcSegs - 1) rightEdge2D = s1;
                }
            }
            if (originOk && leftEdge2D.x != 0.0f && rightEdge2D.x != 0.0f)
            {
                const uint32_t sideCol = isAlert
                    ? UI_COL32(EH::kAlertArcR, EH::kAlertArcG, EH::kAlertArcB, static_cast<int>(EH::kAlertSideAlpha * distFade))
                    : UI_COL32(EH::kCalmArcR,  EH::kCalmArcG,  EH::kCalmArcB,  static_cast<int>(EH::kCalmSideAlpha * distFade));
                dl->AddLine(UIVec2(origin2D.x, origin2D.y), UIVec2(leftEdge2D.x, leftEdge2D.y), sideCol, EH::kVisionSideThick);
                dl->AddLine(UIVec2(origin2D.x, origin2D.y), UIVec2(rightEdge2D.x, rightEdge2D.y), sideCol, EH::kVisionSideThick);
            }
        }

        // 2. 射撃直前0.40秒の深紅レーザー照準テレグラフ（回避ローリングの明確な合図）
        if (telegraphProg > EH::kMinTelegraphProg && enemy->HasCurrentLineOfSight() && !ctx.player->IsDead())
        {
            Vector2 m2D{}, t2D{};
            if (Project3DTo2D(enemy->GetMuzzleWorldPosition(), vp, screenW, screenH, m2D) &&
                Project3DTo2D(playerPos + Vector3{ 0.0f, EH::kTelegraphTargetY, 0.0f }, vp, screenW, screenH, t2D))
            {
                const float flash = (telegraphProg > HudTiming::kTelegraphFlashStart)
                    ? (EH::kTelegraphFlashBase + EH::kTelegraphFlashAmp * std::sin(timeNow * HudTiming::kTelegraphFlashSpeed)) : 1.0f;
                const int beamAlpha   = static_cast<int>((EH::kBeamBaseAlpha + EH::kBeamProgAlpha * telegraphProg) * flash);
                const float beamThick = (EH::kBeamBaseThick + EH::kBeamProgThick * telegraphProg) * uiScale;
                dl->AddLine(UIVec2(m2D.x, m2D.y), UIVec2(t2D.x, t2D.y), UI_COL32(EH::kBeamR, EH::kBeamG, EH::kBeamB, beamAlpha), beamThick);
                DrawDiamondPip(dl, m2D.x, m2D.y, (EH::kMuzzlePipBaseRad + EH::kMuzzlePipProgRad * telegraphProg) * uiScale,
                    UI_COL32(EH::kPipCoreR, EH::kPipCoreG, EH::kPipCoreB, beamAlpha),
                    UI_COL32(EH::kPipOutR,  EH::kPipOutG,  EH::kPipOutB,  beamAlpha));
            }
        }

        // 3. 頭上タクティカル・アーマーHPバー ＆ 状態ピルバッジ (! CONTACT / ? SEARCH / RELOADING)
        const float hpRatio  = (std::clamp)(enemy->GetHPRatio(), 0.0f, 1.0f);
        const float lagRatio = (std::clamp)(enemy->GetDisplayedHPRatio(), 0.0f, 1.0f);
        const bool isDamaged = (enemy->GetHP() < enemy->GetMaxHP());
        const bool isReload  = enemy->IsEnemyReloading();
        const bool isWarden  = (enemy->GetTacticalRole() == Enemy::TacticalRole::Warden);

        Vector2 head2D{};
        if (!Project3DTo2D(ePos + Vector3{ 0.0f, EH::kHeadHeightBase * enemy->GetRoleScale(), 0.0f }, vp, screenW, screenH, head2D)) continue;
        if (head2D.x < -EH::kScreenCullMargin || head2D.x > screenW + EH::kScreenCullMargin ||
            head2D.y < -EH::kScreenCullMargin || head2D.y > screenH + EH::kScreenCullMargin) continue;

        if (state == Enemy::AIState::Patrol && !isDamaged && detectMeter <= EH::kPatrolDetectThresh &&
            !(isWarden && dist <= EH::kWardenAlwaysDist)) continue;

        if (state == Enemy::AIState::Patrol && !isDamaged && detectMeter > EH::kPatrolDetectThresh && !isWarden)
        {
            const float dRad = EH::kSuspectPipRad * uiScale;
            const float pipY = head2D.y - EH::kSuspectOffsetY * uiScale;
            DrawDiamondPip(dl, head2D.x, pipY, dRad, EH::kSuspectBg, EH::kSuspectOutline);
            DrawDiamondPip(dl, head2D.x, pipY, dRad * detectMeter, EH::kSuspectFill);
            continue;
        }

        // コンパクト傾斜クリムゾンHPバー (通常兵: 56x6.5px / 看守長ミニボス: 96x8.5px ゴールド縁取り)
        const float barW     = (isWarden ? EH::kWardenBarW : EH::kNormalBarW) * uiScale;
        const float barH     = (isWarden ? EH::kWardenBarH : EH::kNormalBarH) * uiScale;
        const float slant    = EH::kBarSlant * uiScale;
        const float barX     = head2D.x - barW * 0.5f;
        const float barY     = head2D.y - barH * 0.5f;
        const uint32_t eFill = (state == Enemy::AIState::Investigate) ? EH::kBarInvestigate : (isWarden ? EH::kBarWardenFill : EH::kBarNormalFill);

        DrawProgressBar(dl, barX, barY, barW, barH, slant, hpRatio, EH::kBarBg, eFill,
            isWarden ? EH::kWardenFrameCol : EH::kNormalFrameCol,
            isWarden ? EH::kWardenBorderThick : EH::kNormalBorderThick,
            lagRatio, EH::kBarLagCol, EH::kMinHpFillRatio);

        float extraBossHeaderOffset = 0.0f;
        if (isWarden)
        {
            const char* bossLabel = AsUtf8(u8"BOSS // 看守長");
            const float bossFont  = EH::kBossFont * uiScale;
            const UIVec2 tSz      = MeasureSharpText(UIFontRole::SubBoldJP, bossFont, bossLabel);
            const float tagW      = tSz.x + EH::kBossPadW * uiScale;
            const float tagH      = tSz.y + EH::kBossPadH * uiScale;
            DrawPipBadgePlate(dl, head2D.x - tagW * 0.5f, barY - tagH - EH::kBossGapY * uiScale, tagW, tagH, EH::kBossSlant * uiScale,
                EH::kBossBg, EH::kBossBorder, EH::kBossBorderThick,
                EH::kBossPipX * uiScale, EH::kBossPipRad * uiScale, EH::kBossPipCol,
                bossFont, EH::kBossTextX * uiScale, EH::kBossTextCol, bossLabel);
            extraBossHeaderOffset = tagH + EH::kBossGapY * uiScale;
        }

        auto drawEnemyStatusPill = [&](const char* txt, uint32_t bgCol, uint32_t bdCol, uint32_t txCol) {
            const float pillFont = EH::kPillFont * uiScale;
            const UIVec2 pSz     = MeasureSharpText(UIFontRole::SubBoldJP, pillFont, txt);
            const float pW       = pSz.x + EH::kPillPadW * uiScale;
            const float pH       = pSz.y + EH::kPillPadH * uiScale;
            DrawAccentedTag(dl, head2D.x - pW * 0.5f, barY - extraBossHeaderOffset - pH - EH::kPillGapY * uiScale,
                pW, pH, EH::kPillSlant * uiScale, EH::kPillShadowX, EH::kPillShadowY, HudPalette::kPanelShadow120,
                bgCol, bdCol, EH::kPillBorderThick, 0.0f, pillFont, EH::kPillTextShiftX * uiScale, txCol, txt);
        };

        if (isReload)
        {
            drawEnemyStatusPill(AsUtf8(u8"装填中"), EH::kPillReloadBg, EH::kPillReloadBd, EH::kPillReloadTx);
            DrawSlantedPlate(dl, barX, barY + barH + EH::kReloadSubGapY * uiScale,
                barW * enemy->GetReloadProgress(), EH::kReloadSubH * uiScale, EH::kReloadSubSlant * uiScale, EH::kReloadSubCol);
        }
        else if (state == Enemy::AIState::Investigate)
        {
            drawEnemyStatusPill(AsUtf8(u8"? 警戒"), EH::kPillSearchBg, EH::kPillSearchBd, EH::kPillSearchTx);
        }
        else if (state == Enemy::AIState::Chase && (enemy->GetAlertTimer() > 0.0f || telegraphProg > EH::kMinTelegraphProg))
        {
            drawEnemyStatusPill(AsUtf8(u8"! 交戦"), EH::kPillContactBg, EH::kPillContactBd, EH::kPillContactTx);
        }
    }
}

// =============================================================================
// 3D物資探索カード HUD (フィールド上・文字潰れゼロ高視認カード)
// =============================================================================

void GamePlayHUD::DrawLootingHUD(const GamePlayHUDContext& ctx)
{
    if (!ctx.player || ctx.player->IsDead() || !ctx.camera || ctx.isDeathSequenceActive || !ctx.lootProps) return;

    HudViewport vpInfo{};
    if (!AcquireHudViewport(vpInfo)) return;
    TacticalDrawList* dl = vpInfo.dl;
    const float screenW  = vpInfo.screenW;
    const float screenH  = vpInfo.screenH;
    const float uiScale  = vpInfo.uiScale;
    namespace LC = HudLayout::LootCard;

    const UITextureID digitsTex = ResolveHudTexture(clearDigitsTextureIndex_);
    const Matrix4x4& vp         = ctx.camera->GetViewProjectionMatrix();
    const Vector3 playerPos     = ctx.player->GetPosition();
    const float maxDrawDistSq   = GameConfig::Math::Square(LC::kMaxDrawDist);

    for (const auto& prop : *ctx.lootProps)
    {
        if (prop.isLooted) continue;

        const float dx     = prop.position.x - playerPos.x;
        const float dz     = prop.position.z - playerPos.z;
        const float distSq = dx * dx + dz * dz;
        if (distSq > maxDrawDistSq) continue;
        const float dist   = std::sqrt(distSq);

        Vector2 badge2D{};
        if (!Project3DTo2D(prop.position + Vector3{ 0.0f, LC::kBadgeLiftY, 0.0f }, vp, screenW, screenH, badge2D)) continue;

        const bool isNearby     = (dist <= LootSystem::kInteractRadius);
        const float searchRatio = (prop.maxSearchTime > 0.0f) ? (std::clamp)(prop.searchTimer / prop.maxSearchTime, 0.0f, 1.0f) : 0.0f;
        const bool isRare       = (prop.type == LootType::GoldDuck);

        uint32_t iconTex = UINT32_MAX;
        if (prop.type == LootType::Medkit)        iconTex = medkitTextureIndex_;
        else if (prop.type == LootType::AmmoBox)  iconTex = ammoTextureIndex_;
        else if (prop.type == LootType::GoldDuck) iconTex = goldDuckTextureIndex_;
        else if (prop.type == LootType::Roubles)  iconTex = roublesTextureIndex_;

        if (isNearby)
        {
            const char* keyBadgeTxt   = prop.isBreakableCrate ? "LMB" : "E";
            const char* holdHint      = prop.isBreakableCrate ? AsUtf8(u8"射撃で破壊して回収") : AsUtf8(u8"長押しで回収");
            const float keyBadgeW     = (std::max)(LC::kKeyW * uiScale, MeasureSharpText(UIFontRole::SubBoldJP, LC::kKeyFont * uiScale, keyBadgeTxt).x + 9.0f * uiScale);
            const UIVec2 nameSz       = MeasureSharpText(UIFontRole::SubBoldJP, LC::kNameFont * uiScale, prop.name.c_str());
            const UIVec2 hintSz       = MeasureSharpText(UIFontRole::SubBoldJP, LC::kHintFont * uiScale, holdHint);
            const float rightContentW = (std::max)(keyBadgeW + LC::kHintGapX * uiScale + hintSz.x + LC::kHintRightPadW * uiScale,
                                                   nameSz.x + LC::kNameRightPadW * uiScale);
            const float cardW         = (std::max)(LC::kMinCardW * uiScale, LC::kLeftIconColW * uiScale + rightContentW);
            const float cardH         = LC::kCardH * uiScale;
            const float slant         = LC::kCardSlant * uiScale;
            const float cx            = (std::clamp)(badge2D.x - cardW * 0.5f, LC::kScreenClampPadX * uiScale, screenW - cardW - LC::kScreenClampPadX * uiScale);
            const float cy            = badge2D.y - cardH * 0.5f;
            const uint32_t cBorder    = isRare ? LC::kRareBorder : LC::kNormalBorder;

            DrawSlantedPlate(dl, cx + LC::kShadowX, cy + LC::kShadowY, cardW, cardH, slant, HudPalette::kPanelShadow140);
            DrawSlantedPlate(dl, cx, cy, cardW, cardH, slant, LC::kCardBg, cBorder, LC::kBorderThick);

            const UITextureID iconGpu = ResolveHudTexture(iconTex);
            if (iconGpu != 0)
            {
                const float iSz = LC::kIconSize * uiScale;
                const UIVec2 iMin(cx + LC::kIconPadX * uiScale, cy + (cardH - iSz) * 0.5f);
                const UIVec2 iMax(iMin.x + iSz, iMin.y + iSz);
                dl->AddImageRounded(iconGpu, iMin, iMax, UIVec2(0.0f, 0.0f), UIVec2(1.0f, 1.0f), HudPalette::kTextPureWhite, LC::kIconRounding);
                dl->AddRect(iMin, iMax, cBorder, LC::kIconRounding, UIDrawFlags_None, LC::kIconBorderThick);
            }

            const float textX = cx + LC::kLeftIconColW * uiScale;
            if (prop.searchTimer > 0.0f)
            {
                DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, LC::kSearchingFont * uiScale,
                    textX, cy + LC::kSearchingCenterY * uiScale, LC::kSearchingCol, AsUtf8(u8"回収中..."), HudTextAlign::Left);

                char pctBuf[HudLayout::Common::kShortBufSize];
                std::snprintf(pctBuf, sizeof(pctBuf), "%d%%", static_cast<int>(searchRatio * HudLayout::Common::kPercentMul));
                DrawHudAtlasDigits(dl, digitsTex, pctBuf, cx + cardW - LC::kPctRightPadX * uiScale,
                    cy + LC::kSearchingCenterY * uiScale, LC::kPctDigitH * uiScale, LC::kPctCol, HudTextAlign::Right);

                DrawProgressBar(dl, textX, cy + LC::kProgTopY * uiScale, cardW - LC::kProgWidthTrim * uiScale,
                    LC::kProgH * uiScale, LC::kProgSlant * uiScale, searchRatio, LC::kProgBg, cBorder, 0, 0.0f, 0.0f, 0, 0.0f);
            }
            else
            {
                DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, LC::kNameFont * uiScale,
                    textX, cy + LC::kIdleNameCenterY * uiScale, isRare ? LC::kRareNameCol : LC::kNormalNameCol, prop.name.c_str(), HudTextAlign::Left);

                // キーキャップ ＋ 回収／破壊ガイドテキスト
                const float kW = keyBadgeW, kH = LC::kKeyH * uiScale, kY = cy + LC::kKeyTopY * uiScale;
                DrawKeycapBadge(dl, textX, kY, kW, kH, LC::kKeyRounding, LC::kKeyBg, cBorder, LC::kKeyBorderThick,
                    LC::kKeyFont * uiScale, HudPalette::kTextPureWhite, keyBadgeTxt, kY + kH * 0.5f);
                DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, LC::kHintFont * uiScale,
                    textX + kW + LC::kHintGapX * uiScale, kY + kH * 0.5f, isRare ? LC::kRareHintCol : LC::kNormalHintCol, holdHint, HudTextAlign::Left);
            }
        }
        else
        {
            // 中距離（〜7.5m）：シャープなダイヤマーカー＋距離表示
            const uint32_t pipCol = isRare ? LC::kFarRarePip : LC::kFarNormalPip;
            DrawDiamondPip(dl, badge2D.x, badge2D.y, LC::kFarPipOuterRad * uiScale, LC::kFarPipBg, pipCol);
            DrawDiamondPip(dl, badge2D.x, badge2D.y, LC::kFarPipInnerRad * uiScale, pipCol);

            char dBuf[HudLayout::Common::kShortBufSize];
            std::snprintf(dBuf, sizeof(dBuf), "%.1fm", dist);
            DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, LC::kFarDistFont * uiScale,
                badge2D.x, badge2D.y + LC::kFarDistOffsetY * uiScale, LC::kFarDistCol, dBuf, HudTextAlign::Center);
        }
    }
}

// =============================================================================
// 被弾インジケーター & 低体力ビネット & 死亡演出 HUD
// =============================================================================

void GamePlayHUD::DrawDamageIndicator(const GamePlayHUDContext& ctx)
{
    if (ctx.hitIndicatorTimer <= 0.0f || !ctx.player || ctx.player->IsDead() || !ctx.camera ||
        ctx.isDeathSequenceActive || ctx.isGameCleared || ctx.isFullMapOpen)
    {
        return;
    }

    HudViewport vpInfo{};
    if (!AcquireHudViewport(vpInfo, HudLayout::Common::kReticleScaleMin, HudLayout::Common::kReticleScaleMax)) return;
    TacticalDrawList* drawList = vpInfo.dl;
    const float screenW = vpInfo.screenW;
    const float screenH = vpInfo.screenH;
    const float uiScale = vpInfo.uiScale;
    namespace DF = HudLayout::DamageFeedback;

    const Matrix4x4& vp = ctx.camera->GetViewProjectionMatrix();
    const float sinAng  = std::sin(ctx.hitIndicatorAngle);
    const float cosAng  = std::cos(ctx.hitIndicatorAngle);

    // アヒル自身の3D座標と、被弾元の3D方向ベクトルを直接2Dスクリーンへ投影して正確な方角を算出
    const Vector3 pCenter3D = ctx.player->GetPosition() + Vector3{ 0.0f, DF::kPlayerCenterLiftY, 0.0f };
    const Vector3 pFrom3D   = pCenter3D + Vector3{ sinAng * DF::kFarProbeDist, 0.0f, cosAng * DF::kFarProbeDist };

    Vector2 duck2D{};
    Vector2 from2D{};
    if (!Project3DTo2D(pCenter3D, vp, screenW, screenH, duck2D)) return;
    if (!Project3DTo2D(pFrom3D, vp, screenW, screenH, from2D))
    {
        const Vector3 pFromNear3D = pCenter3D + Vector3{ sinAng * DF::kNearProbeDist, 0.0f, cosAng * DF::kNearProbeDist };
        if (!Project3DTo2D(pFromNear3D, vp, screenW, screenH, from2D)) return;
    }

    const float dx = from2D.x - duck2D.x;
    const float dy = from2D.y - duck2D.y;
    if (dx * dx + dy * dy < DF::kMinDirLenSq) return;

    const float centerRad = std::atan2(dy, dx);
    const float t         = (std::clamp)(ctx.hitIndicatorTimer / HudTiming::kHitIndicatorDuration, 0.0f, 1.0f);
    const float alpha     = SmoothStep01(t);
    if (alpha <= DF::kMinVisibleAlpha) return;

    const UIVec2 center(duck2D.x, duck2D.y);
    const float arcRadius = (DF::kArcBaseRadius + (1.0f - t) * DF::kArcExpandRadius) * uiScale;
    const float startRad  = centerRad - DF::kArcHalfSpanRad;
    const float endRad    = centerRad + DF::kArcHalfSpanRad;

    uint32_t coreCol = 0;
    for (size_t i = 0; i < std::size(DF::kArcLayers); ++i)
    {
        const auto& layer = DF::kArcLayers[i];
        const uint32_t layerCol = UI_COL32(layer.r, layer.g, layer.b, static_cast<int>(alpha * layer.alphaScale));
        if (i == DF::kCoreLayerIndex) coreCol = layerCol;

        drawList->PathClear();
        drawList->PathArcTo(center, arcRadius, startRad, endRad, DF::kArcSegments);
        drawList->PathStroke(layerCol, 0, layer.thickness * uiScale);
    }

    // アーク中央外周に被弾元を指す鋭いポインター三角を添える
    const float dirX  = std::cos(centerRad);
    const float dirY  = std::sin(centerRad);
    const float perpX = -dirY;
    const float perpY = dirX;
    const float baseR = arcRadius + DF::kPointerBaseOffR * uiScale;
    const float tipR  = arcRadius + DF::kPointerTipOffR * uiScale;
    const float halfW = DF::kPointerHalfW * uiScale;

    const UIVec2 tip(center.x + dirX * tipR, center.y + dirY * tipR);
    const UIVec2 bL(center.x + dirX * baseR + perpX * halfW, center.y + dirY * baseR + perpY * halfW);
    const UIVec2 bR(center.x + dirX * baseR - perpX * halfW, center.y + dirY * baseR - perpY * halfW);
    drawList->AddTriangleFilled(tip, bL, bR, coreCol);
}

void GamePlayHUD::DrawLowHpRedVignetteEffect(const GamePlayHUDContext& ctx)
{
    if (!ctx.player || ctx.isDeathSequenceActive) return;

    namespace DF = HudLayout::DamageFeedback;
    const float hpRatio = ctx.player->GetHPRatio();
    if (hpRatio >= DF::kLowHpThreshold) return;

    HudViewport vpInfo{};
    if (!AcquireHudViewport(vpInfo)) return;
    TacticalDrawList* drawList = vpInfo.dl;
    const float screenW = vpInfo.screenW;
    const float screenH = vpInfo.screenH;

    const float dangerFactor = (DF::kLowHpThreshold - hpRatio) / DF::kLowHpThreshold;
    const float timeNow      = static_cast<float>(TacticalUICanvas::GetInstance()->GetTime());
    const float pulse        = DF::kPulseBase + DF::kPulseAmp * std::sin(timeNow * (HudTiming::kVignetteBaseFreq + dangerFactor * HudTiming::kVignetteDangerFreqMul));
    const int edgeAlpha      = static_cast<int>(DF::kMaxEdgeAlpha * dangerFactor * pulse);

    const float edgeThickness = (std::min)(screenW, screenH) * DF::kEdgeThickRatio;
    const uint32_t colOpaque  = UI_COL32(DF::kVignetteR, DF::kVignetteG, DF::kVignetteB, edgeAlpha);
    const uint32_t colClear   = UI_COL32(DF::kVignetteR, DF::kVignetteG, DF::kVignetteB, 0);
    drawList->AddRectFilledMultiColor(UIVec2(0.0f, 0.0f), UIVec2(screenW, edgeThickness), colOpaque, colOpaque, colClear, colClear);
    drawList->AddRectFilledMultiColor(UIVec2(0.0f, screenH - edgeThickness), UIVec2(screenW, screenH), colClear, colClear, colOpaque, colOpaque);
}

void GamePlayHUD::DrawDeathSequenceHUD(const GamePlayHUDContext& /*ctx*/)
{
    return;
}

// =============================================================================
// レーザーサイト ＆ 高精細3D浮遊コールアウト ＆ デバッグギズモ HUD
// =============================================================================

void GamePlayHUD::DrawLaserSight(const GamePlayHUDContext& ctx)
{
    if (!ctx.player || ctx.player->IsDead() || ctx.player->IsWeaponHolstered() ||
        ctx.player->IsReloading() || ctx.player->IsDodging() || ctx.isFullMapOpen ||
        !ctx.camera || ctx.isDeathSequenceActive)
    {
        return;
    }

    if (StingerTransition::GetInstance() && StingerTransition::GetInstance()->IsTransitioning()) return;
    if (DeathTransition::GetInstance() && DeathTransition::GetInstance()->IsTransitioning()) return;
    if (ctx.sceneEntranceFadeTimer > HudTiming::kLaserEntranceGateSec) return;

    HudViewport vpInfo{};
    if (!AcquireHudViewport(vpInfo)) return;
    TacticalDrawList* drawList = vpInfo.dl;
    const float screenW = vpInfo.screenW;
    const float screenH = vpInfo.screenH;
    namespace LS = HudLayout::LaserSight;

    const Matrix4x4& vp      = ctx.camera->GetViewProjectionMatrix();
    const float yaw          = ctx.player->GetRotation().y;
    const Vector3 forward    = { std::sin(yaw), 0.0f, std::cos(yaw) };
    // 弾丸生成原点と1mmの狂いもない3Dライフル銃口座標を共用
    const Vector3 muzzlePos  = ctx.player->GetMuzzleWorldPosition();
    const Vector2 reticlePos = ctx.player->GetReticleScreenPos();

    const float aimDist   = (std::clamp)(ctx.player->GetAimRayDistance(), LS::kMinAimDist, LS::kMaxAimDist);
    float closestWallDist = aimDist;
    bool blockedByWall    = false;

    if (ctx.obstacles)
    {
        for (const auto& obs : *ctx.obstacles)
        {
            if (!obs) continue;
            auto* box = obs->GetCollider();
            if (!box || !box->IsEnabled()) continue;

            // 回転配置されたコンテナや土嚢にも正確にレーザーが止まるようローカルOBB空間でスラブ判定
            const Vector3 center  = box->GetWorldPosition();
            const Vector3 extents = box->GetExtents();

            // 水平レーザー（forward.y == 0）のため、Y軸高さが交差しない床・川・上階屋根やXZ範囲外の障害物を三角関数計算前に即座に除外
            if (std::abs(muzzlePos.y - center.y) > extents.y) continue;

            const Vector3 relO       = muzzlePos - center;
            const float maxXZR       = extents.x + extents.z;
            const float projAlongRay = -(relO.x * forward.x + relO.z * forward.z);
            if (projAlongRay < -maxXZR || projAlongRay > closestWallDist + maxXZR) continue;
            const float perpDist     = std::abs(relO.x * forward.z - relO.z * forward.x);
            if (perpDist > maxXZR) continue;

            const float rotY   = box->GetWorldRotation().y;
            const bool hasRotY = std::abs(rotY) >= LS::kAxisEpsilon;
            const float c      = hasRotY ? std::cos(-rotY) : 1.0f;
            const float s      = hasRotY ? std::sin(-rotY) : 0.0f;

            const Vector3 localO = { relO.x * c + relO.z * s, relO.y, -relO.x * s + relO.z * c };
            const Vector3 localD = { forward.x * c + forward.z * s, forward.y, -forward.x * s + forward.z * c };

            float tMin = 0.0f;
            float tMax = closestWallDist;
            auto checkAxis = [&](float o, float d, float halfExt) -> bool {
                if (std::abs(d) < LS::kAxisEpsilon) return o >= -halfExt && o <= halfExt;
                const float invD = 1.0f / d;
                float t1 = (-halfExt - o) * invD;
                float t2 = (halfExt - o) * invD;
                if (t1 > t2) std::swap(t1, t2);
                tMin = (std::max)(tMin, t1);
                tMax = (std::min)(tMax, t2);
                return tMin <= tMax;
            };

            if (checkAxis(localO.x, localD.x, extents.x) &&
                checkAxis(localO.y, localD.y, extents.y) &&
                checkAxis(localO.z, localD.z, extents.z))
            {
                if (tMin > 0.0f && tMin < closestWallDist - LS::kWallHitBias)
                {
                    closestWallDist = tMin;
                    blockedByWall   = true;
                }
            }
        }
    }

    // 敵兵・標的へのレーザー照射ロックオン判定（照準フィードバック強化）
    const float maxTargetDist = blockedByWall ? closestWallDist : (aimDist + LS::kTargetOvershoot);
    float closestHitDist      = closestWallDist;
    bool lockedOnTarget       = false;

    auto checkSphereHit = [&](const Vector3& center, float radius) {
        const Vector3 m  = muzzlePos - center;
        const float b    = m.x * forward.x + m.z * forward.z;
        const float cVal = (m.x * m.x + m.z * m.z) - radius * radius;
        if (cVal > 0.0f && b > 0.0f) return;
        const float discr = b * b - cVal;
        if (discr < 0.0f) return;
        const float t = -b - std::sqrt(discr);
        if (t > LS::kMinSphereHitDist && t < maxTargetDist)
        {
            lockedOnTarget = true;
            if (t < closestHitDist)
            {
                closestHitDist = t;
                blockedByWall  = false;
            }
        }
    };

    if (ctx.enemies)
    {
        for (const auto& e : *ctx.enemies)
        {
            if (e && !e->IsDead()) checkSphereHit(e->GetPosition(), GameConfig::Combat::kEnemyHitRadius * LS::kEnemyHitRadMul);
        }
    }
    if (ctx.targets)
    {
        for (const auto& t : *ctx.targets)
        {
            if (t && !t->IsDead()) checkSphereHit(t->GetPosition(), t->GetRadius() * LS::kTargetHitRadMul);
        }
    }
    if (ctx.lootProps)
    {
        // 未回収の破壊可能木箱も照準時にロックオン反応させ、「撃って壊せる」手応えを視覚的に提示
        for (const auto& prop : *ctx.lootProps)
        {
            if (prop.isLooted || !prop.isBreakableCrate) continue;
            const Vector3 m  = muzzlePos - prop.position;
            const float b    = m.x * forward.x + m.z * forward.z;
            const float cVal = (m.x * m.x + m.z * m.z) - 0.72f * 0.72f;
            if (cVal > 0.0f && b > 0.0f) continue;
            const float discr = b * b - cVal;
            if (discr < 0.0f) continue;
            const float t = -b - std::sqrt(discr);
            if (t > LS::kMinSphereHitDist && t <= closestWallDist + 0.55f)
            {
                lockedOnTarget = true;
                if (t < closestHitDist) closestHitDist = t;
                blockedByWall = false;
            }
        }
    }

    laserLockedOnTarget_ = lockedOnTarget;

    // レーザーがレティクルを突き抜けて奥へ伸びないよう aimDist で終端クランプ
    const float renderDist = (std::min)(closestHitDist, aimDist);
    const Vector3 laserEnd = muzzlePos + forward * renderDist;
    Vector2 m2D{}, e2D{};
    if (Project3DTo2D(muzzlePos, vp, screenW, screenH, m2D) && Project3DTo2D(laserEnd, vp, screenW, screenH, e2D))
    {
        const bool reachesReticle = (!blockedByWall && renderDist >= aimDist - LS::kReticleSnapBias);
        if (reachesReticle)
        {
            // 非遮蔽時は浮動小数点丸め誤差すら排除し、レティクル中心ドットと完全同一ピクセルに終端ロック
            e2D = reticlePos;
        }

        const uint32_t glowCol = lockedOnTarget ? LS::kGlowLocked : LS::kGlowNormal;
        const uint32_t coreCol = lockedOnTarget ? LS::kCoreLocked : LS::kCoreNormal;
        const uint32_t dotCol  = lockedOnTarget ? LS::kDotLocked  : LS::kDotNormal;

        // 2層レーザービーム（外周ソフトグロー ＋ シャープな高輝度コアライン）
        drawList->AddLine(UIVec2(m2D.x, m2D.y), UIVec2(e2D.x, e2D.y), glowCol, lockedOnTarget ? LS::kGlowThickLocked : LS::kGlowThickNormal);
        drawList->AddLine(UIVec2(m2D.x, m2D.y), UIVec2(e2D.x, e2D.y), coreCol, lockedOnTarget ? LS::kCoreThickLocked : LS::kCoreThickNormal);
        drawList->AddCircleFilled(UIVec2(m2D.x, m2D.y), LS::kMuzzleDotRad, dotCol, LS::kMuzzleDotSegs);

        if (!reachesReticle)
        {
            // 手前の壁や敵でレーザーが止まっている場合は着弾輝点を表示し、壁遮蔽時はレティクルまでの減衰ガイド線を描画
            if (blockedByWall)
            {
                drawList->AddLine(UIVec2(e2D.x, e2D.y), UIVec2(reticlePos.x, reticlePos.y), LS::kWallGuideCol, LS::kWallGuideThick);
                drawList->AddCircleFilled(UIVec2(e2D.x, e2D.y), LS::kWallDotRad, LS::kWallDotCol, LS::kImpactDotSegs);
            }
            else
            {
                drawList->AddCircleFilled(UIVec2(e2D.x, e2D.y), LS::kHitDotRad, dotCol, LS::kImpactDotSegs);
            }
        }
    }
}

void GamePlayHUD::DrawTacticalReticle(const GamePlayHUDContext& ctx)
{
    if (!ctx.player || ctx.player->IsDead() || ctx.isDeathSequenceActive || ctx.isGameCleared) return;
    if (StingerTransition::GetInstance() && StingerTransition::GetInstance()->IsTransitioning()) return;
    if (DeathTransition::GetInstance() && DeathTransition::GetInstance()->IsTransitioning()) return;
    if (ctx.sceneEntranceFadeTimer > HudTiming::kReticleEntranceGateSec) return;

    HudViewport vpInfo{};
    if (!AcquireHudViewport(vpInfo, HudLayout::Common::kReticleScaleMin, HudLayout::Common::kReticleScaleMax)) return;
    TacticalDrawList* dl = vpInfo.dl;
    const float uiScale  = vpInfo.uiScale;
    namespace RT = HudLayout::Reticle;

    // 全体マップ展開中はマップ操作用のクリーンなポインタークロスヘアを生マウス座標に描画
    if (ctx.isFullMapOpen)
    {
        const UIVec2 m   = TacticalUICanvas::GetInstance()->GetMousePos();
        const float span = RT::kMapCursorBaseR * uiScale * RT::kMapCursorSpanMul;
        dl->AddLine(UIVec2(m.x - span, m.y), UIVec2(m.x + span, m.y), RT::kMapShadowCol, RT::kMapShadowThick * uiScale);
        dl->AddLine(UIVec2(m.x, m.y - span), UIVec2(m.x, m.y + span), RT::kMapShadowCol, RT::kMapShadowThick * uiScale);
        dl->AddLine(UIVec2(m.x - span, m.y), UIVec2(m.x + span, m.y), RT::kMapFgCol, RT::kMapFgThick * uiScale);
        dl->AddLine(UIVec2(m.x, m.y - span), UIVec2(m.x, m.y + span), RT::kMapFgCol, RT::kMapFgThick * uiScale);
        DrawDiamondPip(dl, m.x, m.y, RT::kMapPipRad * uiScale, RT::kMapFgCol, RT::kMapShadowCol);
        return;
    }

    const Vector2 rPos = ctx.player->GetReticleScreenPos();
    const UIVec2 c(rPos.x, rPos.y);

    // 1. [C] 銃収納ダッシュ中：コンパクトなスプリント照準ブラケット
    if (ctx.player->IsWeaponHolstered())
    {
        DrawCornerBrackets(
            dl, c, RT::kHolsterGap * uiScale, -RT::kHolsterArm * uiScale,
            RT::kShadowCol, RT::kHolsterShadowThick * uiScale,
            RT::kHolsterCol, RT::kHolsterFgThick * uiScale);
        dl->AddCircleFilled(c, RT::kHolsterDotShadowR * uiScale, RT::kShadowCol, RT::kSmallDotSegs);
        dl->AddCircleFilled(c, RT::kHolsterDotFgR * uiScale, RT::kHolsterCol, RT::kSmallDotSegs);
        return;
    }

    const float timeNow       = static_cast<float>(TacticalUICanvas::GetInstance()->GetTime());
    const float alertRatio    = (std::clamp)(ctx.player->GetEmptyTriggerAlertTimer() / 0.34f, 0.0f, 1.0f);
    const float completeRatio = (std::clamp)(ctx.player->GetReloadCompleteFlashTimer() / RT::kCompleteFlashDuration, 0.0f, 1.0f);
    const float minClampX     = RT::kPromptClampPadX * uiScale;
    const float maxClampX     = vpInfo.screenW - minClampX;

    // 2. リロード中：視線を外さず完了タイミングが把握できる3段階カラー円形アーク＋レティクル直下進捗ピル
    if (ctx.player->IsReloading())
    {
        const float prog = (std::clamp)(ctx.player->GetReloadProgress(), 0.0f, 1.0f);
        const float rad  = (RT::kReloadRadius + alertRatio * RT::kReloadBusyExpandPx) * uiScale;
        const uint32_t stgCol = (prog < HudTiming::kReloadEjectEnd) ? RT::kReloadStageEjectCol
                              : ((prog < HudTiming::kReloadSeatTickRatio) ? RT::kReloadStageInsCol : RT::kReloadStageBoltCol);

        dl->AddCircle(c, rad, RT::kReloadBgCol, RT::kReloadSegs, RT::kReloadBgThick * uiScale);
        dl->AddCircle(c, rad, RT::kReloadTrackCol, RT::kReloadSegs, RT::kReloadTrackThick * uiScale);
        if (prog > RT::kReloadMinProg)
        {
            const float a0 = -kPi * 0.5f;
            const float a1 = a0 + kPi * 2.0f * prog;
            dl->PathClear();
            dl->PathArcTo(c, rad, a0, a1, RT::kReloadSegs);
            dl->PathStroke(stgCol, 0, RT::kReloadArcThick * uiScale);
        }
        dl->AddCircleFilled(c, RT::kReloadDotShadowR * uiScale, RT::kShadowCol, RT::kSmallDotSegs);
        dl->AddCircleFilled(c, RT::kReloadDotFgR * uiScale, stgCol, RT::kSmallDotSegs);

        // レティクル直下のコンパクト進捗ラベル＋ミニプログレスバー
        const float pillH = RT::kPromptSubH * uiScale;
        const float topY  = (std::clamp)(c.y + RT::kPromptOffsetY * uiScale, RT::kPromptClampTopY * uiScale, vpInfo.screenH - RT::kPromptClampBotY * uiScale - pillH);
        const float shakeX = (alertRatio > 0.0f) ? (std::sin(timeNow * RT::kAlertShakeFreq) * 3.5f * uiScale * alertRatio) : 0.0f;
        char relLabel[HudLayout::Common::kSmallBufSize];
        std::snprintf(relLabel, sizeof(relLabel), AsUtf8(u8"リロード中 %d%%"), static_cast<int>(prog * 100.0f));
        const float fPx = RT::kPromptSubFont * uiScale;
        const UIVec2 sz = MeasureSharpText(UIFontRole::SubBoldJP, fPx, relLabel);
        const float bw  = sz.x + 18.0f * uiScale;
        const float bx  = std::round((std::clamp)(c.x + shakeX - bw * 0.5f, minClampX, (std::max)(minClampX, maxClampX - bw)));
        DrawAccentedTag(dl, bx, topY, bw, pillH, RT::kPromptSlant * uiScale, 1.5f, 2.0f, HudPalette::kPanelShadow130,
            RT::kReloadPillBg, stgCol, 1.2f, 3.0f * uiScale, fPx, 1.5f * uiScale, HudPalette::kTextBrightWhite, relLabel);
        DrawProgressBar(dl, bx, topY + pillH + RT::kPromptReloadBarGapY * uiScale, bw, RT::kPromptReloadBarH * uiScale,
            RT::kPromptSlant * 0.45f * uiScale, prog, HudLayout::AmmoPanel::kReloadBarBg, stgCol);
        return;
    }

    // 3. 通常射撃構え ＆ リコイル連動タクティカルクロスヘア（レーザー終端・弾丸着弾点と 0.00px 完全一致）
    const float kick       = (std::clamp)(ctx.player->GetRecoilKickAmount(), 0.0f, 1.0f);
    const int magAmmo      = ctx.player->GetMagazineAmmo();
    const int resAmmo      = ctx.player->GetReserveAmmo();
    const bool isEmptyMag  = (magAmmo <= 0);
    const bool isCancelled = (ctx.player->GetReloadCancelledTimer() > 0.0f);

    uint32_t mainCol = RT::kMainDefaultCol;
    uint32_t dotCol  = RT::kDotDefaultCol;
    if (isEmptyMag)
    {
        mainCol = RT::kMainEmptyCol;
        dotCol  = RT::kDotEmptyCol;
    }
    else if (laserLockedOnTarget_)
    {
        mainCol = (kick > RT::kLockedKickThresh) ? RT::kMainLockKickCol : RT::kMainLockCol;
        dotCol  = RT::kDotLockCol;
    }
    else if (kick > RT::kNormalKickThresh)
    {
        mainCol = RT::kMainKickCol;
        dotCol  = RT::kDotKickCol;
    }

    const float gap     = (RT::kBaseGap + kick * RT::kKickGapMul) * uiScale;
    const float tickLen = (RT::kBaseTickLen + kick * RT::kKickTickMul) * uiScale;
    const float bgThick = RT::kTickShadowThick * uiScale;
    const float fgThick = RT::kTickFgThick * uiScale;

    // 4方向タクティカルティック（暗色アウトライン＋高輝度コア）
    for (const UIVec2& dir : RT::kCrossDirs)
    {
        const UIVec2 p0(c.x + dir.x * gap, c.y + dir.y * gap);
        const UIVec2 p1(c.x + dir.x * (gap + tickLen), c.y + dir.y * (gap + tickLen));
        dl->AddLine(p0, p1, RT::kShadowCol, bgThick);
        dl->AddLine(p0, p1, mainCol, fgThick);
    }

    // 弾切れ時は45度警告コーナーブラケットを脈動表示、通常ロックオン時はロックオンコーナーを表示
    if (isEmptyMag)
    {
        const float emptyPulse = 0.5f + 0.5f * std::sin(timeNow * RT::kEmptyPulseSpeed);
        const float emptyOff   = (RT::kEmptyBracketBaseOff + alertRatio * RT::kEmptyBracketAlertAdd) * uiScale;
        const uint32_t brkCol  = (alertRatio > 0.15f || emptyPulse > 0.55f) ? RT::kEmptyPillBdHi : RT::kMainEmptyCol;
        DrawCornerBrackets(
            dl, c, emptyOff, -RT::kEmptyBracketArm * uiScale,
            RT::kShadowCol, RT::kLockShadowThick * uiScale,
            brkCol, RT::kLockFgThick * uiScale);
    }
    else if (completeRatio > 0.02f)
    {
        const float compOff = (RT::kCompleteBracketOff + (1.0f - completeRatio) * 3.5f) * uiScale;
        DrawCornerBrackets(
            dl, c, compOff, -RT::kCompleteBracketArm * uiScale,
            ScaleHudAlpha(RT::kShadowCol, completeRatio), RT::kLockShadowThick * uiScale,
            ScaleHudAlpha(RT::kCompleteCol, completeRatio), RT::kLockFgThick * uiScale);
    }
    else if (laserLockedOnTarget_)
    {
        const float diagOff = (gap + RT::kLockDiagPad * uiScale) * RT::kLockDiagMul;
        DrawCornerBrackets(
            dl, c, diagOff, RT::kLockArmLen * uiScale,
            RT::kShadowCol, RT::kLockShadowThick * uiScale,
            mainCol, RT::kLockFgThick * uiScale);
    }

    // 精密センタードット（レーザー終端および実弾の通過点と完全一致）
    dl->AddCircleFilled(c, RT::kCenterDotShadowR * uiScale, RT::kShadowCol, RT::kCenterDotSegs);
    dl->AddCircleFilled(c, RT::kCenterDotFgR * uiScale, dotCol, RT::kCenterDotSegs);

    // 4. レティクル直下：視線移動ゼロで気づける `[R] リロード` 警告バッジ ／ 残弾少ヒント ／ 装填完了フラッシュ
    if (isEmptyMag)
    {
        const float emptyPulse = 0.5f + 0.5f * std::sin(timeNow * RT::kEmptyPulseSpeed);
        const float shakeX     = (alertRatio > 0.0f) ? (std::sin(timeNow * RT::kAlertShakeFreq) * RT::kAlertShakeAmpPx * uiScale * alertRatio) : 0.0f;
        const float pillH      = RT::kPromptEmptyH * uiScale;
        const float topY       = (std::clamp)(c.y + RT::kPromptOffsetY * uiScale, RT::kPromptClampTopY * uiScale, vpInfo.screenH - RT::kPromptClampBotY * uiScale - pillH);
        const uint32_t bdCol   = (alertRatio > 0.15f || emptyPulse > 0.55f) ? RT::kEmptyPillBdHi : RT::kEmptyPillBdLo;

        if (resAmmo > 0)
        {
            DrawKeyPromptPill(dl, c.x + shakeX, topY, pillH, RT::kPromptSlant * uiScale, uiScale,
                "R", AsUtf8(u8"リロード"), RT::kPromptKeyFont * uiScale, RT::kPromptEmptyFont * uiScale,
                RT::kEmptyPillBg, bdCol, RT::kEmptyKeyBg, RT::kEmptyKeyBd, RT::kEmptyKeyTx, RT::kEmptyTextCol,
                minClampX, maxClampX);
        }
        else
        {
            DrawKeyPromptPill(dl, c.x + shakeX, topY, pillH, RT::kPromptSlant * uiScale, uiScale,
                "", AsUtf8(u8"弾薬切れ // 木箱を破壊"), RT::kPromptKeyFont * uiScale, RT::kPromptEmptyFont * uiScale,
                RT::kEmptyPillBg, HudPalette::kDangerRed, RT::kEmptyKeyBg, HudPalette::kDangerRed, RT::kEmptyTextCol, RT::kEmptyTextCol,
                minClampX, maxClampX);
        }
    }
    else if (isCancelled && resAmmo > 0)
    {
        const float pillH = RT::kPromptSubH * uiScale;
        const float topY  = (std::clamp)(c.y + RT::kPromptOffsetY * uiScale, RT::kPromptClampTopY * uiScale, vpInfo.screenH - RT::kPromptClampBotY * uiScale - pillH);
        DrawKeyPromptPill(dl, c.x, topY, pillH, RT::kPromptSlant * uiScale, uiScale,
            "R", AsUtf8(u8"中断 // リロード"), RT::kPromptSubKeyFont * uiScale, RT::kPromptSubFont * uiScale,
            RT::kEmptyPillBg, RT::kEmptyPillBdHi, RT::kEmptyKeyBg, RT::kEmptyKeyBd, RT::kEmptyKeyTx, RT::kEmptyTextCol,
            minClampX, maxClampX);
    }
    else if (magAmmo <= RT::kReticleLowAmmoThreshold && resAmmo > 0)
    {
        const float pillH = RT::kPromptSubH * uiScale;
        const float topY  = (std::clamp)(c.y + RT::kPromptOffsetY * uiScale, RT::kPromptClampTopY * uiScale, vpInfo.screenH - RT::kPromptClampBotY * uiScale - pillH);
        char lowBuf[HudLayout::Common::kSmallBufSize];
        std::snprintf(lowBuf, sizeof(lowBuf), AsUtf8(u8"残弾%d // リロード"), magAmmo);
        DrawKeyPromptPill(dl, c.x, topY, pillH, RT::kPromptSlant * uiScale, uiScale,
            "R", lowBuf, RT::kPromptSubKeyFont * uiScale, RT::kPromptSubFont * uiScale,
            RT::kLowPillBg, RT::kLowPillBd, RT::kLowKeyBg, RT::kLowPillBd, RT::kEmptyKeyTx, RT::kLowTextCol,
            minClampX, maxClampX, 0.92f);
    }
    else if (completeRatio > 0.04f)
    {
        const float pillH = RT::kPromptSubH * uiScale;
        const float topY  = (std::clamp)(c.y + RT::kPromptOffsetY * uiScale, RT::kPromptClampTopY * uiScale, vpInfo.screenH - RT::kPromptClampBotY * uiScale - pillH);
        const char* okTxt = AsUtf8(u8"装填完了！");
        const float fPx   = RT::kPromptSubFont * uiScale;
        const UIVec2 sz   = MeasureSharpText(UIFontRole::SubBoldJP, fPx, okTxt);
        const float bw    = sz.x + 18.0f * uiScale;
        const float bx    = std::round((std::clamp)(c.x - bw * 0.5f, minClampX, (std::max)(minClampX, maxClampX - bw)));
        DrawAccentedTag(dl, bx, topY, bw, pillH, RT::kPromptSlant * uiScale, 0.0f, 0.0f, 0,
            ScaleHudAlpha(RT::kCompletePillBg, completeRatio), ScaleHudAlpha(RT::kCompleteCol, completeRatio), 1.15f,
            3.0f * uiScale, fPx, 1.5f * uiScale, ScaleHudAlpha(RT::kCompleteCol, completeRatio), okTxt);
    }
}

void GamePlayHUD::DrawFloatingTexts(const GamePlayHUDContext& ctx)
{
    if (!ctx.camera || !ctx.floatingTexts) return;

    HudViewport vpInfo{};
    if (!AcquireHudViewport(vpInfo)) return;
    TacticalDrawList* dl = vpInfo.dl;
    const float screenW  = vpInfo.screenW;
    const float screenH  = vpInfo.screenH;
    const float uiScale  = vpInfo.uiScale;
    namespace FC = HudLayout::FloatingCallout;

    const Matrix4x4& vp = ctx.camera->GetViewProjectionMatrix();
    const float pillFont = FC::kPillFont * uiScale;

    for (const auto& ft : *ctx.floatingTexts)
    {
        if (ft.lifeTime <= 0.0f) continue;

        Vector2 p2D{};
        if (!Project3DTo2D(ft.position, vp, screenW, screenH, p2D)) continue;

        const float alpha  = (ft.maxLifeTime > 0.0f) ? (std::clamp)(ft.lifeTime / ft.maxLifeTime, 0.0f, 1.0f) : 1.0f;
        const int aByte    = static_cast<int>(alpha * HudPalette::kMaxAlphaFloat);
        const int bgA      = static_cast<int>(alpha * FC::kBgAlphaScale);
        const uint32_t col = UI_COL32(
            static_cast<int>((std::clamp)(ft.color.x, 0.0f, 1.0f) * HudPalette::kMaxAlphaFloat),
            static_cast<int>((std::clamp)(ft.color.y, 0.0f, 1.0f) * HudPalette::kMaxAlphaFloat),
            static_cast<int>((std::clamp)(ft.color.z, 0.0f, 1.0f) * HudPalette::kMaxAlphaFloat), aByte);

        // 1. 専用コールアウトバッジ判定（1:1 ピクセルスナップ TrueType ピルプレート）
        bool matchedBadge = false;
        for (const auto& badge : FC::kBadges)
        {
            if (ft.text.find(badge.matchToken) != std::string::npos)
            {
                const char* label = AsUtf8(badge.displayLabel);
                const UIVec2 sz   = MeasureSharpText(UIFontRole::SubBoldJP, pillFont, label);
                const float bw    = sz.x + FC::kPillPadW * uiScale;
                const float bh    = sz.y + FC::kPillPadH * uiScale;
                DrawAccentedTag(dl, p2D.x - bw * 0.5f, p2D.y - bh * 0.5f, bw, bh, FC::kPillSlant * uiScale,
                    0.0f, 0.0f, 0, UI_COL32(badge.bgR, badge.bgG, badge.bgB, bgA),
                    UI_COL32(badge.bdR, badge.bdG, badge.bdB, aByte), FC::kPillBorderThick,
                    0.0f, pillFont, FC::kPillTextShiftX * uiScale, UI_COL32(badge.txR, badge.txG, badge.txB, aByte), label);
                matchedBadge = true;
                break;
            }
        }
        if (matchedBadge) continue;

        // 2. ダメージ値・回復・戦利品コールアウトをネイティブボールド TrueType で鮮明描画
        const UIFontRole fRole = ft.isCritical ? UIFontRole::TitleBoldJP : UIFontRole::SubBoldJP;
        const float fSize      = (ft.isCritical ? FC::kCritFont : FC::kNormalFont) * uiScale;
        DrawShadowedTextAligned(dl, fRole, fSize, p2D.x, p2D.y, col, ft.text.c_str(), HudTextAlign::Center, FC::kTextShadowScale);

        if (ft.isCritical && ft.text.size() <= FC::kMaxCritSubLen)
        {
            DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, FC::kCritSubFont * uiScale,
                p2D.x, p2D.y - fSize * FC::kCritSubOffsetMul,
                UI_COL32(FC::kCritSubR, FC::kCritSubG, FC::kCritSubB, aByte), "CRITICAL", HudTextAlign::Center, FC::kTextShadowScale);
        }
    }
}

void GamePlayHUD::DrawVisionConesAndGizmos(const GamePlayHUDContext& ctx)
{
#if APP_DEBUG_UI
    if (!ctx.showDebugGizmos || !ctx.camera) return;

    HudViewport vpInfo{};
    if (!AcquireHudViewport(vpInfo)) return;
    TacticalDrawList* drawList = vpInfo.dl;
    const float screenW = vpInfo.screenW;
    const float screenH = vpInfo.screenH;
    const Matrix4x4& vp = ctx.camera->GetViewProjectionMatrix();
    namespace DH = HudLayout::DebugHud;

    if (ctx.player && ctx.playerSoundTimer > 0.0f)
    {
        const float ratio    = ctx.playerSoundTimer / HudTiming::kSoundGizmoDuration;
        const float curR     = ctx.playerSoundMaxRadius * (1.0f - ratio);
        const Vector3 pCenter = ctx.player->GetPosition();
        const uint32_t col   = UI_COL32(DH::kSoundR, DH::kSoundG, DH::kSoundB, static_cast<int>(ratio * DH::kSoundAlphaScale));

        for (int i = 0; i < DH::kSoundRingSegs; ++i)
        {
            const float a1   = (static_cast<float>(i) / static_cast<float>(DH::kSoundRingSegs)) * kTwoPi;
            const float a2   = (static_cast<float>(i + 1) / static_cast<float>(DH::kSoundRingSegs)) * kTwoPi;
            const Vector3 p1 = pCenter + Vector3{ std::cos(a1) * curR, DH::kSoundRingLiftY, std::sin(a1) * curR };
            const Vector3 p2 = pCenter + Vector3{ std::cos(a2) * curR, DH::kSoundRingLiftY, std::sin(a2) * curR };
            Vector2 s1{}, s2{};
            if (Project3DTo2D(p1, vp, screenW, screenH, s1) && Project3DTo2D(p2, vp, screenW, screenH, s2))
            {
                drawList->AddLine(UIVec2(s1.x, s1.y), UIVec2(s2.x, s2.y), col, DH::kSoundLineThick);
            }
        }
    }
#else
    (void)ctx;
#endif
}

void GamePlayHUD::DrawPerformanceTrackerUI(const GamePlayHUDContext& ctx, float deltaTime)
{
#if APP_DEBUG_UI
    if (!ctx.showPerformanceTracker) return;

    HudViewport vpInfo{};
    if (!AcquireHudViewport(vpInfo)) return;
    TacticalDrawList* dl = vpInfo.dl;
    namespace DH = HudLayout::DebugHud;

    const float px = DH::kPanelX;
    const float py = DH::kPanelY;
    const float pw = DH::kPanelW;
    const float ph = DH::kPanelH;
    const float textX = px + DH::kTextPadX;

    DrawSlantedPlate(dl, px, py, pw, ph, DH::kPanelSlant, DH::kPanelBg, DH::kPanelBorder, DH::kPanelBorderThick);
    dl->AddLine(
        UIVec2(px + DH::kHeaderLineLeftPad, py + DH::kHeaderLineY),
        UIVec2(px + pw - DH::kHeaderLineRightPad, py + DH::kHeaderLineY),
        DH::kHeaderLineCol, DH::kHeaderLineThick);
    dl->AddText(UIVec2(textX, py + DH::kTitleOffsetY), DH::kTitleCol, "TACTICAL TELEMETRY // F3");

    const float fps = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;
    char lineBuf[HudLayout::Common::kLineBufSize];
    std::snprintf(lineBuf, sizeof(lineBuf), "FPS: %.1f   (%.2f ms)", fps, deltaTime * HudTiming::kMsPerSecond);
    dl->AddText(UIVec2(textX, py + DH::kFpsOffsetY), DH::kFpsCol, lineBuf);

    if (ctx.player)
    {
        std::snprintf(lineBuf, sizeof(lineBuf), "HP: %.0f / %.0f   MED: %d",
            ctx.player->GetHP(), ctx.player->GetMaxHP(), ctx.player->GetMedkitCount());
        dl->AddText(UIVec2(textX, py + DH::kHpOffsetY), DH::kStatCol, lineBuf);

        std::snprintf(lineBuf, sizeof(lineBuf), "AMMO: %d / %d (RES %d)",
            ctx.player->GetMagazineAmmo(), ctx.player->GetMaxMagazineAmmo(), ctx.player->GetReserveAmmo());
        dl->AddText(UIVec2(textX, py + DH::kAmmoOffsetY), DH::kStatCol, lineBuf);

        std::snprintf(lineBuf, sizeof(lineBuf), "LOOT VALUE: %d RUB", ctx.player->GetLootValue());
        dl->AddText(UIVec2(textX, py + DH::kLootOffsetY), DH::kLootCol, lineBuf);
    }

    std::snprintf(lineBuf, sizeof(lineBuf), "PROPS: %zu   STRESS: %s (%d)",
        ctx.lootProps ? ctx.lootProps->size() : 0,
        ctx.isStressTestActive ? "ON" : "OFF",
        ctx.stressTestCount);
    dl->AddText(UIVec2(textX, py + DH::kPropsOffsetY), DH::kPropsCol, lineBuf);
#else
    (void)ctx;
    (void)deltaTime;
#endif
}

void GamePlayHUD::DrawTutorialSignHUD(const GamePlayHUDContext& ctx)
{
    if (!ctx.tutorialSigns || !ctx.camera || ctx.isFullMapOpen) return;

    // 物資箱のインタラクト圏内では物資カードを優先し、ポップアップの二重重なりを防ぐ
    if (ctx.player && ctx.lootProps)
    {
        const Vector3 pPos     = ctx.player->GetPosition();
        const float interactSq = GameConfig::Math::Square(LootSystem::kInteractRadius);
        for (const auto& prop : *ctx.lootProps)
        {
            if (prop.isLooted) continue;
            const float dx = prop.position.x - pPos.x;
            const float dz = prop.position.z - pPos.z;
            if (dx * dx + dz * dz <= interactSq) return;
        }
    }

    HudViewport vpInfo{};
    if (!AcquireHudViewport(vpInfo)) return;
    TacticalDrawList* dl = vpInfo.dl;
    const float screenW  = vpInfo.screenW;
    const float screenH  = vpInfo.screenH;
    const float uiScale  = vpInfo.uiScale;
    namespace TS = HudLayout::TutorialSign;

    const Matrix4x4& vp = ctx.camera->GetViewProjectionMatrix();
    const char* hdr     = AsUtf8(u8"FIELD GUIDE // 要点");

    for (const auto& sign : *ctx.tutorialSigns)
    {
        if (!sign || !sign->IsPlayerNear()) continue;

        Vector3 signPos = sign->GetPosition();
        signPos.y += TS::kSignLiftY;

        Vector2 screenPos{};
        const bool onScreen = Project3DTo2D(signPos, vp, screenW, screenH, screenPos);

        if (!onScreen ||
            screenPos.x < TS::kScreenBoundX || screenPos.x > screenW - TS::kScreenBoundX ||
            screenPos.y < TS::kScreenBoundTop || screenPos.y > screenH - TS::kScreenBoundBot)
        {
            screenPos = { screenW * 0.5f, TS::kFallbackTopY * uiScale };
        }

        const std::string msg = sign->GetMessage();
        const float fontPx    = TS::kFont * uiScale;
        const UIVec2 hdrSz    = MeasureSharpText(UIFontRole::SubBoldJP, fontPx, hdr);
        const UIVec2 msgSz    = MeasureSharpText(UIFontRole::DefaultJP, fontPx, msg.c_str());

        const float padX = TS::kPadX * uiScale;
        const float padY = TS::kPadY * uiScale;
        const float boxW = (std::max)(hdrSz.x, msgSz.x) + padX * 2.0f;
        const float boxH = hdrSz.y + msgSz.y + padY * 2.0f + TS::kInnerGapH * uiScale;

        auto computeHorizontalBounds = [&](float boxTopY, float& outMinBx, float& outMaxBx) {
            outMinBx = (boxTopY < TS::kMinimapAvoidMaxY * uiScale)
                ? (TS::kMinimapAvoidMinX * uiScale)
                : (TS::kDefaultSideMargin * uiScale);
            outMaxBx = (boxTopY < TS::kQuestAvoidMaxY * uiScale)
                ? (screenW - boxW - TS::kQuestAvoidRightPad * uiScale)
                : (screenW - boxW - TS::kDefaultSideMargin * uiScale);
        };

        float by = (std::clamp)(
            screenPos.y - boxH - TS::kAboveSignGapY * uiScale,
            TS::kMinBoxY * uiScale,
            screenH - boxH - TS::kMaxBoxBotPad * uiScale);
        float minBx = 0.0f, maxBx = 0.0f;
        computeHorizontalBounds(by, minBx, maxBx);
        if (maxBx < minBx)
        {
            minBx = TS::kFallbackSideMargin;
            maxBx = (std::max)(TS::kFallbackSideMargin, screenW - boxW - TS::kFallbackSideMargin);
        }
        float bx = (std::clamp)(screenPos.x - boxW * 0.5f, minBx, maxBx);

        // 画面中央のプレイヤーキャラ周辺と重なる場合は上方の安全帯へ退避
        const float duckCx      = screenW * 0.5f;
        const float duckZoneTop = screenH * TS::kDuckZoneTopRatio;
        const float duckZoneBot = screenH * TS::kDuckZoneBotRatio;
        if (bx + boxW > duckCx - TS::kDuckZoneHalfW * uiScale && bx < duckCx + TS::kDuckZoneHalfW * uiScale &&
            by + boxH > duckZoneTop && by < duckZoneBot)
        {
            by = (std::max)(TS::kMinBoxY * uiScale, duckZoneTop - boxH - TS::kDuckRetreatGapY * uiScale);
            computeHorizontalBounds(by, minBx, maxBx);
            if (maxBx >= minBx)
            {
                bx = (std::clamp)(bx, minBx, maxBx);
            }
        }

        const float slant = TS::kSlant * uiScale;
        DrawFramedPanel(dl, bx, by, boxW, boxH, slant, TS::kPanelStyle);
        DrawSlantedPlate(dl, bx, by, TS::kStripeW * uiScale, boxH, slant, TS::kStripeCol);

        DrawShadowedText(dl, UIFontRole::SubBoldJP, fontPx, UIVec2(bx + padX, by + padY), TS::kHeaderCol, hdr);
        const float divY = by + padY + hdrSz.y + TS::kDividerOffY * uiScale;
        dl->AddLine(
            UIVec2(bx + padX, divY),
            UIVec2(bx + boxW - padX + TS::kDividerExtraX * uiScale, divY),
            TS::kDividerCol, TS::kDividerThick);
        DrawShadowedText(
            dl, UIFontRole::DefaultJP, fontPx,
            UIVec2(bx + padX, by + padY + hdrSz.y + TS::kMsgOffY * uiScale),
            TS::kMessageCol, msg.c_str());
    }
}

// =============================================================================
// 3D地面ルート案内シェブロン・標的マーカー・画面端方向ガイド HUD
// =============================================================================

namespace
{
    void RefreshNavigationPath(
        std::vector<Vector3>& cachedPath, Vector3& cachedGoal, float& refreshTimer,
        float deltaTime, const Vector3& playerPos, const Vector3& goalPos)
    {
        namespace NV = HudLayout::Navigation;
        refreshTimer -= deltaTime;
        const float goalDiffX = goalPos.x - cachedGoal.x;
        const float goalDiffZ = goalPos.z - cachedGoal.z;
        if (refreshTimer <= 0.0f ||
            (goalDiffX * goalDiffX + goalDiffZ * goalDiffZ) > NV::kGoalMoveThreshSq ||
            cachedPath.empty())
        {
            refreshTimer = HudTiming::kGuideRefreshInterval;
            cachedGoal   = goalPos;

            const std::vector<Vector3> navNodes = BaziruEngine::AI::NavMesh::GetInstance()->FindPath(playerPos, goalPos);
            cachedPath.clear();
            cachedPath.push_back(playerPos);

            auto appendIfSeparated = [&](const Vector3& pt) {
                const Vector3& prev = cachedPath.back();
                const float dx = pt.x - prev.x;
                const float dz = pt.z - prev.z;
                if (dx * dx + dz * dz > NV::kMinNodeDistSq) cachedPath.push_back(pt);
            };
            for (const auto& pt : navNodes) appendIfSeparated(pt);
            appendIfSeparated(goalPos);
        }
        else
        {
            cachedPath[0] = playerPos;
        }
    }

    void BuildSmoothedGuidePath(
        const std::vector<Vector3>& rawPath,
        std::vector<Vector3>& smoothPath,
        std::vector<Vector3>& scratchPath)
    {
        namespace NV = HudLayout::Navigation;
        smoothPath.assign(rawPath.begin(), rawPath.end());
        if (smoothPath.size() < NV::kMinSmoothNodes) return;

        scratchPath.assign(smoothPath.begin(), smoothPath.end());
        for (size_t i = 1; i + 1 < smoothPath.size(); ++i)
        {
            scratchPath[i].x = smoothPath[i - 1].x * NV::kSmoothNeighborW
                             + smoothPath[i].x     * NV::kSmoothCenterW
                             + smoothPath[i + 1].x * NV::kSmoothNeighborW;
            scratchPath[i].z = smoothPath[i - 1].z * NV::kSmoothNeighborW
                             + smoothPath[i].z     * NV::kSmoothCenterW
                             + smoothPath[i + 1].z * NV::kSmoothNeighborW;
        }
        smoothPath.swap(scratchPath);

        for (int pass = 0; pass < NV::kChaikinPasses && smoothPath.size() < NV::kMaxSmoothPoints; ++pass)
        {
            scratchPath.clear();
            const size_t neededCap = smoothPath.size() + smoothPath.size();
            if (scratchPath.capacity() < neededCap) scratchPath.reserve(neededCap);
            scratchPath.push_back(smoothPath.front());

            for (size_t i = 0; i + 1 < smoothPath.size(); ++i)
            {
                const Vector3& p0 = smoothPath[i];
                const Vector3& p1 = smoothPath[i + 1];
                const float segDx = p1.x - p0.x;
                const float segDz = p1.z - p0.z;
                if (segDx * segDx + segDz * segDz < NV::kMinChaikinSegSq) continue;

                if (i > 0)
                {
                    scratchPath.push_back({ p0.x * NV::kChaikinNearW + p1.x * NV::kChaikinFarW, 0.0f,
                                            p0.z * NV::kChaikinNearW + p1.z * NV::kChaikinFarW });
                }
                if (i + 1 + 1 < smoothPath.size())
                {
                    scratchPath.push_back({ p0.x * NV::kChaikinFarW + p1.x * NV::kChaikinNearW, 0.0f,
                                            p0.z * NV::kChaikinFarW + p1.z * NV::kChaikinNearW });
                }
            }
            scratchPath.push_back(smoothPath.back());
            smoothPath.swap(scratchPath);
        }
    }

    void DrawGroundNavigationLane(
        TacticalDrawList* dl, const std::vector<Vector3>& smoothPath,
        const Vector3& goalPos, uint32_t accentCol,
        const Matrix4x4& vp, float screenW, float screenH, float uiScale, float timeNow)
    {
        namespace NV = HudLayout::Navigation;
        namespace C  = HudLayout::Common;

        float totalPathLen = 0.0f;
        for (size_t i = 0; i + 1 < smoothPath.size(); ++i)
        {
            const float dx = smoothPath[i + 1].x - smoothPath[i].x;
            const float dz = smoothPath[i + 1].z - smoothPath[i].z;
            totalPathLen += std::sqrt(dx * dx + dz * dz);
        }

        const uint8_t accR = static_cast<uint8_t>((accentCol >> UI_COL32_R_SHIFT) & HudPalette::kChannelMask);
        const uint8_t accG = static_cast<uint8_t>((accentCol >> UI_COL32_G_SHIFT) & HudPalette::kChannelMask);
        const uint8_t accB = static_cast<uint8_t>((accentCol >> UI_COL32_B_SHIFT) & HudPalette::kChannelMask);
        const uint8_t hiR  = static_cast<uint8_t>((std::min)(HudPalette::kMaxAlphaInt, static_cast<int>(accR) + NV::kHiBoostR));
        const uint8_t hiG  = static_cast<uint8_t>((std::min)(HudPalette::kMaxAlphaInt, static_cast<int>(accG) + NV::kHiBoostG));
        const uint8_t hiB  = static_cast<uint8_t>((std::min)(HudPalette::kMaxAlphaInt, static_cast<int>(accB) + NV::kHiBoostB));
        const uint8_t dkR  = static_cast<uint8_t>(static_cast<int>(accR) * NV::kDarkMulR / NV::kColorPercentDiv);
        const uint8_t dkG  = static_cast<uint8_t>(static_cast<int>(accG) * NV::kDarkMulG / NV::kColorPercentDiv);
        const uint8_t dkB  = static_cast<uint8_t>(static_cast<int>(accB) * NV::kDarkMulB / NV::kColorPercentDiv);

        // (A) 地面を走るホログラフィック・ガイドレール（センター光脈 ＋ 左右スリムレール）
        if (totalPathLen > NV::kMinRailPathLen)
        {
            float curDist    = NV::kRailStartDist;
            float segAccum   = 0.0f;
            bool hasPrevRail = false;
            UIVec2 prevCenter{}, prevRailL{}, prevRailR{};
            float prevDist   = NV::kRailStartDist;

            for (size_t i = 0; i + 1 < smoothPath.size() && segAccum < NV::kMaxDrawDist; ++i)
            {
                const Vector3& a = smoothPath[i];
                const Vector3& b = smoothPath[i + 1];
                const float dx = b.x - a.x;
                const float dz = b.z - a.z;
                const float segLen = std::sqrt(dx * dx + dz * dz);
                if (segLen < NV::kRailMinSegLen) continue;

                const Vector3 dir  = { dx / segLen, 0.0f, dz / segLen };
                const Vector3 perp = { dir.z, 0.0f, -dir.x };

                while (curDist <= segAccum + segLen && curDist <= NV::kMaxDrawDist && curDist <= totalPathLen - NV::kRailEndMargin)
                {
                    const float t    = (curDist - segAccum) / segLen;
                    const Vector3 wp = { a.x + dx * t, NV::kRailLiftY, a.z + dz * t };
                    Vector2 sC{}, sL{}, sR{};
                    if (ProjectWorldToScreen(wp, vp, screenW, screenH, sC) &&
                        ProjectWorldToScreen(wp - perp * NV::kRailHalfW, vp, screenW, screenH, sL) &&
                        ProjectWorldToScreen(wp + perp * NV::kRailHalfW, vp, screenW, screenH, sR))
                    {
                        const UIVec2 curC(sC.x, sC.y), curL(sL.x, sL.y), curR(sR.x, sR.y);
                        if (hasPrevRail && (curDist - prevDist) < NV::kRailStepDist * NV::kRailMaxGapMul)
                        {
                            const float startFade = (std::clamp)((curDist - NV::kRailStartDist) / NV::kRailStartFadeSpan, 0.0f, 1.0f);
                            const float endFade   = (std::clamp)((totalPathLen - curDist) / NV::kRailEndFadeSpan, 0.0f, 1.0f);
                            const float distFade  = (std::clamp)(1.0f - (curDist / (NV::kMaxDrawDist + NV::kRailDistPad)) * NV::kRailDistFadeMul, NV::kRailMinDistFade, 1.0f);
                            const float pulseWave = 0.5f + 0.5f * std::sin(curDist * NV::kRailWaveScale - timeNow * HudTiming::kGuideWaveSpeed);
                            const float vis       = startFade * endFade * distFade;

                            const int centerA = static_cast<int>((NV::kRailCenterBaseA + NV::kRailCenterAmpA * pulseWave) * vis);
                            const int sideA   = static_cast<int>((NV::kRailSideBaseA + NV::kRailSideAmpA * pulseWave) * vis);
                            if (centerA > NV::kRailMinCenterAlpha)
                                dl->AddLine(prevCenter, curC, UI_COL32(accR, accG, accB, centerA), NV::kRailCenterThick * uiScale);
                            if (sideA > NV::kRailMinSideAlpha)
                            {
                                const uint32_t sideCol = UI_COL32(accR, accG, accB, sideA);
                                dl->AddLine(prevRailL, curL, sideCol, NV::kRailSideThick * uiScale);
                                dl->AddLine(prevRailR, curR, sideCol, NV::kRailSideThick * uiScale);
                            }
                        }
                        prevCenter  = curC;
                        prevRailL   = curL;
                        prevRailR   = curR;
                        prevDist    = curDist;
                        hasPrevRail = true;
                    }
                    else
                    {
                        hasPrevRail = false;
                    }
                    curDist += NV::kRailStepDist;
                }
                segAccum += segLen;
            }
        }

        // (B) 目標地点に向かって実際に地上を滑走する「立体スイープウイング・シェブロン」
        const float flowOffset = std::fmod(timeNow * HudTiming::kGuideFlowSpeed, NV::kChevronStepDist);
        float accumDist        = 0.0f;
        float nextSampleDist   = NV::kChevronStartDist + flowOffset;

        for (size_t i = 0; i + 1 < smoothPath.size() && accumDist < NV::kMaxDrawDist; ++i)
        {
            const Vector3& a = smoothPath[i];
            const Vector3& b = smoothPath[i + 1];
            const float dx = b.x - a.x;
            const float dz = b.z - a.z;
            const float segLen = std::sqrt(dx * dx + dz * dz);
            if (segLen < NV::kChevronMinSegLen) continue;

            const Vector3 dir  = { dx / segLen, 0.0f, dz / segLen };
            const Vector3 perp = { dir.z, 0.0f, -dir.x };

            while (nextSampleDist <= accumDist + segLen && nextSampleDist <= NV::kMaxDrawDist)
            {
                if (nextSampleDist <= totalPathLen - NV::kChevronEndMargin)
                {
                    const float localT = (nextSampleDist - accumDist) / segLen;
                    const Vector3 p    = { a.x + dx * localT, NV::kChevronLiftY, a.z + dz * localT };

                    // 進行波パルスに合わせて先頭がわずかにワイド化するダイナミック演出
                    const float wave      = 0.5f + 0.5f * std::sin(nextSampleDist * NV::kChevronWaveScale - timeNow * HudTiming::kGuideWaveSpeed);
                    const float startFade = (std::clamp)((nextSampleDist - NV::kChevronStartDist) / NV::kChevStartFadeSpan, 0.0f, 1.0f);
                    const float goalFade  = (std::clamp)((totalPathLen - NV::kChevGoalMargin - nextSampleDist) / NV::kChevGoalFadeSpan, 0.0f, 1.0f);
                    const float distFade  = (std::clamp)(1.0f - (nextSampleDist / (NV::kMaxDrawDist + NV::kRailDistPad)) * NV::kChevDistFadeMul, NV::kChevMinDistFade, 1.0f);
                    const float vis       = startFade * goalFade * distFade;

                    if (vis > NV::kChevMinVis)
                    {
                        const float scaleMul   = NV::kChevBaseScale + NV::kChevWaveScaleAdd * wave;
                        const Vector3 wNose    = p + dir * (NV::kChevNoseFwd * scaleMul);
                        const Vector3 wNotch   = p + dir * (NV::kChevNotchFwd * scaleMul);
                        const Vector3 wOuterL  = p - dir * (NV::kChevOuterBack * scaleMul) - perp * (NV::kChevOuterSide * scaleMul);
                        const Vector3 wTailL   = p - dir * (NV::kChevTailBack * scaleMul)  - perp * (NV::kChevTailSide * scaleMul);
                        const Vector3 wOuterR  = p - dir * (NV::kChevOuterBack * scaleMul) + perp * (NV::kChevOuterSide * scaleMul);
                        const Vector3 wTailR   = p - dir * (NV::kChevTailBack * scaleMul)  + perp * (NV::kChevTailSide * scaleMul);
                        // 残像エコーシェブロン（本体後方に寄り添うスリムVライン）
                        const Vector3 wEchoTip = p - dir * (NV::kEchoTipBack * scaleMul);
                        const Vector3 wEchoL   = p - dir * (NV::kEchoWingBack * scaleMul) - perp * (NV::kEchoWingSide * scaleMul);
                        const Vector3 wEchoR   = p - dir * (NV::kEchoWingBack * scaleMul) + perp * (NV::kEchoWingSide * scaleMul);

                        Vector2 sNose{}, sNotch{}, sOL{}, sTL{}, sOR{}, sTR{};
                        if (ProjectWorldToScreen(wNose,   vp, screenW, screenH, sNose)  &&
                            ProjectWorldToScreen(wNotch,  vp, screenW, screenH, sNotch) &&
                            ProjectWorldToScreen(wOuterL, vp, screenW, screenH, sOL)    &&
                            ProjectWorldToScreen(wTailL,  vp, screenW, screenH, sTL)    &&
                            ProjectWorldToScreen(wOuterR, vp, screenW, screenH, sOR)    &&
                            ProjectWorldToScreen(wTailR,  vp, screenW, screenH, sTR))
                        {
                            const int bodyA   = static_cast<int>((NV::kChevBodyBaseA + NV::kChevBodyAmpA * wave) * vis);
                            const int edgeA   = static_cast<int>((NV::kChevEdgeBaseA + NV::kChevEdgeAmpA * wave) * vis);
                            const int glowA   = static_cast<int>((NV::kChevGlowBaseA + NV::kChevGlowAmpA * wave) * vis);
                            const int shadowA = static_cast<int>((NV::kChevShadowBaseA + NV::kChevShadowAmpA * wave) * vis);

                            const float shX = NV::kChevShadowOffX * uiScale;
                            const float shY = NV::kChevShadowOffY * uiScale;
                            const UIVec2 shWingL[C::kQuadVertCount] = {
                                UIVec2(sNose.x + shX, sNose.y + shY), UIVec2(sOL.x + shX, sOL.y + shY),
                                UIVec2(sTL.x + shX, sTL.y + shY), UIVec2(sNotch.x + shX, sNotch.y + shY)
                            };
                            const UIVec2 shWingR[C::kQuadVertCount] = {
                                UIVec2(sNose.x + shX, sNose.y + shY), UIVec2(sNotch.x + shX, sNotch.y + shY),
                                UIVec2(sTR.x + shX, sTR.y + shY), UIVec2(sOR.x + shX, sOR.y + shY)
                            };
                            const uint32_t shCol = UI_COL32(NV::kChevShadowR, NV::kChevShadowG, NV::kChevShadowB, shadowA);
                            dl->AddConvexPolyFilled(shWingL, C::kQuadVertCount, shCol);
                            dl->AddConvexPolyFilled(shWingR, C::kQuadVertCount, shCol);

                            // 後方エコーライン
                            Vector2 sET{}, sEL{}, sER{};
                            if (ProjectWorldToScreen(wEchoTip, vp, screenW, screenH, sET) &&
                                ProjectWorldToScreen(wEchoL,   vp, screenW, screenH, sEL) &&
                                ProjectWorldToScreen(wEchoR,   vp, screenW, screenH, sER))
                            {
                                const UIVec2 echoPts[C::kTriVertCount] = { UIVec2(sEL.x, sEL.y), UIVec2(sET.x, sET.y), UIVec2(sER.x, sER.y) };
                                dl->AddPolyline(echoPts, C::kTriVertCount,
                                    UI_COL32(accR, accG, accB, static_cast<int>(static_cast<float>(bodyA) * NV::kEchoAlphaMul)),
                                    UIDrawFlags_None, NV::kEchoThick * uiScale);
                            }

                            // 外周ホログラム・ブルームハロー
                            const UIVec2 leadGlow[C::kTriVertCount] = { UIVec2(sOL.x, sOL.y), UIVec2(sNose.x, sNose.y), UIVec2(sOR.x, sOR.y) };
                            dl->AddPolyline(leadGlow, C::kTriVertCount, UI_COL32(accR, accG, accB, glowA), UIDrawFlags_None, NV::kLeadGlowThick * uiScale);

                            // 左右2トーン・ベベル塗りつぶしウイング（凸四角形×2で立体感を演出）
                            const UIVec2 wingL[C::kQuadVertCount] = { UIVec2(sNose.x, sNose.y), UIVec2(sOL.x, sOL.y), UIVec2(sTL.x, sTL.y), UIVec2(sNotch.x, sNotch.y) };
                            const UIVec2 wingR[C::kQuadVertCount] = { UIVec2(sNose.x, sNose.y), UIVec2(sNotch.x, sNotch.y), UIVec2(sTR.x, sTR.y), UIVec2(sOR.x, sOR.y) };
                            dl->AddConvexPolyFilled(wingL, C::kQuadVertCount, UI_COL32(accR, accG, accB, bodyA));
                            dl->AddConvexPolyFilled(wingR, C::kQuadVertCount, UI_COL32(dkR,  dkG,  dkB,  bodyA));

                            // 鋭い高輝度リーディングエッジ＆センタースパイン
                            dl->AddPolyline(leadGlow, C::kTriVertCount, UI_COL32(hiR, hiG, hiB, edgeA), UIDrawFlags_None, NV::kLeadEdgeThick * uiScale);
                            dl->AddLine(UIVec2(sNotch.x, sNotch.y), UIVec2(sNose.x, sNose.y),
                                UI_COL32(HudPalette::kMaxAlphaInt, HudPalette::kMaxAlphaInt, HudPalette::kMaxAlphaInt, static_cast<int>(static_cast<float>(edgeA) * NV::kSpineAlphaMul)),
                                NV::kSpineThick * uiScale);
                        }
                    }
                }
                nextSampleDist += NV::kChevronStepDist;
            }
            accumDist += segLen;
        }

        // (C) 誘導終点（アクティブ目標の足元）に回転パルスする3Dタクティカル・ビーコンリングを描画
        if (totalPathLen > NV::kBeaconMinPathLen && totalPathLen < NV::kBeaconMaxPathLen)
        {
            const float pulseRad = NV::kBeaconBaseRad + NV::kBeaconPulseRad * std::sin(timeNow * HudTiming::kBeaconPulseSpeed);
            UIVec2 ringPts[NV::kBeaconRingSegs]{};
            bool ringOk = true;
            for (int s = 0; s < NV::kBeaconRingSegs; ++s)
            {
                const float ang  = (static_cast<float>(s) / static_cast<float>(NV::kBeaconRingSegs)) * kTwoPi + timeNow * HudTiming::kBeaconRotSpeed;
                const Vector3 rp = { goalPos.x + std::cos(ang) * pulseRad, NV::kBeaconLiftY, goalPos.z + std::sin(ang) * pulseRad };
                Vector2 sp{};
                if (!ProjectWorldToScreen(rp, vp, screenW, screenH, sp)) { ringOk = false; break; }
                ringPts[s] = UIVec2(sp.x, sp.y);
            }
            if (ringOk)
            {
                dl->AddPolyline(ringPts, NV::kBeaconRingSegs, UI_COL32(accR, accG, accB, NV::kBeaconAlpha), UIDrawFlags_Closed, NV::kBeaconThick * uiScale);
            }
        }
    }

    bool DrawWorldObjectiveMarkers(
        TacticalDrawList* dl, const GamePlayHUDContext& ctx, const ActiveObjectiveInfo& obj,
        const Vector3& playerPos, const Matrix4x4& vp,
        float screenW, float screenH, float uiScale, float timeNow)
    {
        namespace NV = HudLayout::Navigation;
        bool primaryVisible = false;

        // 3. 3Dフィールド上の全未破壊「標的 [1]-[4]」フローティング識別マーカー
        if (ctx.targets)
        {
            for (int i = 0; i < static_cast<int>(ctx.targets->size()); ++i)
            {
                const auto& t = (*ctx.targets)[i];
                if (!t || t->IsDead()) continue;

                const Vector3 tPos   = t->GetPosition();
                const float dx       = tPos.x - playerPos.x;
                const float dz       = tPos.z - playerPos.z;
                const float dist     = std::sqrt(dx * dx + dz * dz);
                const bool isPrimary = (i == obj.activeTargetIndex);
                if (!isPrimary && dist > NV::kSecondaryTargetMaxDist) continue;

                // 画面端境界でのボブ振動による高速ON/OFFチラつきを防ぐため、判定はボブ無しの基準座標で行う
                Vector2 base2D{}, p2D{}, anchor2D{};
                if (!ProjectWorldToScreen(tPos + Vector3{ 0.0f, NV::kTargetMarkerLiftY, 0.0f }, vp, screenW, screenH, base2D)) continue;
                if (base2D.x < NV::kTargetScreenPad || base2D.x > screenW - NV::kTargetScreenPad ||
                    base2D.y < NV::kTargetScreenPad || base2D.y > screenH - NV::kTargetScreenPad) continue;

                const float bob = isPrimary ? (NV::kTargetBobAmp * std::sin(timeNow * HudTiming::kTargetMarkerBobSpeed)) : 0.0f;
                if (!ProjectWorldToScreen(tPos + Vector3{ 0.0f, NV::kTargetMarkerLiftY + bob, 0.0f }, vp, screenW, screenH, p2D)) p2D = base2D;

                if (isPrimary)
                {
                    primaryVisible = true;
                    if (ProjectWorldToScreen(tPos + Vector3{ 0.0f, NV::kTargetAnchorLiftY, 0.0f }, vp, screenW, screenH, anchor2D))
                        dl->AddLine(UIVec2(p2D.x, p2D.y + NV::kTetherStartOffY * uiScale), UIVec2(anchor2D.x, anchor2D.y), NV::kTargetTetherCol, NV::kTetherThick * uiScale);
                }

                const uint32_t mainCol = isPrimary ? NV::kTargetPrimaryCol : NV::kTargetSecCol;
                const float dRad       = (isPrimary ? NV::kPrimaryPipRad : NV::kSecondaryPipRad) * uiScale;
                DrawDiamondPip(dl, p2D.x, p2D.y, dRad, NV::kTargetPipBg, mainCol);
                DrawDiamondPip(dl, p2D.x, p2D.y, dRad * NV::kInnerPipMul, mainCol);

                char labelBuf[HudLayout::Common::kMedBufSize];
                std::snprintf(labelBuf, sizeof(labelBuf), isPrimary ? AsUtf8(u8"標的 [%d]  %.0fm") : "[%d] %.0fm", i + 1, dist);

                const float fntSz = (isPrimary ? NV::kPrimaryFont : NV::kSecondaryFont) * uiScale;
                const UIVec2 tSz  = MeasureSharpText(UIFontRole::SubBoldJP, fntSz, labelBuf);
                const float tagW  = tSz.x + NV::kTargetTagPadW * uiScale;
                const float tagH  = tSz.y + NV::kTargetTagPadH * uiScale;
                const float tagX  = (std::clamp)(p2D.x - tagW * 0.5f, NV::kTargetClampX * uiScale, screenW - tagW - NV::kTargetClampX * uiScale);
                const float tagY  = p2D.y - dRad - tagH - NV::kTargetTagGapY * uiScale;

                DrawAccentedTag(dl, tagX, tagY, tagW, tagH, NV::kTargetSlant * uiScale,
                    NV::kTargetShadowX, NV::kTargetShadowY, HudPalette::kPanelShadow120,
                    HudPalette::kGunmetalBg236, mainCol, NV::kTargetBorderThick,
                    isPrimary ? (NV::kTargetStripeW * uiScale) : 0.0f,
                    fntSz, (isPrimary ? NV::kPrimaryTextShiftX : NV::kSecondaryTextShiftX) * uiScale,
                    isPrimary ? NV::kTargetPriText : NV::kTargetSecText, labelBuf);
            }
        }

        // 初期弾薬箱がアクティブ目標の間は弾薬箱の上にも分かりやすい「STEP 1」誘導マーカーを表示
        if (obj.isLootObjective)
        {
            const float dx   = obj.worldPos.x - playerPos.x;
            const float dz   = obj.worldPos.z - playerPos.z;
            const float dist = std::sqrt(dx * dx + dz * dz);
            if (dist <= LootSystem::kInteractRadius)
            {
                primaryVisible = true;
            }
            else
            {
                Vector2 baseL2D{}, l2D{}, anchor2D{};
                if (ProjectWorldToScreen(obj.worldPos + Vector3{ 0.0f, NV::kLootMarkerLiftY, 0.0f }, vp, screenW, screenH, baseL2D) &&
                    baseL2D.x > NV::kLootScreenPad && baseL2D.x < screenW - NV::kLootScreenPad &&
                    baseL2D.y > NV::kLootScreenPad && baseL2D.y < screenH - NV::kLootScreenPad)
                {
                    const float bob = NV::kLootBobAmp * std::sin(timeNow * HudTiming::kLootMarkerBobSpeed);
                    if (!ProjectWorldToScreen(obj.worldPos + Vector3{ 0.0f, NV::kLootMarkerLiftY + bob, 0.0f }, vp, screenW, screenH, l2D)) l2D = baseL2D;
                    primaryVisible = true;

                    // 弾薬箱とマーカーを繋ぐホログラム垂直テザーライン
                    if (ProjectWorldToScreen(obj.worldPos + Vector3{ 0.0f, NV::kLootAnchorLiftY, 0.0f }, vp, screenW, screenH, anchor2D))
                    {
                        dl->AddLine(UIVec2(l2D.x, l2D.y + NV::kTetherStartOffY * uiScale), UIVec2(anchor2D.x, anchor2D.y), NV::kLootTetherCol, NV::kLootTetherThick * uiScale);
                        dl->AddCircleFilled(UIVec2(anchor2D.x, anchor2D.y), NV::kLootAnchorDotRad * uiScale, NV::kLootAnchorCol, NV::kLootAnchorDotSegs);
                    }

                    const float pulseExpand = 1.0f + NV::kLootPulseExpandAmp * (0.5f + 0.5f * std::sin(timeNow * HudTiming::kLootMarkerPulseSpeed));
                    DrawDiamondPip(dl, l2D.x, l2D.y, NV::kLootPulseRad * uiScale * pulseExpand, 0, NV::kLootPulseCol);
                    DrawDiamondPip(dl, l2D.x, l2D.y, NV::kLootOuterRad * uiScale, NV::kLootPipBg, HudPalette::kCyanAccent);
                    DrawDiamondPip(dl, l2D.x, l2D.y, NV::kLootInnerRad * uiScale, NV::kLootPipCore);

                    char buf[HudLayout::Common::kLabelBufSize];
                    std::snprintf(buf, sizeof(buf), "%s  %.0fm", obj.markerLabel, dist);
                    const float lootFont = NV::kLootTagFont * uiScale;
                    const UIVec2 sz      = MeasureSharpText(UIFontRole::SubBoldJP, lootFont, buf);
                    const float plateW   = sz.x + NV::kLootTagPadW * uiScale;
                    const float plateH   = sz.y + NV::kLootTagPadH * uiScale;
                    const float bx       = (std::clamp)(l2D.x - plateW * 0.5f, NV::kTargetClampX * uiScale, screenW - plateW - NV::kTargetClampX * uiScale);
                    const float by       = (std::max)(NV::kLootMinTagY * uiScale, l2D.y - NV::kLootTagOffsetY * uiScale);

                    DrawAccentedTag(dl, bx, by, plateW, plateH, NV::kLootTagSlant * uiScale,
                        NV::kTargetShadowX, NV::kTargetShadowY, HudPalette::kPanelShadow125,
                        NV::kLootTagBg, NV::kLootTagBd, NV::kLootTagBorderThick,
                        NV::kLootTagStripeW * uiScale, lootFont, NV::kLootTextShiftX * uiScale,
                        NV::kLootTagText, buf, HudPalette::kCyanAccent);
                }
            }
        }

        return primaryVisible;
    }

    void DrawOffscreenObjectivePointer(
        TacticalDrawList* dl, const GamePlayHUDContext& ctx, const ActiveObjectiveInfo& obj,
        const Vector3& playerPos, Vector2& ioSmoothedDir, bool& ioHasDir,
        const Matrix4x4& vp, float screenW, float screenH, float uiScale, float deltaTime, float timeNow)
    {
        namespace NV = HudLayout::Navigation;
        namespace C  = HudLayout::Common;

        Vector2 obj2D{};
        const bool projOk    = ProjectWorldToScreen(obj.worldPos + Vector3{ 0.0f, NV::kOffscreenProjLiftY, 0.0f }, vp, screenW, screenH, obj2D);
        const float marginX  = NV::kSafeMarginX * uiScale;
        const float marginT  = NV::kSafeMarginTop * uiScale;
        const float marginB  = NV::kSafeMarginBot * uiScale;
        const bool safeOnScr = projOk && obj.isExtractObjective &&
            obj2D.x >= marginX && obj2D.x <= (screenW - marginX) &&
            obj2D.y >= marginT && obj2D.y <= (screenH - marginB);

        if (safeOnScr || (ctx.isReadyToExtract && ctx.isPlayerInExtractionZone))
        {
            ioHasDir = false;
            return;
        }

        // 旧実装の cachedPath[1]（足元0.5mグリッドノード追い越しによる激しい角度反転）を廃止し、
        // 目標ワールド座標への正確なスクリーン投影ベクトル＋指数平滑化でピタッと安定させる
        const float dirX         = obj.worldPos.x - playerPos.x;
        const float dirZ         = obj.worldPos.z - playerPos.z;
        const float straightDist = std::sqrt(dirX * dirX + dirZ * dirZ);
        if (straightDist <= NV::kMinStraightDist) return;

        const float ux = dirX / straightDist;
        const float uz = dirZ / straightDist;

        float rawSx = ux;
        float rawSy = -uz;
        const Vector3 pAnchor = playerPos + Vector3{ 0.0f, 0.65f, 0.0f };
        Vector2 pScr{}, probeScr{};
        if (ProjectWorldToScreen(pAnchor, vp, screenW, screenH, pScr) &&
            ProjectWorldToScreen(pAnchor + Vector3{ ux * 1.8f, 0.0f, uz * 1.8f }, vp, screenW, screenH, probeScr))
        {
            const float dx2 = probeScr.x - pScr.x;
            const float dy2 = probeScr.y - pScr.y;
            const float dLen = std::sqrt(dx2 * dx2 + dy2 * dy2);
            if (dLen > NV::kMinDirLen)
            {
                rawSx = dx2 / dLen;
                rawSy = dy2 / dLen;
            }
        }

        if (!ioHasDir)
        {
            ioSmoothedDir = { rawSx, rawSy };
            ioHasDir      = true;
        }
        else
        {
            const float blend = 1.0f - std::exp(-12.0f * (std::clamp)(deltaTime, 0.0f, 0.1f));
            ioSmoothedDir.x += (rawSx - ioSmoothedDir.x) * blend;
            ioSmoothedDir.y += (rawSy - ioSmoothedDir.y) * blend;
            const float sLen = std::sqrt(ioSmoothedDir.x * ioSmoothedDir.x + ioSmoothedDir.y * ioSmoothedDir.y);
            if (sLen > NV::kMinDirLen)
            {
                ioSmoothedDir.x /= sLen;
                ioSmoothedDir.y /= sLen;
            }
            else
            {
                ioSmoothedDir = { rawSx, rawSy };
            }
        }

        const float sxDir = ioSmoothedDir.x;
        const float syDir = ioSmoothedDir.y;

        // ラベルや矢印座標自体を毎フレーム揺らさず固定軌道上に落ち着かせ、文字のガタつきを完全解消
        const float px = screenW * 0.5f + sxDir * (screenW * NV::kOrbitRatioX);
        const float py = screenH * NV::kCenterRatioY + syDir * (screenH * NV::kOrbitRatioY);
        const float echoAlphaMul = 0.55f + 0.45f * (0.5f + 0.5f * std::sin(timeNow * HudTiming::kOffscreenPulseSpeed));

        // スイープウイング型タクティカル方向ポインター（2トーン凸ウイング＋ダブルシェブロン）
        const float nx = -syDir;
        const float ny = sxDir;
        const UIVec2 tip(px + sxDir * NV::kArrowTipFwd * uiScale, py + syDir * NV::kArrowTipFwd * uiScale);
        const UIVec2 notch(px + sxDir * NV::kArrowNotchFwd * uiScale, py + syDir * NV::kArrowNotchFwd * uiScale);
        const UIVec2 wingL(px - sxDir * NV::kArrowWingBack * uiScale + nx * NV::kArrowWingSide * uiScale, py - syDir * NV::kArrowWingBack * uiScale + ny * NV::kArrowWingSide * uiScale);
        const UIVec2 wingR(px - sxDir * NV::kArrowWingBack * uiScale - nx * NV::kArrowWingSide * uiScale, py - syDir * NV::kArrowWingBack * uiScale - ny * NV::kArrowWingSide * uiScale);

        const UIVec2 triL[C::kTriVertCount] = { tip, wingL, notch };
        const UIVec2 triR[C::kTriVertCount] = { tip, notch, wingR };
        dl->AddConvexPolyFilled(triL, C::kTriVertCount, obj.accentCol);
        dl->AddConvexPolyFilled(triR, C::kTriVertCount, obj.accentCol);
        const UIVec2 outlinePts[C::kQuadVertCount] = { tip, wingL, notch, wingR };
        dl->AddPolyline(outlinePts, C::kQuadVertCount, NV::kArrowOutlineCol, UIDrawFlags_Closed, NV::kArrowOutlineThick * uiScale);

        // 後方エコーシェブロン（位置は固定のまま輝度のみ穏やかに脈動）
        const UIVec2 eTip(px - sxDir * NV::kArrowEchoTipBack * uiScale, py - syDir * NV::kArrowEchoTipBack * uiScale);
        const UIVec2 eL(px - sxDir * NV::kArrowEchoWingBack * uiScale + nx * NV::kArrowEchoWingSide * uiScale, py - syDir * NV::kArrowEchoWingBack * uiScale + ny * NV::kArrowEchoWingSide * uiScale);
        const UIVec2 eR(px - sxDir * NV::kArrowEchoWingBack * uiScale - nx * NV::kArrowEchoWingSide * uiScale, py - syDir * NV::kArrowEchoWingBack * uiScale - ny * NV::kArrowEchoWingSide * uiScale);
        const UIVec2 ePts[C::kTriVertCount] = { eL, eTip, eR };
        dl->AddPolyline(ePts, C::kTriVertCount, ScaleHudAlpha(obj.accentCol, echoAlphaMul), UIDrawFlags_None, NV::kArrowEchoThick * uiScale);

        char edgeText[C::kWideBufSize];
        std::snprintf(edgeText, sizeof(edgeText), "%s [%.0fm]", obj.markerLabel, straightDist);
        const float edgeFont = NV::kEdgePillFont * uiScale;
        const UIVec2 eSz     = MeasureSharpText(UIFontRole::SubBoldJP, edgeFont, edgeText);
        const float pillW    = eSz.x + NV::kEdgePillPadW * uiScale;
        const float pillH    = eSz.y + NV::kEdgePillPadH * uiScale;
        const float pillX    = std::round((std::clamp)(px - sxDir * (pillW * 0.5f + NV::kEdgePillGap * uiScale) - pillW * 0.5f,
                                                       NV::kEdgeClampX * uiScale, screenW - pillW - NV::kEdgeClampX * uiScale));
        const float pillY    = std::round((std::clamp)(py - syDir * (pillH * 0.5f + NV::kEdgePillGap * uiScale) - pillH * 0.5f,
                                                       NV::kEdgeClampTop * uiScale, screenH - pillH - NV::kEdgeClampBot * uiScale));

        DrawAccentedTag(dl, pillX, pillY, pillW, pillH, NV::kEdgePillSlant * uiScale,
            NV::kTargetShadowX, NV::kTargetShadowY, HudPalette::kPanelShadow130,
            NV::kEdgePillBg, obj.accentCol, NV::kEdgeBorderThick,
            NV::kEdgeStripeW * uiScale, edgeFont, NV::kEdgeTextShiftX * uiScale,
            HudPalette::kTextIceWhite, edgeText);
    }
}

void GamePlayHUD::DrawNavigationGuideHUD(const GamePlayHUDContext& ctx, float deltaTime)
{
    if (!ctx.player || ctx.player->IsDead() || !ctx.camera || ctx.isDeathSequenceActive) return;

    HudViewport vpInfo{};
    if (!AcquireHudViewport(vpInfo)) return;
    TacticalDrawList* dl    = vpInfo.dl;
    const float screenW     = vpInfo.screenW;
    const float screenH     = vpInfo.screenH;
    const float uiScale     = vpInfo.uiScale;
    const Matrix4x4& vp     = ctx.camera->GetViewProjectionMatrix();
    const Vector3 playerPos = ctx.player->GetPosition();
    const float timeNow     = static_cast<float>(TacticalUICanvas::GetInstance()->GetTime());

    const ActiveObjectiveInfo obj = ResolveActiveObjective(ctx, lastActiveTargetIdx_);
    lastActiveTargetIdx_          = obj.activeTargetIndex;

    // 1. NavMesh A* 経路の定期更新
    RefreshNavigationPath(cachedGuidePath_, cachedGuideGoal_, guidePathRefreshTimer_, deltaTime, playerPos, obj.worldPos);

    // 2. 3D地面ホログラフィック誘導レーン ＆ スイープウイング・シェブロン光跡
    if (!ctx.isFullMapOpen && cachedGuidePath_.size() > 1 && !(ctx.isReadyToExtract && ctx.isPlayerInExtractionZone))
    {
        BuildSmoothedGuidePath(cachedGuidePath_, smoothGuidePath_, scratchGuidePath_);
        DrawGroundNavigationLane(dl, smoothGuidePath_, obj.worldPos, obj.accentCol, vp, screenW, screenH, uiScale, timeNow);
    }

    if (ctx.isFullMapOpen) return;

    // 3. 3Dフィールド上の標的・初期物資マーカー
    const bool primaryVisible = DrawWorldObjectiveMarkers(dl, ctx, obj, playerPos, vp, screenW, screenH, uiScale, timeNow);

    // 4. 目標が画面外にある場合の画面端ナビゲーション方向ポインター
    if (!primaryVisible)
    {
        DrawOffscreenObjectivePointer(dl, ctx, obj, playerPos, smoothedOffscreenDir_, hasOffscreenDir_, vp, screenW, screenH, uiScale, deltaTime, timeNow);
    }
    else
    {
        hasOffscreenDir_ = false;
    }
}

// =============================================================================
// スタートからゴールまでの動的クエスト進行ガイド HUD (画面右上)
// =============================================================================

void GamePlayHUD::DrawQuestTrackerHUD(const GamePlayHUDContext& ctx)
{
    if (!ctx.player || ctx.player->IsDead() || ctx.isDeathSequenceActive || ctx.isFullMapOpen) return;

    HudViewport vpInfo{};
    if (!AcquireHudViewport(vpInfo)) return;
    TacticalDrawList* dl = vpInfo.dl;
    const float screenW  = vpInfo.screenW;
    const float uiScale  = vpInfo.uiScale;
    namespace QT = HudLayout::QuestTracker;

    const ActiveObjectiveInfo obj = ResolveActiveObjective(ctx, lastActiveTargetIdx_);

    const char* step1Txt = AsUtf8(QT::kStep1Text);
    char step2Buf[HudLayout::Common::kStepBufSize];
    std::snprintf(step2Buf, sizeof(step2Buf), AsUtf8(QT::kStep2Format), obj.destroyedTargets, obj.totalTargets);
    const char* step3Txt = AsUtf8(QT::kStep3Text);

    const float stepFont = QT::kStepFont * uiScale;
    const float actFont  = QT::kActionFont * uiScale;
    const float maxStepW = (std::max)({
        MeasureSharpText(UIFontRole::SubBoldJP, stepFont, step1Txt).x,
        MeasureSharpText(UIFontRole::SubBoldJP, stepFont, step2Buf).x,
        MeasureSharpText(UIFontRole::SubBoldJP, stepFont, step3Txt).x,
    });
    const float maxActW = (std::max)(
        MeasureSharpText(UIFontRole::SubBoldJP, actFont, obj.actionLine1).x,
        MeasureSharpText(UIFontRole::SubBoldJP, actFont, obj.actionLine2).x);

    // 文字幅に合わせて自動フィットする高コントラスト・タスクカード
    const float panelW = (std::max)({ QT::kMinPanelW * uiScale, maxStepW + QT::kStepWidthPad * uiScale, maxActW + QT::kActionWidthPad * uiScale });
    const float panelH = QT::kPanelH * uiScale;
    const float slant  = QT::kSlant * uiScale;
    const float panelX = std::round(screenW - panelW - QT::kRightMargin * uiScale);
    const float panelY = std::round(QT::kTopY * uiScale);

    DrawFramedPanel(dl, panelX, panelY, panelW, panelH, slant, QT::kPanelStyle);
    dl->AddLine(UIVec2(panelX + slant, panelY), UIVec2(panelX + panelW + slant, panelY), obj.accentCol, QT::kTopAccentThick * uiScale);

    DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, QT::kHeaderFont * uiScale,
        panelX + QT::kHeaderPadX * uiScale, panelY + QT::kHeaderCenterY * uiScale,
        QT::kHeaderCol, AsUtf8(u8"TASK // 作戦目標"), HudTextAlign::Left);

    const float phaseFont = QT::kPhaseFont * uiScale;
    const UIVec2 pTagSz   = MeasureSharpText(UIFontRole::SubBoldJP, phaseFont, obj.phaseTag);
    const float pTagW     = pTagSz.x + QT::kPhasePadW * uiScale;
    DrawAccentedTag(dl, panelX + panelW - pTagW - QT::kPhaseRightPad * uiScale, panelY + QT::kPhaseTopY * uiScale,
        pTagW, QT::kPhaseH * uiScale, QT::kPhaseSlant * uiScale, 0.0f, 0.0f, 0,
        QT::kPhaseBg, obj.accentCol, QT::kPhaseBorderThick,
        0.0f, phaseFont, QT::kPhaseTextShiftX * uiScale, obj.accentCol, obj.phaseTag);

    dl->AddLine(UIVec2(panelX + QT::kDividerLeftPad * uiScale, panelY + QT::kDividerY * uiScale),
                UIVec2(panelX + panelW - QT::kDividerRightPad * uiScale, panelY + QT::kDividerY * uiScale),
                QT::kDividerCol, QT::kDividerThick);

    // 3段階ミッションチェックリスト
    const bool step1Done = obj.hasLootedAny || obj.destroyedTargets > 0;
    const bool step2Done = ctx.isReadyToExtract;
    const bool step3Done = ctx.isGameCleared;

    auto drawStepRow = [&](float rowCy, bool done, bool current, const char* text) {
        DrawCheckmarkBadge(dl, panelX + QT::kCheckCenterX * uiScale, rowCy, QT::kCheckRadius * uiScale,
                           done, current ? obj.accentCol : QT::kIdleCheckCol);
        const uint32_t txtCol = done ? QT::kStepDoneText : (current ? HudPalette::kTextBrightWhite : QT::kStepIdleText);
        DrawShadowedTextAligned(dl, current ? UIFontRole::SubBoldJP : UIFontRole::DefaultJP, stepFont,
                                panelX + QT::kStepTextX * uiScale, rowCy, txtCol, text, HudTextAlign::Left);
    };

    drawStepRow(panelY + QT::kStep1CenterY * uiScale, step1Done, !step1Done, step1Txt);
    drawStepRow(panelY + QT::kStep2CenterY * uiScale, step2Done, step1Done && !step2Done, step2Buf);
    drawStepRow(panelY + QT::kStep3CenterY * uiScale, step3Done, step2Done, step3Txt);

    // 下段スリム指示バー（現在やるべきアクション）
    const float callX     = panelX + QT::kCalloutPadX * uiScale;
    const float callY     = panelY + QT::kCalloutTopY * uiScale;
    const float callW     = panelW - QT::kCalloutTrimW * uiScale;
    const float callH     = QT::kCalloutH * uiScale;
    const float callSlant = QT::kCalloutSlant * uiScale;

    DrawSlantedPlate(dl, callX, callY, callW, callH, callSlant, QT::kCalloutBg, obj.accentCol, QT::kCalloutBorderThick);
    DrawSlantedPlate(dl, callX, callY, QT::kCalloutStripeW * uiScale, callH, callSlant, obj.accentCol);

    DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, actFont,
        callX + QT::kActionTextPadX * uiScale, callY + QT::kActionLine1Y * uiScale,
        HudPalette::kTextBrightWhite, obj.actionLine1, HudTextAlign::Left);
    DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, actFont,
        callX + QT::kActionTextPadX * uiScale, callY + QT::kActionLine2Y * uiScale,
        obj.accentCol, obj.actionLine2, HudTextAlign::Left);
}

// =============================================================================
// Escape from Duckov スタイル 極薄フローティング・キーガイドリップ (画面最下端中央)
// 従来の巨大な2段ボックス(640x54)を廃止し、画面下部中央の射撃視界を広く開放！
// =============================================================================

void GamePlayHUD::DrawControlsGuideHUD(const GamePlayHUDContext& ctx)
{
    if (!ctx.player || ctx.player->IsDead() || ctx.isDeathSequenceActive || ctx.isFullMapOpen) return;

    HudViewport vpInfo{};
    if (!AcquireHudViewport(vpInfo)) return;
    TacticalDrawList* dl = vpInfo.dl;
    const float screenH  = vpInfo.screenH;
    const float uiScale  = vpInfo.uiScale;
    namespace CG = HudLayout::ControlsGuide;

    const float timeNow = static_cast<float>(TacticalUICanvas::GetInstance()->GetTime());
    const float pulse   = 0.5f + 0.5f * std::sin(timeNow * HudTiming::kControlsPulseSpeed);

    bool nearLoot = false;
    if (ctx.lootProps)
    {
        const Vector3 pPos     = ctx.player->GetPosition();
        const float interactSq = GameConfig::Math::Square(LootSystem::kInteractRadius);
        for (const auto& prop : *ctx.lootProps)
        {
            if (prop.isLooted || prop.isBreakableCrate) continue;
            const float dx = prop.position.x - pPos.x;
            const float dz = prop.position.z - pPos.z;
            if (dx * dx + dz * dz <= interactSq)
            {
                nearLoot = true;
                break;
            }
        }
    }

    const bool lowAmmoSuggestReload = (
        !ctx.player->IsReloading() &&
        ctx.player->GetMagazineAmmo() <= CG::kSuggestReloadAmmo &&
        ctx.player->GetReserveAmmo() > 0);

    // [Q]救急キット・[C]速歩は左クイックスロット、[Y]視点はミニマップに統合し、最下段は必須6操作のみ1列スリム配置
    const bool activeStates[CG::kItemCount] = {
        ctx.player->IsMoving(),
        IsAppKeyDown(VK_LBUTTON),
        ctx.player->IsDodging(),
        ctx.player->IsReloading(),
        nearLoot && IsAppKeyDown('E'),
        ctx.isFullMapOpen,
    };
    const bool highlightStates[CG::kItemCount] = {
        false,
        false,
        false,
        lowAmmoSuggestReload,
        nearLoot,
        false,
    };

    // 左クイックスロット右端〜右ウェポンHUD左端の中央領域に1列スリム配置
    const float barW     = CG::kBarW * uiScale;
    const float barH     = CG::kBarH * uiScale;
    const float barX     = std::round(CG::kBarX * uiScale);
    const float barY     = std::round(screenH - barH - CG::kBottomMargin * uiScale);
    const float descFont = CG::kDescFont * uiScale;
    const float keyFont  = CG::kKeyFont * uiScale;
    const float centerY  = barY + barH * 0.5f;

    dl->AddRectFilled(UIVec2(barX, barY), UIVec2(barX + barW, barY + barH), CG::kBarBg, CG::kBarRounding);
    dl->AddRect(UIVec2(barX, barY), UIVec2(barX + barW, barY + barH), CG::kBarBorder, CG::kBarRounding, 0, CG::kBarBorderThick);

    // 各項目の実測幅に応じて均等余白で横並び配置（"SPACE" と "R リロード" の接触を完全防止）
    float itemWidths[CG::kItemCount]{};
    float keyWidths[CG::kItemCount]{};
    float totalItemsW = 0.0f;
    for (int i = 0; i < CG::kItemCount; ++i)
    {
        keyWidths[i]  = MeasureSharpText(UIFontRole::SubBoldJP, keyFont, CG::kItems[i].key).x + CG::kKeyPadW * uiScale;
        const float dW = MeasureSharpText(UIFontRole::SubBoldJP, descFont, AsUtf8(CG::kItems[i].desc)).x;
        itemWidths[i] = keyWidths[i] + CG::kDescGapX * uiScale + dW;
        totalItemsW  += itemWidths[i];
    }
    const float availGap = (std::max)(6.0f * uiScale, (barW - CG::kCellStartPadX * 2.0f * uiScale - totalItemsW) / static_cast<float>(CG::kItemCount - 1));
    float curX = barX + (barW - (totalItemsW + availGap * static_cast<float>(CG::kItemCount - 1))) * 0.5f;

    for (int i = 0; i < CG::kItemCount; ++i)
    {
        const auto& spec   = CG::kItems[i];
        const float cx     = std::round(curX);
        const float kw     = keyWidths[i];
        const float kh     = CG::kKeyH * uiScale;
        const float ky     = barY + (barH - kh) * 0.5f;
        curX += itemWidths[i] + availGap;

        uint32_t kBg = CG::kKeyIdleBg;
        uint32_t kBd = CG::kKeyIdleBd;
        uint32_t kTx = CG::kKeyIdleTx;
        uint32_t dTx = CG::kDescIdleTx;

        if (activeStates[i])
        {
            kBg = CG::kKeyActiveBg;
            kBd = spec.highlightCol;
            kTx = HudPalette::kTextPureWhite;
            dTx = spec.highlightCol;
        }
        else if (highlightStates[i])
        {
            const int glowA = static_cast<int>(CG::kGlowBaseAlpha + CG::kGlowPulseAlpha * pulse);
            kBg = CG::kKeyHighlightBg;
            kBd = (spec.highlightCol & HudPalette::kRgbMask) | (static_cast<uint32_t>(glowA) << UI_COL32_A_SHIFT);
            dTx = spec.highlightCol;
        }

        DrawKeycapBadge(dl, cx, ky, kw, kh, CG::kKeyRounding, kBg, kBd, CG::kKeyBorderThick, keyFont, kTx, spec.key, centerY);
        DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, descFont, cx + kw + CG::kDescGapX * uiScale, centerY, dTx, AsUtf8(spec.desc), HudTextAlign::Left);
    }

    // 下部操作バー直上に控えめで視認しやすい「[RMB] スロー鑑賞 / [F7] 自動デモ実演」ショートカットバッジを併記
    {
        const char* extraHint = ctx.isAutoDemoPlay
            ? AsUtf8(u8"[F7] 手動操作に戻る   |   [Y] 俯瞰カメラ切替")
            : AsUtf8(u8"[RMB長押し] スロー鑑賞   |   [F7] 自動デモ実演");
        const float hFont = 11.5f * uiScale;
        const UIVec2 hSz  = MeasureSharpText(UIFontRole::SubBoldJP, hFont, extraHint);
        const float pillW = hSz.x + 22.0f * uiScale;
        const float pillH = 19.0f * uiScale;
        const float pillX = std::round(barX + (barW - pillW) * 0.5f);
        const float pillY = std::round(barY - pillH - 4.0f * uiScale);
        dl->AddRectFilled(UIVec2(pillX, pillY), UIVec2(pillX + pillW, pillY + pillH), UI_COL32(12, 18, 24, 196), 4.0f * uiScale);
        dl->AddRect(UIVec2(pillX, pillY), UIVec2(pillX + pillW, pillY + pillH),
            ctx.isAutoDemoPlay ? UI_COL32(70, 225, 255, 190) : UI_COL32(120, 155, 180, 135), 4.0f * uiScale, 0, 1.0f);
        DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, hFont, pillX + pillW * 0.5f, pillY + pillH * 0.5f,
            ctx.isAutoDemoPlay ? UI_COL32(165, 242, 255, 255) : UI_COL32(210, 228, 240, 235), extraHint, HudTextAlign::Center);
    }
}

// =============================================================================
// 常時表示タクティカルミニマップ (左上) ＆ [M/TAB] 全体作戦マップオーバーレイ
// =============================================================================

namespace
{
    // ミニマップと全体作戦マップで共有する歪みゼロ等方スケール要塞キャンバス描画
    void RenderCompoundMapCanvas(
        TacticalDrawList* dl, const GamePlayHUDContext& ctx,
        const std::vector<Vector3>& guidePath, const ActiveObjectiveInfo& obj,
        const Vector3& pPos, float pYaw, float uiScale, float pulse,
        float viewX, float viewY, float viewW, float viewH, bool showZoneLabels)
    {
        namespace RD = HudLayout::Radar;
        namespace MM = HudLayout::Minimap;

        // 外周の深い森林エリア背景（要塞内部フロアとのコントラストで非矩形シルエットを際立たせる）
        dl->AddRectFilled(UIVec2(viewX, viewY), UIVec2(viewX + viewW, viewY + viewH), RD::kForestBgCol, RD::kCanvasRounding);
        dl->AddRect(UIVec2(viewX, viewY), UIVec2(viewX + viewW, viewY + viewH), RD::kCanvasBorderCol, RD::kCanvasRounding, UIDrawFlags_None, RD::kCanvasBorderThick);

        const float pad        = RD::kMapInnerPad * uiScale;
        const float ppm        = (std::min)((viewW - pad * 2.0f) / RD::kWorldSpanX, (viewH - pad * 2.0f) / RD::kWorldSpanZ);
        const float mapOriginX = viewX + (viewW - RD::kWorldSpanX * ppm) * 0.5f;
        const float mapOriginY = viewY + (viewH - RD::kWorldSpanZ * ppm) * 0.5f;

        auto toMap = [&](float wx, float wz) -> UIVec2 {
            return UIVec2(mapOriginX + (wx - RD::kWorldMinX) * ppm, mapOriginY + (RD::kWorldMaxZ - wz) * ppm);
        };

        auto fillWorldConvex = [&](const Vector2* pts, int count, uint32_t col) {
            UIVec2 mpts[RD::kMaxConvexVerts];
            for (int i = 0; i < count; ++i) mpts[i] = toMap(pts[i].x, pts[i].y);
            dl->AddConvexPolyFilled(mpts, count, col);
        };

        auto strokeWorldOpen = [&](const Vector2* pts, int count, uint32_t col, float thick) {
            UIVec2 mpts[RD::kMaxPolylineVerts];
            for (int i = 0; i < count; ++i) mpts[i] = toMap(pts[i].x, pts[i].y);
            dl->AddPolyline(mpts, count, col, UIDrawFlags_None, thick);
        };

        dl->PushClipRect(UIVec2(viewX + RD::kClipInset, viewY + RD::kClipInset),
                         UIVec2(viewX + viewW - RD::kClipInset, viewY + viewH - RD::kClipInset), true);

        // 1. 戦術グリッド線 (10m間隔)
        for (float gx = RD::kGridMinX; gx <= RD::kGridMaxX; gx += RD::kGridStep)
        {
            dl->AddLine(toMap(gx, RD::kWorldMinZ), toMap(gx, RD::kWorldMaxZ), RD::kGridLineCol, RD::kGridLineThick);
        }
        for (float gz = RD::kGridMinZ; gz <= RD::kGridMaxZ; gz += RD::kGridStep)
        {
            dl->AddLine(toMap(RD::kWorldMinX, gz), toMap(RD::kWorldMaxX, gz), RD::kGridLineCol, RD::kGridLineThick);
        }

        // 2. 非矩形・非対称マルチウイング要塞の内部プレイアブル床面シルエット描画（厳密凸分割）
        for (const auto& poly : RD::kFloorPolys)
        {
            const uint32_t col = (poly.tone == MapFloorTone::Vault) ? RD::kFloorVaultCol
                               : (poly.tone == MapFloorTone::Helipad) ? RD::kFloorHelipadCol
                               : RD::kFloorStandardCol;
            fillWorldConvex(poly.points, poly.count, col);
        }

        // 3. 中央河川 (Z = 27.5..32.5) ＆ 東西2本の橋 (X = -14.0, +14.0)
        {
            const UIVec2 rMin = toMap(RD::kWorldMinX, GameConfig::Environment::kRiverZMax);
            const UIVec2 rMax = toMap(RD::kWorldMaxX, GameConfig::Environment::kRiverZMin);
            dl->AddRectFilled(rMin, rMax, RD::kRiverFillCol);
            dl->AddLine(UIVec2(rMin.x, rMin.y), UIVec2(rMax.x, rMin.y), RD::kRiverEdgeCol, RD::kRiverEdgeThick);
            dl->AddLine(UIVec2(rMin.x, rMax.y), UIVec2(rMax.x, rMax.y), RD::kRiverEdgeCol, RD::kRiverEdgeThick);

            for (float bx : RD::kBridgeCenterXs)
            {
                const UIVec2 bMin = toMap(bx - RD::kBridgeHalfW, RD::kBridgeMaxZ);
                const UIVec2 bMax = toMap(bx + RD::kBridgeHalfW, RD::kBridgeMinZ);
                dl->AddRectFilled(bMin, bMax, RD::kBridgeFillCol, RD::kBridgeRounding);
                dl->AddRect(bMin, bMax, RD::kBridgeBorderCol, RD::kBridgeRounding, UIDrawFlags_None, RD::kBridgeBorderThick);
            }
        }

        // 4. 外周切り欠き森林のマツ樹冠ドット ＆ 障害物・隔壁・コンテナ・土嚢のOBBフットプリント
        if (ctx.obstacles)
        {
            for (const auto& obs : *ctx.obstacles)
            {
                if (!obs) continue;
                auto* box = obs->GetCollider();
                if (!box || !box->IsEnabled()) continue;
                const Vector3 ext = box->GetExtents();
                if (ext.y < RD::kObsMinExtY || ext.x > RD::kObsMaxExtXZ || ext.z > RD::kObsMaxExtXZ) continue;

                const Vector3 c = box->GetWorldPosition();
                // 河川コライダー矩形は上で川＋橋として描画済みのためスキップ
                if (std::abs(c.z - RD::kRiverCenterZ) < RD::kRiverSkipHalfZ &&
                    ext.z >= RD::kRiverSkipMinExtZ && ext.x >= RD::kRiverSkipMinExtX) continue;

                // 外周の針葉樹（細い幹コライダーかつ背が高い）：切り欠き森林を表現する緑の樹冠サークルを描画
                if (ext.x < RD::kTreeMaxTrunkExt && ext.z < RD::kTreeMaxTrunkExt && ext.y > RD::kTreeMinExtY)
                {
                    const UIVec2 tp = toMap(c.x, c.z);
                    const float tr  = (std::max)(RD::kTreeMinCanopyPx, RD::kTreeCanopyWorldR * ppm);
                    dl->AddCircleFilled(tp, tr, RD::kTreeFillCol, RD::kTreeCanopySegs);
                    dl->AddCircle(tp, tr, RD::kTreeOutlineCol, RD::kTreeCanopySegs, RD::kTreeOutlineThick);
                    continue;
                }

                const float ry = box->GetWorldRotation().y;
                const float cs = std::cos(ry);
                const float sn = std::sin(ry);
                const Vector2 localCorners[HudLayout::Common::kQuadVertCount] = {
                    { -ext.x, -ext.z }, { ext.x, -ext.z }, { ext.x, ext.z }, { -ext.x, ext.z }
                };
                UIVec2 poly[HudLayout::Common::kQuadVertCount];
                for (int k = 0; k < HudLayout::Common::kQuadVertCount; ++k)
                {
                    poly[k] = toMap(c.x + localCorners[k].x * cs + localCorners[k].y * sn,
                                    c.z - localCorners[k].x * sn + localCorners[k].y * cs);
                }

                const bool isFence     = (ext.x >= RD::kFenceMinExtX && ext.z < RD::kFenceMaxExtZ && ext.y > RD::kFenceMinExtY);
                const bool isLargeWall = (ext.y > RD::kLargeWallMinExtY);
                const uint32_t fillC   = isFence ? RD::kFenceFillCol : (isLargeWall ? RD::kWallFillCol : RD::kCoverFillCol);
                dl->AddConvexPolyFilled(poly, HudLayout::Common::kQuadVertCount, fillC);
            }
        }

        // 4B. 非矩形要塞の外郭境界ライン（橋の開口部を開けつつ複雑なステージ形状が一瞬で視認できる戦術ブループリント枠線）
        {
            const uint32_t borderCol = UI_COL32(RD::kBorderR, RD::kBorderG, RD::kBorderB,
                                                showZoneLabels ? RD::kBorderAlphaFull : RD::kBorderAlphaMini);
            const float borderThk = (showZoneLabels ? RD::kBorderThickFull : RD::kBorderThickMini) * uiScale;
            for (const auto& line : RD::kBorderLines)
            {
                strokeWorldOpen(line.points, line.count, borderCol, borderThk);
            }
        }

        // 4C. NavMesh A* 誘導ルートライン
        if (guidePath.size() > 1)
        {
            const uint32_t pathCol = obj.isExtractObjective ? RD::kPathExtractCol : RD::kPathNormalCol;
            const float pathThk    = (showZoneLabels ? RD::kPathThickFull : RD::kPathThickMini) * uiScale;
            for (size_t i = 0; i + 1 < guidePath.size(); ++i)
            {
                dl->AddLine(toMap(guidePath[i].x, guidePath[i].z), toMap(guidePath[i + 1].x, guidePath[i + 1].z), pathCol, pathThk);
            }
        }

        // 5. 物資クレート（未回収のみ：通常物資＝エメラルド、黄金アヒル＝ゴールド）
        if (ctx.lootProps)
        {
            for (const auto& prop : *ctx.lootProps)
            {
                if (prop.isLooted) continue;
                const UIVec2 mp   = toMap(prop.position.x, prop.position.z);
                const bool isGold = (prop.type == LootType::GoldDuck);
                const float r     = (showZoneLabels
                    ? (isGold ? RD::kLootGoldFullR : RD::kLootNormFullR)
                    : (isGold ? RD::kLootGoldMiniR : RD::kLootNormMiniR)) * uiScale;
                DrawDiamondPip(dl, mp.x, mp.y, r, isGold ? RD::kLootGoldCol : RD::kLootNormCol, RD::kLootOutlineCol);
            }
        }

        // 6. 脱出ヘリパッド LZ (14.0, 56.5)
        {
            const UIVec2 lz     = toMap(ctx.extractionGoalPos.x, ctx.extractionGoalPos.z);
            const float lzR     = (showZoneLabels ? RD::kLzRadiusFull : RD::kLzRadiusMini) * uiScale;
            const uint32_t lzCol = ctx.isReadyToExtract
                ? UI_COL32(RD::kLzOpenR, RD::kLzOpenG, RD::kLzOpenB, static_cast<int>(RD::kLzBaseAlpha + RD::kLzPulseAlpha * pulse))
                : RD::kLzLockedCol;
            dl->AddCircleFilled(lz, lzR, RD::kLzBgCol, RD::kLzCircleSegs);
            dl->AddCircle(lz, lzR, lzCol, RD::kLzCircleSegs, RD::kLzCircleThick);
            if (ctx.isReadyToExtract)
            {
                const uint32_t waveCol = UI_COL32(RD::kLzOpenR, RD::kLzOpenG, RD::kLzOpenB,
                                                  static_cast<int>(RD::kLzWaveMaxAlpha * (1.0f - pulse)));
                dl->AddCircle(lz, lzR + (RD::kLzWaveBaseR + RD::kLzWavePulseR * pulse) * uiScale, waveCol, RD::kLzCircleSegs, RD::kLzWaveThick);
            }
        }

        // 7. 未破壊の戦術標的 [1]-[4]
        if (ctx.targets)
        {
            for (int i = 0; i < static_cast<int>(ctx.targets->size()); ++i)
            {
                const auto& t = (*ctx.targets)[i];
                if (!t || t->IsDead()) continue;
                const Vector3 tp     = t->GetPosition();
                const UIVec2 mp      = toMap(tp.x, tp.z);
                const bool isPrimary = (i == obj.activeTargetIndex);
                const float r        = (showZoneLabels ? RD::kTargetRadiusFull : RD::kTargetRadiusMini) * uiScale;
                DrawDiamondPip(dl, mp.x, mp.y, r, isPrimary ? RD::kTargetPriCol : RD::kTargetSecCol, RD::kTargetOutline);
                if (isPrimary)
                {
                    dl->AddCircle(mp, r + (RD::kTargetPulseBaseR + RD::kTargetPulseAmpR * pulse) * uiScale,
                                  RD::kTargetPulseCol, RD::kTargetPulseSegs, RD::kTargetPulseThick);
                }
                if (showZoneLabels)
                {
                    char numBuf[HudLayout::Common::kTinyBufSize];
                    std::snprintf(numBuf, sizeof(numBuf), "[%d]", i + 1);
                    DrawShadowedText(dl, UIFontRole::SubBoldJP, RD::kTargetNumFont * uiScale,
                                     UIVec2(mp.x + RD::kTargetNumOffX * uiScale, mp.y + RD::kTargetNumOffY * uiScale),
                                     RD::kTargetNumCol, numBuf);
                }
            }
        }

        // 8. 敵ユニット（生存中：向き＆視界コーン）
        if (ctx.enemies)
        {
            const float er = (showZoneLabels ? RD::kEnemyRadiusFull : RD::kEnemyRadiusMini) * uiScale;
            for (const auto& ePtr : *ctx.enemies)
            {
                const Enemy* e = ePtr.get();
                if (!e || e->IsDead()) continue;
                const Vector3 ep   = e->GetPosition();
                const UIVec2 emp   = toMap(ep.x, ep.z);
                const float eyaw   = e->GetYaw();
                const bool alerted = (e->GetAIState() == Enemy::AIState::Chase);

                // 巡回視界コーン（ミニマップ・全体マップ上で死角が一目で分かる）
                const float coneDist = (std::min)(RD::kEnemyMaxConeDist, e->GetMaxSightRange() * RD::kEnemyConeRangeMul);
                const float halfFov  = e->GetFieldOfView() * 0.5f;
                const UIVec2 cL      = toMap(ep.x + std::sin(eyaw - halfFov) * coneDist, ep.z + std::cos(eyaw - halfFov) * coneDist);
                const UIVec2 cR      = toMap(ep.x + std::sin(eyaw + halfFov) * coneDist, ep.z + std::cos(eyaw + halfFov) * coneDist);
                dl->AddTriangleFilled(emp, cL, cR, alerted ? RD::kEnemyConeAlert : RD::kEnemyConeCalm);

                dl->AddCircleFilled(emp, er, alerted ? RD::kEnemyAlertCol : RD::kEnemyCalmCol, RD::kEnemyCircleSegs);
                dl->AddCircle(emp, er, RD::kEnemyOutlineCol, RD::kEnemyCircleSegs, RD::kEnemyOutlineThick);
            }
        }

        // 9. プレイヤー現在位置 ＆ 照準コーン
        {
            const UIVec2 pp   = toMap(pPos.x, pPos.z);
            const UIVec2 aimL = toMap(pPos.x + std::sin(pYaw - RD::kPlayerAimHalfAng) * RD::kPlayerAimLen,
                                      pPos.z + std::cos(pYaw - RD::kPlayerAimHalfAng) * RD::kPlayerAimLen);
            const UIVec2 aimR = toMap(pPos.x + std::sin(pYaw + RD::kPlayerAimHalfAng) * RD::kPlayerAimLen,
                                      pPos.z + std::cos(pYaw + RD::kPlayerAimHalfAng) * RD::kPlayerAimLen);
            dl->AddTriangleFilled(pp, aimL, aimR, RD::kPlayerConeCol);

            const float dx = std::sin(pYaw);
            const float dy = -std::cos(pYaw);
            const float nx = -dy;
            const float ny = dx;
            const float pr = (showZoneLabels ? RD::kPlayerRadiusFull : RD::kPlayerRadiusMini) * uiScale;
            const UIVec2 pTip(pp.x + dx * pr * RD::kPlayerTipMul, pp.y + dy * pr * RD::kPlayerTipMul);
            const UIVec2 pL(pp.x - dx * pr * RD::kPlayerBackMul + nx * pr * RD::kPlayerWingMul,
                            pp.y - dy * pr * RD::kPlayerBackMul + ny * pr * RD::kPlayerWingMul);
            const UIVec2 pR(pp.x - dx * pr * RD::kPlayerBackMul - nx * pr * RD::kPlayerWingMul,
                            pp.y - dy * pr * RD::kPlayerBackMul - ny * pr * RD::kPlayerWingMul);
            dl->AddTriangleFilled(pTip, pL, pR, RD::kPlayerFillCol);
            dl->AddTriangle(pTip, pL, pR, RD::kPlayerOutline, RD::kPlayerOutlineThick);
        }

        // 10. 全体作戦マップ展開時の日本語エリア名称ラベル
        if (showZoneLabels)
        {
            const float lblFont = MM::kZoneLabelFont * uiScale;
            for (const auto& zl : MM::kZoneLabels)
            {
                const char* txt = AsUtf8(zl.label);
                const UIVec2 lp = toMap(zl.worldX, zl.worldZ);
                const UIVec2 sz = MeasureSharpText(UIFontRole::SubBoldJP, lblFont, txt);
                const float bx  = std::round(lp.x - sz.x * 0.5f - MM::kZoneLabelHalfPadX * uiScale);
                const float by  = std::round(lp.y - sz.y * 0.5f - MM::kZoneLabelHalfPadY * uiScale);
                dl->AddRectFilled(UIVec2(bx, by),
                                  UIVec2(bx + sz.x + MM::kZoneLabelPadW * uiScale, by + sz.y + MM::kZoneLabelPadH * uiScale),
                                  MM::kZoneLabelBg, MM::kZoneLabelRound);
                DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, lblFont, lp.x, lp.y, zl.color, txt, HudTextAlign::Center);
            }
        }

        // 方位 N マーク
        DrawShadowedText(dl, UIFontRole::SubBoldJP, RD::kNorthFont * uiScale,
                         UIVec2(viewX + RD::kNorthPadX * uiScale, viewY + RD::kNorthPadY * uiScale), RD::kNorthCol, "N");
        dl->PopClipRect();
    }
}

void GamePlayHUD::DrawTacticalMapHUD(const GamePlayHUDContext& ctx)
{
    if (!ctx.player || ctx.player->IsDead() || ctx.isDeathSequenceActive) return;

    HudViewport vp;
    if (!AcquireHudViewport(vp)) return;
    TacticalDrawList* dl = vp.dl;
    const float screenW  = vp.screenW;
    const float screenH  = vp.screenH;
    const float uiScale  = vp.uiScale;

    namespace MM = HudLayout::Minimap;
    const float timeNow           = static_cast<float>(TacticalUICanvas::GetInstance()->GetTime());
    const float pulse             = 0.5f + 0.5f * std::sin(timeNow * HudTiming::kMapPulseSpeed);
    const Vector3 pPos            = ctx.player->GetPosition();
    const float pYaw              = ctx.player->GetRotation().y;
    const ActiveObjectiveInfo obj = ResolveActiveObjective(ctx, lastActiveTargetIdx_);

    // =========================================================================
    // A. 常時表示 左上タクティカルミニマップレーダー ＆ 回収戦利品(RUB)ポーチバー
    // =========================================================================
    if (!ctx.isFullMapOpen)
    {
        const float miniW = MM::kMiniW * uiScale;
        const float miniH = MM::kMiniH * uiScale;
        const float miniX = std::round(MM::kMiniX * uiScale);
        const float miniY = std::round(MM::kMiniY * uiScale);
        const float slant = MM::kMiniSlant * uiScale;

        DrawFramedPanel(dl, miniX, miniY, miniW, miniH, slant, MM::kMiniPanelStyle);
        dl->AddLine(UIVec2(miniX + slant, miniY), UIVec2(miniX + miniW + slant, miniY), MM::kMiniTopLineCol, MM::kTopAccentThick * uiScale);

        DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, MM::kSectorFont * uiScale,
            miniX + MM::kSectorPadX * uiScale, miniY + MM::kSectorCenterY * uiScale,
            MM::kSectorTextCol, GetSectorNameJP(pPos), HudTextAlign::Left);

        RenderCompoundMapCanvas(dl, ctx, cachedGuidePath_, obj, pPos, pYaw, uiScale, pulse,
            miniX + MM::kMapPadX * uiScale, miniY + MM::kMapTopY * uiScale,
            miniW - MM::kMapTrimW * uiScale, miniH - MM::kMapTrimH * uiScale, false);

        const char* footTxt = ctx.isOverheadCameraMode ? AsUtf8(u8"[M]地図  [Y]通常") : AsUtf8(u8"[M]地図  [Y]上空");
        DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, MM::kFooterFont * uiScale,
            miniX + miniW * 0.5f, miniY + miniH - MM::kFooterBotPad * uiScale,
            MM::kFooterTextCol, footTxt, HudTextAlign::Center);

        // レーダー直下に戦利品総額(SECURED RUB)をスマートにドッキング表示
        const UITextureID digitsTex = ResolveHudTexture(clearDigitsTextureIndex_);
        const float lootY           = miniY + miniH + MM::kLootBarGapY * uiScale;
        const float lootH           = MM::kLootBarH * uiScale;
        DrawSlantedPlate(dl, miniX, lootY, miniW, lootH, MM::kLootBarSlant * uiScale,
                         HudPalette::kGunmetalBg232, MM::kLootBarBorder, MM::kLootBarBorderThick);

        const UITextureID goldTex = ResolveHudTexture(goldDuckTextureIndex_);
        if (goldTex != 0)
        {
            const float gSz = MM::kLootIconSize * uiScale;
            const UIVec2 gMin(miniX + MM::kLootIconPadX * uiScale, lootY + (lootH - gSz) * 0.5f);
            dl->AddImageRounded(goldTex, gMin, UIVec2(gMin.x + gSz, gMin.y + gSz),
                                UIVec2(0.0f, 0.0f), UIVec2(1.0f, 1.0f), MM::kLootIconTint, MM::kLootIconRound);
        }

        char commaLoot[HudLayout::Common::kSmallBufSize];
        FormatHudCommas(ctx.player->GetLootValue(), commaLoot, sizeof(commaLoot));
        char lootFull[HudLayout::Common::kMedBufSize];
        std::snprintf(lootFull, sizeof(lootFull), "%s RUB", commaLoot);
        DrawHudAtlasDigits(dl, digitsTex, lootFull,
            miniX + miniW - MM::kLootTextRightPad * uiScale, lootY + lootH * 0.5f,
            MM::kLootDigitH * uiScale, MM::kLootValueCol, HudTextAlign::Right);
        return;
    }

    // =========================================================================
    // B. [M] / [TAB] 全体作戦マップオーバーレイ (エリア構造・凡例・攻略手順)
    // =========================================================================
    dl->AddRectFilled(UIVec2(0.0f, 0.0f), UIVec2(screenW, screenH), MM::kBackdropDimCol);

    const float boardW     = MM::kBoardW * uiScale;
    const float boardH     = MM::kBoardH * uiScale;
    const float boardX     = std::round((screenW - boardW) * 0.5f);
    const float boardY     = std::round((screenH - boardH) * 0.5f + MM::kBoardOffsetY * uiScale);
    const float boardSlant = MM::kBoardSlant * uiScale;

    DrawFramedPanel(dl, boardX, boardY, boardW, boardH, boardSlant, MM::kBoardPanelStyle);
    dl->AddLine(UIVec2(boardX + boardSlant, boardY), UIVec2(boardX + boardW + boardSlant, boardY),
                MM::kBoardTopLineCol, MM::kBoardTopLineThick * uiScale);

    DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, MM::kBoardTitleFont * uiScale,
        boardX + MM::kBoardTitlePadX * uiScale, boardY + MM::kBoardHeaderCenterY * uiScale,
        MM::kBoardTitleCol, AsUtf8(u8"TACTICAL MAP // 作戦マップ"), HudTextAlign::Left);
    DrawShadowedTextAligned(dl, UIFontRole::SubBoldJP, MM::kCloseHintFont * uiScale,
        boardX + boardW - MM::kCloseHintRightPad * uiScale, boardY + MM::kBoardHeaderCenterY * uiScale,
        MM::kCloseHintCol, AsUtf8(u8"[M] / [ESC] 閉じる"), HudTextAlign::Right);
    dl->AddLine(UIVec2(boardX + MM::kHeaderDivLeftPad * uiScale, boardY + MM::kHeaderDivY * uiScale),
                UIVec2(boardX + boardW - MM::kHeaderDivRightPad * uiScale, boardY + MM::kHeaderDivY * uiScale),
                MM::kHeaderDivCol, MM::kHeaderDivThick);

    // 左ペイン：大型等方コンパウンドマップ
    const float mapW = MM::kFullMapW * uiScale;
    const float mapH = boardH - MM::kFullMapTrimH * uiScale;
    RenderCompoundMapCanvas(dl, ctx, cachedGuidePath_, obj, pPos, pYaw, uiScale, pulse,
        boardX + MM::kFullMapPadX * uiScale, boardY + MM::kFullMapTopY * uiScale, mapW, mapH, true);

    // 右ペイン：凡例 ＆ 攻略チャート
    const float sideX   = boardX + mapW + MM::kSideGapX * uiScale;
    const float sideW   = boardW - mapW - MM::kSideTrimW * uiScale;
    const float secFont = MM::kSectionFont * uiScale;
    float curY          = boardY + MM::kSideStartY * uiScale;

    DrawShadowedText(dl, UIFontRole::SubBoldJP, secFont, UIVec2(sideX, curY), MM::kCurrentHdrCol, AsUtf8(u8"【 現在地 】"));
    curY += MM::kCurrentHdrStepY * uiScale;
    DrawShadowedText(dl, UIFontRole::SubBoldJP, secFont, UIVec2(sideX + MM::kCurrentSecIndentX * uiScale, curY),
                     HudPalette::kTextBrightWhite, GetSectorNameJP(pPos));
    curY += MM::kCurrentSecStepY * uiScale;

    dl->AddLine(UIVec2(sideX, curY), UIVec2(sideX + sideW, curY), MM::kSideDivCol, MM::kHeaderDivThick);
    curY += MM::kSideDivStepY * uiScale;

    DrawShadowedText(dl, UIFontRole::SubBoldJP, secFont, UIVec2(sideX, curY), MM::kSubHdrCol, AsUtf8(u8"【 凡例 】"));
    curY += MM::kSubHdrStepY * uiScale;

    for (const auto& item : MM::kLegendItems)
    {
        const float ix = sideX + MM::kLegendIconOffX * uiScale;
        const float iy = curY + MM::kLegendIconOffY * uiScale;
        if (item.isCircle)
        {
            dl->AddCircleFilled(UIVec2(ix, iy), MM::kLegendCircleRad * uiScale, item.color, MM::kLegendCircleSegs);
        }
        else
        {
            DrawDiamondPip(dl, ix, iy, MM::kLegendDiamondRad * uiScale, item.color, MM::kLegendPipOut);
        }
        DrawShadowedText(dl, UIFontRole::DefaultJP, MM::kLegendFont * uiScale,
                         UIVec2(sideX + MM::kLegendTextOffX * uiScale, curY), MM::kLegendTextCol, AsUtf8(item.label));
        curY += MM::kLegendRowStepY * uiScale;
    }

    curY += MM::kAfterLegendGapY * uiScale;
    dl->AddLine(UIVec2(sideX, curY), UIVec2(sideX + sideW, curY), MM::kSideDivCol, MM::kHeaderDivThick);
    curY += MM::kSideDivStepY * uiScale;

    DrawShadowedText(dl, UIFontRole::SubBoldJP, secFont, UIVec2(sideX, curY), MM::kSubHdrCol, AsUtf8(u8"【 攻略チャート 】"));
    curY += MM::kSubHdrStepY * uiScale;

    for (size_t s = 0; s < std::size(MM::kGuideSteps); ++s)
    {
        const uint32_t stCol = (s >= MM::kGuideTipStartIdx) ? MM::kGuideTipCol : MM::kLegendTextCol;
        DrawShadowedText(dl, UIFontRole::DefaultJP, MM::kGuideFont * uiScale,
                         UIVec2(sideX + MM::kGuideIndentX * uiScale, curY), stCol, AsUtf8(MM::kGuideSteps[s]));
        curY += MM::kGuideRowStepY * uiScale;
    }
}
