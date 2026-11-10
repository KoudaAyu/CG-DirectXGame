#pragma once
#include "Vector.h"
#include <cmath>
#include <cstdint>
#include <numbers>

namespace GameConfig
{
    // =========================================================================
    // 数学定数 & 高速算術ヘルパー (Math Constants & Fast Intrinsics)
    // =========================================================================
    namespace Math
    {
        inline constexpr float kPi        = std::numbers::pi_v<float>;
        inline constexpr float kTwoPi     = 2.0f * std::numbers::pi_v<float>;
        inline constexpr float kHalfPi    = 0.5f * std::numbers::pi_v<float>;
        inline constexpr float kQuarterPi = 0.25f * std::numbers::pi_v<float>;
        inline constexpr float kInvPi     = std::numbers::inv_pi_v<float>;
        inline constexpr float kDegToRad  = std::numbers::pi_v<float> / 180.0f;
        inline constexpr float kRadToDeg  = 180.0f * std::numbers::inv_pi_v<float>;

        [[nodiscard]] constexpr float Square(float x) noexcept { return x * x; }
        [[nodiscard]] constexpr float Cube(float x) noexcept { return x * x * x; }

        // <cmath> std::remainder による O(1) 角度正規化 [-pi, +pi]
        [[nodiscard]] inline float WrapAngleSigned(float rad) noexcept
        {
            return (std::abs(rad) <= kPi) ? rad : std::remainder(rad, kTwoPi);
        }
    }

    // =========================================================================
    // 環境 & ステージ演出設定 (Environment & Stage)
    // =========================================================================
    namespace Environment
    {
        // 川 (River) 水底・水面モデル配置設定（ズームアウト・上空視点でも途切れないよう東西1800m全域をカバー）
        constexpr Vector3 kRiverPosition          = { 0.0f, 0.002f, 30.0f };
        constexpr Vector3 kRiverScale             = { 1800.0f, 1.0f, 5.20f };
        constexpr Vector3 kRiverRotation          = { 0.0f, 0.0f, 0.0f };
        constexpr float   kRiverSurfaceY          = 0.032f;

        // 川の水流パーティクル設定 (水面高さ kRiverSurfaceY に自動連動)
        constexpr float   kRiverWaveParticleY     = kRiverSurfaceY + 0.024f; // 0.056f (きらめく波頭輝点・細泡)
        constexpr float   kRiverSplashY           = kRiverSurfaceY + 0.012f; // 0.044f
        constexpr float   kRiverFoamPillarY       = kRiverSurfaceY + 0.020f; // 0.052f
        constexpr float   kRiverZMin              = 27.65f;
        constexpr float   kRiverZMax              = 32.35f;
        constexpr float   kRiverWaveInterval      = 0.12f;  // 水面きらめき・さざ波放出インターバル (秒)
        constexpr float   kRiverSplashInterval    = 0.18f;  // 水滴アーチ放出インターバル (秒)
        constexpr float   kRiverWaveSpeedMin      = 2.4f;   // 流速 (m/s)
        constexpr float   kRiverWaveSpeedMax      = 4.2f;
        constexpr float   kRiverWaveScaleMin      = 0.18f;  // 繊細な白泡・リップルスケール
        constexpr float   kRiverWaveScaleMax      = 0.46f;

        // 脱出ヘリパッド設定（北東の八角形突出ヘリパッド半島中央）
        constexpr Vector3 kExtractionPadPosition  = { 15.5f, 0.018f, 59.5f };
        constexpr float   kExtractionRadius       = 2.2f;   // 脱出ゾーン判定半径 (m)
        constexpr float   kExtractionMaxTime      = 3.0f;   // 脱出カウントダウン時間 (秒)

        // シーン演出時間
        constexpr float   kClearSlowMoDuration    = 2.25f;  // 生還クリア時のヘリ回収クライマックス演出時間 (秒)
        constexpr float   kDeathSequenceDuration  = 2.0f;   // 戦死時の暗転演出時間 (秒)
    }

    // =========================================================================
    // プレイヤーパラメータ設定 (Player)
    // =========================================================================
    namespace Player
    {
        // 基本移動 & 体力 & スタミナ
        constexpr float kDefaultMaxHp                  = 100.0f;
        constexpr float kDefaultMaxStamina             = 100.0f;
        constexpr float kMoveSpeed                     = 0.05f;  // 通常移動速度
        constexpr float kHolsterSpeedMultiplier        = 1.30f;  // [C] 銃収納ダッシュ時の移動速度倍率 (Escape from Duckov準拠)
        constexpr float kColliderRadius                = 0.38f;  // 衝突判定球半径 (通路通行をスムーズ化)
        constexpr float kStaminaRegenRate              = 15.0f;  // 毎秒スタミナ回復量

        // 回避ローリング (Dodge Roll)
        constexpr float kDodgeDuration                 = 0.4f;   // 回避所要時間 (秒)
        constexpr float kDodgeSpeed                    = 0.15f;  // 回避移動速度
        constexpr float kDodgeStaminaCost              = 30.0f;  // 回避スタミナ消費量

