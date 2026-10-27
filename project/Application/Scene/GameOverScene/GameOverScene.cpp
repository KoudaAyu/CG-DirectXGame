#include "GameOverScene.h"
#include "KeyInput.h"
#include "DirectXCom.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Object3dCom.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Graphics/Light/Light.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"
#include "Baziru3_Engine/Framework/Audio/AudioManager.h"
#include "Application/Transition/StingerTransition.h"
#include "Application/Transition/DeathTransition.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <vector>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace
{
	constexpr float kPi = 3.14159265358979323846f;

	float EaseInQuad(float t) { return t * t; }
	float EaseOutQuad(float t) { return t * (2.0f - t); }
	float EaseInOutCubic(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return (t < 0.5f) ? (4.0f * t * t * t) : (1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f);
	}

	void DrawModel(Object3dCom* com, Object3d* model, const RenderContext& base, uint32_t texIndex)
	{
		if (!com || !model) return;
		RenderContext ctx = base;
		if (texIndex != UINT32_MAX)
		{
			ctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIndex);
		}
		com->Draw(model, ctx, model->GetModelData(), true);
	}

	void BuildStretcherModel(Object3d::ModelData& data)
	{
		data.vertices.clear();
		data.indices.clear();

		auto AddBox = [&](Vector3 center, Vector3 size) {
			Vector3 h = size * 0.5f;
			Vector3 p[8] = {
				{ center.x - h.x, center.y - h.y, center.z - h.z },
				{ center.x + h.x, center.y - h.y, center.z - h.z },
				{ center.x + h.x, center.y + h.y, center.z - h.z },
				{ center.x - h.x, center.y + h.y, center.z - h.z },
				{ center.x - h.x, center.y - h.y, center.z + h.z },
				{ center.x + h.x, center.y - h.y, center.z + h.z },
				{ center.x + h.x, center.y + h.y, center.z + h.z },
				{ center.x - h.x, center.y + h.y, center.z + h.z },
			};

			int faces[6][4] = {
				{ 0, 1, 2, 3 }, // Front (-Z)
				{ 5, 4, 7, 6 }, // Back (+Z)
				{ 4, 0, 3, 7 }, // Left (-X)
				{ 1, 5, 6, 2 }, // Right (+X)
				{ 3, 2, 6, 7 }, // Top (+Y)
				{ 4, 5, 1, 0 }  // Bottom (-Y)
			};
			Vector3 normals[6] = {
				{ 0.0f, 0.0f, -1.0f },
				{ 0.0f, 0.0f, 1.0f },
				{ -1.0f, 0.0f, 0.0f },
				{ 1.0f, 0.0f, 0.0f },
				{ 0.0f, 1.0f, 0.0f },
				{ 0.0f, -1.0f, 0.0f }
			};

			for (int f = 0; f < 6; ++f)
			{
				uint32_t fBase = static_cast<uint32_t>(data.vertices.size());
				for (int v = 0; v < 4; ++v)
				{
					Sprite::VertexData vd{};
					vd.position = { p[faces[f][v]].x, p[faces[f][v]].y, p[faces[f][v]].z, 1.0f };
					vd.texcoord = { (v == 1 || v == 2) ? 1.0f : 0.0f, (v == 2 || v == 3) ? 1.0f : 0.0f };
					vd.normal = normals[f];
					data.vertices.push_back(vd);
				}
				data.indices.push_back(fBase + 0);
				data.indices.push_back(fBase + 1);
				data.indices.push_back(fBase + 2);
				data.indices.push_back(fBase + 0);
				data.indices.push_back(fBase + 2);
				data.indices.push_back(fBase + 3);
			}
		};

		// 2 本のアルミニウム担架棒（長辺 X: 2.40m, 太さ: 0.06m, 左右幅 1.20m -> Z: ±0.60m）
		AddBox({ 0.0f, 0.0f, -0.60f }, { 2.40f, 0.06f, 0.06f });
		AddBox({ 0.0f, 0.0f,  0.60f }, { 2.40f, 0.06f, 0.06f });

		// ハンドルグリップ（前後端 X: ±1.15m）
		AddBox({ -1.15f, 0.0f, -0.60f }, { 0.16f, 0.08f, 0.08f });
		AddBox({ -1.15f, 0.0f,  0.60f }, { 0.16f, 0.08f, 0.08f });
		AddBox({  1.15f, 0.0f, -0.60f }, { 0.16f, 0.08f, 0.08f });
		AddBox({  1.15f, 0.0f,  0.60f }, { 0.16f, 0.08f, 0.08f });

		// 担架のナイロンキャンバス生地（幅 1.15m, 長さ 1.85m, 厚さ 0.03m）
		AddBox({ 0.0f, -0.015f, 0.0f }, { 1.85f, 0.03f, 1.15f });

		// 4本の折りたたみ脚（高さ 0.12m）
		AddBox({ -0.75f, -0.07f, -0.60f }, { 0.05f, 0.12f, 0.05f });
		AddBox({ -0.75f, -0.07f,  0.60f }, { 0.05f, 0.12f, 0.05f });
		AddBox({  0.75f, -0.07f, -0.60f }, { 0.05f, 0.12f, 0.05f });
		AddBox({  0.75f, -0.07f,  0.60f }, { 0.05f, 0.12f, 0.05f });

		// 負傷アヒルの頭部を支える大型ミリタリー救急枕
		AddBox({ -0.60f, 0.04f, 0.0f }, { 0.45f, 0.08f, 0.85f });

		data.boundingRadius = 4.0f;
	}

	void BuildHelicopterDoorModel(Object3d::ModelData& data)
	{
		data.vertices.clear();
		data.indices.clear();

		auto AddBox = [&](Vector3 center, Vector3 size, Vector2 uvMin, Vector2 uvMax) {
			Vector3 h = size * 0.5f;
			Vector3 p[8] = {
				{ center.x - h.x, center.y - h.y, center.z - h.z },
				{ center.x + h.x, center.y - h.y, center.z - h.z },
				{ center.x + h.x, center.y + h.y, center.z - h.z },
				{ center.x - h.x, center.y + h.y, center.z - h.z },
				{ center.x - h.x, center.y - h.y, center.z + h.z },
				{ center.x + h.x, center.y - h.y, center.z + h.z },
				{ center.x + h.x, center.y + h.y, center.z + h.z },
				{ center.x - h.x, center.y + h.y, center.z + h.z },
			};

			int faces[6][4] = {
				{ 0, 1, 2, 3 }, // Front (-Z)
				{ 5, 4, 7, 6 }, // Back (+Z)
				{ 4, 0, 3, 7 }, // Left (-X)
				{ 1, 5, 6, 2 }, // Right (+X)
				{ 3, 2, 6, 7 }, // Top (+Y)
				{ 4, 5, 1, 0 }  // Bottom (-Y)
			};
			Vector3 normals[6] = {
				{  0.0f,  0.0f, -1.0f },
				{  0.0f,  0.0f,  1.0f },
				{ -1.0f,  0.0f,  0.0f },
				{  1.0f,  0.0f,  0.0f },
				{  0.0f,  1.0f,  0.0f },
				{  0.0f, -1.0f,  0.0f }
			};

			for (int f = 0; f < 6; ++f)
			{
				uint32_t fBase = static_cast<uint32_t>(data.vertices.size());
				for (int v = 0; v < 4; ++v)
				{
					Sprite::VertexData vd{};
					vd.position = { p[faces[f][v]].x, p[faces[f][v]].y, p[faces[f][v]].z, 1.0f };
					vd.texcoord = { (v == 1 || v == 2) ? uvMax.x : uvMin.x, (v == 2 || v == 3) ? uvMax.y : uvMin.y };
					vd.normal = normals[f];
					data.vertices.push_back(vd);
				}
				data.indices.push_back(fBase + 0);
				data.indices.push_back(fBase + 1);
				data.indices.push_back(fBase + 2);
				data.indices.push_back(fBase + 0);
				data.indices.push_back(fBase + 2);
				data.indices.push_back(fBase + 3);
			}
		};

		// UVパレット（helicopter_texture.png 内の各カラーゾーン）
		const Vector2 uvOliveMin{ 0.25f, 0.05f };
		const Vector2 uvOliveMax{ 0.35f, 0.15f }; // オリーブドラブ装甲（機体同色）
		const Vector2 uvMetalMin{ 0.65f, 0.05f };
		const Vector2 uvMetalMax{ 0.75f, 0.15f }; // 黒染めスチール・レール・ベゼル
		const Vector2 uvGlassMin{ 0.10f, 0.75f };
		const Vector2 uvGlassMax{ 0.25f, 0.85f }; // 濃色スモークキャビンウィンドウ
		const Vector2 uvWarnMin { 0.85f, 0.05f };
		const Vector2 uvWarnMax { 0.95f, 0.15f }; // レスキューイエロー（緊急解除ハンドル）

		// 1. メイン装甲スライドドアパネル (厚さ 0.04m, 高さ 2.12m, 前後幅 2.72m)
		// 開口部 Z [-0.60, 2.10] (幅2.70m) を完全にカバー
		AddBox({ 0.0f, 0.0f, 0.0f }, { 0.04f, 2.12f, 2.72f }, uvOliveMin, uvOliveMax);

		// 2. 上下スライディングガイドレール＆ランナー金具
		AddBox({ 0.012f,  1.08f, 0.0f }, { 0.065f, 0.06f, 2.76f }, uvMetalMin, uvMetalMax);
		AddBox({ 0.012f, -1.08f, 0.0f }, { 0.065f, 0.06f, 2.76f }, uvMetalMin, uvMetalMax);

		// 3. ドア外側の補強リブ（ミリタリー構造体プレスリブ）
		AddBox({ 0.022f, -0.45f, 0.0f }, { 0.015f, 0.04f, 2.62f }, uvOliveMin, uvOliveMax);
		AddBox({ 0.022f,  0.80f, 0.0f }, { 0.015f, 0.04f, 2.62f }, uvOliveMin, uvOliveMax);

		// 4. 視認用キャビン小型強化スモークガラス窓 (Z = 0.45m, Y = 0.35m)
		AddBox({ 0.022f, 0.35f, 0.45f }, { 0.012f, 0.65f, 0.75f }, uvGlassMin, uvGlassMax);
		// 窓枠ベゼル（外周4辺）
		AddBox({ 0.026f, 0.70f, 0.45f }, { 0.020f, 0.05f, 0.85f }, uvMetalMin, uvMetalMax);
		AddBox({ 0.026f, 0.00f, 0.45f }, { 0.020f, 0.05f, 0.85f }, uvMetalMin, uvMetalMax);
		AddBox({ 0.026f, 0.35f, 0.05f }, { 0.020f, 0.65f, 0.05f }, uvMetalMin, uvMetalMax);
		AddBox({ 0.026f, 0.35f, 0.85f }, { 0.020f, 0.65f, 0.05f }, uvMetalMin, uvMetalMax);

		// 5. 緊急操作ラッチ・ドアハンドル（前方端の開閉用グリップ）
		AddBox({ 0.026f, -0.05f, 1.18f }, { 0.020f, 0.34f, 0.07f }, uvMetalMin, uvMetalMax);
		AddBox({ 0.042f, -0.05f, 1.18f }, { 0.020f, 0.26f, 0.03f }, uvWarnMin,  uvWarnMax);

		data.boundingRadius = 4.0f;
	}

