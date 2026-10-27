#pragma once

#include "BaseScene.h"
#include "../GameScene/RaidStats.h"
#include "Object3d.h"
#include "Camera.h"
#include <string>
#include <memory>
#include <vector>

class KeyInput;
struct SceneRenderRequests;

/// <summary>
/// ゲームオーバーシーン (GameOverScene)
///
/// タイトル画面同様の重厚で洗練された【3Dミニチュア・ジオラマ】による本格敗北シネマティック。
/// 
/// - 本作の3Dアヒル兵士モデル（player.obj / duck.png）
/// - 本作の3D救難ヘリコプター（helicopter_body.obj / helicopter_rotor.obj）
/// - 3Dステージ小道具（土嚢バリケード、軍用木箱、ドラム缶、針葉樹、リアルな地面）
/// - 2羽の3D衛生兵アヒルが担架で3D主役アヒルをヘリへ向かってよちよち搬送
/// - [SPACE] でヘリのキャビンドアへ駆け寄り、担架を持ち上げて積み込み！
/// - ローターが猛烈に回転加速し、ヘリが機首を前傾させて夕空へ飛び立つ大迫力の離陸シネマティック
/// - 画面上には洗練されたミリタリー見出しと、文字化けのない美しい戦績レポートカードをオーバーレイ
/// </summary>
class GameOverScene : public BaseScene
{
public:
	void InitializeScene() override;
	void Finalize() override;
	void Update() override;
	void Draw(SceneRenderRequests& renderRequests) override;

	const char* GetSceneType() const { return "GAMEOVER"; }

private:
	enum class State
	{
		Approaching, // 搬送中：担架チームがヘリに向かって自動歩行搬送（約2.5秒）
		Loading,     // 収容中：ヘリのカーゴドアへ担架を持ち上げてスムーズに機内収容（約1.4秒）
		Ready,       // 収容完了・離陸待機：アヒル全員搭乗完了、ローター待機回転でSPACE離陸待ち
		Takeoff,     // 離陸中：ローター急加速、ヘリが夕空へ浮上し離陸・再出撃！
	};

	void InitializeDiorama();
	void UpdateDiorama(float deltaTime);
	void DrawScreen();

private:
	KeyInput* input_ = nullptr;
	RaidStats raidStats_{};

	State state_ = State::Approaching;
	float timer_ = 0.0f;       // シーン開始からの経過時間
	float actionTimer_ = 0.0f; // 各ステート内の経過時間
	bool transitionRequested_ = false;
	std::string nextScene_ = "GAMEPLAY";
	int duckSeSoundId_ = -1;

	// 演出時間定数（秒）
	static constexpr float kEnterFadeDuration = 0.40f;
	static constexpr float kApproachDuration  = 2.50f; // 自動歩行搬送時間
	static constexpr float kLoadDuration      = 1.30f; // ヘリ機内収容時間
	static constexpr float kTakeoffDuration   = 2.20f; // 離陸・上昇・フェード時間
	static constexpr float kDoorCloseDuration = 0.65f; // SPACE押下後のスライドドア密閉時間

	// -------------------------------------------------------------------------
	// 3D ジオラマオブジェクト
	// -------------------------------------------------------------------------

	// 3D 救難ヘリコプター（右奥LZに駐機、カーゴドア(+X)を担架チーム(-X)および手前(-Z)へ向ける）
	std::unique_ptr<Object3d> heliBody_;
	std::unique_ptr<Object3d> heliMainRotor_;
	std::unique_ptr<Object3d> heliMainRotorBlur1_; // 緊急離陸時の高回転モーションブラー残像ブレード1
	std::unique_ptr<Object3d> heliMainRotorBlur2_; // 緊急離陸時の高回転モーションブラー残像ブレード2
	std::unique_ptr<Object3d> heliTailRotor_;
	std::unique_ptr<Object3d> heliCargoDoor_; // スライド式大型カーゴドア（SPACE押下で自動密閉）
	float doorSlideProgress_ = 0.0f;          // 0.0f = 全開（担架搬送・搭乗中）, 1.0f = 密閉完了
	Vector3 heliPos_{ 3.2f, 0.435f, 2.2f };
	Vector3 heliRot_{ 0.0f, 2.65f, 0.0f }; // 152度回転：開口カーゴドアが手前左（担架側）を向き、機首は右奥へ逃げる
	float mainRotorAngle_ = 0.0f;
	float tailRotorAngle_ = 0.0f;

	// 3D 担架チーム
	std::unique_ptr<Object3d> duckRearMedic_;
	std::unique_ptr<Object3d> duckFrontMedic_;
	std::unique_ptr<Object3d> duckWounded_;
	std::unique_ptr<Object3d> stretcherBed_;
	Vector3 teamPos_{ -4.2f, 0.0f, 1.4f };
	Vector3 teamStartPos_{ -4.2f, 0.0f, 1.4f };
	float walkPhase_ = 0.0f;
	float stretcherLiftY_ = 0.0f;
	float stretcherSlideX_ = 0.0f;
	bool ducksLoaded_ = false;

	// 3D 地形 ＆ ステージ小道具
	std::unique_ptr<Object3d> groundModel_;
	std::unique_ptr<Object3d> helipadModel_;
	struct PropItem
	{
		std::unique_ptr<Object3d> model;
		uint32_t texIndex = UINT32_MAX;
	};
	std::vector<PropItem> props_;

	// テクスチャインデックス
	uint32_t heliTexIndex_ = UINT32_MAX;
	uint32_t duckTexIndex_ = UINT32_MAX;       // プレイヤー自アバター（イエローダック＆負傷ガーゼ）
	uint32_t duckMedicTexIndex_ = UINT32_MAX;  // 搬送衛生兵（ホワイトダック＆レッドレスキューベスト）
	uint32_t groundTexIndex_ = UINT32_MAX;
	uint32_t helipadTexIndex_ = UINT32_MAX;
	uint32_t stretcherTexIndex_ = UINT32_MAX;
	uint32_t pineTexIndex_ = UINT32_MAX;
	uint32_t sandbagTexIndex_ = UINT32_MAX;
	uint32_t barrelTexIndex_ = UINT32_MAX;
	uint32_t crateTexIndex_ = UINT32_MAX;
	uint32_t fenceTexIndex_ = UINT32_MAX;
	uint32_t vignetteTexIndex_ = UINT32_MAX;
	uint32_t logoTexIndex_ = UINT32_MAX;     // 本格専用グラフィックタイトルロゴ（gameover_logo.png）
	uint32_t promptTexIndex_ = UINT32_MAX;   // 本格専用グラフィック操作バッジ（gameover_prompt.png）
	uint32_t debriefTexIndex_ = UINT32_MAX;  // 本格専用タクティカル戦績カード（gameover_debrief.png）
};
