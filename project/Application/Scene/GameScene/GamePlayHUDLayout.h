#pragma once
#include <cstddef>
#include <cstdint>
#include "Vector.h"
#include "Application/Config/GameConfig.h"
#include "Application/UI/TacticalUICanvas.h"

// --- タクティカルHUD 共通データ構造体 (Data-Driven Spec Structs) ---

/// <summary>テキスト水平揃え指定</summary>
enum class HudTextAlign : int
{
    Left   = 0,
    Center = 1,
    Right  = 2,
};

/// <summary>影付き傾斜プレートの描画スタイル定義</summary>
struct SlantedPanelStyle
{
    float shadowOffsetX;
    float shadowOffsetY;
    uint32_t shadowCol;
    uint32_t fillCol;
    uint32_t borderCol;
    float borderThick;
};

/// <summary>アトラス内スプライト矩形定義 (ピクセル単位)</summary>
struct AtlasSpriteRect { float sx0; float sy0; float sx1; float sy1; };

/// <summary>進行フェーズ別ナビゲーション案内テキスト＆アクセント色定義</summary>
struct ObjectivePhaseSpec
{
    const char* phaseTag;
    const char8_t* markerLabel;
    const char8_t* actionLine1;
    const char8_t* actionLine2;
    uint32_t accentCol;
};

/// <summary>銃シルエット等のベクター矩形パーツ定義 (uiScale乗算前オフセット)</summary>
struct HudRectPart { float x0; float y0; float x1; float y1; float rounding; bool useAccentCol; };

/// <summary>アークストロークの多重レイヤー定義 (被弾インジケーター等)</summary>
struct ArcStrokeLayerSpec { uint8_t r; uint8_t g; uint8_t b; float alphaScale; float thickness; };

/// <summary>3D浮遊コールアウトバッジ定義</summary>
struct CalloutBadgeSpec
{
    const char* matchToken;
    const char8_t* displayLabel;
    uint8_t bgR;
    uint8_t bgG;
    uint8_t bgB;
    uint8_t bdR;
    uint8_t bdG;
    uint8_t bdB;
    uint8_t txR;
    uint8_t txG;
    uint8_t txB;
};

/// <summary>下部キー操作ガイド項目定義</summary>
struct ControlGuideItemSpec { const char* key; const char8_t* desc; uint32_t highlightCol; };

/// <summary>マップ床面ポリゴン種別</summary>
enum class MapFloorTone : uint8_t
{
    Standard = 0,
    Vault    = 1,
    Helipad  = 2,
};

/// <summary>マップ凸ポリゴン定義</summary>
struct MapConvexPolySpec { const Vector2* points; int count; MapFloorTone tone; };

/// <summary>マップ外郭ポリライン定義</summary>
struct MapPolylineSpec { const Vector2* points; int count; };

/// <summary>全体作戦マップ エリア名称ラベル定義</summary>
struct MapZoneLabelSpec { float worldX; float worldZ; const char8_t* label; uint32_t color; };

/// <summary>全体作戦マップ 凡例項目定義</summary>
struct MapLegendItemSpec { uint32_t color; bool isCircle; const char8_t* label; };

// --- HUD共通カラーパレット (HudPalette) ---
namespace HudPalette
{
    // ビットマスク ＆ チャネル変換定数
    inline constexpr uint32_t kChannelMask = 0xFFu, kRgbMask = 0x00FFFFFFu;
    inline constexpr int kMaxAlphaInt = 255;
    inline constexpr float kMaxAlphaFloat = 255.0f;

    // シーン入場フェード暗幕ベース色
    inline constexpr int kEntranceFadeR = 8, kEntranceFadeG = 14, kEntranceFadeB = 20;

    // 共通シャドウ・テキスト色
    inline constexpr uint32_t kPanelShadow128 = UI_COL32(0, 0, 0, 128), kPanelShadow125 = UI_COL32(0, 0, 0, 125);
    inline constexpr uint32_t kPanelShadow120 = UI_COL32(0, 0, 0, 120), kPanelShadow130 = UI_COL32(0, 0, 0, 130);
    inline constexpr uint32_t kPanelShadow140 = UI_COL32(0, 0, 0, 140), kPanelShadow145 = UI_COL32(0, 0, 0, 145);
    inline constexpr uint32_t kPanelShadow175 = UI_COL32(0, 0, 0, 175), kSlotShadow115 = UI_COL32(0, 0, 0, 115);

    // ガンメタル装甲プレート共通背景・枠線色
    inline constexpr uint32_t kGunmetalBg234 = UI_COL32(9, 14, 21, 234), kGunmetalBg232 = UI_COL32(9, 14, 21, 232);
    inline constexpr uint32_t kGunmetalBg236 = UI_COL32(9, 14, 21, 236), kGunmetalBg238 = UI_COL32(9, 14, 21, 238);
    inline constexpr uint32_t kGunmetalBorder180 = UI_COL32(82, 108, 130, 180), kGunmetalBorder175 = UI_COL32(78, 104, 126, 175);
    inline constexpr uint32_t kTaskBorder175 = UI_COL32(82, 108, 130, 175);

    // 共通アクセントカラー
    inline constexpr uint32_t kCyanAccent = UI_COL32(72, 232, 255, 255), kEmeraldAccent = UI_COL32(52, 248, 146, 255);
    inline constexpr uint32_t kEmeraldReady = UI_COL32(52, 246, 146, 248), kGoldAccent = UI_COL32(255, 198, 58, 245);
    inline constexpr uint32_t kDangerRed = UI_COL32(255, 68, 62, 250), kTextPureWhite = UI_COL32(255, 255, 255, 255);
    inline constexpr uint32_t kTextIceWhite = UI_COL32(246, 250, 255, 255), kTextBrightWhite = UI_COL32(248, 252, 255, 255);

    // チェックマークバッジ色
    inline constexpr uint32_t kCheckDoneFill = UI_COL32(28, 168, 98, 240), kCheckDoneBorder = UI_COL32(120, 255, 185, 245);
    inline constexpr uint32_t kCheckStroke = UI_COL32(245, 255, 250, 255), kCheckPendingFill = UI_COL32(18, 26, 34, 230);
}

// --- HUDアニメーション・演出タイミング定数 (HudTiming) ---
namespace HudTiming
{
    inline constexpr float kSceneEntranceFadeSec  = 0.75f;  // シーン開始時の暗幕フェードイン時間
    inline constexpr float kLaserEntranceGateSec  = 0.05f;  // 入場直後のレーザー非表示閾値
    inline constexpr float kReticleEntranceGateSec = 0.55f; // 入場直後のレティクル非表示閾値

    // レイドタイマー警告閾値 (秒)
    inline constexpr int kSecondsPerMinute = 60;
    inline constexpr float kUrgentTimeSec = 30.0f, kWarnTimeSec = 60.0f, kUrgentBlinkSpeed = 8.5f;

    // バイタル＆クイックスロット脈動
    inline constexpr float kHpLagDecayRate = 0.55f, kVitalsPulseSpeed = 6.5f;

    // 脱出シネマ＆バナー演出時間
    inline constexpr float kClimaxFlashEnd = 0.22f, kClimaxLetterboxEnd = 0.22f, kClimaxBannerStart = 0.04f;
    inline constexpr float kClimaxBannerSpan = 0.20f, kClimaxSubStart = 0.10f, kClimaxSubSpan = 0.22f;
    inline constexpr float kHelipadPulseSpeed = 5.0f, kHelipadRingRotSpeed = 0.35f, kHelipadMarkerBobSpeed = 3.2f;
    inline constexpr float kUnlockBannerTotalSec = 4.2f, kUnlockBannerFadeInSec = 0.28f, kUnlockBannerFadeOutSec = 0.45f;

    // PMAGリロード段階フェーズ比率
    inline constexpr float kReloadEjectEnd = 0.20f, kReloadEmptyEnd = 0.30f, kReloadInsertEnd = 0.67f;
    inline constexpr float kReloadInsertSpan = 0.37f, kReloadSeatTickRatio = 0.68f;

    // 敵テレグラフ＆被弾フィードバック
    inline constexpr float kTelegraphFlashStart = 0.72f, kTelegraphFlashSpeed = 38.0f, kHitIndicatorDuration = 0.65f;
    inline constexpr float kVignetteBaseFreq = 4.0f, kVignetteDangerFreqMul = 6.0f, kSoundGizmoDuration = 0.5f, kMsPerSecond = 1000.0f;

    // 3Dナビゲーション＆マップ脈動
    inline constexpr float kGuideRefreshInterval = 0.20f, kGuideWaveSpeed = 7.2f, kGuideFlowSpeed = 2.75f;
    inline constexpr float kBeaconPulseSpeed = 5.0f, kBeaconRotSpeed = 1.1f, kTargetMarkerBobSpeed = 4.5f;
    inline constexpr float kLootMarkerBobSpeed = 4.8f, kLootMarkerPulseSpeed = 5.2f, kOffscreenPulseSpeed = 6.5f;
    inline constexpr float kControlsPulseSpeed = 6.5f, kMapPulseSpeed = 5.2f;
}

// --- HUD各レイヤー 座標・サイズ・スタイル定数群 (HudLayout) ---
namespace HudLayout
{
    // --- 画面スケール・バッファ・基本プリミティブ定数 ---
    namespace Common
    {
        inline constexpr float kReferenceScreenW = 1280.0f, kStandardScaleMin = 0.75f, kStandardScaleMax = 1.55f;
        inline constexpr float kExtractionScaleMin = 0.75f, kExtractionScaleMax = 1.65f, kReticleScaleMin = 0.80f;
        inline constexpr float kReticleScaleMax = 1.50f;

        inline constexpr float kFontMinPx = 12.0f, kFontMaxPx = 96.0f, kDefaultShadowAlpha = 0.86f;
        inline constexpr float kMinShadowAlpha = 0.01f, kShadowOffsetPx = 1.0f, kAtlasDigitFontMul = 0.88f;
        inline constexpr float kAtlasDigitShadowAlpha = 0.88f;

        inline constexpr float kSmoothStepA = 3.0f, kSmoothStepB = 2.0f, kPercentMul = 100.0f;

        // 文字列フォーマット＆作業バッファサイズ
        inline constexpr size_t kTinyBufSize = 8, kShortBufSize = 16, kSmallBufSize = 32, kMedBufSize = 48;
        inline constexpr size_t kLabelBufSize = 64, kWideBufSize = 80, kStepBufSize = 96, kLineBufSize = 128;

        inline constexpr int kCommaGroupDigits = 3, kCommaMaxWriteIdx = 44;

        // ポリゴン頂点数・配列予約サイズ
        inline constexpr int kTriVertCount = 3, kQuadVertCount = 4, kQuadBottomRightIdx = 2;
        inline constexpr size_t kGuidePathReserve = 64, kSmoothPathReserve = 128;

        // 3D->2D 透視投影 行列インデックス
        inline constexpr int kMatX = 0, kMatY = 1, kMatZ = 2, kMatW = 3;

        // 基本図形パラメータ
        inline constexpr float kMinPlateSize = 0.5f, kMinPipRadius = 0.5f, kDiamondOutlineThick = 1.3f;
        inline constexpr int kCheckCircleSegs = 16;
        inline constexpr float kCheckCircleThick = 1.4f, kCheckStrokeThick = 2.0f, kCheckP0X = -0.42f;
        inline constexpr float kCheckP0Y = 0.02f, kCheckP1X = -0.10f, kCheckP1Y = 0.36f, kCheckP2X = 0.46f;
        inline constexpr float kCheckP2Y = -0.32f, kCheckOuterPipMul = 0.88f, kCheckInnerPipMul = 0.36f;
    }

    // --- セクター名判定 ＆ アクティブ目標解決閾値 ---
    namespace SectorBounds
    {
        inline constexpr float kLandingPadMaxZ = 6.5f, kLandingPadHalfX = 10.2f, kBridgeMinZ = 27.0f;
        inline constexpr float kBridgeMaxZ = 33.2f, kSouthMaxZ = 27.5f, kSmuggleDockMinX = 24.5f;
        inline constexpr float kSmuggleDockMinZ = 14.0f, kShootingRangeMinX = 7.5f, kCombatYardMaxX = -8.5f;
        inline constexpr float kNorthWestMaxX = -1.5f, kVaultMinZ = 50.5f, kExtractPadMinZ = 51.5f;
    }

    namespace Objective
    {
        inline constexpr float kStarterCrateMinX = 5.0f, kStarterCrateMaxZ = 10.0f, kStarterGuideMaxZ = 20.0f;
        inline constexpr float kStarterGuideMinX = -8.0f, kTarget0ZoneMaxZ = 27.5f, kTarget0ZoneMinX = -3.0f;
        inline constexpr float kTarget1ZoneMaxZ = 29.0f, kBridgePlayerMaxZ = 29.0f, kBridgeTargetMinZ = 32.0f, kMaxSearchDistSq = 1e9f;