#ifdef USE_IMGUI
	ImU32 WithAlpha(ImU32 col, float a)
	{
		a = std::clamp(a, 0.0f, 1.0f);
		ImU32 baseA = (col >> IM_COL32_A_SHIFT) & 0xFF;
		ImU32 newA = static_cast<ImU32>(static_cast<float>(baseA) * a);
		return (col & ~IM_COL32_A_MASK) | (newA << IM_COL32_A_SHIFT);
	}

	void AddCenteredText(ImDrawList* dl, ImFont* font, float size, float centerX, float topY, ImU32 col, const char* text)
	{
		ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
		dl->AddText(font, size, ImVec2(centerX - ts.x * 0.5f, topY), col, text);
	}

	void AddLeftAlignedText(ImDrawList* dl, ImFont* font, float size, float leftX, float topY, ImU32 col, const char* text)
	{
		dl->AddText(font, size, ImVec2(leftX, topY), col, text);
	}
#endif
}

// -----------------------------------------------------------------------------
// シーン初期化 / 終了
// -----------------------------------------------------------------------------

void GameOverScene::InitializeScene()
{
	DirectXCom* dx = GetDirectXCom();
	if (dx)
	{
		input_ = new KeyInput();
		input_->Initialize(dx->GetWindowAPI());
	}

	raidStats_ = GetSceneData<RaidStats>("RaidStats", RaidStats::GetInstance());

	state_ = State::Approaching;
	timer_ = 0.0f;
	actionTimer_ = 0.0f;
	transitionRequested_ = false;
	nextScene_ = "GAMEPLAY";
	ducksLoaded_ = false;

	// 音声マネージャーからアヒルSEをプリロード
	duckSeSoundId_ = -1;
	if (AudioManager* am = GetAudioManager())
	{
		duckSeSoundId_ = am->Load("Resources/Audio/SE_Duck.mp3");
	}

	// スカイボックスを表示して黄昏の空を展開
	if (SceneManager::GetInstance())
	{
		SceneManager::GetInstance()->SetShowSkybox(true);
	}

	// 黄昏・夜間救難作戦ライティング（冷たい青灰色の薄明 ＆ ヘリの強力なハロゲン救難投光器）
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetLight())
	{
		Light* light = SceneManager::GetInstance()->GetLight();
		// 全体を包む冷たい青灰色の夜気・薄明光（彩度を落としてミリタリーの重厚感を演出）
		light->SetDirectionalLight({ 0.38f, 0.44f, 0.58f, 1.0f }, { -0.35f, -0.85f, 0.40f }, 0.85f);
		// ヘリコプター直上から救難スポットライト（暖かなハロゲン光）
		light->SetPointLight({ 1.0f, 0.88f, 0.60f, 1.0f }, { 3.2f, 4.5f, 2.2f }, 4.5f, 32.0f, 1.0f);
	}

	// 3D ジオラマオブジェクトの初期化
	InitializeDiorama();
}

