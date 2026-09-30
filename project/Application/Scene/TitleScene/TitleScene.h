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

	// 3D ジオラマオブジェクト（川の水面、軍用コンテナ、ドラム缶、土嚢、木箱）
	std::unique_ptr<Object3d> duckModel_;
	std::unique_ptr<Object3d> enemyDuckModel_;   // 背景のパトロール敵アヒル兵士
	std::unique_ptr<Object3d> riverModel_;       // 川の水面プレーン
	std::unique_ptr<Object3d> containerModel_;   // 軍用コンテナ
	std::unique_ptr<Object3d> barrelStackModel_; // 危険物ドラム缶スタック
	std::unique_ptr<Object3d> crateModel1_;      // 軍用木箱（大）
	std::unique_ptr<Object3d> crateModel2_;      // 軍用木箱（小）
	std::unique_ptr<Object3d> sandbagModel_;     // 土嚢バリケード

	uint32_t riverTexIndex_ = UINT32_MAX;
	uint32_t containerTexIndex_ = UINT32_MAX;
	uint32_t barrelTexIndex_ = UINT32_MAX;
	uint32_t crateTexIndex_ = UINT32_MAX;
	uint32_t sandbagTexIndex_ = UINT32_MAX;

	// 3D動的波面メッシュデータ（高密度ゲルストナー波物理シミュレーション）
	static constexpr int kWaterGridCols = 40;
	static constexpr int kWaterGridRows = 26;
	struct WaterVertexBase
	{
		float basePosX = 0.0f;
		float basePosZ = 0.0f;
		float baseU = 0.0f;
		float baseV = 0.0f;
	};
	std::vector<WaterVertexBase> waterBaseVertices_;
	float waveFoamTimer_ = 0.0f;

	// マウスによる動的3D水面凹凸リップル発生源
	struct MouseWaveRippleSource
	{
		Vector3 center;
		float age = 0.0f;
		float maxLife = 1.0f;
		float power = 1.0f;
	};
	std::vector<MouseWaveRippleSource> mouseWaveSources_;

	// ミノフスキー粒子エフェクト（閃光のハサウェイ・キルケーの魔女風 二重螺旋＆浮遊ボケ）
	std::unique_ptr<AppParticleManager> appParticleManager_;
	uint32_t circleTexIndex_ = UINT32_MAX;
	uint32_t starTexIndex_ = UINT32_MAX;
	float minovskyAngle_ = 0.0f;
	float ambientMoteTimer_ = 0.0f;

	// 3D タイトルカメラ制御
	Vector3 cameraTranslate_ = { 0.0f, 0.25f, -7.5f };
	Vector3 cameraRotate_ = { 0.05f, 0.0f, 0.0f };
	float duckScale_ = 0.55f;
	Vector3 duckBasePos_ = { 1.45f, -0.45f, 0.0f };

	// 2D タイトルスプライトUI（ImGuiを完全排除したネイティブ描画）
	std::unique_ptr<SpriteManager> spriteManager_;
	std::vector<std::unique_ptr<Sprite>> sprites_;
	Sprite* vignetteSprite_ = nullptr;
	Sprite* titleLogoSprite_ = nullptr;
	Sprite* pressSpaceSprite_ = nullptr;
	Sprite* fadeOverlaySprite_ = nullptr;

	// マウス水面波紋（川の波・引き波・波紋インタラクション）
	MouseInput mouseInput_;
	Vector2 prevMousePos_{ 0.0f, 0.0f };
	Vector3 prevMouseWorldPos_{ 0.0f, 0.0f, 0.0f };
	bool hasPrevMousePos_ = false;
	float mouseRippleCooldown_ = 0.0f;
	float mouseWaveEmitCooldown_ = 0.0f;

	void InitializeSprites();

	// ゲルストナー波・動的リップル・法線ベクトル解析計算
	struct WaterSurfaceSample
	{
		Vector3 displacement{ 0.0f, 0.0f, 0.0f }; // 水平X/Zの引き寄せ + 鉛直Yの波高
		Vector3 normal{ 0.0f, 1.0f, 0.0f };       // 解析的法線ベクトル（動的ライティング用）
		float foamFactor = 0.0f;                  // 波頭の尖り・白波発生度
	};
	WaterSurfaceSample EvaluateWaterSurface(float x, float z, float time) const;
};