        inline constexpr int kTargetIdxEastRange = 0, kTargetIdxWestYard = 1, kTargetIdxNorthWest = 2;

        inline constexpr ObjectivePhaseSpec kPhaseExtractInZone = { "FINAL", u8"脱出中", u8"緑スモーク円内を維持", u8"3秒待機でヘリ生還",
        	HudPalette::kEmeraldAccent };
        inline constexpr ObjectivePhaseSpec kPhaseExtractReady = { "FINAL", u8"脱出地点", u8"全標的を破壊完了！", u8"北東の緑スモークへ急行",
        	HudPalette::kEmeraldAccent };
        inline constexpr ObjectivePhaseSpec kPhaseStarterLoot = { "STEP 1 / 5", u8"弾薬箱", u8"東棟の木箱へ進む", u8"[左クリック] 射撃で木箱を破壊",
        	HudPalette::kCyanAccent };
        inline constexpr ObjectivePhaseSpec kPhaseTarget1 = {
        	"STEP 2 / 5", u8"標的 [1]", u8"東棟の【標的[1]】を射撃", u8"[左クリック]射撃 / [R]リロード", UI_COL32(255, 204, 62, 255) };
        inline constexpr ObjectivePhaseSpec kPhaseTarget2 = {
        	"STEP 3 / 5", u8"標的 [2]", u8"西棟の【標的[2]】を破壊", u8"土嚢遮蔽 / [SPACE]回避", UI_COL32(255, 166, 56, 255) };
        inline constexpr ObjectivePhaseSpec kPhaseBridgeTo3 = {
        	"STEP 4 / 5", u8"標的 [3]", u8"西か東の橋で北へ渡河", u8"[C]ダッシュで高速移動", HudPalette::kCyanAccent };
        inline constexpr ObjectivePhaseSpec kPhaseBridgeTo4 = {
        	"STEP 4 / 5", u8"標的 [4]", u8"西か東の橋で北へ渡河", u8"[C]ダッシュで高速移動", HudPalette::kCyanAccent };
        inline constexpr ObjectivePhaseSpec kPhaseTarget3 = {
        	"STEP 4 / 5", u8"標的 [3]", u8"視界を避け【標的[3]】破壊", u8"北西奥に黄金アヒル像あり", UI_COL32(255, 196, 62, 255) };
        inline constexpr ObjectivePhaseSpec kPhaseTarget4 = {
        	"STEP 4 / 5", u8"標的 [4]", u8"司令部の【標的[4]】を破壊", u8"負傷時は物陰で [Q] 回復", UI_COL32(255, 184, 58, 255) };
    }

    // --- 画面上部中央：作戦目標 ＆ レイドタイマーヘッダー ---
    namespace MissionHeader
    {
        inline constexpr float kWidth = 456.0f, kHeight = 38.0f, kSlant = 7.0f, kTopY = 10.0f, kTopAccentThick = 2.2f;

        inline constexpr SlantedPanelStyle kPanelStyle = { 2.0f, 2.5f, HudPalette::kPanelShadow128,
        	HudPalette::kGunmetalBg234, HudPalette::kGunmetalBorder180, 1.2f };

        inline constexpr float kDiv1OffsetX = 144.0f, kDiv2OffsetX = 302.0f, kDivTopSlantMul = 0.8f;
        inline constexpr float kDivBotSlantMul = 0.2f, kDivPadY = 5.5f, kDivThick = 1.0f;

        inline constexpr float kLabelFont = 14.0f, kTimerFont = 18.0f, kTargetCountFont = 17.0f, kDistFont = 16.0f;

        inline constexpr float kLeftLabelPadX = 11.0f, kTimerRightPadX = 9.0f, kMidLabelPadX = 10.0f;
        inline constexpr float kTargetRightPadX = 9.0f, kRightLabelPadX = 10.0f, kDistRightPadX = 10.0f;

        inline constexpr int kMaxTargetPips = 6;
        inline constexpr float kPipStartX = 52.0f, kPipSpacing = 14.0f, kPipDoneRadius = 4.6f;
        inline constexpr float kPipOpenOuterRad = 4.4f, kPipOpenInnerRad = 1.9f;

        inline constexpr uint32_t kUrgentBlinkHi = UI_COL32(255, 58, 52, 255), kUrgentBlinkLo = UI_COL32(165, 28, 28, 235);
        inline constexpr uint32_t kWarnAccent = UI_COL32(255, 152, 36, 248), kDividerCol = UI_COL32(96, 122, 142, 135);
        inline constexpr uint32_t kTimeLabelCol = UI_COL32(255, 212, 92, 255), kTimeUrgentCol = UI_COL32(255, 92, 84, 255);
        inline constexpr uint32_t kTimeWarnCol = UI_COL32(255, 210, 86, 255), kTargetLabelCol = UI_COL32(224, 238, 250, 255);
        inline constexpr uint32_t kPipDoneFill = UI_COL32(52, 248, 146, 252), kPipDoneOutline = UI_COL32(205, 255, 228, 230);
        inline constexpr uint32_t kPipOpenFill = UI_COL32(36, 20, 24, 230), kPipOpenOutline = UI_COL32(255, 112, 92, 245);
        inline constexpr uint32_t kPipOpenCore = UI_COL32(255, 102, 82, 235), kTargetReadyCol = UI_COL32(98, 255, 172, 255);
        inline constexpr uint32_t kTargetActiveCol = UI_COL32(255, 216, 96, 255), kLzOpenLabelCol = UI_COL32(86, 255, 170, 255);
        inline constexpr uint32_t kLzLockLabelCol = UI_COL32(255, 136, 114, 255), kLzOpenDistCol = UI_COL32(126, 255, 188, 255);
        inline constexpr uint32_t kLzLockDistCol = UI_COL32(218, 232, 244, 248);
    }

    // --- 画面左下：バイタル・スタミナ・遮蔽ステルス ＆ クイックスロット HUD ---
    namespace StatusPanel
    {
        inline constexpr float kWidth = 232.0f, kHeight = 76.0f, kSlant = 7.0f, kMarginX = 14.0f;
        inline constexpr float kMarginBottom = 14.0f, kStripeWidth = 4.0f;

        inline constexpr SlantedPanelStyle kPanelStyle = { 2.0f, 2.5f, HudPalette::kPanelShadow125,
        	HudPalette::kGunmetalBg234, HudPalette::kGunmetalBorder175, 1.1f };

        // STEALTH バッジ
        inline constexpr float kStealthFont = 13.0f, kStealthPadW = 26.0f, kStealthHeight = 21.0f;
        inline constexpr float kStealthOffsetX = 4.0f, kStealthGapY = 5.0f, kStealthSlant = 4.5f;
        inline constexpr float kStealthBorderThick = 1.2f, kStealthPipX = 10.0f, kStealthPipRad = 3.6f, kStealthTextX = 18.0f;
        inline constexpr uint32_t kStealthBg = UI_COL32(10, 38, 28, 238), kStealthBorder = UI_COL32(62, 250, 156, 245);
        inline constexpr uint32_t kStealthPipCol = UI_COL32(82, 255, 172, 255), kStealthTextCol = UI_COL32(150, 255, 202, 255);

        // HP・スタミナゲージ
        inline constexpr float kCritHpRatio = 0.35f, kWarnHpRatio = 0.65f, kMinFillRatio = 0.005f;
        inline constexpr float kHpLabelFont = 14.0f, kHpNumFont = 16.0f, kLabelOffsetX = 12.0f, kHpHeaderY = 14.5f;
        inline constexpr float kHpNumRightPad = 10.0f;

        inline constexpr float kBarOffsetX = 12.0f, kBarWidthTrim = 22.0f, kHpBarY = 26.5f, kHpBarH = 10.5f;
        inline constexpr float kHpBarSlant = 4.0f, kHpBarBorderThick = 1.0f;

        inline constexpr float kStamLabelFont = 13.0f, kStamLabelY = 49.0f, kStamBarY = 60.5f, kStamBarH = 7.0f;
        inline constexpr float kStamBarSlant = 2.6f, kDodgeStamRatio = 0.30f, kDodgeTickPadY = 1.5f, kDodgeTickThick = 1.3f;

        inline constexpr uint32_t kHpLabelCol = UI_COL32(228, 240, 250, 255), kHpNumCritCol = UI_COL32(255, 96, 88, 255);
        inline constexpr uint32_t kHpNumWarnCol = UI_COL32(255, 216, 96, 255);
        inline constexpr uint32_t kHpNumFineCol = UI_COL32(244, 250, 255, 255), kHpBarBg = UI_COL32(24, 32, 40, 235);
        inline constexpr uint32_t kHpBarBorder = UI_COL32(82, 104, 124, 140), kHpBarLagCol = UI_COL32(255, 238, 225, 225);
        inline constexpr uint32_t kHpBarFineCol = UI_COL32(52, 240, 146, 245), kHpBarWarnCol = UI_COL32(252, 188, 48, 248);
        inline constexpr uint32_t kHpBarCritCol = UI_COL32(248, 62, 58, 250), kStamLabelCol = UI_COL32(174, 228, 250, 250);
        inline constexpr uint32_t kStamBarBg = UI_COL32(22, 30, 38, 235), kStamLowCol = UI_COL32(255, 142, 42, 245);
        inline constexpr uint32_t kStamNormalCol = UI_COL32(72, 220, 255, 242), kDodgeTickCol = UI_COL32(240, 248, 255, 195);

        // クイックスロット [Q] / [C]
        inline constexpr float kSlotWidth = 56.0f, kSlotHeight = 60.0f, kSlotGap = 6.0f, kSlotPanelGap = 8.0f;
        inline constexpr float kSlotShadowOffX = 1.5f, kSlotShadowOffY = 2.0f, kSlotRounding = 4.0f;
        inline constexpr float kSlotActiveThick = 1.6f, kSlotIdleThick = 1.1f;

        inline constexpr float kKeycapMin = 3.0f, kKeycapMaxX = 20.0f, kKeycapMaxY = 18.0f, kKeycapRounding = 2.5f;
        inline constexpr float kKeycapBorderThick = 1.0f, kKeycapFont = 12.0f, kKeycapCenterX = 11.5f, kKeycapCenterY = 10.5f;
        inline constexpr uint32_t kKeycapBg = UI_COL32(22, 34, 44, 245), kKeycapTextCol = UI_COL32(246, 252, 255, 255);

        // [Q] 救急キットスロット詳細
        inline constexpr float kUrgeHealHpRatio = 0.68f, kUrgeAlphaBase = 140.0f, kUrgeAlphaPulse = 105.0f;
        inline constexpr float kHealMinProg = 0.01f, kHealFillInset = 1.0f, kHealFillRounding = 3.0f;
        inline constexpr float kMedIconSize = 31.0f, kMedIconTopY = 13.5f, kMedIconRounding = 3.0f;
        inline constexpr float kMedSubRightPad = 4.0f, kMedSubBotPad = 9.0f, kMedPctHeight = 15.0f, kMedCountHeight = 15.5f;

        inline constexpr uint32_t kMedActiveBg = UI_COL32(14, 42, 30, 238), kMedActiveBorder = UI_COL32(58, 250, 152, 248);
        inline constexpr int kMedUrgeR = 58, kMedUrgeG = 248, kMedUrgeB = 152;
        inline constexpr uint32_t kMedReadyBorder = UI_COL32(78, 114, 126, 185), kMedEmptyBorder = UI_COL32(56, 68, 78, 135);
        inline constexpr uint32_t kMedHealFill = UI_COL32(42, 210, 122, 125), kMedIconActive = UI_COL32(255, 255, 255, 250);
        inline constexpr uint32_t kMedIconEmpty = UI_COL32(115, 125, 135, 130), kMedPctTextCol = UI_COL32(138, 255, 194, 255);
        inline constexpr uint32_t kMedHasCountCol = UI_COL32(106, 252, 174, 255), kMedZeroCountCol = UI_COL32(158, 172, 186, 205);

        // [C] ホルスター速歩スロット詳細
        inline constexpr float kHolsterIconCy = 29.5f, kSlingX0 = -12.0f, kSlingY0 = 6.0f, kSlingX1 = 7.0f;
        inline constexpr float kSlingY1 = -7.0f, kSlingThick = 2.6f, kGripX0 = -3.0f, kGripY0 = 1.0f, kGripX1 = -1.0f;
        inline constexpr float kGripY1 = 7.0f, kGripThick = 2.2f;
        inline constexpr int kSprintChevronCount = 2;
        inline constexpr float kChevronStartX = 5.0f, kChevronStepX = 6.0f, kChevronTopY = -3.5f, kChevronMidY = 0.5f;
        inline constexpr float kChevronBotY = 4.5f, kChevronSpanX = 4.0f, kChevronThick = 1.8f;
        inline constexpr float kHolsterSubFont = 13.0f, kHolsterSubBotPad = 9.0f;