void GameOverScene::InitializeDiorama()
{
	Object3dCom* com = GetObject3dCom();
	if (!com) return;

	// 1. エンジンのメインカメラを取得・初期化（正面ワイド望遠構図：全体がすっきり一望できる映画アングル）
	if (!camera_)
	{
		camera_ = SceneManager::GetInstance() ? SceneManager::GetInstance()->GetCamera() : nullptr;
	}
	if (camera_)
	{
		camera_->SetTranslate({ -0.2f, 2.0f, -7.2f });
		camera_->SetRotate({ 0.10f, 0.12f, 0.0f });
		camera_->SetFovY(0.55f);
		camera_->Update();
	}

	// 2. テクスチャ読み込み
	TextureManager* tm = TextureManager::GetInstance();
	if (tm)
	{
		duckTexIndex_      = tm->Load("Resources/duck_player_wounded.png");
		if (duckTexIndex_ == UINT32_MAX)
		{
			duckTexIndex_  = tm->Load("Resources/duck.png");
		}
		duckMedicTexIndex_ = tm->Load("Resources/duck_medic.png");
		heliTexIndex_      = tm->Load("Resources/helicopter_texture.png");
		helipadTexIndex_   = tm->Load("Resources/extraction_pad.png");
		stretcherTexIndex_ = tm->Load("Resources/container_military.png");
		pineTexIndex_      = tm->Load("Resources/duckov_pine_tree.png");
		sandbagTexIndex_   = tm->Load("Resources/duckov_sandbag.png");
		barrelTexIndex_    = tm->Load("Resources/duckov_barrel.png");
		crateTexIndex_     = tm->Load("Resources/duckov_crate.png");
		fenceTexIndex_     = tm->Load("Resources/fence.png");
		groundTexIndex_    = tm->Load("Resources/grass.png");
		vignetteTexIndex_  = tm->Load("Resources/title_vignette.png");
		logoTexIndex_      = tm->Load("Resources/gameover_logo.png");
		promptTexIndex_    = tm->Load("Resources/gameover_prompt.png");
		debriefTexIndex_   = tm->Load("Resources/gameover_debrief.png");
	}

	// 3. 救難ヘリコプター（MEDEVAC HELICOPTER）
	{
		Object3d::ModelData bodyData = Object3d::LoadObjFile("Resources", "helicopter_body.obj");
		bodyData.material.textureFilePath = "Resources/helicopter_texture.png";
		bodyData.material.textureIndex = heliTexIndex_;
		bodyData.boundingRadius = 80.0f;
		heliBody_ = std::make_unique<Object3d>();
		heliBody_->Initialize(com, bodyData);
		heliBody_->SetCamera(camera_);
		heliBody_->SetScale({ 0.65f, 0.65f, 0.65f });
		heliBody_->SetEnableLighting(true);

		Object3d::ModelData rotorData = Object3d::LoadObjFile("Resources", "helicopter_rotor.obj");
		rotorData.material.textureFilePath = "Resources/helicopter_texture.png";
		rotorData.material.textureIndex = heliTexIndex_;
		rotorData.boundingRadius = 80.0f;
		heliMainRotor_ = std::make_unique<Object3d>();
		heliMainRotor_->Initialize(com, rotorData);
		heliMainRotor_->SetCamera(camera_);
		heliMainRotor_->SetScale({ 0.65f, 0.65f, 0.65f });
		heliMainRotor_->SetEnableLighting(true);

		heliMainRotorBlur1_ = std::make_unique<Object3d>();
		heliMainRotorBlur1_->Initialize(com, rotorData);
		heliMainRotorBlur1_->SetCamera(camera_);
		heliMainRotorBlur1_->SetScale({ 0.65f, 0.65f, 0.65f });
		heliMainRotorBlur1_->SetEnableLighting(true);

		heliMainRotorBlur2_ = std::make_unique<Object3d>();
		heliMainRotorBlur2_->Initialize(com, rotorData);
		heliMainRotorBlur2_->SetCamera(camera_);
		heliMainRotorBlur2_->SetScale({ 0.65f, 0.65f, 0.65f });
		heliMainRotorBlur2_->SetEnableLighting(true);

		Object3d::ModelData tailData = Object3d::LoadObjFile("Resources", "helicopter_tailrotor.obj");
		tailData.material.textureFilePath = "Resources/helicopter_texture.png";
		tailData.material.textureIndex = heliTexIndex_;
		tailData.boundingRadius = 80.0f;
		heliTailRotor_ = std::make_unique<Object3d>();
		heliTailRotor_->Initialize(com, tailData);
		heliTailRotor_->SetCamera(camera_);
		heliTailRotor_->SetScale({ 0.65f, 0.65f, 0.65f });
		heliTailRotor_->SetEnableLighting(true);

		// 救難ヘリ大型スライディング・カーゴドア
		Object3d::ModelData doorData{};
		BuildHelicopterDoorModel(doorData);
		doorData.material.textureFilePath = "Resources/helicopter_texture.png";
		doorData.material.textureIndex = heliTexIndex_;
		doorData.boundingRadius = 80.0f;
		heliCargoDoor_ = std::make_unique<Object3d>();
		heliCargoDoor_->Initialize(com, doorData);
		heliCargoDoor_->SetCamera(camera_);
		heliCargoDoor_->SetScale({ 0.65f, 0.65f, 0.65f });
		heliCargoDoor_->SetEnableLighting(true);

		doorSlideProgress_ = 0.0f;

		heliPos_ = { 3.2f, 0.435f, 2.2f };
		heliRot_ = { 0.0f, 2.65f, 0.0f }; // 開口カーゴドア(+X)を手前左（担架アプローチ側）に向ける
		mainRotorAngle_ = 0.0f;
		tailRotorAngle_ = 0.0f;
	}

	// 4. 脱出ヘリパッド（EXTRACTION HELIPAD）
	{
		Object3d::ModelData padData = Object3d::LoadObjFile("Resources", "plane.obj");
		padData.material.textureFilePath = "Resources/extraction_pad.png";
		padData.material.textureIndex = helipadTexIndex_;
		padData.boundingRadius = 15.0f;
		helipadModel_ = std::make_unique<Object3d>();
		helipadModel_->Initialize(com, padData);
		helipadModel_->SetCamera(camera_);
		helipadModel_->SetScale({ 8.0f, 8.0f, 1.0f });
		helipadModel_->SetRotate({ -1.5707963f, 0.0f, 0.0f }); // XYプレーンをXZ地面プレーンへ水平に寝かせる
		helipadModel_->SetTranslate({ 3.2f, 0.02f, 2.2f });
		helipadModel_->SetEnableLighting(true);
	}

	// 5. 担架チーム（2羽の衛生兵アヒル＋本格担架＋負傷主役アヒル）
	{
		Object3d::ModelData duckData = Object3d::LoadObjFile("Resources", "player.obj");
		duckData.material.textureFilePath = "Resources/duck.png";
		duckData.material.textureIndex = duckTexIndex_;
		duckData.boundingRadius = 10.0f;

		// 後方衛生兵
		duckRearMedic_ = std::make_unique<Object3d>();
		duckRearMedic_->Initialize(com, duckData);
		duckRearMedic_->SetCamera(camera_);
		duckRearMedic_->SetScale({ 0.38f, 0.38f, 0.38f });
		duckRearMedic_->SetEnableLighting(true);

		// 前方衛生兵
		duckFrontMedic_ = std::make_unique<Object3d>();
		duckFrontMedic_->Initialize(com, duckData);
		duckFrontMedic_->SetCamera(camera_);
		duckFrontMedic_->SetScale({ 0.38f, 0.38f, 0.38f });
		duckFrontMedic_->SetEnableLighting(true);

		// 負傷した主役アヒル（仰向けに横たわる）
		duckWounded_ = std::make_unique<Object3d>();
		duckWounded_->Initialize(com, duckData);
		duckWounded_->SetCamera(camera_);
		duckWounded_->SetScale({ 0.38f, 0.38f, 0.38f });
		duckWounded_->SetEnableLighting(true);

		// 担架ベッド（ミリタリーキャンバス調）
		Object3d::ModelData stretcherData;
		BuildStretcherModel(stretcherData);
		stretcherData.material.textureFilePath = "Resources/container_military.png";
		stretcherData.material.textureIndex = stretcherTexIndex_;
		stretcherBed_ = std::make_unique<Object3d>();
		stretcherBed_->Initialize(com, stretcherData);
		stretcherBed_->SetCamera(camera_);
		stretcherBed_->SetScale({ 0.65f, 0.65f, 0.65f });
		stretcherBed_->SetEnableLighting(true);

		teamPos_ = { -4.2f, 0.0f, 1.4f };
		teamStartPos_ = teamPos_;
		walkPhase_ = 0.0f;
		stretcherLiftY_ = 0.0f;
		stretcherSlideX_ = 0.0f;
		ducksLoaded_ = false;
	}

	// 6. 地形（地面プレーン）
	{
		Object3d::ModelData groundData = Object3d::LoadObjFile("Resources", "ground.obj");
		groundData.material.textureFilePath = "Resources/grass.png";
		groundData.material.textureIndex = groundTexIndex_;
		groundData.boundingRadius = 150.0f;
		groundModel_ = std::make_unique<Object3d>();
		groundModel_->Initialize(com, groundData);
		groundModel_->SetCamera(camera_);
		groundModel_->SetScale({ 60.0f, 1.0f, 60.0f });
		groundModel_->SetTranslate({ 0.0f, 0.0f, 0.0f });
		groundModel_->SetEnableLighting(true);
	}

	// 7. リアルな軍事救難前線基地・脱出ゾーン（LZ）の背景小道具構築
	props_.clear();
	auto AddProp = [&](const std::string& objFile, const std::string& texFile, uint32_t texIdx,
		Vector3 pos, Vector3 rot, Vector3 scale) {
		Object3d::ModelData data = Object3d::LoadObjFile("Resources", objFile);
		data.material.textureFilePath = texFile;
		data.material.textureIndex = texIdx;
		data.boundingRadius = 35.0f;
		auto model = std::make_unique<Object3d>();
		model->Initialize(com, data);
		model->SetCamera(camera_);
		model->SetScale(scale);
		model->SetTranslate(pos);
		model->SetRotate(rot);
		model->SetEnableLighting(true);
		props_.push_back({ std::move(model), texIdx });
	};

	// (A) 最奥の針葉樹林帯（鬱蒼としたタイガの森・多層シルエット）
	AddProp("duckov_pine_tree.obj", "Resources/duckov_pine_tree.png", pineTexIndex_, { -16.5f, 0.0f, 19.0f }, { 0.0f, 0.4f, 0.0f }, { 2.0f, 2.3f, 2.0f });
	AddProp("duckov_pine_tree.obj", "Resources/duckov_pine_tree.png", pineTexIndex_, { -13.0f, 0.0f, 16.5f }, { 0.0f, 1.1f, 0.0f }, { 1.7f, 1.9f, 1.7f });
	AddProp("duckov_pine_tree.obj", "Resources/duckov_pine_tree.png", pineTexIndex_, { -10.0f, 0.0f, 18.0f }, { 0.0f, 2.3f, 0.0f }, { 1.9f, 2.2f, 1.9f });
	AddProp("duckov_pine_tree.obj", "Resources/duckov_pine_tree.png", pineTexIndex_, {  -6.5f, 0.0f, 15.0f }, { 0.0f, 0.8f, 0.0f }, { 1.6f, 1.8f, 1.6f });
	AddProp("duckov_pine_tree.obj", "Resources/duckov_pine_tree.png", pineTexIndex_, {  -3.0f, 0.0f, 17.5f }, { 0.0f, 1.7f, 0.0f }, { 1.8f, 2.1f, 1.8f });
	AddProp("duckov_pine_tree.obj", "Resources/duckov_pine_tree.png", pineTexIndex_, {   0.5f, 0.0f, 19.5f }, { 0.0f, 2.9f, 0.0f }, { 2.1f, 2.4f, 2.1f });
	AddProp("duckov_pine_tree.obj", "Resources/duckov_pine_tree.png", pineTexIndex_, {   4.0f, 0.0f, 16.5f }, { 0.0f, 0.5f, 0.0f }, { 1.7f, 2.0f, 1.7f });
	AddProp("duckov_pine_tree.obj", "Resources/duckov_pine_tree.png", pineTexIndex_, {   7.5f, 0.0f, 18.5f }, { 0.0f, 1.9f, 0.0f }, { 1.9f, 2.2f, 1.9f });
	AddProp("duckov_pine_tree.obj", "Resources/duckov_pine_tree.png", pineTexIndex_, {  11.0f, 0.0f, 15.5f }, { 0.0f, 0.2f, 0.0f }, { 1.6f, 1.8f, 1.6f });
	AddProp("duckov_pine_tree.obj", "Resources/duckov_pine_tree.png", pineTexIndex_, {  14.5f, 0.0f, 17.5f }, { 0.0f, 2.1f, 0.0f }, { 1.8f, 2.1f, 1.8f });
	AddProp("duckov_pine_tree.obj", "Resources/duckov_pine_tree.png", pineTexIndex_, {  18.0f, 0.0f, 20.0f }, { 0.0f, 1.3f, 0.0f }, { 2.0f, 2.3f, 2.0f });

	// (B) 敵勢力監視・前哨見張り塔（森林境界左翼）
	AddProp("watchtower.obj", "Resources/duckov_crate.png", crateTexIndex_, { -8.5f, 0.0f, 12.5f }, { 0.0f, 0.25f, 0.0f }, { 1.0f, 1.0f, 1.0f });

	// (C) 救難基地境界の有刺鉄線・防護フェンス（Z=8.8m）
	AddProp("fence.obj", "Resources/fence.png", fenceTexIndex_, { -4.2f, 1.0f, 8.8f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });
	AddProp("fence.obj", "Resources/fence.png", fenceTexIndex_, {  0.0f, 1.0f, 8.8f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });
	AddProp("fence.obj", "Resources/fence.png", fenceTexIndex_, {  4.2f, 1.0f, 8.8f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });
	AddProp("fence.obj", "Resources/fence.png", fenceTexIndex_, {  8.4f, 1.0f, 8.8f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });

	// (D) 前線司令・救護所ミリタリーコンテナ（左奥）
	AddProp("container.obj", "Resources/container_military.png", stretcherTexIndex_, { -6.5f, 0.0f, 5.5f }, { 0.0f, 0.35f, 0.0f }, { 1.0f, 1.0f, 1.0f });

	// (E) 土嚢陣地バリア（左側アプローチ警備 ＆ 右側外縁防衛）
	AddProp("duckov_sandbag_wall.obj", "Resources/duckov_sandbag.png", sandbagTexIndex_, { -3.8f, 0.0f, 4.6f }, { 0.0f, 0.15f, 0.0f }, { 1.0f, 1.0f, 1.0f });
	AddProp("duckov_sandbag_wall.obj", "Resources/duckov_sandbag.png", sandbagTexIndex_, {  8.0f, 0.0f, 5.2f }, { 0.0f, -0.20f, 0.0f }, { 1.0f, 1.0f, 1.0f });

	// (F) 軍用ドラム缶・燃料補給スタック（左右基地施設）
	AddProp("duckov_barrel_stack.obj", "Resources/duckov_barrel.png", barrelTexIndex_, { -5.2f, 0.0f, 3.8f }, { 0.0f, 0.40f, 0.0f }, { 1.0f, 1.0f, 1.0f });
	AddProp("duckov_barrel_stack.obj", "Resources/duckov_barrel.png", barrelTexIndex_, {  6.2f, 0.0f, 4.8f }, { 0.0f, -0.35f, 0.0f }, { 1.0f, 1.0f, 1.0f });

	// (G) 軍用弾薬・補給物資木箱スタック（段積み・配置）
	AddProp("duckov_crate.obj", "Resources/duckov_crate.png", crateTexIndex_, { -4.4f, 0.0f, 4.0f }, { 0.0f, 0.20f, 0.0f }, { 0.95f, 0.95f, 0.95f });
	AddProp("duckov_crate.obj", "Resources/duckov_crate.png", crateTexIndex_, { -4.4f, 1.12f, 4.0f }, { 0.0f, 0.50f, 0.0f }, { 0.72f, 0.72f, 0.72f });
	AddProp("duckov_crate.obj", "Resources/duckov_crate.png", crateTexIndex_, {  7.0f, 0.0f, 4.2f }, { 0.0f, -0.15f, 0.0f }, { 1.0f, 1.0f, 1.0f });
	AddProp("duckov_crate.obj", "Resources/duckov_crate.png", crateTexIndex_, {  8.2f, 0.0f, 4.0f }, { 0.0f, 0.30f, 0.0f }, { 0.90f, 0.90f, 0.90f });

	// (H) ヘリパッド外縁の境界標識マーカー（進入・着陸誘導）
	AddProp("duckov_crate.obj", "Resources/duckov_crate.png", crateTexIndex_, { -1.0f, 0.0f, -2.0f }, { 0.0f, 0.10f, 0.0f }, { 0.45f, 0.45f, 0.45f });
	AddProp("duckov_crate.obj", "Resources/duckov_crate.png", crateTexIndex_, {  7.4f, 0.0f, -2.0f }, { 0.0f, -0.10f, 0.0f }, { 0.45f, 0.45f, 0.45f });
}

