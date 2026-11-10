#pragma once
#include <memory>
#include <string>
#include "Vector.h"
#include "Object3d.h"
#include "RenderContext.h"

class GamePlayScene;

/// <summary>
/// ステージ環境・脱出進行管理システム (EnvironmentSystem)
/// 脱出ヘリパッドの判定、カウントダウン、バリア演出、祝砲Confetti、
/// ワールド空間接地影＆腐植土レイヤー・多層3D河川（水流波・白泡・橋脚引き波・苔岩流木護岸）・未舗装ダート道・風揺れ3D草木、
/// および川の水流さざ波・水滴・川霧等の環境エフェクトを統括します。
/// </summary>
class EnvironmentSystem
{
public:
	/// <summary>
	/// コンストラクタ
	/// </summary>
	/// <param name="scene">対象となるGamePlayScene</param>
	explicit EnvironmentSystem(GamePlayScene* scene);
	~EnvironmentSystem() = default;

	/// <summary>
	/// 初期化処理
	/// </summary>
	void Initialize();

	/// <summary>
	/// 3D自然レイヤーのリソース解放処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 毎フレームの環境・脱出演出の更新
	/// </summary>
	/// <param name="deltaTime">フレーム経過時間</param>
	void Update(float deltaTime);

	/// <summary>
	/// 3D自然レイヤー（流れる川面・白泡・橋脚引き波・風揺れ3D芝生）のアニメーションとWVP同期
	/// </summary>
	/// <param name="deltaTime">フレーム経過時間</param>
	void UpdateNatureAnimation(float deltaTime);

	/// <summary>
	/// 広域外周芝生フィールドの先行描画（樹木や構造物のαエッジがSkyboxと合成されるのを防ぐため最背面に描画）
	/// </summary>
	/// <param name="ctx">描画コンテキスト</param>
	void DrawOuterGround(const RenderContext& ctx);

	/// <summary>
	/// 3D自然レイヤー（接地影＆腐植土・ダート道・苔岩護岸・多層3D水面・風揺れ3D草木）の統合描画
	/// </summary>
	/// <param name="ctx">描画コンテキスト</param>
	void DrawNatureLayers(const RenderContext& ctx);

	/// <summary>
	/// 脱出目標と生還判定・カウントダウン・祝砲演出の更新
	/// </summary>
	/// <param name="deltaTime">フレーム経過時間</param>
	void UpdateExtractionGoal(float deltaTime);

	/// <summary>
	/// 脱出バリア球の回転・パルス鼓動・非表示制御
	/// </summary>
	void UpdateBarrier();

	/// <summary>
	/// 川の水流さざ波および水滴パーティクルの周期的放出
	/// </summary>
	/// <param name="deltaTime">フレーム経過時間</param>
	void UpdateRiverEffects(float deltaTime);

	/// <summary>
	/// ヒットエフェクトの更新
	/// </summary>
	/// <param name="deltaTime">フレーム経過時間</param>
	void UpdateHitEffect(float deltaTime);

private:
	void InitializeNatureLayers();
	std::unique_ptr<Object3d> CreateNatureLayerObject(const std::string& objFilename, const std::string& texFilename);

	GamePlayScene* scene_ = nullptr;

	// タイマー類（GamePlayScene から移譲・カプセル化）
	float riverWaveTimer_ = 0.0f;
	float riverSplashTimer_ = 0.0f;
	float riverMistTimer_ = 0.0f;
	float barrierTimer_ = 0.0f;
	float natureAnimTime_ = 0.0f;
	int lastExfilCountdownSec_ = -1;

	// リアル自然環境3Dレイヤー（広域外周芝生フィールド・ワールド接地影＆腐植土・ダート道・苔岩川岸・3D波水面・白泡・橋脚引き波・風揺れ立体草木）
	std::unique_ptr<Object3d> outerGroundObj_;
	std::unique_ptr<Object3d> groundRealismObj_;
	std::unique_ptr<Object3d> compoundPathsObj_;
	std::unique_ptr<Object3d> riverBanksObj_;
	std::unique_ptr<Object3d> riverWaterMainObj_;
	std::unique_ptr<Object3d> riverWaterFoamObj_;
	std::unique_ptr<Object3d> riverBridgeWakesObj_;
	std::unique_ptr<Object3d> grassFoliageObjA_;
	std::unique_ptr<Object3d> grassFoliageObjB_;
	std::unique_ptr<Object3d> woodlandUnderstoryObj_;
	std::unique_ptr<Object3d> woodlandSwayObj_;
};