        inline constexpr uint32_t kHolsterActiveBg = UI_COL32(12, 42, 58, 238), kHolsterActiveBd = UI_COL32(72, 232, 255, 248);
        inline constexpr uint32_t kHolsterIdleBd = UI_COL32(76, 98, 118, 175), kHolsterIconHi = UI_COL32(115, 242, 255, 255);
        inline constexpr uint32_t kHolsterIconIdle = UI_COL32(174, 196, 214, 220), kHolsterTextHi = UI_COL32(118, 248, 255, 255);
        inline constexpr uint32_t kHolsterTextIdle = UI_COL32(186, 206, 224, 235);

        // 足元ラジアル進捗リング ＆ 頭上リロード／弾切れコールアウト
        inline constexpr float kFeetOffsetY = 0.10f, kFeetRingRadius = 24.0f;
        inline constexpr int kFeetBgSegs = 32;
        inline constexpr float kFeetBgThick = 4.4f, kFeetMinProg = 0.01f;
        inline constexpr int kFeetArcSegs = 28;
        inline constexpr float kFeetArcThick = 3.2f;
        inline constexpr uint32_t kFeetBgCol = UI_COL32(8, 14, 20, 185), kFeetHealCol = UI_COL32(58, 250, 152, 235);
        inline constexpr uint32_t kFeetRelEjectCol = UI_COL32(255, 192, 62, 240), kFeetRelInsCol = UI_COL32(82, 232, 255, 240);
        inline constexpr uint32_t kFeetRelBoltCol = UI_COL32(72, 252, 158, 248);

        inline constexpr float kHeadPromptLiftY = 1.56f, kHeadReticleFadeStart = 54.0f, kHeadReticleFadeSpan = 42.0f;
        inline constexpr float kHeadBobAmpPx = 2.2f, kHeadBobSpeed = 5.4f, kHeadAlertShakePx = 4.2f;
        inline constexpr float kHeadPillFont = 13.5f, kHeadKeyFont = 12.5f, kHeadPillH = 22.0f, kHeadProgBarH = 3.2f, kHeadProgGapY = 2.5f;
    }

    // --- 脱出シーケンス・バナー・カウントダウン演出 HUD ---
    namespace ExtractionBanner
    {
        inline constexpr float kAtlasSize = 1024.0f;
        inline constexpr AtlasSpriteRect kClearBannerRect      = { 0.0f, 704.0f, 1024.0f, 808.0f };
        inline constexpr AtlasSpriteRect kUnlockedBannerRect   = { 0.0f, 448.0f, 1024.0f, 544.0f };
        inline constexpr AtlasSpriteRect kExtractingHeaderRect = { 0.0f, 616.0f, 1024.0f, 704.0f };

        inline constexpr float kClearBannerSrcH = 104.0f, kUnlockedBannerSrcH = 96.0f, kExtractingHdrSrcH = 88.0f;

        // A. 生還クライマックス演出
        inline constexpr float kFlashMaxAlpha = 0.22f;
        inline constexpr int kFlashR = 215, kFlashG = 255, kFlashB = 228;
        inline constexpr float kLetterboxRatio = 0.088f, kMinLetterboxPx = 1.0f;
        inline constexpr uint32_t kLetterboxFill = UI_COL32(5, 7, 10, 248);
        inline constexpr int kTopBarLineR = 255, kTopBarLineG = 198, kTopBarLineB = 52;
        inline constexpr float kTopBarLineAlpha = 135.0f, kTopBarLineThick = 1.5f;
        inline constexpr int kBotBarLineR = 56, kBotBarLineG = 238, kBotBarLineB = 138;
        inline constexpr float kBotBarLineAlpha = 120.0f, kBotBarLineThick = 1.2f;

        inline constexpr float kMinAnimVis = 0.01f, kClearBannerH = 74.0f, kClearBannerTopGap = 16.0f, kClearBannerSlideY = 18.0f;

        inline constexpr float kSubFont = 18.0f, kClearSubPadW = 36.0f, kSubPadH = 14.0f, kClearSubBotGap = 14.0f;
        inline constexpr float kSubSlant = 5.0f, kSubBorderThick = 1.2f, kClearSubTextAlpha = 248.0f;
        inline constexpr float kSubBgAlpha = 232.0f, kSubBorderAlpha = 210.0f;
        inline constexpr int kClearSubBgR = 8, kClearSubBgG = 14, kClearSubBgB = 20;
        inline constexpr int kClearSubBdR = 72, kClearSubBdG = 246, kClearSubBdB = 152;
        inline constexpr int kClearSubTxR = 236, kClearSubTxG = 255, kClearSubTxB = 244;

        // B. 3Dヘリパッド境界リング ＆ ウェイポイント
        inline constexpr int kRingSegs = 32;
        inline constexpr float kRingMaxDrawDist = 42.0f, kRingLiftY = 0.06f, kRingLockedThick = 2.0f;
        inline constexpr float kRingFilledThick = 4.0f, kRingReadyThick = 2.5f, kRingPulseBaseA = 140.0f, kRingPulseAmpA = 85.0f;
        inline constexpr uint32_t kRingLockedCol = UI_COL32(255, 82, 68, 130), kRingFilledCol = UI_COL32(255, 214, 64, 245);
        inline constexpr int kRingReadyR = 48, kRingReadyG = 255, kRingReadyB = 142;

        inline constexpr float kLockedMarkerDist = 18.0f, kMarkerBaseY = 2.15f, kMarkerBobAmp = 0.12f;
        inline constexpr float kMarkerClampX = 120.0f, kMarkerClampTop = 75.0f, kMarkerClampBot = 110.0f;
        inline constexpr float kWpFont = 14.0f, kWpPadW = 22.0f, kWpPadH = 8.0f, kWpShadowX = 1.5f, kWpShadowY = 2.0f;
        inline constexpr float kWpSlant = 4.5f, kWpBorderThick = 1.2f, kWpStripeW = 3.6f, kWpTextShiftX = 2.0f;
        inline constexpr uint32_t kWpOpenBg = UI_COL32(8, 28, 20, 236), kWpLockBg = UI_COL32(18, 12, 16, 234);
        inline constexpr uint32_t kWpOpenBd = UI_COL32(62, 252, 156, 248), kWpLockBd = UI_COL32(255, 126, 104, 235);
        inline constexpr uint32_t kWpOpenTx = UI_COL32(165, 255, 208, 255), kWpLockTx = UI_COL32(255, 174, 158, 250);

        // C. EXTRACTION UNLOCKED バナー
        inline constexpr float kUnlockBannerH = 66.0f, kUnlockBannerY = 66.0f, kUnlockSlideY = 16.0f;
        inline constexpr float kUnlockSpriteAlpha = 250.0f, kUnlockSubPadW = 34.0f, kUnlockSubBotMargin = 42.0f;
        inline constexpr float kUnlockSubTextAlpha = 252.0f;
        inline constexpr int kUnlockSubBgR = 8, kUnlockSubBgG = 16, kUnlockSubBgB = 20;
        inline constexpr int kUnlockSubBdR = 62, kUnlockSubBdG = 248, kUnlockSubBdB = 152;
        inline constexpr int kUnlockSubTxR = 228, kUnlockSubTxG = 255, kUnlockSubTxB = 238;

        // D. EXTRACTING カウントダウンバー
        inline constexpr float kCountdownBaseY = 68.0f, kCountdownHdrH = 58.0f;
        inline constexpr int kCountdownHdrAlpha = 252, kCountdownSegs = 16;
        inline constexpr float kTotalBarW = 356.0f, kSegGap = 4.0f, kSegH = 13.0f, kSegSlant = 5.0f;
        inline constexpr float kBarTopGap = 4.0f, kBackdropPadX = 10.0f, kBackdropPadY = 4.0f, kBackdropRound = 2.0f;
        inline constexpr float kNearFullRatio = 0.75f, kPartBaseAlpha = 90.0f, kPartSpanAlpha = 160.0f;
        inline constexpr float kTimerOffsetY = 19.0f, kTimerDigitH = 25.0f;
        inline constexpr uint32_t kBackdropCol = UI_COL32(6, 12, 16, 195), kSegNearFullCol = UI_COL32(255, 214, 62, 250);
        inline constexpr uint32_t kSegActiveCol = UI_COL32(52, 250, 142, 248);
        inline constexpr int kSegPartR = 110, kSegPartG = 255, kSegPartB = 178;
        inline constexpr uint32_t kSegEmptyCol = UI_COL32(24, 38, 44, 175), kDigitNearFull = UI_COL32(255, 224, 88, 255);
        inline constexpr uint32_t kDigitNormal = UI_COL32(225, 255, 238, 255);
    }

    // --- 画面右下：兵装インベントリ ＆ 湾曲30連PMAG断面 HUD ---
    namespace AmmoPanel
    {
        inline constexpr float kWidth = 272.0f, kHeight = 94.0f, kSlant = 7.5f, kMargin = 14.0f, kStripeWidth = 4.0f;
        inline constexpr int kLowAmmoThreshold = 6;

        inline constexpr SlantedPanelStyle kPanelStyle = { 2.0f, 2.5f, HudPalette::kPanelShadow125,
        	HudPalette::kGunmetalBg234, HudPalette::kGunmetalBorder175, 1.1f };

        inline constexpr uint32_t kReloadAccent = UI_COL32(58, 246, 152, 250), kLowAmmoAccent = UI_COL32(255, 156, 38, 248);

        // 左コンパートメント：PMAGシースルー断面図
        inline constexpr float kMagWellX = 12.0f, kMagWellY = 8.0f, kMagW = 34.0f, kMagH = 74.0f, kClipPad = 4.0f;
        inline constexpr float kClipMaxX = 62.0f, kSlideDropPx = 38.0f, kEjectFadeMul = 0.85f;
        inline constexpr float kInsertBaseAlpha = 0.35f, kInsertSpanAlpha = 0.65f, kMinVisibleMagAlpha = 0.04f, kShellMaxAlpha = 236.0f;

        inline constexpr float kCollarExtendX = 2.5f, kCollarH = 9.0f, kCollarRounding = 2.0f, kCollarBorderThick = 1.0f;
        inline constexpr uint32_t kCollarFill = UI_COL32(42, 54, 66, 235), kCollarBorder = UI_COL32(96, 120, 140, 195);

        inline constexpr float kBodyTopOffset = 5.0f, kBodyHeightTrim = 9.0f, kCurveBendPx = 7.5f;
        inline constexpr int kShellSegs = 6;
        inline constexpr float kShellInnerPad = 3.2f, kShellEdgeThick = 1.2f;
        inline constexpr int kShellOuterR = 94, kShellOuterG = 80, kShellOuterB = 62;
        inline constexpr int kShellInnerR = 15, kShellInnerG = 19, kShellInnerB = 24;
        inline constexpr int kShellEdgeR = 150, kShellEdgeG = 132, kShellEdgeB = 102;

        inline constexpr float kFloorExtendX = 2.0f, kFloorTopTrim = 4.0f, kFloorExtraW = 4.0f, kFloorH = 6.5f;
        inline constexpr float kFloorRounding = 2.0f, kFloorBorderThick = 1.0f;
        inline constexpr int kFloorFillR = 30, kFloorFillG = 34, kFloorFillB = 40, kFloorBdR = 118, kFloorBdG = 104, kFloorBdB = 82;

        inline constexpr int kMaxVisibleRows = 15;
        inline constexpr float kRowTopPad = 3.0f, kRowSpanTrim = 9.0f, kRowInsetX = 4.5f, kRowWidthTrim = 9.0f;
        inline constexpr float kMinRowH = 2.0f, kRowGapH = 1.1f, kCaseWidthRatio = 0.66f, kCaseRounding = 1.0f;
        inline constexpr int kBrassWarnR = 255, kBrassWarnG = 168, kBrassWarnB = 52;
        inline constexpr int kBrassNormR = 236, kBrassNormG = 198, kBrassNormB = 86;
        inline constexpr int kCopperWarnR = 255, kCopperWarnG = 108, kCopperWarnB = 48;
        inline constexpr int kCopperNormR = 224, kCopperNormG = 128, kCopperNormB = 78;

        inline constexpr float kFollowerInsetX = 4.2f, kFollowerWidthTrim = 8.4f, kFollowerH = 2.8f, kFollowerRounding = 1.0f;
        inline constexpr int kFollowerR = 226, kFollowerG = 104, kFollowerB = 42;

        inline constexpr float kSpringTopOff = 3.2f, kSpringBotTrim = 4.5f;
        inline constexpr int kSpringZigs = 5;
        inline constexpr float kSpringMinSpan = 2.0f, kSpringInsetX = 7.0f, kSpringAlpha = 190.0f, kSpringThick = 1.2f;
        inline constexpr int kSpringR = 112, kSpringG = 124, kSpringB = 136;

        // 右コンパートメント：M4A1シルエット・残弾・リロードバー
        inline constexpr float kRightOffsetX = 62.0f, kRightWidthTrim = 74.0f, kDivLeftShift = 5.0f, kDivPadY = 8.0f, kDivThick = 1.0f;
        inline constexpr uint32_t kDivCol = UI_COL32(72, 92, 110, 105);