void GameOverScene::Finalize()
{
	delete input_;
	input_ = nullptr;

	heliBody_.reset();
	heliMainRotor_.reset();
	heliMainRotorBlur1_.reset();
	heliMainRotorBlur2_.reset();
	heliTailRotor_.reset();
	heliCargoDoor_.reset();
	helipadModel_.reset();
	duckRearMedic_.reset();
	duckFrontMedic_.reset();
	duckWounded_.reset();
	stretcherBed_.reset();
	groundModel_.reset();
	props_.clear();
}

// -----------------------------------------------------------------------------
// 更新
// -----------------------------------------------------------------------------

void GameOverScene::Update()
{
	const float deltaTime = 1.0f / 60.0f;

	// シーン切り替えトランジション中（シャッター閉鎖〜開扉中）の判定
	const bool isTransitioning = 
		(DeathTransition::GetInstance() && DeathTransition::GetInstance()->IsTransitioning()) ||
		(StingerTransition::GetInstance() && StingerTransition::GetInstance()->IsTransitioning());

	// シャッターが完全に開き切るまで、シーンタイマー（文字フェード）は進めない
	if (!isTransitioning)
	{
		timer_ += deltaTime;
	}

	bool confirm = false;
	bool toTitle = false;
	if (input_ && !isTransitioning)
	{
		input_->Update();
		confirm = input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN);
		toTitle = input_->TriggerKey(DIK_T);
	}

	if (state_ == State::Approaching)
	{
		if (!isTransitioning)
		{
			actionTimer_ += deltaTime;
		}

		// プレイヤーがSPACEまたはTを押した場合は即座に搭乗完了して離陸へスキップ！
		if (timer_ >= 0.50f && (confirm || toTitle))
		{
			state_ = State::Takeoff;
			actionTimer_ = 0.0f;
			ducksLoaded_ = true;
			nextScene_ = toTitle ? "TITLE" : "GAMEPLAY";

			if (duckSeSoundId_ >= 0)
			{
				if (AudioManager* am = GetAudioManager())
				{
					am->PlayOneShot(duckSeSoundId_, 1.0f);
				}
			}
		}
		// 時間経過で自動的にヘリドア前へ到着し、機内収容ステートへ移行！
		else if (actionTimer_ >= kApproachDuration)
		{
			state_ = State::Loading;
			actionTimer_ = 0.0f;
		}
	}
	else if (state_ == State::Loading)
	{
		if (!isTransitioning)
		{
			actionTimer_ += deltaTime;
		}

		// 収容中にSPACEまたはTを押した場合は即座に搭乗完了して離陸！
		if (confirm || toTitle)
		{
			state_ = State::Takeoff;
			actionTimer_ = 0.0f;
			ducksLoaded_ = true;
			nextScene_ = toTitle ? "TITLE" : "GAMEPLAY";

			if (duckSeSoundId_ >= 0)
			{
				if (AudioManager* am = GetAudioManager())
				{
					am->PlayOneShot(duckSeSoundId_, 1.0f);
				}
			}
		}
		// 自動収容完了：全アヒル搭乗完了！Ready（離陸待機）へ移行
		else if (actionTimer_ >= kLoadDuration)
		{
			state_ = State::Ready;
			actionTimer_ = 0.0f;
			ducksLoaded_ = true;
		}
	}
	else if (state_ == State::Ready)
	{
		if (!isTransitioning)
		{
			actionTimer_ += deltaTime;
		}

		// 収容完了状態：SPACE（またはT）で救難ヘリが離陸！
		if (confirm || toTitle)
		{
			state_ = State::Takeoff;
			actionTimer_ = 0.0f;
			nextScene_ = toTitle ? "TITLE" : "GAMEPLAY";

			if (duckSeSoundId_ >= 0)
			{
				if (AudioManager* am = GetAudioManager())
				{
					am->PlayOneShot(duckSeSoundId_, 1.0f);
				}
			}
		}
	}
	else if (state_ == State::Takeoff)
	{
		if (!isTransitioning)
		{
			actionTimer_ += deltaTime;
		}

		// 離陸中に再入力で即座にシーン切り替えスキップ
		const bool skip = confirm && actionTimer_ > 0.25f;
		if (!transitionRequested_ && (actionTimer_ >= kTakeoffDuration || skip))
		{
			transitionRequested_ = true;
			SceneTransition::Change(nextScene_, 0.5f);
		}
	}

	// 3D ジオラマアニメーションの更新（シャッターが開く間もヘリのローターは滑らかに回転）
	UpdateDiorama(deltaTime);

	// 2D 映画的HUDの描画要求
	DrawScreen();
}

