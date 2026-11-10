#pragma once

#include "BaseScene.h"
#include "../GameScene/RaidStats.h"
#include "Application/Audio/TacticalAudio.h"
#include "Object3d.h"
#include "Vector.h"
#include <array>
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

class KeyInput;
class Camera;
class Object3dCom;
class AppParticleManager;
class TacticalUICanvas;
class TacticalDrawList;
struct SceneRenderRequests;

/// <summary>
/// ヘリコプター撤退・上空帰還クリアシーン (ClearScene)
/// 
/// 任務を達成したオペレーターが、前哨拠点（FOB）と針葉樹林・連峰に囲まれた脱出地点（LZ）へ
/// 飛来した回収ヘリに駆け込み離脱。M18緑発煙筒の多段乱流スモークとローター吹き下ろし砂塵が舞う.
/// リザルトでは3機編隊で山岳渓谷・蛇行河川・積層雲海の上空を巡航する回収ヘリの開放キャビンで
/// 風を受ける生還アヒルを右側に映し、左側には海外AAAシューター調の戦果デブリーフィングUIを展開する.
/// </summary>
class ClearScene : public BaseScene
{
public:
	void InitializeScene() override;
	void Finalize() override;
	void Update() override;
	void Draw(SceneRenderRequests& renderRequests) override;

	const char* GetSceneType() const { return "CLEAR"; }

private:
	enum class State
	{
		Inbound,       // 回収ヘリが上空から進入・旋回降下して着陸 (3.4s)
		RunningToHeli, // カーゴドア開放、オペレーターがヘリへ駆け寄る (1.4s)
		Boarding,      // ヘリ機内へ搭乗 ＆ オープンキャビン維持 (1.2s)
		Takeoff,       // ローター急加速、砂塵と煙を巻き上げてホットゾーンから急上昇離脱 (2.5s)
		Result,        // 編隊上空巡航キャビンショット ＆ ミニマル戦果デブリーフィングUI
	};

	void InitializeDiorama();
	void LoadSceneTextures();
	void InitializeHelicopterModels(Object3dCom* com);
	void InitializeGroundLzProps(Object3dCom* com, const Object3d::ModelData* propMeshes);
	void InitializeAerialCruiseTiles(Object3dCom* com, const Object3d::ModelData* propMeshes);

	void EnterResultState();
	void RequestSceneTransition(const std::string& targetScene);
	void PlayDuckVoice(float volume);
	void HandleResultConfirmAction();
	void UpdateAudioLoops(float deltaTime);

	void UpdateDiorama(float deltaTime);
	void UpdateHelicopterMotion(float deltaTime);
	void UpdateDuckOperator(float deltaTime, const Matrix4x4& heliRotMat);
	void UpdateCinematicCamera();
	void UpdateAerialScrollBelt(float scrollStep, bool updateTileModels);
	void UpdateEscortFormation();
	void UpdateSceneLighting(const Matrix4x4& heliRotMat);

	void EmitGroundLzAtmosphere(float deltaTime, bool includeRotorWash);
	void EmitAerialCruiseAtmosphere(float deltaTime);
	void DrawScreen();
	void DrawCutsceneOverlay(TacticalDrawList* dl, float sw, float sh, float ox, float uiScale);
	void DrawResultDebriefUI(TacticalUICanvas* ui, TacticalDrawList* dl, float sw, float sh, float ox, float oy, float uiScale);

private:
	KeyInput* input_ = nullptr;
	Camera* camera_ = nullptr;
	RaidStats raidStats_{};

	State state_ = State::Inbound;
	float timer_ = 0.0f;       // シーン開始からの総経過時間 (sec)
	float actionTimer_ = 0.0f; // 現在のステート内の経過時間 (sec)
	bool transitionRequested_ = false;
	std::string nextScene_ = "GAMEPLAY";
	int duckSeSoundId_ = -1;

