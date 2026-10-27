#pragma once
#include <memory>
#include <vector>
#include <string>
#include "Vector.h"
#include "Matrix4x4.h"
#include "RenderContext.h"
#include "Object3d.h"

class GamePlayScene;

/// <summary>
/// 軍用強襲ヘリ低空急襲・ファストロープ降下＆ミリタリーHUDブート演出コントローラー
/// (HelicopterInsertionController)
/// 
/// 1. 【ヘリ低空飛来＆サーチライト走査】 (0.0s 〜 1.6s):
///    夜間上空から軍用ヘリが轟音飛来。地面を強烈なサーチライトとローター下降風圧が薙ぎ払う。
/// 2. 【ホバリング＆ファストロープ降下】 (1.6s 〜 3.0s):
///    ヘリが定点ホバリング。キャビンから太いロープが投下され、アヒル兵士が高速滑空ラペリング。
/// 3. 【着地インパクト＆戦闘態勢コッキング】 (3.0s 〜 4.0s):
///    地面激突寸前に着地！衝撃波リング＆砂塵バースト。ロープを離し武器をチャキッと構える。
/// 4. 【ヘリ急上昇離脱＆シームレスTPS合流】 (4.0s 〜 5.2s):
///    ヘリが機首を下げて北空へ急加速離脱。シネマスコープが解放されシームレスにプレイ開始！
/// </summary>
class HelicopterInsertionController
{
public:
	explicit HelicopterInsertionController(GamePlayScene* scene);
	~HelicopterInsertionController();

	void Initialize();
	void Finalize();
	void Update(float deltaTime);
	void Draw3D(const RenderContext& ctx);
	void DrawUI();

	bool IsActive() const { return isActive_; }
	void Skip();

private:
	void UpdateHelicopterMotion(float deltaTime);
	void UpdateCamera(float deltaTime);
	void UpdateModelTransforms(float deltaTime);
	void UpdateParticles(float deltaTime);
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

	// 3Dモデルコンポーネント
	std::unique_ptr<Object3d> heliBody_;
	std::unique_ptr<Object3d> heliMainRotor_;
	std::unique_ptr<Object3d> heliTailRotor_;
	std::unique_ptr<Object3d> fastRope_;
	std::unique_ptr<Object3d> heliShadow_;

	// ヘリの現在トランスフォーム
	Vector3 heliPos_{ -45.0f, 32.0f, -80.0f };
	Vector3 heliRot_{ -0.15f, 0.08f, -0.32f };
	float mainRotorAngle_ = 0.0f;
	float tailRotorAngle_ = 0.0f;

	// プレイヤー（アヒル兵士）の演出トランスフォーム
	Vector3 duckPos_{ 0.0f, 0.0f, 0.0f };
	Vector3 duckScale_{ 1.0f, 1.0f, 1.0f };
	Vector3 duckRot_{ 0.0f, 0.0f, 0.0f };

	// アニメ演出・カット管理変数
	int currentCut_ = 1;
	int prevCut_ = 1;
	float cutFlashAlpha_ = 0.0f;     // カット切り替え時の瞬間ホワイトフラッシュ
	float impactFlashAlpha_ = 0.0f;  // 着地激突インパクトフレーム

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

	// テクスチャインデックス
	uint32_t heliTexIndex_ = UINT32_MAX;
	uint32_t whiteTexIndex_ = UINT32_MAX;
	uint32_t spotTexIndex_ = UINT32_MAX;
	uint32_t leafTexIndex_ = UINT32_MAX;

	// サウンドID
	int duckSoundId_ = -1;
};
