#pragma once
#include "Vector.h"
#include "Application/UI/TacticalUICanvas.h"
#include <cstdint>

/// <summary>
/// ゲームオーバー3幕MEDEVAC演出コントローラー用 音響・タイムライン・HUDパレット定数定義 (GameOverSequenceTuning)
/// </summary>
namespace GameOverSequenceTuning
{
	// --- タイムライン・バレットタイム・シーン遷移定数 ---
	inline constexpr float kBulletTimeSlowEnd       = 0.20f; // バレットタイム18%減速終了時刻 (sec)
	inline constexpr float kBulletTimeRecoverEnd    = 0.36f; // 等倍速復帰時刻 (sec)
	inline constexpr float kBulletTimeRecoverSpan   = 0.16f; // 0.20s -> 0.36s 加速区間 (sec)
	inline constexpr float kBulletTimeMinScale      = 0.18f;
	inline constexpr float kBulletTimeAccelGain     = 0.82f;
	inline constexpr float kBaseSceneParticleScale  = 0.15f;

	inline constexpr float kSkipEnableTime          = 0.22f; // スキップ入力受付開始時刻 (sec)
	inline constexpr float kNormalShutterDuration   = 1.40f; // 通常完走時シャッター時間 (sec)
	inline constexpr float kSkipShutterDuration     = 0.85f; // スキップ時シャッター時間 (sec)

	// --- タクティカル音響ボリューム・ループ周期定数 ---
	inline constexpr float kKiaImpactVolume         = 0.82f;
	inline constexpr float kDuckVoiceVolume         = 0.85f;
	inline constexpr float kBodyCollapseVolume      = 0.78f;
	inline constexpr float kRifleBounceVolume       = 0.62f;
	inline constexpr float kBeaconAlarmVolume       = 0.68f;
	inline constexpr float kHeliDoorVolume          = 0.58f;
	inline constexpr float kRadioCallVolume         = 0.52f;
	inline constexpr float kSkipClickVolume         = 0.60f;

	inline constexpr float kFlareHissLoopPeriodSec  = 3.00f;
	inline constexpr float kFlareHissVolume         = 0.28f;
	inline constexpr float kRotorLoopPeriodSec      = 4.00f;
	inline constexpr float kRotorBaseVolume         = 0.14f;
	inline constexpr float kRotorArrivalGain        = 0.28f;

	// --- シネマスコープ黒帯・テレメトリ・救難無線コールUI定数 ---
	inline constexpr float kProjMinClipW            = 0.15f;
	inline constexpr float kLetterboxHeightRatio    = 0.076f;
	inline constexpr float kLetterboxOpenDuration   = 0.22f;
	inline constexpr float kLetterboxMinHeightPx    = 0.5f;

	inline constexpr float kTelemetryFontSize       = 13.0f;
	inline constexpr float kSkipFontSize            = 13.5f;
	inline constexpr float kRadioCallFontSize       = 18.5f;
	inline constexpr float kRadioCallStartTime      = 1.12f;
	inline constexpr float kRadioCallEndTime        = 2.42f;
}