	// 各演出時間・構成数・初期座標定数 (Timing & Initial Pose Constants)
	static constexpr float   kInboundDuration     = 3.40f; // 飛来・バンク旋回・フレア制動・着陸接地時間 (sec)
	static constexpr float   kRunDuration         = 1.40f; // 駆け寄り時間 (sec)
	static constexpr float   kBoardDuration       = 1.20f; // 搭乗時間 (sec)
	static constexpr float   kTakeoffDuration     = 2.50f; // 離陸・上昇時間 (sec)
	static constexpr float   kDoorCloseDuration   = 0.60f; // ドア閉鎖時間 (sec)
	static constexpr size_t  kAerialTileCount     = 3;     // 上空スクロール渓谷タイル数
	static constexpr size_t  kOutpostsPerTile     = 2;     // 1タイル当たりの地上前哨拠点数
	static constexpr size_t  kEscortHeliCount     = 2;     // 護衛編隊ヘリ機数
	static constexpr float   kDefaultEscortScale  = 0.50f; // 護衛ヘリ既定スケール
	static constexpr Vector3 kInitialHeliPos      = { 2.8f, 0.435f, 2.0f };
	static constexpr Vector3 kInitialHeliRot      = { 0.0f, 2.65f, 0.0f };
	static constexpr Vector3 kInitialPlayerPos    = { -2.4f, 0.0f, 0.6f };
	static constexpr Vector3 kInitialFlarePos     = { 0.8f, 0.0f, -0.2f };
	static constexpr Vector3 kInitialFlareRot     = { 0.12f, 0.35f, -0.06f };

	// -------------------------------------------------------------------------
	// 3D ジオラマオブジェクト (Hero Helicopter Dustoff-1)
	// -------------------------------------------------------------------------
	std::unique_ptr<Object3d> heliBody_;
	std::unique_ptr<Object3d> heliCabinInterior_;
	std::unique_ptr<Object3d> heliMainRotor_;
	std::unique_ptr<Object3d> heliMainRotorBlur1_;
	std::unique_ptr<Object3d> heliMainRotorBlur2_;
	std::unique_ptr<Object3d> heliTailRotor_;
	std::unique_ptr<Object3d> heliCargoDoor_;
	float doorSlideProgress_ = 0.0f; // 0.0f = Open, 1.0f = Closed
	Vector3 heliPos_ = kInitialHeliPos;
	Vector3 heliRot_ = kInitialHeliRot;
	float mainRotorAngle_ = 0.0f;
	float tailRotorAngle_ = 0.0f;

	// プレイヤーアヒル ＆ 携行ライフル (Operator & Rifle)
	std::unique_ptr<Object3d> duckPlayer_;
	std::unique_ptr<Object3d> duckRifle_;
	Vector3 playerPos_ = kInitialPlayerPos;
	Vector3 playerStartPos_ = kInitialPlayerPos;
	float runPhase_ = 0.0f;

	// 遠景3層山岳パノラマ (3-Layer Panoramic Mountain Rings)
	std::unique_ptr<Object3d> mountainFar_;
	std::unique_ptr<Object3d> mountainMid_;
	std::unique_ptr<Object3d> mountainNear_;

	// 地上LZ用 3D 地形 ＆ 前哨拠点（FOB）小道具・森林群 (Ground LZ & FOB Props)
	std::unique_ptr<Object3d> groundModel_;
	std::unique_ptr<Object3d> helipadModel_;
	struct PropItem
	{
		std::unique_ptr<Object3d> model;
		uint32_t texIndex = UINT32_MAX;
	};
	std::vector<PropItem> props_;

	// 上空巡航（State::Result）用：スクロール渓谷・蛇行河川・森林・地上拠点 ＆ 護衛編隊ヘリ (Aerial Cruise)
	std::array<std::unique_ptr<Object3d>, kAerialTileCount> aerialValleyTiles_;
	std::array<std::unique_ptr<Object3d>, kAerialTileCount> aerialRiverTiles_;
	std::array<std::unique_ptr<Object3d>, kAerialTileCount> aerialForestTiles_;
	std::array<std::unique_ptr<Object3d>, kAerialTileCount> aerialOutpostTiles_;
	std::array<Vector3, kOutpostsPerTile> aerialOutpostLocalPos_{};
	std::array<Vector3, kAerialTileCount * kOutpostsPerTile> aerialOutpostWorldPos_{};
	float aerialScrollOffset_ = 0.0f;