void GameOverScene::UpdateDiorama(float deltaTime)
{
	// -------------------------------------------------------------------------
	// 1. 救難ヘリコプターのアニメーション
	// -------------------------------------------------------------------------
	// 各ステートに応じた角速度 (rad/s)
	// メインローター(4枚羽, 90度周期): 1フレームあたり 19度〜39度(20〜40.5 rad/s)に調整し、
	// ストロボ現象（逆回転・静止錯視）を完全回避して極限まで超高速回転・緊急感を演出！
	float mainRotorSpeed = 9.0f;  // 86 RPM (Approaching: 緩やかな待機回転)
	float tailRotorSpeed = 24.0f; // 229 RPM

	if (state_ == State::Approaching)
	{
		mainRotorSpeed = 9.0f;
		tailRotorSpeed = 24.0f;
	}
	else if (state_ == State::Loading)
	{
		mainRotorSpeed = 14.5f; // 138 RPM (収容開始による回転上昇)
		tailRotorSpeed = 30.0f;
	}
	else if (state_ == State::Ready)
	{
		mainRotorSpeed = 20.0f; // 191 RPM (収容完了・離陸直前のアイドリング高回転)
		tailRotorSpeed = 36.0f;
	}
	else if (state_ == State::Takeoff)
	{
		float t = std::clamp(actionTimer_ / kTakeoffDuration, 0.0f, 1.0f);
		// SPACE押下による緊急離陸：ローターがフルスロットル急加速（20 rad/s -> 40.5 rad/s: 387 RPM!）
		float spoolT = std::clamp(actionTimer_ / 0.45f, 0.0f, 1.0f);
		float spoolEase = EaseOutQuad(spoolT);
		mainRotorSpeed = 20.0f + spoolEase * 20.5f;
		tailRotorSpeed = 36.0f + spoolEase * 46.0f; // 36 -> 82 rad/s (783 RPM!)

		// 離陸演出 (0.15s 以降に機首を前傾させて斜め上空へ力強く急上昇)
		if (t > 0.15f)
		{
			float liftT = (t - 0.15f) / 0.85f;
			float ascend = EaseInQuad(liftT);
			heliPos_.y = 0.435f + ascend * 9.5f;
			heliPos_.x = 3.2f + std::pow(liftT, 1.3f) * 6.5f;
			heliPos_.z = 2.2f + std::pow(liftT, 1.3f) * 4.5f;
			// 緊急離陸の力強い前傾ピッチ＆ロール
			heliRot_.x = 0.10f * std::sin(liftT * kPi * 0.9f);
			heliRot_.z = -0.15f * std::sin(liftT * kPi * 0.8f);
		}
	}

	mainRotorAngle_ += deltaTime * mainRotorSpeed;
	tailRotorAngle_ += deltaTime * tailRotorSpeed;

	// ミリタリー・ハロゲン投光器の重厚な微細ゆらぎ演出（前線基地の夜間臨場感向上）
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetLight())
	{
		Light* light = SceneManager::GetInstance()->GetLight();
		float pulse = 4.4f + std::sin(timer_ * 5.0f) * 0.18f;
		light->SetPointLight({ 1.0f, 0.88f, 0.62f, 1.0f }, { 3.2f, 4.5f, 2.2f }, pulse, 32.0f, 1.0f);
	}

	if (heliBody_)
	{
		heliBody_->SetTranslate(heliPos_);
		heliBody_->SetRotate(heliRot_);
		heliBody_->Update();
	}

	// ローターブレードのトランスフォーム（スケール 0.65f に対応）
	if (heliMainRotor_)
	{
		Matrix4x4 R = Multiply(MakeRotateXMatrix(heliRot_.x), Multiply(MakeRotateYMatrix(heliRot_.y), MakeRotateZMatrix(heliRot_.z)));
		Vector3 mastOffset = { 0.0f, 4.15f * 0.65f, 0.75f * 0.65f };
		Vector3 mastWorld = {
			mastOffset.x * R.m[0][0] + mastOffset.y * R.m[1][0] + mastOffset.z * R.m[2][0],
			mastOffset.x * R.m[0][1] + mastOffset.y * R.m[1][1] + mastOffset.z * R.m[2][1],
			mastOffset.x * R.m[0][2] + mastOffset.y * R.m[1][2] + mastOffset.z * R.m[2][2]
		};
		heliMainRotor_->SetTranslate(heliPos_ + mastWorld);
		heliMainRotor_->SetRotate({ heliRot_.x, heliRot_.y + mainRotorAngle_, heliRot_.z });
		heliMainRotor_->Update();

		// 緊急離陸時の高回転モーションブラー（残像ブレードを角度オフセット配置）
		float blurOffset = 0.0f;
		if (state_ == State::Takeoff)
		{
			float spoolT = std::clamp(actionTimer_ / 0.45f, 0.0f, 1.0f);
			blurOffset = EaseOutQuad(spoolT) * 0.095f; // 最大約5.4度の残像ファンアウト
		}

		if (heliMainRotorBlur1_)
		{
			heliMainRotorBlur1_->SetTranslate(heliPos_ + mastWorld);
			heliMainRotorBlur1_->SetRotate({ heliRot_.x, heliRot_.y + mainRotorAngle_ - blurOffset, heliRot_.z });
			heliMainRotorBlur1_->Update();
		}
		if (heliMainRotorBlur2_)
		{
			heliMainRotorBlur2_->SetTranslate(heliPos_ + mastWorld);
			heliMainRotorBlur2_->SetRotate({ heliRot_.x, heliRot_.y + mainRotorAngle_ - blurOffset * 2.0f, heliRot_.z });
			heliMainRotorBlur2_->Update();
		}
	}

	if (heliTailRotor_)
	{
		Matrix4x4 R = Multiply(MakeRotateXMatrix(heliRot_.x), Multiply(MakeRotateYMatrix(heliRot_.y), MakeRotateZMatrix(heliRot_.z)));
		Vector3 tailOffset = { 0.20f * 0.65f, 3.35f * 0.65f, -8.60f * 0.65f };
		Vector3 tailWorld = {
			tailOffset.x * R.m[0][0] + tailOffset.y * R.m[1][0] + tailOffset.z * R.m[2][0],
			tailOffset.x * R.m[0][1] + tailOffset.y * R.m[1][1] + tailOffset.z * R.m[2][1],
			tailOffset.x * R.m[0][2] + tailOffset.y * R.m[1][2] + tailOffset.z * R.m[2][2]
		};
		heliTailRotor_->SetTranslate(heliPos_ + tailWorld);
		heliTailRotor_->SetRotate({ heliRot_.x + tailRotorAngle_, heliRot_.y, heliRot_.z });
		heliTailRotor_->Update();
	}

	// -------------------------------------------------------------------------
	// 2. 担架チーム（自動接近歩行・機内収容積み込み・ヘリ同乗脱出）
	// -------------------------------------------------------------------------
	auto LocalToHeliWorld = [&](const Vector3& localOffset) -> Vector3 {
		Matrix4x4 R = Multiply(MakeRotateXMatrix(heliRot_.x), Multiply(MakeRotateYMatrix(heliRot_.y), MakeRotateZMatrix(heliRot_.z)));
		Vector3 scaled = { localOffset.x * 0.65f, localOffset.y * 0.65f, localOffset.z * 0.65f };
		Vector3 rotated = {
			scaled.x * R.m[0][0] + scaled.y * R.m[1][0] + scaled.z * R.m[2][0],
			scaled.x * R.m[0][1] + scaled.y * R.m[1][1] + scaled.z * R.m[2][1],
			scaled.x * R.m[0][2] + scaled.y * R.m[1][2] + scaled.z * R.m[2][2]
		};
		return heliPos_ + rotated;
	};

	// スライドカーゴドアの開閉アニメーション（SPACE押下による離陸開始で速やかに密閉）
	if (state_ == State::Takeoff)
	{
		float doorT = std::clamp(actionTimer_ / kDoorCloseDuration, 0.0f, 1.0f);
		doorSlideProgress_ = EaseInOutCubic(doorT);
	}
	else
	{
		doorSlideProgress_ = 0.0f; // 搬送中・収容中・離陸待機中は全開（アヒルの搭乗状況がクリアに見える）
	}

	if (heliCargoDoor_)
	{
		// 開位置 Z = -1.90m, X = 1.470m（機体外レール上） -> 閉位置 Z = 0.75m, X = 1.455m（開口部枠に密着）
		float curX = 1.470f - 0.015f * doorSlideProgress_;
		float curZ = -1.90f + (0.75f - (-1.90f)) * doorSlideProgress_;
		Vector3 doorLocal = { curX, 1.14f, curZ };

		heliCargoDoor_->SetTranslate(LocalToHeliWorld(doorLocal));
		heliCargoDoor_->SetRotate(heliRot_);
		heliCargoDoor_->SetScale({ 0.65f, 0.65f, 0.65f });
		heliCargoDoor_->Update();
	}

	auto LerpVec = [](const Vector3& a, const Vector3& b, float t) -> Vector3 {
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t
		};
	};

	// ヘリキャビン内部の座席・配置ローカル座標（床面Y=0.10m、開口ドアZ=[-0.6, 2.1]を基準に完璧に配置）
	const Vector3 kStretcherCabinLocal  = { -0.35f, 0.19f, 0.25f };       // キャビン中央奥・床面（脚が床上に着地）
	const Vector3 kWoundedCabinLocal    = { -0.35f, 0.30f, 0.25f };       // 担架キャンバス上の負傷アヒル（ドア越しに顔が見える）
	const Vector3 kFrontMedicCabinLocal = {  0.35f, 0.105f, 0.55f };      // 開口ドア前寄りで直立待機（右ピラーから十分な安全余白）
	const Vector3 kRearMedicCabinLocal  = {  0.35f, 0.105f, -0.15f };     // 開口ドア後寄りで直立待機（左ピラーから十分な安全余白）

	// キャビン内での担架の長手方向（機体前後Z軸方向）姿勢
	const Vector3 kStretcherInsideRot  = { heliRot_.x, heliRot_.y + 1.57f, heliRot_.z };
	const Vector3 kWoundedInsideRot    = { heliRot_.x - 0.10f, heliRot_.y + 1.57f, heliRot_.z - 1.55f };
	const Vector3 kMedicInsideRot      = { heliRot_.x, heliRot_.y + 1.57f, heliRot_.z }; // ドア・カメラ側を向く (+90度)

	const float breath = std::sin(timer_ * 2.8f) * 0.015f;

	if (state_ == State::Approaching)
	{
		walkPhase_ += deltaTime * 6.5f;
		float t = std::clamp(actionTimer_ / kApproachDuration, 0.0f, 1.0f);
		float progress = EaseOutQuad(t);
		teamPos_ = LerpVec(teamStartPos_, { 1.50f, 0.0f, 1.45f }, progress);

		float waddle = std::sin(walkPhase_) * 0.14f;
		float bob = std::fabs(std::sin(walkPhase_)) * 0.07f;

		// 後方衛生兵
		if (duckRearMedic_)
		{
			duckRearMedic_->SetTranslate(teamPos_ + Vector3{ -0.95f, bob, 0.0f });
			duckRearMedic_->SetRotate({ 0.0f, 1.57f, waddle });
			duckRearMedic_->Update();
		}
		// 前方衛生兵
		if (duckFrontMedic_)
		{
			duckFrontMedic_->SetTranslate(teamPos_ + Vector3{ 0.95f, bob, 0.0f });
			duckFrontMedic_->SetRotate({ 0.0f, 1.57f, -waddle });
			duckFrontMedic_->Update();
		}
		// 担架ベッド
		if (stretcherBed_)
		{
			stretcherBed_->SetTranslate(teamPos_ + Vector3{ 0.0f, 0.24f + bob * 0.5f, 0.0f });
			stretcherBed_->SetRotate({ 0.0f, 0.0f, 0.0f });
			stretcherBed_->Update();
		}
		// 負傷した主役アヒル
		if (duckWounded_)
		{
			duckWounded_->SetTranslate(teamPos_ + Vector3{ 0.0f, 0.35f + bob * 0.5f + breath, 0.0f });
			duckWounded_->SetRotate({ -0.10f, 0.0f, -1.55f });
			duckWounded_->Update();
		}
	}
	else if (state_ == State::Loading)
	{
		float t = std::clamp(actionTimer_ / kLoadDuration, 0.0f, 1.0f);

		// ドア前初期位置
		const Vector3 frontStart   = { 1.50f + 0.95f, 0.0f, 1.45f };
		const Vector3 rearStart    = { 1.50f - 0.95f, 0.0f, 1.45f };
		const Vector3 bedStart     = { 1.50f, 0.24f, 1.45f };
		const Vector3 woundedStart = { 1.50f, 0.35f, 1.45f };

		// 前方衛生兵：ドアをくぐって機内前方座席へ
		float frontT = EaseInOutCubic(std::clamp(t / 0.65f, 0.0f, 1.0f));
		Vector3 frontPos = LerpVec(frontStart, LocalToHeliWorld(kFrontMedicCabinLocal), frontT);
		Vector3 frontRot = LerpVec({ 0.0f, 1.57f, 0.0f }, kMedicInsideRot, frontT);
		if (duckFrontMedic_)
		{
			duckFrontMedic_->SetTranslate(frontPos);
			duckFrontMedic_->SetRotate(frontRot);
			duckFrontMedic_->Update();
		}

		// 担架ベッド：少し持ち上げてドア敷居を跨ぎ、向きを変えながら機内中央へスライド収容
		float bedT = EaseInOutCubic(std::clamp((t - 0.10f) / 0.80f, 0.0f, 1.0f));
		float liftArc = std::sin(bedT * kPi) * 0.14f;
		Vector3 bedPos = LerpVec(bedStart, LocalToHeliWorld(kStretcherCabinLocal), bedT) + Vector3{ 0.0f, liftArc, 0.0f };
		Vector3 bedRot = LerpVec({ 0.0f, 0.0f, 0.0f }, kStretcherInsideRot, bedT);
		if (stretcherBed_)
		{
			stretcherBed_->SetTranslate(bedPos);
			stretcherBed_->SetRotate(bedRot);
			stretcherBed_->Update();
		}

		// 負傷アヒル：担架に密着して一緒に機内へ
		if (duckWounded_)
		{
			Vector3 woundedPos = LerpVec(woundedStart, LocalToHeliWorld(kWoundedCabinLocal), bedT) + Vector3{ 0.0f, liftArc + breath, 0.0f };
			Vector3 woundedRot = LerpVec({ -0.10f, 0.0f, -1.55f }, kWoundedInsideRot, bedT);
			duckWounded_->SetTranslate(woundedPos);
			duckWounded_->SetRotate(woundedRot);
			duckWounded_->Update();
		}

		// 後方衛生兵：担架を押し込みながら機内後方座席へ乗り込む
		float rearT = EaseInOutCubic(std::clamp((t - 0.30f) / 0.70f, 0.0f, 1.0f));
		Vector3 rearPos = LerpVec(rearStart, LocalToHeliWorld(kRearMedicCabinLocal), rearT);
		Vector3 rearRot = LerpVec({ 0.0f, 1.57f, 0.0f }, kMedicInsideRot, rearT);
		if (duckRearMedic_)
		{
			duckRearMedic_->SetTranslate(rearPos);
			duckRearMedic_->SetRotate(rearRot);
			duckRearMedic_->Update();
		}
	}
	else // State::Ready または State::Takeoff（全員ヘリ機内に搭乗し、ヘリの動きと完全同期！）
	{
		// 担架ベッド（ヘリ機内床面に縦置き固定）
		if (stretcherBed_)
		{
			stretcherBed_->SetTranslate(LocalToHeliWorld(kStretcherCabinLocal));
			stretcherBed_->SetRotate(kStretcherInsideRot);
			stretcherBed_->Update();
		}
		// 負傷アヒル（担架上で安静）
		if (duckWounded_)
		{
			duckWounded_->SetTranslate(LocalToHeliWorld(kWoundedCabinLocal) + Vector3{ 0.0f, breath, 0.0f });
			duckWounded_->SetRotate(kWoundedInsideRot);
			duckWounded_->Update();
		}
		// 前方衛生兵（開口ドア付近で前方を向いて同乗）
		if (duckFrontMedic_)
		{
			duckFrontMedic_->SetTranslate(LocalToHeliWorld(kFrontMedicCabinLocal));
			duckFrontMedic_->SetRotate(kMedicInsideRot);
			duckFrontMedic_->Update();
		}
		// 後方衛生兵（担架横に座りドア側を向いて付き添い）
		if (duckRearMedic_)
		{
			duckRearMedic_->SetTranslate(LocalToHeliWorld(kRearMedicCabinLocal));
			duckRearMedic_->SetRotate(kMedicInsideRot);
			duckRearMedic_->Update();
		}
	}

	// -------------------------------------------------------------------------
	// 3. 地形・ヘリパッド・遠景樹木の更新
	// -------------------------------------------------------------------------
	if (groundModel_) groundModel_->Update();
	if (helipadModel_) helipadModel_->Update();
	for (auto& prop : props_)
	{
		if (prop.model) prop.model->Update();
	}

	// -------------------------------------------------------------------------
	// 4. 映画的カメラのリアルタイム追従（毎フレーム確実に位置・角度・画角を反映）
	// -------------------------------------------------------------------------
	if (camera_)
	{
		Vector3 camPos = { -0.2f, 2.0f, -7.2f };
		Vector3 camRot = { 0.10f, 0.12f, 0.0f };
		float camFov = 0.55f;

		if (state_ == State::Takeoff)
		{
			float t = std::clamp(actionTimer_ / kTakeoffDuration, 0.0f, 1.0f);
			if (t > 0.15f)
			{
				float lookUpT = (t - 0.15f) / 0.85f;
				camRot.x -= lookUpT * 0.32f;
				camRot.y += lookUpT * 0.08f;
				camPos.y += lookUpT * 1.8f;
			}

			// 緊急離陸のダウンウォッシュ・高回転ローター振動演出（Takeoff中のミリタリー臨場感シェイク）
			float shakeFactor = (1.0f - t * 0.75f);
			camPos.x += std::sin(actionTimer_ * 52.0f) * 0.022f * shakeFactor;
			camPos.y += std::cos(actionTimer_ * 64.0f) * 0.016f * shakeFactor;
		}

		camera_->SetTranslate(camPos);
		camera_->SetRotate(camRot);
		camera_->SetFovY(camFov);
		camera_->Update();
	}
}