        inline constexpr float kTopOffsetY = 7.0f, kTopCenterOffY = 9.5f, kStatusFont = 14.0f, kStatusTextPadX = 2.0f;
        inline constexpr uint32_t kReloadTextCol = UI_COL32(92, 255, 174, 255), kCancelTextCol = UI_COL32(255, 118, 104, 255);
        inline constexpr uint32_t kEmptyTextCol = UI_COL32(255, 112, 98, 255), kLowAmmoTextCol = UI_COL32(255, 206, 78, 255);

        inline constexpr float kGunOriginPadX = 2.0f, kGunOriginPadY = 10.0f;
        inline constexpr uint32_t kGunBodyCol = UI_COL32(190, 214, 232, 235), kGunOpticCol = UI_COL32(110, 226, 255, 230);
        inline constexpr HudRectPart kRifleSilhouetteParts[] = {
            {  0.0f, -2.0f, 16.0f,  5.5f, 1.0f, false }, // ストック
            { 16.0f, -3.5f, 46.0f,  3.5f, 1.0f, false }, // レシーバー
            { 24.0f, -7.5f, 35.0f, -3.5f, 1.0f, true  }, // ホロサイト
            { 20.0f,  3.5f, 25.5f, 10.0f, 1.0f, false }, // グリップ
            { 46.0f, -2.5f, 72.0f,  2.2f, 1.0f, false }, // ハンドガード
            { 90.0f, -1.8f, 96.0f,  1.2f, 0.0f, false }, // フラッシュハイダー
        };
        inline constexpr float kBarrelStartX = 72.0f, kBarrelEndX = 92.0f, kBarrelOffY = -0.3f, kBarrelThick = 2.2f;
        inline constexpr float kCaliberFont = 13.0f;
        inline constexpr uint32_t kCaliberCol = UI_COL32(182, 210, 232, 245);

        inline constexpr float kMidCenterY = 46.5f, kMagAmmoFont = 25.0f, kCapAmmoFont = 16.0f, kCapGapX = 6.0f;
        inline constexpr float kCapOffY = 2.5f, kReserveDigitH = 20.0f, kReserveOffY = 1.0f, kAmmoIconSize = 19.0f;
        inline constexpr float kAmmoIconGapX = 5.0f, kAmmoIconOffY = 1.0f, kAmmoIconRound = 2.5f;

        inline constexpr uint32_t kMagZeroCol = UI_COL32(255, 92, 84, 255), kMagLowCol = UI_COL32(255, 204, 72, 255);
        inline constexpr uint32_t kCapTextCol = UI_COL32(174, 196, 214, 240), kResHasCol = UI_COL32(226, 238, 248, 250);
        inline constexpr uint32_t kResEmptyCol = UI_COL32(255, 102, 92, 245), kAmmoIconTint = UI_COL32(255, 255, 255, 240);

        inline constexpr float kBotBarY = 69.0f, kReloadBarH = 16.0f, kPipBarH = 12.0f, kReloadBarSlant = 3.0f;
        inline constexpr float kReloadBarBdThick = 1.0f, kStageTickSlant = 2.0f, kStageTickThick = 1.6f, kStageFont = 12.0f;
        inline constexpr uint32_t kReloadBarBg = UI_COL32(16, 26, 34, 242), kReloadBarBd = UI_COL32(82, 114, 132, 165);
        inline constexpr uint32_t kStageEjectCol = UI_COL32(255, 192, 62, 245), kStageInsertCol = UI_COL32(72, 228, 255, 245);
        inline constexpr uint32_t kStageBoltCol = UI_COL32(58, 250, 152, 250), kStageTickCol = UI_COL32(10, 15, 21, 240);
        inline constexpr uint32_t kStageTextCol = UI_COL32(250, 254, 255, 255);

        inline constexpr int kMaxAmmoPips = 40;
        inline constexpr float kPipGap = 1.6f, kMinPipW = 1.8f, kPipPadY = 1.5f, kPipTrimH = 3.0f, kPipSlant = 1.8f;
        inline constexpr uint32_t kPipEmptyCol = UI_COL32(24, 34, 44, 195), kPipLowCol = UI_COL32(255, 168, 42, 248);
        inline constexpr uint32_t kPipFullCol = UI_COL32(228, 204, 128, 242);
    }

    // --- 3D敵タクティカル頭上ゲージ・視界アーク・射撃予兆レーザー HUD ---
    namespace EnemyHud
    {
        inline constexpr float kMaxDrawDist = 34.0f, kVisionArcMaxDist = 16.5f, kVisionMaxConeLen = 7.5f;
        inline constexpr float kVisionRangeRatio = 0.55f, kVisionFadeSpan = 5.5f;
        inline constexpr int kVisionArcSegs = 12;
        inline constexpr float kVisionDetectThresh = 0.08f, kVisionLiftY = 0.06f, kVisionArcThick = 1.8f;
        inline constexpr float kVisionSideThick = 1.2f, kAlertArcAlpha = 135.0f, kCalmArcAlpha = 78.0f;
        inline constexpr float kAlertSideAlpha = 85.0f, kCalmSideAlpha = 45.0f;
        inline constexpr int kAlertArcR = 255, kAlertArcG = 192, kAlertArcB = 58, kCalmArcR = 140, kCalmArcG = 215, kCalmArcB = 245;

        inline constexpr float kMinTelegraphProg = 0.01f, kTelegraphTargetY = 0.56f, kTelegraphFlashBase = 0.55f;
        inline constexpr float kTelegraphFlashAmp = 0.45f, kBeamBaseAlpha = 85.0f, kBeamProgAlpha = 165.0f;
        inline constexpr float kBeamBaseThick = 1.2f, kBeamProgThick = 1.8f, kMuzzlePipBaseRad = 3.2f, kMuzzlePipProgRad = 3.0f;
        inline constexpr int kBeamR = 255, kBeamG = 42, kBeamB = 46, kPipCoreR = 255, kPipCoreG = 220, kPipCoreB = 190;
        inline constexpr int kPipOutR = 255, kPipOutG = 52, kPipOutB = 48;

        inline constexpr float kHeadHeightBase = 1.52f, kScreenCullMargin = 80.0f, kPatrolDetectThresh = 0.04f;
        inline constexpr float kWardenAlwaysDist = 9.5f, kSuspectPipRad = 7.5f, kSuspectOffsetY = 8.0f;
        inline constexpr uint32_t kSuspectBg = UI_COL32(16, 22, 28, 220), kSuspectOutline = UI_COL32(255, 196, 56, 235);
        inline constexpr uint32_t kSuspectFill = UI_COL32(255, 86, 56, 245);

        inline constexpr float kWardenBarW = 96.0f, kNormalBarW = 56.0f, kWardenBarH = 8.5f, kNormalBarH = 6.5f;
        inline constexpr float kBarSlant = 3.0f, kWardenBorderThick = 1.5f, kNormalBorderThick = 1.0f, kMinHpFillRatio = 0.005f;
        inline constexpr uint32_t kBarBg = UI_COL32(12, 16, 22, 232), kWardenFrameCol = UI_COL32(255, 206, 68, 240);
        inline constexpr uint32_t kNormalFrameCol = UI_COL32(68, 84, 98, 175), kBarLagCol = UI_COL32(255, 242, 230, 235);
        inline constexpr uint32_t kBarInvestigate = UI_COL32(255, 186, 48, 245), kBarWardenFill = UI_COL32(228, 38, 52, 252);
        inline constexpr uint32_t kBarNormalFill = UI_COL32(242, 62, 62, 248);

        inline constexpr float kBossFont = 13.0f, kBossPadW = 24.0f, kBossPadH = 5.0f, kBossGapY = 3.5f;
        inline constexpr float kBossSlant = 3.0f, kBossBorderThick = 1.1f, kBossPipX = 9.0f, kBossPipRad = 3.5f, kBossTextX = 16.5f;
        inline constexpr uint32_t kBossBg = UI_COL32(26, 14, 16, 240), kBossBorder = UI_COL32(255, 208, 72, 240);
        inline constexpr uint32_t kBossPipCol = UI_COL32(255, 214, 72, 255), kBossTextCol = UI_COL32(255, 230, 118, 255);

        inline constexpr float kPillFont = 13.0f, kPillPadW = 18.0f, kPillPadH = 5.0f, kPillGapY = 4.0f;
        inline constexpr float kPillShadowX = 1.0f, kPillShadowY = 1.5f, kPillSlant = 3.2f, kPillBorderThick = 1.1f;
        inline constexpr float kPillTextShiftX = 1.0f, kReloadSubGapY = 2.0f, kReloadSubH = 2.5f, kReloadSubSlant = 1.5f;

        inline constexpr uint32_t kPillReloadBg = UI_COL32(10, 26, 36, 238), kPillReloadBd = UI_COL32(82, 232, 255, 245);
        inline constexpr uint32_t kPillReloadTx = UI_COL32(158, 246, 255, 255), kReloadSubCol = UI_COL32(82, 226, 255, 235);
        inline constexpr uint32_t kPillSearchBg = UI_COL32(30, 22, 10, 238), kPillSearchBd = UI_COL32(255, 198, 62, 245);
        inline constexpr uint32_t kPillSearchTx = UI_COL32(255, 228, 120, 255), kPillContactBg = UI_COL32(38, 12, 14, 240);
        inline constexpr uint32_t kPillContactBd = UI_COL32(255, 88, 78, 250), kPillContactTx = UI_COL32(255, 158, 146, 255);
    }

    // --- 3D物資探索カード HUD ---
    namespace LootCard
    {
        inline constexpr float kMaxDrawDist = 7.5f, kBadgeLiftY = 0.92f, kNameFont = 16.0f, kHintFont = 14.0f;
        inline constexpr float kHintKeyReserveW = 26.0f, kHintRightPadW = 16.0f, kNameRightPadW = 18.0f;
        inline constexpr float kMinCardW = 194.0f, kLeftIconColW = 56.0f, kCardH = 54.0f, kCardSlant = 7.0f;
        inline constexpr float kScreenClampPadX = 14.0f, kShadowX = 2.0f, kShadowY = 3.0f, kBorderThick = 1.4f;
        inline constexpr float kIconSize = 38.0f, kIconPadX = 10.0f, kIconRounding = 3.0f, kIconBorderThick = 1.0f;

        inline constexpr float kSearchingFont = 14.0f, kSearchingCenterY = 16.5f, kPctRightPadX = 10.0f;
        inline constexpr float kPctDigitH = 17.5f, kProgWidthTrim = 68.0f, kProgTopY = 33.5f, kProgH = 8.5f, kProgSlant = 3.0f;

        inline constexpr float kIdleNameCenterY = 16.0f, kKeyW = 20.0f, kKeyH = 17.5f, kKeyTopY = 28.5f;
        inline constexpr float kKeyRounding = 3.0f, kKeyBorderThick = 1.2f, kKeyFont = 13.0f, kHintGapX = 6.0f;

        inline constexpr float kFarPipOuterRad = 6.2f, kFarPipInnerRad = 2.6f, kFarDistFont = 13.0f, kFarDistOffsetY = 14.5f;

        inline constexpr uint32_t kCardBg = UI_COL32(9, 15, 22, 240), kRareBorder = UI_COL32(255, 208, 62, 248);
        inline constexpr uint32_t kNormalBorder = UI_COL32(52, 246, 146, 240);
        inline constexpr uint32_t kSearchingCol = UI_COL32(150, 255, 202, 255), kPctCol = UI_COL32(255, 222, 82, 255);
        inline constexpr uint32_t kProgBg = UI_COL32(20, 32, 28, 242), kRareNameCol = UI_COL32(255, 226, 98, 255);
        inline constexpr uint32_t kNormalNameCol = UI_COL32(244, 250, 255, 255), kKeyBg = UI_COL32(18, 38, 30, 245);
        inline constexpr uint32_t kRareHintCol = UI_COL32(255, 228, 114, 255), kNormalHintCol = UI_COL32(114, 255, 184, 255);
        inline constexpr uint32_t kFarRarePip = UI_COL32(255, 212, 62, 235);
        inline constexpr uint32_t kFarNormalPip = UI_COL32(82, 228, 172, 215), kFarPipBg = UI_COL32(12, 18, 24, 225);
        inline constexpr uint32_t kFarDistCol = UI_COL32(220, 236, 248, 225);
    }

    // --- 被弾方向インジケーター ＆ 低体力赤ビネット演出 ---
    namespace DamageFeedback
    {
        inline constexpr float kPlayerCenterLiftY = 0.45f, kFarProbeDist = 1.5f, kNearProbeDist = 0.25f;
        inline constexpr float kMinDirLenSq = 1e-4f, kMinVisibleAlpha = 0.005f, kArcBaseRadius = 52.0f;
        inline constexpr float kArcExpandRadius = 8.0f, kArcHalfSpanRad = 0.42f;
        inline constexpr int kArcSegments = 20;

