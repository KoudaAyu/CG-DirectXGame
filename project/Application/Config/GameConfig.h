#pragma once
#include "Vector.h"
#include <cstdint>

namespace GameConfig
{
    // =========================================================================
    // 🌍 環境 & ステージ演出設定 (Environment & Stage)
    // =========================================================================
    namespace Environment
    {
        // 川 (River) 水面モデル配置設定
        constexpr Vector3 kRiverPosition          = { 0.0f, 0.04f, 18.75f };
        constexpr Vector3 kRiverScale             = { 60.0f, 1.0f, 5.0f };
        constexpr Vector3 kRiverRotation          = { 0.0f, 0.0f, 0.0f };
        constexpr float   kRiverSurfaceY          = 0.04f;

        // 川の水流パーティクル設定 (水面高さ kRiverSurfaceY に自動連動)
        constexpr float   kRiverWaveParticleY     = kRiverSurfaceY + 0.03f; // 0.07f (水面の3cm上)
        constexpr float   kRiverSplashY           = kRiverSurfaceY + 0.03f; // 0.07f
        constexpr float   kRiverFoamPillarY       = kRiverSurfaceY + 0.035f; // 0.075f
        constexpr float   kRiverZMin              = 16.5f;
        constexpr float   kRiverZMax              = 21.0f;
        constexpr float   kRiverWaveInterval      = 0.15f;  // 波紋放出インターバル (秒)
        constexpr float   kRiverSplashInterval    = 0.25f;  // 水滴放出インターバル (秒)
        constexpr float   kRiverWaveSpeedMin      = 4.5f;   // 流速 (m/s)
        constexpr float   kRiverWaveSpeedMax      = 7.0f;
        constexpr float   kRiverWaveScaleMin      = 3.0f;   // リップルスケール
        constexpr float   kRiverWaveScaleMax      = 5.5f;

        // 脱出ヘリパッド設定
        constexpr Vector3 kExtractionPadPosition  = { 0.0f, 0.01f, 32.0f };
        constexpr float   kExtractionRadius       = 2.2f;   // 脱出ゾーン判定半径 (m)
        constexpr float   kExtractionMaxTime      = 3.0f;   // 脱出カウントダウン時間 (秒)

        // シーン演出時間
        constexpr float   kClearSlowMoDuration    = 1.5f;   // 生還クリア時のスロー演出時間 (秒)
        constexpr float   kDeathSequenceDuration  = 2.0f;   // 戦死時の暗転演出時間 (秒)
    }

    // =========================================================================
    // 🏃 プレイヤーパラメータ設定 (Player)
    // =========================================================================
    namespace Player
    {
        // 基本移動 & 体力 & スタミナ
        constexpr float kDefaultMaxHp                  = 100.0f;
        constexpr float kDefaultMaxStamina             = 100.0f;
        constexpr float kMoveSpeed                     = 0.05f;  // 通常移動速度
        constexpr float kColliderRadius                = 0.55f;  // 衝突判定球半径
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
    // 💥 戦闘 & 射撃パラメータ設定 (Combat)
    // =========================================================================
    namespace Combat
    {
        // 弾丸挙動
        constexpr float kBulletSpeed                   = 0.45f;  // 弾丸初速
        constexpr float kBulletRadius                  = 0.12f;  // 障害物・遮蔽物との衝突判定半径
        constexpr float kBulletHitRadius               = 0.25f;  // キャラクターとの当たり判定半径
        constexpr float kBulletLifeTime                = 2.0f;   // 弾丸生存時間 (秒)
        constexpr float kShotCooldown                  = 0.12f;  // 連射レート (秒)

        // 照準ブレ・拡散
        constexpr float kBaseSpread                    = 0.01f;  // 静止時照準ブレ
        constexpr float kMoveSpreadPenalty             = 0.06f;  // 移動時照準ブレ加算
        constexpr float kShootSpreadPenalty            = 0.04f;  // 射撃時照準ブレ加算
        constexpr float kSpreadRecoverRate             = 0.3f;   // 照準収束速度
        constexpr float kMaxSpread                     = 0.25f;  // 最大照準ブレ

        // ダメージ & ヒット判定
        constexpr float kEnemyBulletDamage             = 10.0f;  // 敵の射撃ダメージ
        constexpr float kContactDamage                 = 20.0f;  // 敵接触ダメージ
        constexpr float kPlayerHitRadius               = 0.6f;   // プレイヤー当たり判定半径
        constexpr float kEnemyHitRadius                = 0.6f;   // 敵当たり判定半径
    }
}