// -----------------------------------------------------------------------------
// 3D 描画（DirectX12 ネイティブ並列描画パイプライン）
// -----------------------------------------------------------------------------

void GameOverScene::Draw(SceneRenderRequests& renderRequests)
{
	DirectXCom* dx = GetDirectXCom();
	Object3dCom* com = GetObject3dCom();
	if (!dx || !com || !camera_) return;

	renderRequests.sceneDrawn = true;
	camera_->Update();

	// スカイボックスの描画
	if (SceneManager::GetInstance())
	{
		SceneManager::GetInstance()->DrawSkybox(dx->GetCommandList().Get());
	}

	RenderContext base{};
	base.commandList = dx->GetCommandList().Get();
	base.windowAPI = dx->GetWindowAPI();
	base.camera = camera_;
	base.light = SceneManager::GetInstance() ? SceneManager::GetInstance()->GetLight() : nullptr;
	MaterialManager* matMgr = GetMaterialManager();
	if (matMgr && matMgr->GetMaterialResource())
	{
		base.materialGPUAddress = matMgr->GetMaterialResource()->GetGPUVirtualAddress();
	}

	// 1. 地面 & 脱出ヘリパッド
	DrawModel(com, groundModel_.get(), base, groundTexIndex_);
	DrawModel(com, helipadModel_.get(), base, helipadTexIndex_);

	// 2. 遠景樹木
	for (auto& prop : props_)
	{
		DrawModel(com, prop.model.get(), base, prop.texIndex);
	}

	// 3. 担架チーム（救難衛生兵2羽 ＋ 担架ベッド ＋ 負傷したプレイヤー主役アヒル）
	DrawModel(com, stretcherBed_.get(), base, stretcherTexIndex_);
	DrawModel(com, duckRearMedic_.get(), base, duckMedicTexIndex_);   // 搬送衛生兵（ホワイトダック＆赤レスキューベスト）
	DrawModel(com, duckFrontMedic_.get(), base, duckMedicTexIndex_);  // 搬送衛生兵（ホワイトダック＆赤レスキューベスト）
	DrawModel(com, duckWounded_.get(), base, duckTexIndex_);          // プレイヤー主人公（自アバター：イエローダック＆頭部包帯ガーゼ）

	// 4. 救難ヘリコプター
	DrawModel(com, heliBody_.get(), base, heliTexIndex_);
	DrawModel(com, heliCargoDoor_.get(), base, heliTexIndex_);
	if (state_ == State::Takeoff)
	{
		DrawModel(com, heliMainRotorBlur2_.get(), base, heliTexIndex_);
		DrawModel(com, heliMainRotorBlur1_.get(), base, heliTexIndex_);
	}
	DrawModel(com, heliMainRotor_.get(), base, heliTexIndex_);
	DrawModel(com, heliTailRotor_.get(), base, heliTexIndex_);
}