        inline constexpr ArcStrokeLayerSpec kArcLayers[] = {
            { 255, 32, 32,  72.0f, 8.0f }, // 外周グロー
            {  16,  4,  4, 165.0f, 5.4f }, // 背面シャドウ
            { 255, 48, 48, 240.0f, 3.4f }, // 高輝度コア
        };
        inline constexpr size_t kCoreLayerIndex = 2;

        inline constexpr float kPointerBaseOffR = 3.2f, kPointerTipOffR = 10.5f, kPointerHalfW = 5.2f;

        // 低体力ビネット
        inline constexpr float kLowHpThreshold = 0.40f, kPulseBase = 0.75f, kPulseAmp = 0.25f, kMaxEdgeAlpha = 180.0f;
        inline constexpr float kEdgeThickRatio = 0.18f;
        inline constexpr int kVignetteR = 180, kVignetteG = 10, kVignetteB = 10;
    }

    // --- 3Dレーザーサイト ＆ タクティカルレティクル HUD ---
    namespace LaserSight
    {
        inline constexpr float kMinAimDist = 0.18f, kMaxAimDist = 45.0f, kAxisEpsilon = 1e-5f, kWallHitBias = 0.04f;
        inline constexpr float kTargetOvershoot = 0.85f, kMinSphereHitDist = 0.05f, kEnemyHitRadMul = 0.96f;
        inline constexpr float kTargetHitRadMul = 0.98f, kReticleSnapBias = 0.03f;

        inline constexpr float kGlowThickLocked = 4.4f, kGlowThickNormal = 3.8f, kCoreThickLocked = 1.75f;
        inline constexpr float kCoreThickNormal = 1.45f, kMuzzleDotRad = 2.0f;
        inline constexpr int kMuzzleDotSegs = 10;
        inline constexpr float kWallGuideThick = 1.0f, kWallDotRad = 3.4f, kHitDotRad = 3.8f;
        inline constexpr int kImpactDotSegs = 12;

        inline constexpr uint32_t kGlowLocked = UI_COL32(255, 198, 58, 52), kGlowNormal = UI_COL32(62, 255, 162, 38);
        inline constexpr uint32_t kCoreLocked = UI_COL32(255, 224, 92, 190), kCoreNormal = UI_COL32(118, 255, 188, 152);
        inline constexpr uint32_t kDotLocked = UI_COL32(255, 238, 130, 248), kDotNormal = UI_COL32(165, 255, 212, 238);
        inline constexpr uint32_t kWallGuideCol = UI_COL32(255, 110, 92, 42), kWallDotCol = UI_COL32(255, 142, 96, 235);
    }

    namespace Reticle
    {
        // 全体マップ時のマウスカーソル
        inline constexpr float kMapCursorBaseR = 6.5f, kMapCursorSpanMul = 1.6f, kMapShadowThick = 2.8f;
        inline constexpr float kMapFgThick = 1.3f, kMapPipRad = 3.2f;
        inline constexpr uint32_t kMapShadowCol = UI_COL32(6, 10, 14, 210), kMapFgCol = UI_COL32(120, 238, 255, 240);

        inline constexpr uint32_t kShadowCol = UI_COL32(8, 12, 16, 215);

        // [C] 銃収納ダッシュブラケット
        inline constexpr float kHolsterGap = 6.5f, kHolsterArm = 3.6f, kHolsterShadowThick = 2.8f;
        inline constexpr float kHolsterFgThick = 1.4f, kHolsterDotShadowR = 2.4f, kHolsterDotFgR = 1.5f;
        inline constexpr int kSmallDotSegs = 10;
        inline constexpr uint32_t kHolsterCol = UI_COL32(210, 228, 242, 195);

        // リロード円形プログレス
        inline constexpr float kReloadRadius = 14.5f, kReloadBusyExpandPx = 2.2f;
        inline constexpr int kReloadSegs = 32;
        inline constexpr float kReloadBgThick = 4.4f, kReloadTrackThick = 2.2f, kReloadMinProg = 0.01f;
        inline constexpr float kReloadArcThick = 3.0f, kReloadDotShadowR = 2.6f, kReloadDotFgR = 1.7f;
        inline constexpr uint32_t kReloadBgCol = UI_COL32(8, 14, 20, 205), kReloadTrackCol = UI_COL32(190, 210, 225, 68);
        inline constexpr uint32_t kReloadArcCol = UI_COL32(255, 202, 74, 242), kReloadDotCol = UI_COL32(255, 214, 102, 235);
        inline constexpr uint32_t kReloadStageEjectCol = UI_COL32(255, 196, 62, 248);
        inline constexpr uint32_t kReloadStageInsCol   = UI_COL32(78, 234, 255, 248);
        inline constexpr uint32_t kReloadStageBoltCol  = UI_COL32(62, 252, 156, 252);

        // レティクル直下：弾切れ [R] リロード ／ リロード進捗 ／ 装填完了プロンプト
        inline constexpr int kReticleLowAmmoThreshold = 5;
        inline constexpr float kPromptOffsetY = 25.0f, kPromptClampPadX = 18.0f, kPromptClampTopY = 54.0f, kPromptClampBotY = 44.0f;
        inline constexpr float kPromptEmptyH = 24.0f, kPromptEmptyFont = 14.5f, kPromptKeyFont = 13.0f;
        inline constexpr float kPromptSubH = 20.0f, kPromptSubFont = 12.5f, kPromptSubKeyFont = 11.5f;
        inline constexpr float kPromptSlant = 3.8f, kPromptPadX = 8.0f, kPromptKeyPadW = 8.5f, kPromptKeyHTrim = 5.0f, kPromptKeyGapX = 6.0f;
        inline constexpr float kPromptReloadBarH = 3.2f, kPromptReloadBarGapY = 2.2f;
        inline constexpr float kEmptyBracketBaseOff = 11.5f, kEmptyBracketAlertAdd = 3.2f, kEmptyBracketArm = 3.4f;
        inline constexpr float kAlertShakeAmpPx = 5.2f, kAlertShakeFreq = 36.0f, kEmptyPulseSpeed = 9.0f;
        inline constexpr float kCompleteFlashDuration = 0.55f, kCompleteBracketOff = 12.0f, kCompleteBracketArm = 3.6f;

        inline constexpr uint32_t kEmptyPillBg     = UI_COL32(28, 10, 14, 242);
        inline constexpr uint32_t kEmptyPillBdLo   = UI_COL32(255, 72, 64, 235);
        inline constexpr uint32_t kEmptyPillBdHi   = UI_COL32(255, 196, 62, 255);
        inline constexpr uint32_t kEmptyKeyBg      = UI_COL32(58, 22, 18, 250);
        inline constexpr uint32_t kEmptyKeyBd      = UI_COL32(255, 206, 68, 255);
        inline constexpr uint32_t kEmptyKeyTx      = UI_COL32(255, 244, 196, 255);
        inline constexpr uint32_t kEmptyTextCol    = UI_COL32(255, 236, 230, 255);
        inline constexpr uint32_t kLowPillBg       = UI_COL32(24, 18, 10, 222);
        inline constexpr uint32_t kLowPillBd       = UI_COL32(255, 186, 56, 225);
        inline constexpr uint32_t kLowKeyBg        = UI_COL32(46, 34, 14, 240);
        inline constexpr uint32_t kLowTextCol      = UI_COL32(255, 226, 138, 248);
        inline constexpr uint32_t kReloadPillBg    = UI_COL32(9, 17, 25, 236);
        inline constexpr uint32_t kCompletePillBg  = UI_COL32(10, 32, 22, 232);
        inline constexpr uint32_t kCompleteCol     = UI_COL32(72, 252, 162, 250);

        // 通常射撃クロスヘア
        inline constexpr float kLockedKickThresh = 0.35f, kNormalKickThresh = 0.30f, kBaseGap = 5.2f;
        inline constexpr float kKickGapMul = 6.8f, kBaseTickLen = 6.4f, kKickTickMul = 2.2f, kTickShadowThick = 3.4f;
        inline constexpr float kTickFgThick = 1.75f;

        inline constexpr UIVec2 kCrossDirs[] = {
            UIVec2( 0.0f, -1.0f),
            UIVec2( 0.0f,  1.0f),
            UIVec2(-1.0f,  0.0f),
            UIVec2( 1.0f,  0.0f),
        };

        // ロックオン45度コーナー
        inline constexpr float kLockDiagPad = 1.8f, kLockDiagMul = 0.72f, kLockArmLen = 2.6f, kLockShadowThick = 2.4f;
        inline constexpr float kLockFgThick = 1.25f;

        inline constexpr float kCenterDotShadowR = 2.8f, kCenterDotFgR = 1.85f;
        inline constexpr int kCenterDotSegs = 12;

        inline constexpr uint32_t kMainDefaultCol = UI_COL32(126, 255, 192, 242), kDotDefaultCol = UI_COL32(185, 255, 220, 252);
        inline constexpr uint32_t kMainEmptyCol = UI_COL32(255, 92, 86, 245), kDotEmptyCol = UI_COL32(255, 138, 128, 252);
        inline constexpr uint32_t kMainLockKickCol = UI_COL32(255, 172, 74, 252), kMainLockCol = UI_COL32(255, 222, 86, 250);
        inline constexpr uint32_t kDotLockCol = UI_COL32(255, 244, 156, 255), kMainKickCol = UI_COL32(176, 255, 212, 248);
        inline constexpr uint32_t kDotKickCol = UI_COL32(232, 255, 242, 255);
    }

    // --- フローティングテキスト・デバッグギズモ・チュートリアル看板 ---
    namespace FloatingCallout
    {
        inline constexpr float kPillFont = 15.0f, kPillPadW = 22.0f, kPillPadH = 7.0f, kPillSlant = 4.0f;
        inline constexpr float kPillBorderThick = 1.2f, kPillTextShiftX = 1.5f, kBgAlphaScale = 226.0f;

        inline constexpr CalloutBadgeSpec kBadges[] = {
            { "HOSTILE DOWN",     u8"撃破！",     32, 12, 14,  255, 198,  68,  255, 232, 118 },
            { "TARGET DESTROYED", u8"標的破壊！", 10, 32, 22,   72, 250, 162,  150, 255, 204 },
            { "GRAZE",            u8"GRAZE",      14, 22, 30,  156, 196, 224,  220, 238, 250 },
        };

        inline constexpr float kCritFont = 22.0f, kNormalFont = 17.0f, kTextShadowScale = 0.90f;
        inline constexpr size_t kMaxCritSubLen = 4;
        inline constexpr float kCritSubFont = 12.0f, kCritSubOffsetMul = 0.68f;
        inline constexpr int kCritSubR = 255, kCritSubG = 226, kCritSubB = 98;
    }

    namespace DebugHud
    {
        // サウンドギズモ
        inline constexpr int kSoundRingSegs = 32;
        inline constexpr float kSoundRingLiftY = 0.05f, kSoundAlphaScale = 200.0f, kSoundLineThick = 2.0f;
        inline constexpr int kSoundR = 0, kSoundG = 200, kSoundB = 255;

        // パフォーマンステレメトリ (F3)
        inline constexpr float kPanelX = 228.0f, kPanelY = 68.0f, kPanelW = 296.0f, kPanelH = 158.0f;
        inline constexpr float kPanelSlant = 8.0f, kPanelBorderThick = 1.3f, kHeaderLineLeftPad = 12.0f;
        inline constexpr float kHeaderLineRightPad = 8.0f, kHeaderLineY = 28.0f, kHeaderLineThick = 1.0f;
        inline constexpr float kTextPadX = 16.0f, kTitleOffsetY = 7.0f, kFpsOffsetY = 34.0f, kHpOffsetY = 56.0f;
        inline constexpr float kAmmoOffsetY = 78.0f, kLootOffsetY = 100.0f, kPropsOffsetY = 124.0f;

        inline constexpr uint32_t kPanelBg = UI_COL32(8, 13, 19, 224), kPanelBorder = UI_COL32(72, 232, 255, 165);
        inline constexpr uint32_t kHeaderLineCol = UI_COL32(72, 232, 255, 92);
        inline constexpr uint32_t kTitleCol = UI_COL32(72, 232, 255, 245), kFpsCol = UI_COL32(236, 244, 250, 245);
        inline constexpr uint32_t kStatCol = UI_COL32(198, 214, 228, 240), kLootCol = UI_COL32(238, 204, 126, 245);
        inline constexpr uint32_t kPropsCol = UI_COL32(156, 174, 192, 235);
    }

    namespace TutorialSign
    {
        inline constexpr float kSignLiftY = 2.0f, kScreenBoundX = 60.0f, kScreenBoundTop = 54.0f;
        inline constexpr float kScreenBoundBot = 60.0f, kFallbackTopY = 132.0f;

