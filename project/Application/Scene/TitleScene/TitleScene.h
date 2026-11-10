#pragma once

#include "BaseScene.h"
#include "Object3d.h"
#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"
#include <memory>
#include <vector>
#include <string>
#include <chrono>

class DirectXCom;
class KeyInput;
class Camera;
class AppParticleManager;
class Sprite;
class SpriteManager;
struct SceneRenderRequests;

class TitleScene : public BaseScene
{
public:
	void InitializeScene() override;
	void Finalize() override;
	void Update() override;
	void Draw(SceneRenderRequests& renderRequests) override;

	const char* GetSceneType() const { return "TITLE"; }

private:
	KeyInput* input_ = nullptr;
	std::chrono::steady_clock::time_point lastTime_;
	float bgTimer_ = 0.0f;
	float startTransitionTimer_ = 0.0f;
	bool isStarting_ = false;

	// 3D ジオラマオブジェクト (川の水面、軍用コンテナ、ドラム缶、土嚢、木箱) 
	std::unique_ptr<Object3d> duckModel_;
	std::unique_ptr<Object3d> riverModel_;   // 川の水面プレーン 
	std::unique_ptr<Object3d> crateModel1_;  // 軍用木箱 (大) 
	std::unique_ptr<Object3d> crateModel2_;  // 軍用木箱 (小) 
	std::unique_ptr<Object3d> sandbagModel_; // 土嚢バリケード 

	uint32_t riverTexIndex_ = UINT32_MAX;
	uint32_t containerTexIndex_ = UINT32_MAX;
	uint32_t barrelTexIndex_ = UINT32_MAX;
	uint32_t crateTexIndex_ = UINT32_MAX;
	uint32_t sandbagTexIndex_ = UINT32_MAX;

	// 3D動的波面メッシュデータ (高密度ゲルストナー波物理シミュレーション) 
	static constexpr int kWaterGridCols = 96;
	static constexpr int kWaterGridRows = 26;
	struct WaterVertexBase
	{
		float basePosX = 0.0f;
		float basePosZ = 0.0f;
		float baseU = 0.0f;
		float baseV = 0.0f;
	};
	std::vector<WaterVertexBase> waterBaseVertices_;

	// マウスによる動的3D水面凹凸リップル発生源 
	struct MouseWaveRippleSource
	{
		Vector3 center;
		float age = 0.0f;
		float maxLife = 1.0f;
		float power = 1.0f;
	};
	std::vector<MouseWaveRippleSource> mouseWaveSources_;

	// パーティクルエフェクト管理 
	std::unique_ptr<AppParticleManager> appParticleManager_;
	uint32_t circleTexIndex_ = UINT32_MAX;
	uint32_t starTexIndex_ = UINT32_MAX;

	// 3D タイトルカメラ制御 
	static constexpr Vector3 kDefaultCameraTranslate = { 0.0f, 0.25f, -7.5f };
	static constexpr Vector3 kDefaultCameraRotate = { 0.05f, 0.0f, 0.0f };
	static constexpr float kDefaultDuckScale = 0.55f;
	static constexpr Vector3 kDefaultDuckBasePos = { 1.45f, -0.45f, 0.0f };

	Vector3 cameraTranslate_ = kDefaultCameraTranslate;
	Vector3 cameraRotate_ = kDefaultCameraRotate;
	float duckScale_ = kDefaultDuckScale;
	Vector3 duckBasePos_ = kDefaultDuckBasePos;

	// 2D タイトルスプライトUI (ImGuiを完全排除したネイティブ描画) 
	std::unique_ptr<SpriteManager> spriteManager_;
	std::vector<std::unique_ptr<Sprite>> sprites_;
	Sprite* vignetteSprite_ = nullptr;
	Sprite* titleLogoSprite_ = nullptr;
	Sprite* pressSpaceSprite_ = nullptr;
	Sprite* fadeOverlaySprite_ = nullptr;

	// マウス水面波紋 (川の波・引き波・波紋インタラクション) 
	MouseInput mouseInput_;
	Vector3 prevMouseWorldPos_{ 0.0f, 0.0f, 0.0f };
	bool hasPrevMousePos_ = false;
	float mouseRippleCooldown_ = 0.0f;
	float mouseWaveEmitCooldown_ = 0.0f;

	void InitializeAudioAndInput();
	void InitializeParticlesAndCamera();
	void InitializeHeroAndRiver();
	void InitializeTerrain();
	void InitializePropsAndEnemies();
	void InitializeShadowDecals();
	void InitializeSceneryAndPaths();
	void InitializeFallingLeaves();
	void InitializeSprites();

	void UpdateStartAndTransition(float deltaTime);
	void UpdateCameraAndWaterRay(float idleBlend, float& outNormX, Vector3& outWaterHit, bool& outHoveringRiver);
	void UpdateMouseRiverSpray(float deltaTime, const Vector3& mouseWaterHit, bool isHoveringRiver);
	void UpdateLightingAndHero(float deltaTime, float idleBlend, float normX, bool isHoveringRiver);
	void UpdateEnemySquad();
	void UpdateDynamicMeshes();
	void UpdateEnvironmentParticles(float deltaTime);
	void UpdateSpritesUI();

	// ゲルストナー波・動的リップル・法線ベクトル解析計算 
	struct WaterSurfaceSample
	{
		Vector3 displacement{ 0.0f, 0.0f, 0.0f }; // 水平X/Zの引き寄せ + 鉛直Yの波高 
		Vector3 normal{ 0.0f, 1.0f, 0.0f };       // 解析的法線ベクトル (動的ライティング用) 
		float foamFactor = 0.0f;                  // 波頭の尖り・白波発生度 
	};
	WaterSurfaceSample EvaluateWaterSurface(float x, float z, float time) const;
	float SampleRiverSurfaceY(float x, float z, float offsetY = 0.0f) const;
};