        // 回避宙返り演出 (アヒル形状に最適化したリフト高さ)
        constexpr float kDodgeClearanceBase            = 0.30f;  // くちばし・お尻の埋もれ防止リフト基準値
        constexpr float kDodgeClearanceSide            = 0.12f;  // 横転時の逃げリフト値
        constexpr float kDodgeHopMax                   = 0.20f;  // 宙返り放物線頂点オフセット
        constexpr float kDodgeDustInterval             = 0.07f;  // 回避中の土煙放出インターバル (秒)
        constexpr float kStepDustInterval              = 0.12f;  // 走り中の土埃放出インターバル (秒)

        // 被弾・無敵演出
        constexpr float kInvincibilityDuration         = 1.0f;   // 被弾後無敵時間 (秒)
        constexpr float kHitFlashDuration              = 0.22f;  // 赤フラッシュ演出時間 (秒)

        // 救急キット & リロード
        constexpr int   kDefaultMedkitCount            = 1;
        constexpr float kMedkitHealAmount              = 40.0f;
        constexpr float kHealDuration                  = 1.8f;
        constexpr int   kMaxMagazineAmmo               = 30;
        constexpr int   kDefaultReserveAmmo            = 90;
        constexpr float kReloadDuration                = 1.5f;
        constexpr float kReloadCancelledNotifyDuration = 1.2f;
    }

    // =========================================================================
    // 戦闘 & 射撃パラメータ設定 (Combat)
    // =========================================================================
    namespace Combat
    {
        // 弾丸挙動
        constexpr float kBulletSpeed                   = 0.45f;  // 弾丸初速
        constexpr float kBulletRadius                  = 0.12f;  // 障害物・遮蔽物との衝突判定半径
        constexpr float kBulletHitRadius               = 0.25f;  // キャラクターとの当たり判定半径
        constexpr float kBulletLifeTime                = 2.0f;   // 弾丸生存時間 (秒)
        constexpr float kShotCooldown                  = 0.12f;  // 連射レート (秒)

        // 照準ブレ・拡散（旧互換定数） ＆ レティクル物理リコイル設定 (720p基準ピクセル)
        constexpr float kBaseSpread                    = 0.01f;  // 静止時照準ブレ
        constexpr float kMoveSpreadPenalty             = 0.06f;  // 移動時照準ブレ加算
        constexpr float kShootSpreadPenalty            = 0.04f;  // 射撃時照準ブレ加算
        constexpr float kSpreadRecoverRate             = 0.3f;   // 照準収束速度
        constexpr float kMaxSpread                     = 0.25f;  // 最大照準ブレ

        constexpr float kReticleSpringStiffness        = 185.0f; // レティクル復元バネ剛性 (omega^2)
        constexpr float kReticleSpringDamping          = 21.0f;  // レティクル減衰係数 (2*zeta*omega)
        constexpr float kReticleClimbSnapPx            = 6.2f;   // 発砲瞬間の縦跳ね上がり初動スナップ (px)
        constexpr float kReticleClimbVelPx             = 185.0f; // 発砲瞬間の縦跳ね上がり初速インパルス (px/s)
        constexpr float kReticleLateralSnapPx          = 3.8f;   // 発砲瞬間の横ブレ初動スナップ (px)
        constexpr float kReticleLateralVelPx           = 135.0f; // 発砲瞬間の横ブレ初速インパルス (px/s)
        constexpr float kReticleMaxOffsetPx            = 52.0f;  // フルオート時の最大レティクル変位半径 (px)

        // ダメージ & ヒット判定
        constexpr float kEnemyBulletDamage             = 10.0f;  // 敵の射撃ダメージ
        constexpr float kContactDamage                 = 20.0f;  // 敵接触ダメージ
        constexpr float kPlayerHitRadius               = 0.40f;  // プレイヤー当たり判定半径
        constexpr float kEnemyHitRadius                = 0.45f;  // 敵当たり判定半径
    }

    // =========================================================================
    // カメラパラメータ設定 (Camera)
    // =========================================================================
    namespace Camera
    {
        // プレイヤー追従オフセット（Escape from Duckov準拠の広角クォータービュー俯瞰）
        // 周囲の部屋構造・橋・遮蔽物・敵の視界コーンが一目で把握できる高さ・引き距離（約27.1m）
        constexpr Vector3 kDefaultFollowOffset         = { 0.0f, 21.5f, -16.5f };
        constexpr Vector3 kFollowRotation              = { 0.915f, 0.0f, 0.0f }; // 約52.4度クォータービュー見下ろし

        // [Y] 視点切替：マップ構造をより広く見渡せる上空タクティカル俯瞰モード
        constexpr Vector3 kOverheadFollowOffset        = { 0.0f, 36.0f, -14.5f };
        constexpr Vector3 kOverheadFollowRotation      = { 1.185f, 0.0f, 0.0f }; // 約68度ハイアングル戦術俯瞰

        // マウス照準方向へのスムーズ先読みカメラオフセット最大距離 (m)
        constexpr float   kCursorLookaheadMax          = 4.2f;

        // ズームスケール制限 (0.55倍〜2.4倍)
        constexpr float   kMinZoomScale                = 0.55f;  // 接近確認
        constexpr float   kMaxZoomScale                = 2.4f;   // 広域俯瞰

        // クリア時のズームイン
        constexpr Vector3 kClearOffset                 = { 0.0f, 6.0f, -6.5f };
    }
}