        inline constexpr float kFont = 15.0f, kPadX = 14.0f, kPadY = 8.5f, kInnerGapH = 10.0f, kAboveSignGapY = 6.0f;
        inline constexpr float kMinBoxY = 56.0f, kMaxBoxBotPad = 102.0f, kMinimapAvoidMaxY = 245.0f;
        inline constexpr float kMinimapAvoidMinX = 212.0f, kQuestAvoidMaxY = 146.0f, kQuestAvoidRightPad = 284.0f;
        inline constexpr float kDefaultSideMargin = 20.0f, kFallbackSideMargin = 16.0f;

        inline constexpr float kDuckZoneTopRatio = 0.37f, kDuckZoneBotRatio = 0.67f, kDuckZoneHalfW = 85.0f, kDuckRetreatGapY = 8.0f;

        inline constexpr SlantedPanelStyle kPanelStyle = { 2.0f, 3.0f, HudPalette::kPanelShadow145,
        	HudPalette::kGunmetalBg238, UI_COL32(92, 118, 140, 195), 1.2f };
        inline constexpr float kSlant = 5.5f, kStripeW = 4.0f, kDividerOffY = 4.0f, kDividerExtraX = 2.0f;
        inline constexpr float kDividerThick = 1.0f, kMsgOffY = 8.0f;

        inline constexpr uint32_t kStripeCol = UI_COL32(255, 198, 58, 248), kHeaderCol = UI_COL32(255, 212, 78, 255);
        inline constexpr uint32_t kDividerCol = UI_COL32(86, 112, 134, 135), kMessageCol = UI_COL32(242, 248, 255, 255);
    }

    // --- 3D地面ルート案内・標的マーカー・画面端方向ガイド HUD ---
    namespace Navigation
    {
        // A* 経路更新 ＆ Chaikinコーナー平滑化
        inline constexpr float kGoalMoveThreshSq = 0.25f, kMinNodeDistSq = 0.16f;
        inline constexpr size_t kMinSmoothNodes = 3;
        inline constexpr float kSmoothNeighborW = 0.25f, kSmoothCenterW = 0.50f;
        inline constexpr int kChaikinPasses = 2;
        inline constexpr size_t kMaxSmoothPoints = 96;
        inline constexpr float kMinChaikinSegSq = 0.09f, kChaikinNearW = 0.75f, kChaikinFarW = 0.25f;

        // 発光カラー派生係数
        inline constexpr int kHiBoostR = 95, kHiBoostG = 75, kHiBoostB = 65, kDarkMulR = 72, kDarkMulG = 76;
        inline constexpr int kDarkMulB = 84, kColorPercentDiv = 100;

        inline constexpr float kMaxDrawDist = 25.0f;

        // (A) ホログラフィック・ガイドレール
        inline constexpr float kMinRailPathLen = 1.3f, kRailStepDist = 0.65f, kRailStartDist = 0.90f;
        inline constexpr float kRailMinSegLen = 0.03f, kRailEndMargin = 0.35f, kRailLiftY = 0.055f;
        inline constexpr float kRailHalfW = 0.42f, kRailMaxGapMul = 1.6f, kRailStartFadeSpan = 1.10f;
        inline constexpr float kRailEndFadeSpan = 1.20f, kRailDistPad = 2.0f, kRailDistFadeMul = 0.72f;
        inline constexpr float kRailMinDistFade = 0.18f, kRailWaveScale = 0.95f, kRailCenterBaseA = 28.0f;
        inline constexpr float kRailCenterAmpA = 54.0f, kRailSideBaseA = 18.0f, kRailSideAmpA = 34.0f;
        inline constexpr int kRailMinCenterAlpha = 4, kRailMinSideAlpha = 3;
        inline constexpr float kRailCenterThick = 2.2f, kRailSideThick = 1.15f;

        // (B) スイープウイング・シェブロン
        inline constexpr float kChevronStepDist = 1.75f, kChevronStartDist = 0.85f, kChevronMinSegLen = 0.04f;
        inline constexpr float kChevronEndMargin = 0.55f, kChevronLiftY = 0.08f, kChevronWaveScale = 0.85f;
        inline constexpr float kChevStartFadeSpan = 1.15f, kChevGoalMargin = 0.45f, kChevGoalFadeSpan = 1.25f;
        inline constexpr float kChevDistFadeMul = 0.62f, kChevMinDistFade = 0.24f, kChevMinVis = 0.03f;
        inline constexpr float kChevBaseScale = 0.92f, kChevWaveScaleAdd = 0.12f;

        inline constexpr float kChevNoseFwd = 0.37f, kChevNotchFwd = 0.07f, kChevOuterBack = 0.21f;
        inline constexpr float kChevOuterSide = 0.38f, kChevTailBack = 0.33f, kChevTailSide = 0.27f;
        inline constexpr float kEchoTipBack = 0.12f, kEchoWingBack = 0.45f, kEchoWingSide = 0.26f;

        inline constexpr float kChevBodyBaseA = 115.0f, kChevBodyAmpA = 125.0f, kChevEdgeBaseA = 145.0f;
        inline constexpr float kChevEdgeAmpA = 110.0f, kChevGlowBaseA = 36.0f, kChevGlowAmpA = 68.0f;
        inline constexpr float kChevShadowBaseA = 85.0f, kChevShadowAmpA = 65.0f, kChevShadowOffX = 1.6f, kChevShadowOffY = 2.2f;
        inline constexpr int kChevShadowR = 4, kChevShadowG = 10, kChevShadowB = 18;
        inline constexpr float kEchoAlphaMul = 0.42f, kEchoThick = 1.5f, kLeadGlowThick = 5.2f;
        inline constexpr float kLeadEdgeThick = 1.65f, kSpineAlphaMul = 0.75f, kSpineThick = 1.3f;

        // (C) ゴール足元ビーコンリング
        inline constexpr float kBeaconMinPathLen = 1.6f, kBeaconMaxPathLen = 34.0f;
        inline constexpr int kBeaconRingSegs = 16;
        inline constexpr float kBeaconBaseRad = 0.62f, kBeaconPulseRad = 0.08f, kBeaconLiftY = 0.07f;
        inline constexpr int kBeaconAlpha = 130;
        inline constexpr float kBeaconThick = 1.8f;

        // 3D標的マーカー
        inline constexpr float kSecondaryTargetMaxDist = 38.0f, kTargetBobAmp = 0.12f, kTargetMarkerLiftY = 1.95f;
        inline constexpr float kTargetScreenPad = 24.0f, kTargetAnchorLiftY = 1.05f, kTetherStartOffY = 8.0f;
        inline constexpr float kTetherThick = 1.3f, kPrimaryPipRad = 7.8f, kSecondaryPipRad = 5.8f;
        inline constexpr float kInnerPipMul = 0.45f, kPrimaryFont = 14.0f, kSecondaryFont = 13.0f;
        inline constexpr float kTargetTagPadW = 16.0f, kTargetTagPadH = 6.0f, kTargetClampX = 16.0f;
        inline constexpr float kTargetTagGapY = 4.0f, kTargetShadowX = 1.5f, kTargetShadowY = 2.0f;
        inline constexpr float kTargetSlant = 3.5f, kTargetBorderThick = 1.1f, kTargetStripeW = 3.4f;
        inline constexpr float kPrimaryTextShiftX = 1.5f, kSecondaryTextShiftX = 0.5f;

        inline constexpr uint32_t kTargetTetherCol = UI_COL32(255, 206, 62, 120), kTargetPrimaryCol = UI_COL32(255, 206, 62, 250);
        inline constexpr uint32_t kTargetSecCol = UI_COL32(255, 112, 82, 215), kTargetPipBg = UI_COL32(14, 18, 24, 230);
        inline constexpr uint32_t kTargetPriText = UI_COL32(255, 240, 152, 255), kTargetSecText = UI_COL32(242, 216, 208, 245);

        // 初期弾薬箱 STEP 1 マーカー
        inline constexpr float kLootBobAmp = 0.12f, kLootMarkerLiftY = 1.55f, kLootScreenPad = 30.0f;
        inline constexpr float kLootAnchorLiftY = 0.65f, kLootTetherThick = 1.35f, kLootAnchorDotRad = 2.6f;
        inline constexpr int kLootAnchorDotSegs = 12;
        inline constexpr float kLootPulseExpandAmp = 0.22f, kLootPulseRad = 8.8f, kLootOuterRad = 7.2f;
        inline constexpr float kLootInnerRad = 3.0f, kLootTagFont = 14.0f, kLootTagPadW = 20.0f, kLootTagPadH = 7.0f;
        inline constexpr float kLootMinTagY = 144.0f, kLootTagOffsetY = 31.0f, kLootTagSlant = 4.0f;
        inline constexpr float kLootTagBorderThick = 1.2f, kLootTagStripeW = 3.6f, kLootTextShiftX = 1.5f;

        inline constexpr uint32_t kLootTetherCol = UI_COL32(72, 232, 255, 135), kLootAnchorCol = UI_COL32(150, 248, 255, 210);
        inline constexpr uint32_t kLootPulseCol = UI_COL32(72, 232, 255, 110), kLootPipBg = UI_COL32(10, 24, 34, 235);
        inline constexpr uint32_t kLootPipCore = UI_COL32(195, 252, 255, 255), kLootTagBg = UI_COL32(9, 18, 28, 238);
        inline constexpr uint32_t kLootTagBd = UI_COL32(72, 232, 255, 248), kLootTagText = UI_COL32(220, 252, 255, 255);

        // 画面端オフスクリーン方向ポインター
        inline constexpr float kOffscreenProjLiftY = 1.2f, kSafeMarginX = 72.0f, kSafeMarginTop = 68.0f;
        inline constexpr float kSafeMarginBot = 92.0f, kMinStraightDist = 1.5f, kMinDirLen = 0.01f;
        inline constexpr float kArrowPulseAmp = 3.5f, kCenterRatioY = 0.49f, kOrbitRatioX = 0.31f;
        inline constexpr float kOrbitRatioY = 0.29f, kArrowTipFwd = 14.5f, kArrowNotchFwd = 2.5f;
        inline constexpr float kArrowWingBack = 5.0f, kArrowWingSide = 8.5f, kArrowOutlineThick = 1.3f;
        inline constexpr float kArrowEchoTipBack = 2.5f, kArrowEchoWingBack = 9.5f, kArrowEchoWingSide = 6.5f, kArrowEchoThick = 1.8f;

        inline constexpr float kEdgePillFont = 14.0f, kEdgePillPadW = 18.0f, kEdgePillPadH = 7.0f;
        inline constexpr float kEdgePillGap = 15.0f, kEdgeClampX = 232.0f, kEdgeClampTop = 66.0f;
        inline constexpr float kEdgeClampBot = 92.0f, kEdgePillSlant = 4.0f, kEdgeBorderThick = 1.2f;
        inline constexpr float kEdgeStripeW = 3.6f, kEdgeTextShiftX = 1.5f;

        inline constexpr uint32_t kArrowOutlineCol = UI_COL32(10, 15, 22, 235), kEdgePillBg = UI_COL32(9, 15, 23, 238);
    }

    // --- 画面右上：クエスト進行ガイド HUD ---
    namespace QuestTracker
    {
        inline constexpr float kStepFont = 14.0f, kActionFont = 14.0f, kMinPanelW = 258.0f, kStepWidthPad = 48.0f;
        inline constexpr float kActionWidthPad = 34.0f, kPanelH = 128.0f, kSlant = 6.5f, kRightMargin = 14.0f;
        inline constexpr float kTopY = 10.0f, kTopAccentThick = 2.2f;

        inline constexpr SlantedPanelStyle kPanelStyle = { 2.0f, 2.5f, HudPalette::kPanelShadow125,
        	HudPalette::kGunmetalBg232, HudPalette::kTaskBorder175, 1.1f };

        inline constexpr float kHeaderFont = 14.0f, kHeaderPadX = 11.0f, kHeaderCenterY = 12.5f, kPhaseFont = 12.0f;
        inline constexpr float kPhasePadW = 12.0f, kPhaseH = 17.0f, kPhaseRightPad = 7.0f, kPhaseTopY = 4.0f;
        inline constexpr float kPhaseSlant = 2.5f, kPhaseBorderThick = 1.0f, kPhaseTextShiftX = 1.0f;

        inline constexpr float kDividerLeftPad = 10.0f, kDividerRightPad = 8.0f, kDividerY = 24.0f, kDividerThick = 1.0f;

        inline constexpr float kCheckCenterX = 16.5f, kCheckRadius = 5.2f, kStepTextX = 27.5f, kStep1CenterY = 34.5f;
        inline constexpr float kStep2CenterY = 52.5f, kStep3CenterY = 70.5f;

        inline constexpr float kCalloutPadX = 8.0f, kCalloutTopY = 83.0f, kCalloutTrimW = 16.0f, kCalloutH = 39.0f;
        inline constexpr float kCalloutSlant = 3.0f, kCalloutBorderThick = 1.1f, kCalloutStripeW = 3.4f;
        inline constexpr float kActionTextPadX = 10.0f, kActionLine1Y = 11.0f, kActionLine2Y = 28.5f;