// -----------------------------------------------------------------------------
// 2D 映画的HUD ＆ 戦績レポート描画（ImGui による高品位タイポグラフィ）
// -----------------------------------------------------------------------------

void GameOverScene::DrawScreen()
{
#ifdef USE_IMGUI
	// シーン切り替えトランジション中（シャッターが開く前）は一切の文字・UIを描画しない
	const bool isTransitioning = 
		(DeathTransition::GetInstance() && DeathTransition::GetInstance()->IsTransitioning()) ||
		(StingerTransition::GetInstance() && StingerTransition::GetInstance()->IsTransitioning());
	if (isTransitioning) return;

	ImGuiIO& io = ImGui::GetIO();
	const float sw = io.DisplaySize.x;
	const float sh = io.DisplaySize.y;
	if (sw <= 0.0f || sh <= 0.0f) return;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	ImFont* fontDefault   = io.FontDefault ? io.FontDefault : ImGui::GetFont();
	ImFont* fontTitle     = (io.Fonts && io.Fonts->Fonts.Size > 1) ? io.Fonts->Fonts[1] : fontDefault;
	ImFont* fontSub       = (io.Fonts && io.Fonts->Fonts.Size > 2) ? io.Fonts->Fonts[2] : fontDefault;
	ImFont* fontTacticalL = (io.Fonts && io.Fonts->Fonts.Size > 3) ? io.Fonts->Fonts[3] : fontDefault;
	ImFont* fontTacticalS = (io.Fonts && io.Fonts->Fonts.Size > 4) ? io.Fonts->Fonts[4] : fontDefault;
	ImFont* fontTacticalM = (io.Fonts && io.Fonts->Fonts.Size > 5) ? io.Fonts->Fonts[5] : fontDefault;

	const float centerX = sw * 0.5f;

	// シャッターが完全に開いた後、一呼吸おいてから厳かにフェードイン
	// (0.0s〜0.20s: 映像を見せる静寂の間, 0.20s〜1.00s: 「作戦失敗」が滑らかにフェードイン)
	const float enterT = std::clamp((timer_ - 0.20f) / 0.80f, 0.0f, 1.0f);

	// -------------------------------------------------------------------------
	// 0. 重厚なシネマティック・ビネット（周辺減光）
	// -------------------------------------------------------------------------
	if (vignetteTexIndex_ != UINT32_MAX)
	{
		ImTextureID vigID = (ImTextureID)(intptr_t)TextureManager::GetInstance()->GetSrvHandleGPU(vignetteTexIndex_).ptr;
		dl->AddImage(vigID, ImVec2(0.0f, 0.0f), ImVec2(sw, sh), ImVec2(0, 0), ImVec2(1, 1), WithAlpha(IM_COL32(255, 255, 255, 220), enterT));
	}

	// -------------------------------------------------------------------------
	// 1. シネマスコープ黒帯（上下の映画的レターボックス）
	// -------------------------------------------------------------------------
	const float barHeight = sh * 0.065f;
	dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(sw, barHeight), IM_COL32(8, 10, 14, 255));
	dl->AddRectFilled(ImVec2(0.0f, sh - barHeight), ImVec2(sw, sh), IM_COL32(8, 10, 14, 255));
	dl->AddLine(ImVec2(0.0f, barHeight), ImVec2(sw, barHeight), WithAlpha(IM_COL32(35, 48, 64, 180), enterT), 1.0f);
	dl->AddLine(ImVec2(0.0f, sh - barHeight), ImVec2(sw, sh - barHeight), WithAlpha(IM_COL32(35, 48, 64, 180), enterT), 1.0f);

	// -------------------------------------------------------------------------
	// 2. 画面中央：ダークソウル／映画タイトル調の重厚かつ静寂なタイトル（四角い枠や小窓は一切出さない）
	// -------------------------------------------------------------------------
	{
		const float fadeAlpha = EaseInOutCubic(enterT);
		const float titleY = sh * 0.38f;

		// 主見出し「作　戦　失　敗」（威厳ある文字間隔と端正な56px BIZ UDゴシック Bold）
		const char* titleText = (const char*)u8"作　戦　失　敗";

		// 映画調の重厚な漆黒ドロップシャドウ（多方向オフセット）
		AddCenteredText(dl, fontTitle, 56.0f, centerX, titleY + 3.0f, WithAlpha(IM_COL32(0, 0, 0, 240), fadeAlpha), titleText);
		AddCenteredText(dl, fontTitle, 56.0f, centerX + 1.5f, titleY + 1.5f, WithAlpha(IM_COL32(0, 0, 0, 220), fadeAlpha), titleText);
		AddCenteredText(dl, fontTitle, 56.0f, centerX - 1.5f, titleY + 1.5f, WithAlpha(IM_COL32(0, 0, 0, 220), fadeAlpha), titleText);

		// 本体（凛とした静寂のボーンホワイト）
		AddCenteredText(dl, fontTitle, 56.0f, centerX, titleY, WithAlpha(IM_COL32(248, 250, 252, 255), fadeAlpha), titleText);

		// 極細ヘアライン区切り（上品なクリムゾン／スレートの静かなアクセント）
		dl->AddLine(ImVec2(centerX - 170.0f, titleY + 68.0f), ImVec2(centerX + 170.0f, titleY + 68.0f),
			WithAlpha(IM_COL32(185, 28, 28, 175), fadeAlpha), 1.0f);

		// 日本語サブ見出し「緊　急　救　難　・　後　方　搬　送」（端正なBIZ UDゴシック）
		const char* subStatus = (const char*)u8"緊　急　救　難　・　後　方　搬　送";
		AddCenteredText(dl, fontDefault, 16.0f, centerX, titleY + 76.0f,
			WithAlpha(IM_COL32(180, 195, 215, 230), fadeAlpha), subStatus);
	}

	// -------------------------------------------------------------------------
	// 3. 下部シネマスコープ黒帯：映画のエンドロール調の静かで控えめな操作案内
	// -------------------------------------------------------------------------
	const float promptY = sh - barHeight * 0.5f - 8.0f;

	if (state_ == State::Approaching)
	{
		const float blink = 0.75f + 0.25f * std::sin(timer_ * 4.5f);
		const char* promptText = (const char*)u8"SPACE : 再出撃         T : タイトル画面へ";
		AddCenteredText(dl, fontDefault, 16.0f, centerX, promptY, WithAlpha(IM_COL32(215, 225, 235, 230), enterT * blink), promptText);
	}
	else if (state_ == State::Loading)
	{
		const char* loadMsg = (const char*)u8"負傷兵を救難ヘリへ収容中...";
		AddCenteredText(dl, fontDefault, 16.0f, centerX, promptY, IM_COL32(251, 191, 36, 240), loadMsg);
	}
	else if (state_ == State::Ready)
	{
		const float blink = 0.70f + 0.30f * std::sin(timer_ * 6.0f);
		const char* readyMsg = (const char*)u8"収容完了  |  SPACE : 離陸して再出撃         T : タイトル画面へ";
		AddCenteredText(dl, fontDefault, 16.0f, centerX, promptY, WithAlpha(IM_COL32(74, 222, 128, 255), enterT * blink), readyMsg);
	}
	else if (state_ == State::Takeoff)
	{
		static const char* kDots[] = { "", ".", "..", "..." };
		const int dotIndex = static_cast<int>(actionTimer_ * 5.0f) % 4;
		char buf[128];
		if (actionTimer_ < kDoorCloseDuration)
		{
			sprintf_s(buf, (const char*)u8"カーゴドア閉鎖中... %d%%", static_cast<int>(doorSlideProgress_ * 100.0f));
		}
		else
		{
			sprintf_s(buf, (const char*)u8"救難ヘリ離陸中  前線から緊急脱出%s", kDots[dotIndex]);
		}
		AddCenteredText(dl, fontDefault, 16.0f, centerX, promptY, IM_COL32(203, 213, 225, 240), buf);
	}

	// -------------------------------------------------------------------------
	// 4. 離陸時の退場フェード
	// -------------------------------------------------------------------------
	if (state_ == State::Takeoff)
	{
		const float fadeT = std::clamp((actionTimer_ - kTakeoffDuration * 0.55f) / (kTakeoffDuration * 0.45f), 0.0f, 1.0f);
		if (fadeT > 0.0f)
		{
			dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(sw, sh), WithAlpha(IM_COL32(8, 10, 14, 255), fadeT));
		}
	}
#endif
}
