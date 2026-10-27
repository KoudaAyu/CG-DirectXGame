#pragma once
#include <memory>
#include <string>
#include <vector>
#include "Vector.h"
#include "Matrix4x4.h"
#include "RenderContext.h"
#include "Object3d.h"

class GamePlayScene;

/// <summary>
/// 軍用超シネマティック・ゲームオーバー演出コントローラー (GameOverSequenceController)
/// 
/// スタート演出（HelicopterInsertionController）やタイトル（TitleScene）の3Dジオラマと同格以上の
/// 圧倒的な特撮映画クオリティを誇るゲームオーバーシネマティック。
/// 
/// 1. 【Act 1: 致命弾被弾・宙を舞う吹き飛び＆激突バウンド】 (0.0s 〜 0.85s):
///    被弾の瞬間、衝撃ベクトルでアヒル兵士が後方上空へ放物線を描いて激しく吹き飛ぶ！
///    武器（ライフル）が手から弾き飛ばされ、羽毛が爆散。地面に激突インパクト（白砂塵バースト＋衝撃波リング）。
///    カメラは地面スレスレのローアングルでアヒルの吹き飛びを高速並走追従。
/// 
/// 2. 【Act 2: 激突の白砂塵バースト＆立ち上る硝煙＆キラーパン】 (0.85s 〜 2.10s):
///    地面激突の衝撃波ダストと火花・羽毛が四散。立ち上る硝煙。
///    カメラがゆっくりと斜め上から旋回し、倒れた兵士と遠くの戦場をピント捕捉。
/// 
/// 3. 【Act 3: 救難ヘリ急行無線＆生体フラットライン＆シネマ暗転】 (2.10s 〜 2.85s):
///    心電図モニターがフラットライン化。シネマスコープ黒帯が画面を包み、
///    救難ヘリ（MEDEVAC）の緊急無線が響くなか画面が暗転してゲームオーバー画面へ移行。
/// </summary>
class GameOverSequenceController
{
public:
	explicit GameOverSequenceController(GamePlayScene* scene);
	~GameOverSequenceController();

	void Initialize();
	void Finalize();

	/// <summary>
	/// 死亡シネマティック演出の開始
	/// </summary>
	void Start(const Vector3& deathPos, const Vector3& killerPos, const std::string& killerName, const std::string& causeOfDeath, bool isMIA);

	/// <summary>
	/// 毎フレームの更新（実時間 realDeltaTime で更新）
	/// </summary>
	void Update(float realDeltaTime);

	/// <summary>
	/// 3D描画（吹き飛ぶライフルなど）
	/// </summary>
	void Draw3D(const RenderContext& ctx);

	/// <summary>
	/// 映画風シネマスコープ黒帯・衛星HUD・キラーロックオン・心電図UIの描画
	/// </summary>
	void DrawUI();

	bool IsActive() const { return isActive_; }

	/// <summary>
	/// 即座にスキップしてゲームオーバー画面へ移行
	/// </summary>
	void Skip();

private:
	void UpdateMotion(float realDeltaTime);
	void UpdateCamera(float realDeltaTime);
	void UpdateWeaponMotion(float realDeltaTime);
	void UpdateAtmosphere(float realDeltaTime);
	void UpdateParticles(float realDeltaTime);
	void FinishSequence();

	static bool Project3DTo2D(const Vector3& pos3D, const Matrix4x4& vp, float screenW, float screenH, Vector2& outPos);
	static void BuildRifleModel(Object3d::ModelData& data);
	static void BuildStretcherModel(Object3d::ModelData& data);

private:
	GamePlayScene* scene_ = nullptr;
	bool isActive_ = false;
	float timer_ = 0.0f;

	// 特撮映画調 4幕シネマティックタイムライン定数（GameOverSceneのヘリ搬送に直結）
	static constexpr float kAct1Duration = 0.85f; // Act 1: 致命被弾インパクト＆地面激突 (0.0s - 0.85s)
	static constexpr float kAct2Duration = 1.70f; // Act 2: 倒れた兵士＆緊急救難無線・ヘリ接近 (0.85s - 1.70s)
	static constexpr float kAct3Duration = 2.80f; // Act 3: 衛生兵チームが担架を持って駆けつけ！ (1.70s - 2.80s)
	static constexpr float kDuration     = 2.90f; // Act 4: 軍用メディバック・ヘビーシャッター閉鎖トリガー (2.90s)

