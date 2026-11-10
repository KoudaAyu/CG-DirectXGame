#pragma once
#include <memory>
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include "Vector.h"
#include "Matrix4x4.h"
#include "RenderContext.h"
#include "Object3d.h"

class GamePlayScene;

/// <summary>
/// 軍用シネマティック・ゲームオーバー演出コントローラー (GameOverSequenceController)
///
/// ステージ上のオブジェクトを一切消去せず（不自然なポップ消滅ゼロ）、
/// 狭い遮蔽物密集地での戦死でも「埋まりゼロ・視界遮蔽ゼロ・極端なドアップゼロ」を保証しつつ、
/// 最終シーン（GameOverScene の救難ヘリ担架収容ジオラマ）へ一本の連続した救出劇として直結する3幕構成：
///
/// 1. 【Act 1: 致命被弾バレットタイム ＆ 安全転倒 ＆ 赤色救難フレア発煙】 (0.00s 〜 0.90s):
///    全身OBB押し出し判定により木箱やコンテナへ一切埋まらず自然に崩れ落ち、
///    360度クリア視線スキャンで選ばれた俯瞰タクティカルアングルへ滑らかにドリー。
///    脇に落ちた AN-M18 救難スモーク発煙筒から真紅の誘導煙が空高く立ち昇る。
///
/// 2. 【Act 2: 3D救難ヘリ（MEDEVAC）上空急行・フレア制動・カーゴドア開放】 (0.90s 〜 2.30s):
///    赤色スモークを目印に、GameOverScene と同一機体の 3D 救難ヘリが上空へ飛来して制動ホバリング。
///    ダウンウォッシュの砂塵とハロゲン救難投光器が倒れたアヒルを照らし出し、
///    開放された側面カーゴドアから衛生兵アヒル2羽と担架・ホイストロープが展開される。
///
/// 3. 【Act 3: 救難キャビンへのマッチカット寄り → 重装甲シャッター遷移】 (2.30s 〜 3.05s):
///    ホバリングする救難ヘリの開放キャビンへカメラが滑らかにクレーンアップし、
///    DeathTransition（装甲シャッター）を経て、同アングルで前線救護ヘリパッドへ着陸した
///    GameOverScene の担架収容シーンへ違和感なくシームレスに接続する。
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
	/// 3D描画（吹き飛ぶライフル・救難フレア・飛来する3D救難ヘリ＆衛生兵クルー）
	/// </summary>
	void Draw3D(const RenderContext& ctx);

	/// <summary>
	/// 映画風シネマスコープ黒帯・救難ヘリ交信字幕の描画
	/// </summary>
	void DrawUI();

	bool IsActive() const { return isActive_; }

	/// <summary>
	/// 即座にスキップしてゲームオーバー画面へ移行
	/// </summary>
	void Skip();

private:
	struct ObstacleBox
	{
		Vector3 center{ 0.0f, 0.0f, 0.0f };
		Vector3 size{ 1.0f, 1.0f, 1.0f };
		Vector3 rotation{ 0.0f, 0.0f, 0.0f };
		bool isBoundary = false; // 川や外周の進入不可バリア
		bool isCanopy = false;   // 針葉樹の上部枝葉ボリューム（視線・ヘリ高度判定用）
	};

	void CollectNearbyObstacles();
	void PlanSafeDuckFallPose(float defaultKillerYaw);
	void FinalizeDuckPoseForCamera();
	void ResolveDuckPoseCollision(Vector3& ioPos, const Vector3& rot, const Vector3& scale) const;
	void ResolveHorizontalPointClearance(Vector3& ioPos, float radius, float testHeightOffset) const;
	void SelectUnobstructedCameraRig();
	bool IsCameraSightClear(const Vector3& eye, const std::vector<Vector3>& targets) const;
	void PushCameraOutOfObstacles(Vector3& ioCamPos) const;

	void SuppressGameplayHUD();
	void RestoreGameplayHUD();
	void UpdateMotion(float simDelta);
	void UpdateWeaponMotion(float simDelta);
	void UpdateFlareAndAtmosphere(float simDelta, float realDeltaTime);
	void UpdateMedevacHelicopter(float realDeltaTime);
	void UpdateCamera(float realDeltaTime);
	void RestoreSceneLighting();
	void FinishSequence();

	Vector3 GetDuckVisualCenter() const;
	Vector3 GetFinalFallenFocus() const;
	Vector3 LocalToHeliWorld(const Vector3& localUnscaledOffset) const;
	void InitializeVolumetricPipeline();
	void DrawVolumetricObject(Object3d* obj, const RenderContext& ctx);

	static void BuildRifleModel(Object3d::ModelData& data);
	static void BuildFlareCanisterModel(Object3d::ModelData& data);
	static void BuildFlareGlowCapModel(Object3d::ModelData& data);
	static void BuildHelicopterDoorModel(Object3d::ModelData& data);
	static void BuildStretcherModel(Object3d::ModelData& data);
	static void BuildShadowDiscModel(Object3d::ModelData& data, float rx, float rz);
	static void BuildVolumetricConeModel(Object3d::ModelData& data, float apexRadius, float baseRadius, int radialSegs, int heightRings);