        inline constexpr uint32_t kHeaderCol = UI_COL32(255, 214, 84, 255), kPhaseBg = UI_COL32(16, 28, 38, 238);
        inline constexpr uint32_t kDividerCol = UI_COL32(82, 106, 126, 125), kIdleCheckCol = UI_COL32(120, 144, 162, 205);
        inline constexpr uint32_t kStepDoneText = UI_COL32(138, 236, 178, 225), kStepIdleText = UI_COL32(176, 194, 210, 220);
        inline constexpr uint32_t kCalloutBg = UI_COL32(12, 22, 32, 238);

        inline constexpr const char8_t* kStep1Text   = u8"1. 木箱を射撃で破壊して補給";
        inline constexpr const char8_t* kStep2Format = u8"2. 標的を破壊 (%d/%d)";
        inline constexpr const char8_t* kStep3Text   = u8"3. 北東LZで3秒待機";
    }

    // --- 画面最下端中央：スリム・キーガイドリップ ---
    namespace ControlsGuide
    {
        inline constexpr int kSuggestReloadAmmo = 8, kItemCount = 6;

        inline constexpr ControlGuideItemSpec kItems[kItemCount] = {
            { "WASD",  u8"移動",     HudPalette::kCyanAccent         },
            { "LMB",   u8"射撃",     UI_COL32(255, 198, 58, 255)     },
            { "SPACE", u8"回避",     HudPalette::kCyanAccent         },
            { "R",     u8"リロード", UI_COL32(255, 186, 52, 255)     },
            { "E",     u8"回収",     HudPalette::kEmeraldAccent      },
            { "M",     u8"地図",     UI_COL32(255, 206, 68, 255)     },
        };

        inline constexpr float kBarW = 568.0f, kBarH = 24.0f, kBarX = 376.0f, kBottomMargin = 8.0f, kDescFont = 13.0f;
        inline constexpr float kBarRounding = 4.0f, kBarBorderThick = 1.0f, kInnerWidthTrim = 12.0f;
        inline constexpr float kCellStartPadX = 7.0f, kKeyFont = 12.0f, kKeyPadW = 8.0f, kKeyH = 17.0f;
        inline constexpr float kKeyRounding = 2.5f, kKeyBorderThick = 1.0f, kDescGapX = 4.0f, kGlowBaseAlpha = 145.0f;
        inline constexpr float kGlowPulseAlpha = 105.0f;

        inline constexpr uint32_t kBarBg = UI_COL32(8, 13, 20, 208), kBarBorder = UI_COL32(76, 102, 122, 145);
        inline constexpr uint32_t kKeyIdleBg = UI_COL32(22, 34, 46, 232), kKeyIdleBd = UI_COL32(102, 128, 148, 185);
        inline constexpr uint32_t kKeyIdleTx = UI_COL32(242, 248, 255, 255), kDescIdleTx = UI_COL32(204, 222, 238, 245);
        inline constexpr uint32_t kKeyActiveBg = UI_COL32(22, 68, 88, 245), kKeyHighlightBg = UI_COL32(34, 52, 32, 240);
        inline constexpr uint32_t kKeyUrgentBg = UI_COL32(68, 20, 22, 248), kKeyUrgentCol = UI_COL32(255, 96, 78, 255);
    }

    // --- タクティカルレーダー・コンパウンドジオメトリ共通定数 (Radar) ---
    namespace Radar
    {
        // 要塞全域ワールド範囲 (X: -34m..+34m = 68m, Z: -12m..+69m = 81m)
        inline constexpr float kWorldMinX = -34.0f, kWorldMaxX = 34.0f, kWorldMinZ = -12.0f, kWorldMaxZ = 69.0f;
        inline constexpr float kWorldSpanX = kWorldMaxX - kWorldMinX, kWorldSpanZ = kWorldMaxZ - kWorldMinZ;

        inline constexpr float kCanvasRounding = 3.0f, kCanvasBorderThick = 1.2f, kMapInnerPad = 5.5f, kClipInset = 1.0f;
        inline constexpr size_t kMaxConvexVerts = 12, kMaxPolylineVerts = 32;

        inline constexpr uint32_t kForestBgCol = UI_COL32(7, 13, 14, 246), kCanvasBorderCol = UI_COL32(72, 98, 118, 185);

        // 戦術グリッド線
        inline constexpr float kGridMinX = -30.0f, kGridMaxX = 30.0f, kGridMinZ = -10.0f, kGridMaxZ = 60.0f;
        inline constexpr float kGridStep = 10.0f, kGridLineThick = 1.0f;
        inline constexpr uint32_t kGridLineCol = UI_COL32(28, 44, 52, 85);

        // 要塞床面凸ポリゴン頂点テーブル
        inline constexpr uint32_t kFloorStandardCol = UI_COL32(24, 38, 50, 236), kFloorVaultCol = UI_COL32(34, 42, 34, 238);
        inline constexpr uint32_t kFloorHelipadCol = UI_COL32(22, 44, 48, 240);

        inline constexpr Vector2 kPolySouthHeli[]  = { {-10.0f, 6.5f}, {-10.0f, -5.5f}, {-6.0f, -9.5f}, {6.0f, -9.5f}, {10.0f, -5.5f}, {10.0f, 6.5f} };
        inline constexpr Vector2 kPolySouthHub[]   = { {-10.0f, 23.6f}, {-10.0f, 6.5f}, {10.0f, 6.5f}, {10.0f, 23.6f} };
        inline constexpr Vector2 kPolySouthWestA[] = { {-21.5f, 27.15f}, {-28.5f, 22.0f}, {-28.5f, 11.5f}, {-22.5f, 6.5f}, {-10.0f, 6.5f}, {-10.0f, 27.15f} };
        inline constexpr Vector2 kPolySouthWestB[] = { {-10.0f, 27.15f}, {-10.0f, 23.6f}, {-5.5f, 23.6f}, {-8.5f, 27.15f} };
        inline constexpr Vector2 kPolySouthEastA[] = { {10.0f, 27.15f}, {10.0f, -0.5f}, {23.5f, -0.5f}, {26.5f, 3.5f}, {26.5f, 25.5f}, {25.5f, 27.15f} };
        inline constexpr Vector2 kPolySouthEastB[] = { {25.5f, 27.15f}, {26.5f, 13.5f}, {31.5f, 17.5f}, {31.5f, 23.5f} };
        inline constexpr Vector2 kPolySouthEastC[] = { {8.5f, 27.15f}, {5.5f, 23.6f}, {10.0f, 23.6f}, {10.0f, 27.15f} };

        inline constexpr Vector2 kPolyNorthWest[]  = { {-26.5f, 48.0f}, {-26.5f, 37.5f}, {-22.0f, 32.85f}, {-8.2f, 32.85f}, {-8.2f, 51.0f}, {-13.0f, 51.0f} };
        inline constexpr Vector2 kPolyNorthVault[] = { {-26.5f, 48.0f}, {-13.0f, 51.0f}, {-13.0f, 59.5f}, {-16.5f, 64.5f}, {-27.0f, 64.5f}, {-31.5f, 60.5f}, {-31.5f, 52.0f} };
        inline constexpr Vector2 kPolyNorthNeck[]  = { {-8.2f, 51.0f}, {-8.2f, 35.6f}, {5.5f, 35.6f}, {5.5f, 51.0f} };
        inline constexpr Vector2 kPolyNorthNeckW[] = { {-8.2f, 35.6f}, {-8.2f, 32.85f}, {-5.5f, 35.6f} };
        inline constexpr Vector2 kPolyNorthNeckE[] = { {5.5f, 35.6f}, {8.2f, 32.85f}, {8.2f, 35.6f} };
        inline constexpr Vector2 kPolyNorthEast[]  = { {5.0f, 53.0f}, {5.0f, 35.6f}, {8.2f, 32.85f}, {24.0f, 32.85f}, {30.0f, 38.5f}, {30.0f, 49.0f}, {26.0f, 53.0f} };
        inline constexpr Vector2 kPolyNorthHeli[]  = { {5.0f, 53.0f}, {26.0f, 53.0f}, {26.0f, 61.5f}, {21.0f, 66.5f}, {10.0f, 66.5f}, {5.0f, 61.5f} };

        inline constexpr MapConvexPolySpec kFloorPolys[] = {
            { kPolySouthHub,   4, MapFloorTone::Standard },
            { kPolySouthWestA, 6, MapFloorTone::Standard },
            { kPolySouthWestB, 4, MapFloorTone::Standard },
            { kPolySouthEastA, 6, MapFloorTone::Standard },
            { kPolySouthEastB, 4, MapFloorTone::Vault    },
            { kPolySouthEastC, 4, MapFloorTone::Standard },
            { kPolySouthHeli,  6, MapFloorTone::Helipad  },
            { kPolyNorthWest,  6, MapFloorTone::Standard },
            { kPolyNorthVault, 7, MapFloorTone::Vault    },
            { kPolyNorthNeck,  4, MapFloorTone::Standard },
            { kPolyNorthNeckW, 3, MapFloorTone::Standard },
            { kPolyNorthNeckE, 3, MapFloorTone::Standard },
            { kPolyNorthEast,  7, MapFloorTone::Standard },
            { kPolyNorthHeli,  6, MapFloorTone::Helipad  },
        };

        // 河川 ＆ 東西2本の橋
        inline constexpr float kBridgeCenterXs[]   = { -14.0f, 14.0f };
        inline constexpr float kBridgeHalfW = 1.55f, kBridgeMaxZ = 33.1f, kBridgeMinZ = 26.9f, kBridgeRounding = 1.5f;
        inline constexpr float kBridgeBorderThick = 1.1f, kRiverEdgeThick = 1.0f;
        inline constexpr uint32_t kRiverFillCol = UI_COL32(18, 72, 122, 220), kRiverEdgeCol = UI_COL32(72, 168, 232, 165);
        inline constexpr uint32_t kBridgeFillCol = UI_COL32(148, 114, 72, 245), kBridgeBorderCol = UI_COL32(236, 202, 138, 230);

        // 障害物・針葉樹のマップ分類閾値
        inline constexpr float kObsMinExtY = 0.35f, kObsMaxExtXZ = 20.0f, kRiverCenterZ = 30.0f;
        inline constexpr float kRiverSkipHalfZ = 1.2f, kRiverSkipMinExtZ = 2.2f, kRiverSkipMinExtX = 4.0f;
        inline constexpr float kTreeMaxTrunkExt = 0.50f, kTreeMinExtY = 2.0f, kTreeMinCanopyPx = 2.2f, kTreeCanopyWorldR = 2.3f;
        inline constexpr int kTreeCanopySegs = 10;
        inline constexpr float kTreeOutlineThick = 1.0f, kFenceMinExtX = 1.4f, kFenceMaxExtZ = 0.45f;
        inline constexpr float kFenceMinExtY = 1.1f, kLargeWallMinExtY = 1.3f;

        inline constexpr uint32_t kTreeFillCol = UI_COL32(18, 56, 38, 155), kTreeOutlineCol = UI_COL32(42, 108, 72, 130);
        inline constexpr uint32_t kFenceFillCol = UI_COL32(108, 152, 182, 235), kWallFillCol = UI_COL32(68, 92, 112, 235);
        inline constexpr uint32_t kCoverFillCol = UI_COL32(108, 124, 92, 210);

        // 外郭境界ポリライン
        inline constexpr int kBorderAlphaFull = 225, kBorderAlphaMini = 195, kBorderR = 126, kBorderG = 192, kBorderB = 228;
        inline constexpr float kBorderThickFull = 1.8f, kBorderThickMini = 1.3f;

        inline constexpr Vector2 kBorderSouthOuter[] = {
            {-15.45f, 27.15f}, {-21.50f, 27.15f}, {-28.50f, 22.00f}, {-28.50f, 11.50f},
            {-22.50f,  6.50f}, {-10.00f,  6.50f}, {-10.00f, -5.50f}, { -6.00f, -9.50f},
            {  6.00f, -9.50f}, { 10.00f, -5.50f}, { 10.00f, -0.50f}, { 23.50f, -0.50f},
            { 26.50f,  3.50f}, { 26.50f, 13.50f}, { 31.50f, 17.50f}, { 31.50f, 23.50f},
            { 25.50f, 27.15f}, { 15.45f, 27.15f}
        };
        inline constexpr Vector2 kBorderSouthInner[] = {
            {-12.55f, 27.15f}, { -8.50f, 27.15f}, { -5.50f, 23.60f},
            {  5.50f, 23.60f}, {  8.50f, 27.15f}, { 12.55f, 27.15f}
        };
        inline constexpr Vector2 kBorderNorthOuter[] = {
            {-15.45f, 32.85f}, {-22.00f, 32.85f}, {-26.50f, 37.50f}, {-26.50f, 48.00f},
            {-31.50f, 52.00f}, {-31.50f, 60.50f}, {-27.00f, 64.50f}, {-16.50f, 64.50f},
            {-13.00f, 59.50f}, {-13.00f, 51.00f}, {  5.00f, 51.00f}, {  5.00f, 61.50f},
            { 10.00f, 66.50f}, { 21.00f, 66.50f}, { 26.00f, 61.50f}, { 26.00f, 53.00f},
            { 30.00f, 49.00f}, { 30.00f, 38.50f}, { 24.00f, 32.85f}, { 15.45f, 32.85f}
        };
        inline constexpr Vector2 kBorderNorthInner[] = {
            {-12.55f, 32.85f}, { -8.20f, 32.85f}, { -5.50f, 35.60f},
            {  5.50f, 35.60f}, {  8.20f, 32.85f}, { 12.55f, 32.85f}
        };