	// 死亡時ステータス
	Vector3 deathPos_{ 0.0f, 0.0f, 0.0f };
	Vector3 killerPos_{ 0.0f, 0.0f, 0.0f };
	Vector3 flingDir_{ 0.0f, 0.0f, -1.0f }; // 被弾ノックバック方向
	std::string killerName_;
	std::string causeOfDeath_;
	bool isMIA_ = false;
	float killerDistance_ = 0.0f;
	float killerBearingDeg_ = 0.0f;

	// 3Dアヒル兵士の物理アニメーション用トランスフォーム
	Vector3 duckPos_{ 0.0f, 0.0f, 0.0f };
	Vector3 duckRot_{ 0.0f, 0.0f, 0.0f };
	Vector3 duckScale_{ 1.0f, 1.0f, 1.0f };

	// 3D吹き飛ぶライフルオブジェクト
	std::unique_ptr<Object3d> droppedRifle_;
	Vector3 riflePos_{ 0.0f, 0.0f, 0.0f };
	Vector3 rifleRot_{ 0.0f, 0.0f, 0.0f };
	Vector3 rifleVel_{ 0.0f, 0.0f, 0.0f };
	Vector3 rifleRotVel_{ 0.0f, 0.0f, 0.0f };
	bool rifleGrounded_ = false;

	// 3D 衛生兵レスキューチーム（GameOverSceneのヘリ搬送に直結する救難演出）
	std::unique_ptr<Object3d> medicFront_;
	std::unique_ptr<Object3d> medicRear_;
	std::unique_ptr<Object3d> stretcherBed_;
	Vector3 medicTeamPos_{ 0.0f, 0.0f, 0.0f };
	Vector3 medicTeamStartPos_{ 0.0f, 0.0f, 0.0f };
	Vector3 medicTeamTargetPos_{ 0.0f, 0.0f, 0.0f };
	float medicWalkPhase_ = 0.0f;
	uint32_t medicTexIndex_ = UINT32_MAX;
	uint32_t stretcherTexIndex_ = UINT32_MAX;

	// 地面激突フラグ
	bool groundImpactTriggered_ = false;

	// カメラワーク用トランスフォーム
	Vector3 initialCamPos_{ 0.0f, 0.0f, 0.0f };
	Vector3 initialCamRot_{ 0.0f, 0.0f, 0.0f };
	float initialCamFov_ = 0.45f;

	// 特撮エフェクト用フラグ・タイマー
	float darkVignetteAlpha_ = 0.0f;    // 画面端のシネマティック暗転ヴィネット
	float fadeBlackAlpha_ = 0.0f;       // 終盤のブラックアウト
	float smokeTimer_ = 0.0f;
	float featherTimer_ = 0.0f;
	bool transitionTriggered_ = false;

	// オーディオID
	int32_t alarmSoundId_ = -1;
	int32_t duckSoundId_ = -1;

	// テクスチャインデックス
	uint32_t circle2TextureIndex_ = UINT32_MAX;
	uint32_t leafTextureIndex_ = UINT32_MAX;
	uint32_t vignetteTextureIndex_ = UINT32_MAX;
	uint32_t starburstTextureIndex_ = UINT32_MAX;
	uint32_t smokeDarkTextureIndex_ = UINT32_MAX;
	uint32_t featherTextureIndex_ = UINT32_MAX;

	// 心電図（ECG）波形バッファ
	std::vector<float> ecgHistory_;
	float ecgTimer_ = 0.0f;

	// 負傷テクスチャ & アラーム管理
	uint32_t woundedDuckTexIndex_ = UINT32_MAX;
	uint32_t normalTexIndex_ = UINT32_MAX;
	uint32_t originalPlayerTexIndex_ = UINT32_MAX;
	bool alarmTriggered_ = false;
};