private:
	GamePlayScene* scene_ = nullptr;
	bool isActive_ = false;
	float timer_ = 0.0f;

	// タイムライン定数（テンポ良く再出撃できるよう凝縮した3幕構成：計 2.68秒）
	static constexpr float kAct1Duration = 0.84f; // Act 1: 致命被弾インパクト・崩れ落ち・赤色フレア点火
	static constexpr float kAct2Duration = 1.98f; // Act 2: 3D救難ヘリ上空飛来・制動ホバリング・ドア開放
	static constexpr float kDuration     = 2.68f; // Act 3: 救難キャビン寄り → 重装甲シャッター閉鎖 (2.68s)

	static constexpr float kHeliScale = 0.65f;    // GameOverScene と完全同一の機体スケール

	// 近傍ステージ障害物の実寸OBBキャッシュ（オブジェクトは一切消さず、衝突回避と視線探索に使用）
	std::vector<ObstacleBox> nearbyObstacles_;

	// 死亡時ステータス ＆ 3種の転倒バリエーション
	Vector3 deathPos_{ 0.0f, 0.0f, 0.0f };
	Vector3 safeLandingPos_{ 0.0f, 0.0f, 0.0f };
	Vector3 killerPos_{ 0.0f, 0.0f, 0.0f };
	Vector3 flingDir_{ 0.0f, 0.0f, -1.0f };
	std::string killerName_;
	std::string causeOfDeath_;
	bool isMIA_ = false;
	float killerDistance_ = 0.0f;
	float initialPlayerYaw_ = 0.0f;
	float fallBaseYaw_ = 0.0f;
	int fallVariant_ = 0; // 0: 3/4仰向け転倒, 1: 被弾スピン横倒れ, 2: 膝つき崩れ落ち
	Vector3 fallFinalRot_{ -1.22f, 0.0f, 0.0f };
	float groundY_ = 0.0f;

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
	bool rifleBounceSparked_ = false;

	// 3D 緊急救難スモークフレア（AN-M18 赤色発煙筒）
	std::unique_ptr<Object3d> flareCanister_;
	std::unique_ptr<Object3d> flareGlowCap_;
	Vector3 flareStartPos_{ 0.0f, 0.0f, 0.0f };
	Vector3 flareTargetPos_{ 0.0f, 0.0f, 0.0f };
	Vector3 flarePos_{ 0.0f, 0.0f, 0.0f };
	float flareYaw_ = 0.0f;
	bool flareActive_ = false;
	float flareSmokeTimer_ = 0.0f;
	float flareSparkTimer_ = 0.0f;

	// 3D 救難ヘリコプター（MEDEVAC HELICOPTER：GameOverScene と同一機体・同一衛生兵クルー）
	std::unique_ptr<Object3d> heliBody_;
	std::unique_ptr<Object3d> heliMainRotor_;
	std::unique_ptr<Object3d> heliMainRotorBlur1_;
	std::unique_ptr<Object3d> heliMainRotorBlur2_;
	std::unique_ptr<Object3d> heliTailRotor_;
	std::unique_ptr<Object3d> heliCargoDoor_;
	std::unique_ptr<Object3d> heliFastRope_;
	std::unique_ptr<Object3d> heliShadow_;
	std::unique_ptr<Object3d> searchlightSpot_;
	std::unique_ptr<Object3d> volumetricOuterBeam_;
	std::unique_ptr<Object3d> volumetricInnerBeam_;
	std::unique_ptr<Object3d> volumetricCabinBeam_;
	std::unique_ptr<Object3d> impactShockDisc_;
	std::unique_ptr<Object3d> beaconWaveDisc1_;
	std::unique_ptr<Object3d> beaconWaveDisc2_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> volumetricAdditivePso_;
	std::unique_ptr<Object3d> cabinFrontMedic_;
	std::unique_ptr<Object3d> cabinRearMedic_;
	std::unique_ptr<Object3d> cabinStretcher_;

	bool heliVisible_ = false;
	bool shockDiscActive_ = false;
	bool beaconWave1Active_ = false;
	bool beaconWave2Active_ = false;
	Vector3 heliStartPos_{ 0.0f, 18.0f, 0.0f };
	Vector3 heliHoverPos_{ 0.0f, 5.2f, 0.0f };
	Vector3 heliPos_{ 0.0f, 18.0f, 0.0f };
	Vector3 heliRot_{ 0.0f, 0.0f, 0.0f };
	Vector3 searchlightTurretPos_{ 0.0f, 5.0f, 0.0f };
	Vector3 searchlightGroundPos_{ 0.0f, 0.0f, 0.0f };
	float searchlightIntensity_ = 0.0f;
	float heliBaseYaw_ = 2.65f;
	float heliHoverY_ = 5.2f;
	float mainRotorAngle_ = 0.0f;
	float tailRotorAngle_ = 0.0f;
	float doorOpenProgress_ = 0.0f; // 0.0 = 閉, 1.0 = 全開
	float downwashTimer_ = 0.0f;
	float rotorVaporTimer_ = 0.0f;
	float exhaustTimer_ = 0.0f;
	float strobeTimer_ = 0.0f;
	float tyndallMoteTimer_ = 0.0f;

	// 障害物ゼロ保証の俯瞰タクティカルカメラリグ
	Vector3 initialCamPos_{ 0.0f, 0.0f, 0.0f };
	Vector3 initialCamRot_{ 0.0f, 0.0f, 0.0f };
	float initialCamFov_ = 0.45f;
	Vector3 camAzimuthDir_{ 0.0f, 0.0f, -1.0f }; // 注視点からカメラへの水平単位ベクトル
	float cameraSide_ = -1.0f;
	float rigHorizDistAct1_ = 5.3f;
	float rigHeightAct1_ = 4.7f;
	float rigHorizDistAct2_ = 6.7f;
	float rigHeightAct2_ = 5.0f;

	// シーン照明・プレイ中UIフラグの保存＆退避用
	bool hasSavedLighting_ = false;
	Object3d::DirectionalLight savedDirLight_{};
	Object3d::PointLight savedPointLight_{};
	bool hasSavedHudFlags_ = false;
	bool savedShowPerfTracker_ = false;
	bool savedShowDebugGizmos_ = false;

	// 特撮エフェクト用フラグ・タイマー
	bool groundImpactTriggered_ = false;
	bool alarmTriggered_ = false;
	bool transitionTriggered_ = false;
	float impactFlashAlpha_ = 0.0f;
	float darkVignetteAlpha_ = 0.0f;
	float gunSmokeTimer_ = 0.0f;
	float featherTimer_ = 0.0f;
	float skidDustTimer_ = 0.0f;

	// オーディオID
	int32_t alarmSoundId_ = -1;
	int32_t duckSoundId_ = -1;

	// テクスチャインデックス
	uint32_t circle2TextureIndex_ = UINT32_MAX;
	uint32_t whitePuffTextureIndex_ = UINT32_MAX;
	uint32_t smokeDarkTextureIndex_ = UINT32_MAX;
	uint32_t featherTextureIndex_ = UINT32_MAX;
	uint32_t canisterTexIndex_ = UINT32_MAX;
	uint32_t whiteTexIndex_ = UINT32_MAX;
	uint32_t woundedDuckTexIndex_ = UINT32_MAX;
	uint32_t medicDuckTexIndex_ = UINT32_MAX;
	uint32_t normalTexIndex_ = UINT32_MAX;
	uint32_t heliTexIndex_ = UINT32_MAX;
	uint32_t leafTextureIndex_ = UINT32_MAX;
};