        inline constexpr MapPolylineSpec kBorderLines[] = {
            { kBorderSouthOuter, 18 },
            { kBorderSouthInner,  6 },
            { kBorderNorthOuter, 20 },
            { kBorderNorthInner,  6 },
        };

        // 誘導パス・物資・LZ・標的・敵・プレイヤーアイコン
        inline constexpr float kPathThickFull = 2.6f, kPathThickMini = 1.9f;
        inline constexpr uint32_t kPathExtractCol = UI_COL32(52, 250, 148, 220), kPathNormalCol = UI_COL32(72, 232, 255, 220);

        inline constexpr float kLootGoldFullR = 5.6f, kLootNormFullR = 4.2f, kLootGoldMiniR = 4.0f, kLootNormMiniR = 3.0f;
        inline constexpr uint32_t kLootGoldCol = UI_COL32(255, 214, 52, 250), kLootNormCol = UI_COL32(72, 236, 178, 225);
        inline constexpr uint32_t kLootOutlineCol = UI_COL32(10, 16, 22, 230);

        inline constexpr float kLzRadiusFull = 8.5f, kLzRadiusMini = 5.6f;
        inline constexpr int kLzCircleSegs = 16;
        inline constexpr float kLzCircleThick = 1.8f, kLzBaseAlpha = 190.0f, kLzPulseAlpha = 65.0f;
        inline constexpr float kLzWaveBaseR = 2.5f, kLzWavePulseR = 3.5f, kLzWaveMaxAlpha = 150.0f, kLzWaveThick = 1.3f;
        inline constexpr int kLzOpenR = 52, kLzOpenG = 255, kLzOpenB = 146;
        inline constexpr uint32_t kLzLockedCol = UI_COL32(255, 118, 88, 195), kLzBgCol = UI_COL32(12, 28, 22, 225);

        inline constexpr float kTargetRadiusFull = 6.2f, kTargetRadiusMini = 4.4f, kTargetPulseBaseR = 2.0f, kTargetPulseAmpR = 3.0f;
        inline constexpr int kTargetPulseSegs = 14;
        inline constexpr float kTargetPulseThick = 1.3f, kTargetNumFont = 13.0f, kTargetNumOffX = 6.5f, kTargetNumOffY = -7.5f;
        inline constexpr uint32_t kTargetPriCol = UI_COL32(255, 212, 58, 255), kTargetSecCol = UI_COL32(255, 96, 72, 245);
        inline constexpr uint32_t kTargetOutline = UI_COL32(255, 255, 255, 220), kTargetPulseCol = UI_COL32(255, 214, 62, 185);
        inline constexpr uint32_t kTargetNumCol = UI_COL32(255, 236, 148, 255);

        inline constexpr float kEnemyMaxConeDist = 8.0f, kEnemyConeRangeMul = 0.65f, kEnemyRadiusFull = 4.8f, kEnemyRadiusMini = 3.5f;
        inline constexpr int kEnemyCircleSegs = 10;
        inline constexpr float kEnemyOutlineThick = 1.0f;
        inline constexpr uint32_t kEnemyConeAlert = UI_COL32(255, 58, 58, 58), kEnemyConeCalm = UI_COL32(255, 196, 68, 42);
        inline constexpr uint32_t kEnemyAlertCol = UI_COL32(255, 52, 52, 255), kEnemyCalmCol = UI_COL32(255, 146, 52, 245);
        inline constexpr uint32_t kEnemyOutlineCol = UI_COL32(12, 16, 22, 230);

        inline constexpr float kPlayerAimLen = 7.5f, kPlayerAimHalfAng = 0.30f, kPlayerRadiusFull = 6.8f;
        inline constexpr float kPlayerRadiusMini = 5.2f, kPlayerTipMul = 1.25f, kPlayerBackMul = 0.80f;
        inline constexpr float kPlayerWingMul = 0.78f, kPlayerOutlineThick = 1.4f;
        inline constexpr uint32_t kPlayerConeCol = UI_COL32(82, 246, 255, 52), kPlayerFillCol = UI_COL32(95, 248, 255, 255);
        inline constexpr uint32_t kPlayerOutline = UI_COL32(8, 16, 24, 245);

        inline constexpr float kNorthFont = 13.0f, kNorthPadX = 6.0f, kNorthPadY = 4.0f;
        inline constexpr uint32_t kNorthCol = UI_COL32(168, 204, 228, 235);
    }

    // --- 左上ミニマップ ＆ [M] 全体作戦マップオーバーレイ (Minimap) ---
    namespace Minimap
    {
        // A. 左上常時ミニマップ
        inline constexpr float kMiniW = 192.0f, kMiniH = 208.0f, kMiniX = 14.0f, kMiniY = 10.0f, kMiniSlant = 5.5f;
        inline constexpr float kTopAccentThick = 2.0f;

        inline constexpr SlantedPanelStyle kMiniPanelStyle = { 2.0f, 2.5f, HudPalette::kPanelShadow125,
        	HudPalette::kGunmetalBg232, HudPalette::kGunmetalBorder175, 1.1f };

        inline constexpr float kSectorFont = 13.0f, kSectorPadX = 9.0f, kSectorCenterY = 11.5f, kMapPadX = 7.0f;
        inline constexpr float kMapTopY = 22.0f, kMapTrimW = 14.0f, kMapTrimH = 42.0f, kFooterFont = 13.0f, kFooterBotPad = 10.0f;

        inline constexpr float kLootBarGapY = 5.0f, kLootBarH = 26.0f, kLootBarSlant = 4.0f;
        inline constexpr float kLootBarBorderThick = 1.0f, kLootIconSize = 18.0f, kLootIconPadX = 8.0f;
        inline constexpr float kLootIconRound = 2.5f, kLootTextRightPad = 8.0f, kLootDigitH = 17.0f;

        inline constexpr uint32_t kMiniTopLineCol = UI_COL32(72, 232, 255, 240), kSectorTextCol = UI_COL32(174, 238, 255, 255);
        inline constexpr uint32_t kFooterTextCol = UI_COL32(204, 224, 240, 245), kLootBarBorder = UI_COL32(158, 134, 72, 175);
        inline constexpr uint32_t kLootIconTint = UI_COL32(255, 255, 255, 250), kLootValueCol = UI_COL32(255, 220, 82, 255);

        // B. 全体作戦マップオーバーレイ ([M])
        inline constexpr uint32_t kBackdropDimCol = UI_COL32(4, 8, 14, 196);
        inline constexpr float kBoardW = 768.0f, kBoardH = 596.0f, kBoardOffsetY = 10.0f, kBoardSlant = 10.0f;
        inline constexpr float kBoardTopLineThick = 2.8f;

        inline constexpr SlantedPanelStyle kBoardPanelStyle = { 3.0f, 4.0f, HudPalette::kPanelShadow175,
        	UI_COL32(10, 16, 24, 248), UI_COL32(92, 124, 148, 220), 1.5f };

        inline constexpr float kBoardTitleFont = 16.0f, kBoardTitlePadX = 20.0f, kBoardHeaderCenterY = 19.0f;
        inline constexpr float kCloseHintFont = 14.0f, kCloseHintRightPad = 18.0f, kHeaderDivLeftPad = 16.0f;
        inline constexpr float kHeaderDivRightPad = 14.0f, kHeaderDivY = 37.0f, kHeaderDivThick = 1.0f;

        inline constexpr float kFullMapPadX = 16.0f, kFullMapTopY = 44.0f, kFullMapW = 436.0f, kFullMapTrimH = 54.0f;

        inline constexpr float kSideGapX = 28.0f, kSideTrimW = 44.0f, kSideStartY = 46.0f, kSectionFont = 15.0f;
        inline constexpr float kCurrentHdrStepY = 22.0f, kCurrentSecIndentX = 8.0f, kCurrentSecStepY = 28.0f;
        inline constexpr float kSideDivStepY = 10.0f, kSubHdrStepY = 24.0f;

        inline constexpr float kLegendIconOffX = 10.0f, kLegendIconOffY = 8.5f, kLegendCircleRad = 5.0f;
        inline constexpr int kLegendCircleSegs = 12;
        inline constexpr float kLegendDiamondRad = 5.5f, kLegendTextOffX = 24.0f, kLegendFont = 14.0f;
        inline constexpr float kLegendRowStepY = 22.5f, kAfterLegendGapY = 7.0f;

        inline constexpr float kGuideIndentX = 4.0f, kGuideFont = 14.0f, kGuideRowStepY = 22.0f;
        inline constexpr size_t kGuideTipStartIdx = 5;

        // 全体マップ エリア名ピルラベル
        inline constexpr float kZoneLabelFont = 13.0f, kZoneLabelHalfPadX = 5.0f, kZoneLabelHalfPadY = 2.5f;
        inline constexpr float kZoneLabelPadW = 10.0f, kZoneLabelPadH = 5.0f, kZoneLabelRound = 3.0f;
        inline constexpr uint32_t kZoneLabelBg = UI_COL32(8, 13, 20, 224);

        inline constexpr MapZoneLabelSpec kZoneLabels[] = {
            {   0.0f, -3.5f, u8"START",        UI_COL32(165, 242, 255, 250) },
            {  18.0f,  8.5f, u8"東：射撃場",    UI_COL32(242, 248, 255, 250) },
            {  27.5f, 20.5f, u8"密輸ドック",    UI_COL32(148, 246, 198, 250) },
            { -18.5f, 15.5f, u8"西：訓練場",    UI_COL32(242, 248, 255, 250) },
            { -14.0f, 30.0f, u8"西の橋",        UI_COL32(255, 226, 150, 255) },
            {  14.0f, 30.0f, u8"東の橋",        UI_COL32(255, 226, 150, 255) },
            {   0.0f, 30.0f, u8"中央河川",      UI_COL32(138, 208, 252, 240) },
            { -15.5f, 41.5f, u8"北西：広場",    UI_COL32(242, 248, 255, 250) },
            { -22.5f, 59.5f, u8"秘宝庫",        UI_COL32(255, 224, 88, 255)  },
            {  16.5f, 41.5f, u8"北東：司令部",  UI_COL32(242, 248, 255, 250) },
            {  15.5f, 63.2f, u8"GOAL: 脱出LZ",  UI_COL32(98, 255, 168, 255)  },
        };

        inline constexpr MapLegendItemSpec kLegendItems[] = {
            { UI_COL32(95, 248, 255, 255),  true,  u8"現在地・誘導ルート"         },
            { UI_COL32(255, 212, 58, 255),  false, u8"破壊標的 [1] - [4]"         },
            { UI_COL32(72, 236, 178, 255),  false, u8"補給物資（弾薬・回復・現金）" },
            { UI_COL32(255, 214, 52, 255),  false, u8"黄金アヒル像（高額秘宝）"   },
            { UI_COL32(255, 96, 52, 255),   true,  u8"敵兵・視界コーン"           },
            { UI_COL32(52, 255, 146, 255),  true,  u8"脱出LZ（標的4基破壊で開放）" },
        };

        inline constexpr const char8_t* kGuideSteps[] = {
            u8"1. 東棟の木箱を破壊 ＆ 標的[1] 射撃",
            u8"2. 西棟で遮蔽・回避 ＆ 標的[2] 破壊",
            u8"3. 西か東の橋で北エリアへ渡河",
            u8"4. 北西の標的[3]・北東の標的[4] 破壊",
            u8"5. 北東の緑スモーク内で 3秒 待機",
            u8"・[C] ダッシュ / [Y] 上空視点",
        };

        inline constexpr uint32_t kBoardTopLineCol = UI_COL32(255, 206, 68, 250), kBoardTitleCol = UI_COL32(255, 218, 82, 255);
        inline constexpr uint32_t kCloseHintCol = UI_COL32(192, 226, 248, 248), kHeaderDivCol = UI_COL32(76, 102, 122, 145);
        inline constexpr uint32_t kCurrentHdrCol = UI_COL32(138, 236, 255, 255);
        inline constexpr uint32_t kSideDivCol = UI_COL32(76, 102, 122, 130), kSubHdrCol = UI_COL32(255, 214, 82, 255);
        inline constexpr uint32_t kLegendPipOut = UI_COL32(255, 255, 255, 195), kLegendTextCol = UI_COL32(236, 244, 252, 250);
        inline constexpr uint32_t kGuideTipCol = UI_COL32(162, 232, 255, 248);
    }
}
