#pragma once
#include <memory>
#include <vector>
#include <string>
#include "Vector.h"
#include "Matrix4x4.h"
#include "RenderContext.h"
#include "Object3d.h"
#include "Application/Audio/TacticalAudio.h"

class GamePlayScene;
class AppParticleManager;

/// <summary>
/// 軍用強襲ヘリ低空急襲・ファストロープ降下 ＆ 脱出回収ヘリ（Dustoff-1）演出コントローラー
/// (HelicopterInsertionController)
/// 
/// 【開幕インサーション演出（5幕構成）】
/// 1. Act 1 (0.0s 〜 3.4s): 白昼の超低空急襲アプローチ ＆ フレア制動（スライドドア開放）
/// 2. Act 2 (3.4s 〜 5.6s): 開放キャビン前待機・RED LIGHT -> GREEN LIGHT 合図・ロープ飛び移り
/// 3. Act 3 (5.6s 〜 7.1s): ファストロープ高速滑降 ＆ 地上直前コンバットブレーキ
/// 4. Act 4 (7.1s 〜 8.8s): 着地インパクト・360度セクタークリアリング ＆ コッキング排莢
/// 5. Act 5 (8.8s 〜 9.7s): ヘリ急上昇離脱 ＆ シームレスTPSカメラ合流
///
/// 【終盤エクストラクション回収演出（Dustoff-1）】
/// 全標的破壊後、北空から回収機 Dustoff-1 が誘導緑煙へ向けて飛来・旋回待機し、
/// 脱出カウントダウン〜生還クライマックスにかけてパッド直上へ低空ホバリング進入します。
/// </summary>
class HelicopterInsertionController
{
public:
	explicit HelicopterInsertionController(GamePlayScene* scene);
	~HelicopterInsertionController();

	void Initialize();
	void Finalize();
	void Update(float deltaTime);
	void UpdateExtractionPickup(float realDeltaTime);
	void Draw3D(const RenderContext& ctx);
	void DrawUI();

	bool IsActive() const { return isActive_; }
	bool IsExtractionVisible() const { return isExtractionVisible_; }
	Vector3 GetHelicopterPosition() const { return heliPos_; }
	void Skip();

private:
	void UpdateHelicopterMotion(float deltaTime);
	void UpdateAct1ApproachMotion();
	void UpdateAct2CabinPrepMotion();
	void UpdateAct3FastRopeMotion(float deltaTime);
	void UpdateAct4LandingClearMotion();
	void UpdateAct5BreakawayMotion();
	void EmitLandingImpactEffects();
	void EmitWeaponCockEffects();
	void UpdateCamera(float deltaTime);
	void UpdateModelTransforms(float deltaTime);
	void UpdateParticles(float deltaTime);
	void UpdateHelicopterCommonParts(float blurOffset, bool showShadow, float shadowAlphaScale);
	void EmitHelicopterFlightParticles(float deltaTime, bool isHighG, bool isFullThrottle, float washHeightLimit);
	AppParticleManager* GetSmokeMgr() const;
	void FinishInsertion();

private:
	GamePlayScene* scene_ = nullptr;
	bool isActive_ = true;
	float timer_ = 0.0f;

	// 映画・アニメ的 5幕シネマティックタイムライン定数（タクティカル降下＆クリアリング）
	static constexpr float kCut1Duration = 3.40f; // Act 1: 【白昼の超低空急襲・ロングレンジアプローチ＆フレア制動】(0.0s - 3.40s)
	static constexpr float kCut2Duration = 5.60f; // Act 2: 【ドア前出撃準備・RED/GREEN LIGHT合図のタメ・飛び移り】(3.40s - 5.60s)
	static constexpr float kCut3Duration = 7.10f; // Act 3: 【緩急ファストロープ降下・急加速＆地上直前コンバットブレーキ】(5.60s - 7.10s)
	static constexpr float kCut4Duration = 8.80f; // Act 4: 【着地激突・即座の左右セクター360クリアリング＆コッキング】(7.10s - 8.80s)
	static constexpr float kDuration     = 9.70f; // Act 5: 【ヘリ急上昇離脱＆シームレスTPSドリーアウト】(8.80s - 9.70s)