	struct EscortHeli
	{
		std::unique_ptr<Object3d> body;
		std::unique_ptr<Object3d> mainRotor;
		std::unique_ptr<Object3d> mainRotorBlur;
		std::unique_ptr<Object3d> tailRotor;
		Vector3 worldPos{};
		Vector3 worldRot{};
		float scale = kDefaultEscortScale;
		float rotorPhase = 0.0f;
	};
	std::array<EscortHeli, kEscortHeliCount> escortHelis_;

	// 脱出地点シグナル・発煙筒スモーク缶 ＆ 2系統パーティクル (Signal Flare & Particles)
	std::unique_ptr<Object3d> signalFlareModel_;
	std::unique_ptr<AppParticleManager> smokeParticleManager_;
	std::unique_ptr<AppParticleManager> glintParticleManager_;
	Vector3 signalFlarePos_ = kInitialFlarePos;
	Vector3 signalFlareRot_ = kInitialFlareRot;
	float signalFlareEmitTimer_ = 0.0f;
	float rotorDustEmitTimer_   = 0.0f;
	float turbineExhaustTimer_  = 0.0f;
	float bgPlumeEmitTimer_     = 0.0f;
	float aerialCloudEmitTimer_ = 0.0f;
	float aerialWindEmitTimer_  = 0.0f;

	// テクスチャインデックス (Texture Handles)
	uint32_t heliTexIndex_              = UINT32_MAX;
	uint32_t duckTexIndex_              = UINT32_MAX;
	uint32_t rifleTexIndex_             = UINT32_MAX;
	uint32_t helipadTexIndex_           = UINT32_MAX;
	uint32_t flareTexIndex_             = UINT32_MAX;
	uint32_t wispySmokeTexIndex_        = UINT32_MAX;
	uint32_t smokeTexIndex_             = UINT32_MAX;
	uint32_t circleTexIndex_            = UINT32_MAX;
	uint32_t whiteTexIndex_             = UINT32_MAX;
	uint32_t waterTexIndex_             = UINT32_MAX;
	uint32_t fenceTexIndex_             = UINT32_MAX;
	uint32_t signpostTexIndex_          = UINT32_MAX;
	uint32_t pineTexIndex_              = UINT32_MAX;
	uint32_t sandbagTexIndex_           = UINT32_MAX;
	uint32_t barrelTexIndex_            = UINT32_MAX;
	uint32_t crateTexIndex_             = UINT32_MAX;
	uint32_t groundTexIndex_            = UINT32_MAX;
	uint32_t containerTexIndex_         = UINT32_MAX;
	uint32_t vignetteTexIndex_          = UINT32_MAX;
	uint32_t clearDigitsTexIndex_       = UINT32_MAX;
	uint32_t clearUiAtlasTexIndex_      = UINT32_MAX;
	uint32_t clearCutsceneSubsTexIndex_ = UINT32_MAX;

	// リザルト演出・UI状態 (Result UI State)
	float countUpProgress_   = 0.0f;
	float resultCutFlash_    = 0.0f;
	bool rankRevealed_       = false;
	float nextRaidHoverAnim_ = 0.0f;
	float toTitleHoverAnim_  = 0.0f;

	// オーディオ制御 (Audio Loops & One-Shot Guards)
	TacticalAudio::LoopHandle bgmLoop_{};
	TacticalAudio::LoopHandle rotorLoop_{};
	TacticalAudio::LoopHandle flareLoop_{};
	bool doorOpenPlayed_     = false;
	bool touchdownPlayed_    = false;
	bool boardSoundPlayed_   = false;
	bool takeoffSoundPlayed_ = false;
	int prevRunStep_         = -1;
	bool prevHoverNext_      = false;
	bool prevHoverTitle_     = false;
};