	// 3Dモデルコンポーネント（機体・キャビン内装・スライドドア・多重ブラーローター・ロープ・地上影）
	std::unique_ptr<Object3d> heliBody_;
	std::unique_ptr<Object3d> heliCabinInterior_;
	std::unique_ptr<Object3d> heliCargoDoor_;
	std::unique_ptr<Object3d> heliMainRotor_;
	std::unique_ptr<Object3d> heliMainRotorBlur1_;
	std::unique_ptr<Object3d> heliMainRotorBlur2_;
	std::unique_ptr<Object3d> heliTailRotor_;
	std::unique_ptr<Object3d> fastRope_;
	std::unique_ptr<Object3d> heliShadow_;

	// ヘリの現在トランスフォーム
	static constexpr Vector3 kInitialHeliPos{ -45.0f, 32.0f, -80.0f };
	static constexpr Vector3 kInitialHeliRot{ -0.15f, 0.08f, -0.32f };
	Vector3 heliPos_ = kInitialHeliPos;
	Vector3 heliRot_ = kInitialHeliRot;
	float mainRotorAngle_ = 0.0f;
	float tailRotorAngle_ = 0.0f;
	float doorSlideProgress_ = 1.0f; // 1.0 = 閉鎖, 0.0 = 後方スライド全開
	float ropeSwayAngleX_ = 0.0f;
	float ropeSwayAngleZ_ = 0.0f;

	// プレイヤー（アヒル兵士）の演出トランスフォーム
	Vector3 duckPos_{ 0.0f, 0.0f, 0.0f };
	Vector3 duckScale_{ 1.0f, 1.0f, 1.0f };
	Vector3 duckRot_{ 0.0f, 0.0f, 0.0f };

	// アニメ演出・カット管理変数
	int currentCut_ = 1;
	int prevCut_ = 1;
	float cutFlashAlpha_ = 0.0f;     // カット切り替え時の微細な光学シャッターフラッシュ
	float impactFlashAlpha_ = 0.0f;  // 着地激突インパクトフレーム

	// 脱出回収ヘリ（Dustoff-1）進行管理
	bool isExtractionVisible_ = false;
	float extractionApproachTimer_ = 0.0f;
	float pickupDescentBlend_ = 0.0f;
	bool clearQuackPlayed_ = false;

	// タイマー＆イベントフラグ
	float downwashDustTimer_ = 0.0f;
	float ropeSparkTimer_ = 0.0f;
	float rotorVortexTimer_ = 0.0f;
	float engineExhaustTimer_ = 0.0f;
	float beaconTimer_ = 0.0f;
	float tailRotorVortexTimer_ = 0.0f;
	bool landed_ = false;
	bool headSnapEmitted_ = false;
	bool eyeGlintEmitted_ = false;
	bool featherShakeEmitted_ = false;
	bool footstepDust1_ = false;
	bool footstepDust2_ = false;
	bool cockSparkEmitted_ = false;
	bool duckVoiced_ = false;
	bool wasSkipped_ = false;
	bool skipInputArmed_ = false;
	bool doorOpenSoundPlayed_ = false;
	bool doorCloseSoundPlayed_ = false;
	bool greenLightBeepPlayed_ = false;
	bool jumpSoundPlayed_ = false;
	bool ropeSlideSoundPlayed_ = false;
	bool extractionDoorSoundPlayed_ = false;
	TacticalAudio::LoopHandle rotorLoop_;

	// テクスチャインデックス
	uint32_t heliTexIndex_ = UINT32_MAX;
	uint32_t whiteTexIndex_ = UINT32_MAX;
	uint32_t spotTexIndex_ = UINT32_MAX;
	uint32_t leafTexIndex_ = UINT32_MAX;

	// サウンドID
	int duckSoundId_ = -1;
};
