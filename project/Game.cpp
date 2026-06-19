#include"Game.h"
#include "DebugUI.h"

#include <combaseapi.h>

#include <sstream>
#include <iomanip>

#include "Baziru3_Engine\Graphics\SceneRenderRequests.h"
#include"RenderContext.h"
#include"RootParam.h"
#include"SubsystemFactory.h"
#include "CustomObject3dRenderer.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#endif

void Game::Initialize()
{

	Framework::Initialize();
	crashDump.Install();
	log.Initialize();

	InitializeEngine();

	auto* dx = engine_->GetDirectXCom();
	auto* window = engine_->GetWindowAPI();
	auto* spriteCom = engine_->GetSpriteCom();


	LogEngineDiagnostics();

	InitializeSceneCore();
	CustomObject3dRenderer::GetInstance()->Initialize(dx);

	InitializeModelResources();

	InitializeSceneResources();

	//Transform変数を作る
	Sprite::Transform transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	//Sphere用
	transformObject = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	{
     Sprite::Transform t = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
		if (auto sp = Sprite::Create(spriteCom, t, "Resources/uvChecker.png"))
		{
			sprites.emplace_back(std::move(sp));
		}
		else
		{
			Logger::Log(logStream, "Failed to create sprite: Resources/uvChecker.png\n");
		}

		// Create a small cursor sprite and keep its index
		Sprite::Transform tc = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
		if (auto cursor = Sprite::Create(spriteCom, tc, "Resources/CG4/circle2.png"))
		{
            cursor->SetSize({32.0f, 32.0f});
			// Set anchor to top-left so sprite's top-left aligns with mouse position
			cursor->SetAnchorPoint({0.0f, 0.0f});
			sprites.emplace_back(std::move(cursor));
			cursorSpriteIndex = static_cast<int>(sprites.size()) - 1;
		}
	}

	InitializeAudioAndInput();


	object3dCom->SetDefaultCamera(camera_.get());

	imguiManager = std::make_unique<ImGuiManager>();
	imguiManager->Initialize(window, dx);

	debugCamera_.Initialize(window);

	SpriteManager* uiSpriteManager = engine_ ? engine_->GetSpriteManager() : nullptr;
	debugUI = std::make_unique<DebugUI>(materialManager_.get(), uiSpriteManager, camera_.get(), &transformObject, &useMonsterBall, &drawObject, &drawSprite, &debugCamera_);
	debugUI->Initialize();

	fadeApplication_ = std::make_unique<FadeApplication>();
	fadeApplication_->Initialize(spriteCom, window);
	SceneManager::GetInstance()->SetFadeApplication(fadeApplication_.get());

	SceneRegistration::RegisterScenes();
	SceneManager::GetInstance()->ChangeScene("TITLE");

	textureIndexUvChecker = TextureManager::GetInstance()->Load("Resources/uvChecker.png"); // Load UV Checker texture
	textureIndexModelTex = TextureManager::GetInstance()->Load(modelData.material.textureFilePath); // Load model texture
	textureIndexSkybox_ = TextureManager::GetInstance()->Load("Resources/CG4/dds/CG4_test.dds"); // Load skybox texture
   SceneManager::GetInstance()->SetSkyboxTextureIndex(textureIndexSkybox_);
}


void Game::Finalize()
{
	CustomObject3dRenderer::GetInstance()->Finalize();
	// ImGuiの終了処理
#ifdef USE_IMGUI
	if (imguiManager)
	{
		try { imguiManager->Finalize(); }
		catch (...) { Logger::Log(logStream, "imgui finalize failed\n"); }
		imguiManager.reset();
	}
#endif

	// DebugUIの終了処理
	if (debugUI)
	{
		debugUI.reset();
	}

	if (SceneManager::GetInstance())
	{
		SceneManager::GetInstance()->SetFadeApplication(nullptr);
	}

	if (fadeApplication_)
	{
		fadeApplication_->Finalize();
		fadeApplication_.reset();
	}

	// AudioManagerの終了処理
	if (audioManager_)
	{
		if (SceneManager::GetInstance()) SceneManager::GetInstance()->SetAudioManager(nullptr);
		try { audioManager_->Finalize(); }
		catch (...) { Logger::Log(logStream, "audio finalize failed\n"); }
		audioManager_.reset();
	}

	// 3) Unregister SceneManager references (ensure SceneManager still exists)
	if (SceneManager::GetInstance())
	{
		SceneManager::GetInstance()->SetParticleManager(nullptr);
		SceneManager::GetInstance()->SetSpriteCom(nullptr); // owned by EngineContext but unregister anyway
		SceneManager::GetInstance()->SetMaterialManager(nullptr);
		SceneManager::GetInstance()->SetCamera(nullptr);
		SceneManager::GetInstance()->SetLight(nullptr);
		SceneManager::GetInstance()->SetObject3dCom(nullptr);
       SceneManager::GetInstance()->SetSkyboxCom(nullptr);
		SceneManager::GetInstance()->SetSkyBox(nullptr);
		SceneManager::GetInstance()->SetSkyboxTextureIndex(TextureManager::kInvalidTextureIndex);
	}

	// 4) Particle manager
	if (particleManager)
	{
		try { particleManager->Finalize(); }
		catch (...) { Logger::Log(logStream, "particle finalize failed\n"); }
		particleManager.reset();
	}

	// 5) Game-owned sprites
	for (auto& sp : sprites)
	{
		if (sp)
		{
			try { sp->Finalize(); }
			catch (...) { Logger::Log(logStream, "sprite finalize failed\n"); }
		}
	}
	sprites.clear();

	// 6) Material / Models / Skybox / Object3d / Camera / Light
	if (materialManager_)
	{
		try { materialManager_->Finalize(); }
		catch (...) { Logger::Log(logStream, "material finalize failed\n"); }
		materialManager_.reset();
	}

	if (model_) { model_.reset(); }
	if (modelCom_) { modelCom_.reset(); }

	if (skybox_) { skybox_.reset(); }
	if (skyboxCom_) { skyboxCom_.reset(); }

	if (object3d_) { object3d_.reset(); }
	if (object3dCom) { object3dCom.reset(); }

	if (camera_)
	{
		try { camera_->Finalize(); }
		catch (...) { Logger::Log(logStream, "camera finalize failed\n"); }
		camera_.reset();
	}

	if (light) { light.reset(); }

	// 7) Ensure TextureManager releases GPU resources before engine teardown
	try { TextureManager::GetInstance()->Finalize(); }
	catch (...) { Logger::Log(logStream, "TextureManager finalize failed\n"); }

	// 8) Engine teardown
	if (engine_)
	{
		try { engine_->Finalize(); }
		catch (...) { Logger::Log(logStream, "engine finalize failed\n"); }
		engine_.reset();
	}

	// 9) Finally destroy SceneManager and framework
	SceneManager::Destroy();

	Framework::Finalize();
}

void Game::Update()
{
	Framework::Update();

	if (audioManager_)
	{
		audioManager_->Update();
	}

	if (fadeApplication_)
	{
		fadeApplication_->Update();
	}


    // Update scenes and engine subsystems. Use fixed timestep here (same as scenes expect).
	SceneManager::GetInstance()->Update(1.0f / 60.0f);

	//if (windowAPI->ProcessMassage())
	//{
	//	//ゲームループ抜ける
	//	break;
	//}


	//ImGuiにここからフレームが始まる趣旨をつたえる
	imguiManager->Update();

	debugCamera_.Update();

	camera_->Update();
	if (skybox_)
	{
		skybox_->Update();
	}

	object3d_->SetRotate(transformObject.rotate);
	object3d_->Update();



#ifdef USE_IMGUI
	if (debugUI)
	{
		debugUI->Update();
	}
#endif



	inputManager.Update();

	// Update mouse input and move cursor sprite
	mouseInput.Update();
    if (cursorSpriteIndex >= 0 && cursorSpriteIndex < static_cast<int>(sprites.size()))
	{
		auto* cur = sprites[cursorSpriteIndex].get();
		if (cur)
		{
            // float の Vector2 に変換
            Vector2 pos{ static_cast<float>(mouseInput.GetX()), static_cast<float>(mouseInput.GetY()) };
            // マウスのクライアント座標をスプライト投影空間にスケーリングします。
			// スプライト投影は WindowAPI::GetClientWidth()/GetClientHeight() を使用します。
			if (engine_ && engine_->GetWindowAPI())
			{
				RECT rc{};
				if (GetClientRect(engine_->GetWindowAPI()->GetHwnd(), &rc))
				{
					float clientW = float(rc.right - rc.left);
					float clientH = float(rc.bottom - rc.top);
					if (clientW > 0.0f && clientH > 0.0f)
					{
						float sx = float(engine_->GetWindowAPI()->GetClientWidth()) / clientW;
						float sy = float(engine_->GetWindowAPI()->GetClientHeight()) / clientH;
						pos.x *= sx;
						pos.y *= sy;
					}
				}
			}

			cur->SetPosition(pos);
            // 軽量な変換行列更新
			cur->UpdateTransformOnly(engine_ ? engine_->GetWindowAPI() : nullptr);
            // 左ボタン押下中は赤にする
			if (mouseInput.PushButton(0))
			{
				cur->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
			}
			else
			{
				cur->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			}
		}
	}

	//// ImGuiを使わずにSpaceキーでパーティクルを追加
	//if (inputManager.TriggerKey(DIK_SPACE))
	//{
	//	particles.splice(particles.end(), ParticleEmitter{}.Emit(emitter, particleManager->GetRandomEngine(), *particleManager));
	//}
}

void Game::Draw()
{
	auto* dx = engine_ ? engine_->GetDirectXCom() : nullptr;
	auto* window = engine_ ? engine_->GetWindowAPI() : nullptr;

	if (dx) dx->PreDraw();

	if (object3dCom) object3dCom->PreDraw();

	RenderContext ctx = PrepareRenderContext();
    SceneRenderRequests renderRequests{};

	if (camera_ && camera_->GetCameraResource() && dx)
	{
		dx->GetCommandList()->SetGraphicsRootConstantBufferView(4, camera_->GetCameraResource()->GetGPUVirtualAddress());
	}
	else
	{
		Logger::Log(logStream, "Warning: camera GPU resource not available before SceneManager draw.\n");
	}

    // object3dCom PreDraw already called above

	if (SceneManager::GetInstance())
	{
        SceneManager::GetInstance()->Draw(renderRequests);
	}

	sphereRenderer_.Draw(ctx, renderRequests);



	if (drawObject)
	{
		if (object3dCom && object3d_)
		{
			CustomObject3dRenderer::GetInstance()->Draw(object3d_.get(), ctx, modelData, drawObject);
		}
	}
    DrawSprites(ctx);


	if (renderRequests.sceneDrawn)
	{
		DrawParticles(ctx);
	}
	if (fadeApplication_)
	{
		fadeApplication_->Draw();
	}

	//Objectの描画

	{
		// Removed debug print to reduce game loop log spam
	}

	//実際のcommandListのImGuiの描画コマンドを積む
#ifdef USE_IMGUI
	if (imguiManager) imguiManager->Render();
	if (dx) ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx->GetCommandList().Get());
#endif

	if (dx) dx->PostDraw();
}

bool Game::IsQuitRequested()
{
	auto* dx = engine_ ? engine_->GetDirectXCom() : nullptr;
	if (dx)
	{
		return (dx->GetMsg().message == WM_QUIT);
	}
	return false;
}

bool Game::InitializeEngine()
{
	engine_ = std::make_unique<EngineContext>();

	if (!engine_->Initialize(logStream, InitConfig{}))
	{
		Logger::Log(logStream, "EngineContext initialization failed. Check previous logs for details.\n");
		return false;
	}

	if (!engine_->GetDirectXCom())
	{
		Logger::Log(logStream, "Error: EngineContext initialized but DirectXCom is null.\n");
		return false;
	}

	return true;
}

void Game::LogEngineDiagnostics()
{
	auto* dx = engine_->GetDirectXCom();

	{
		std::ostringstream oss;
		oss << "Diagnostics: DirectXCom=" << std::hex << (uintptr_t)dx;
		oss << " device=" << (uintptr_t)(dx ? dx->GetDevice().Get() : nullptr);
		oss << " commandList=" << (uintptr_t)(dx ? dx->GetCommandList().Get() : nullptr) << std::dec << "\n";
		Logger::Log(logStream, oss.str());
	}
}

void Game::InitializeSceneCore()
{
	auto* dx = engine_->GetDirectXCom();

	SceneManager::GetInstance()->Initialize(engine_->GetDirectXCom());

	// 既存の手動テクスチャ読み込みはそのまま利用（Sphere用）

	object3dCom = std::make_unique<Object3dCom>(logStream);
	object3dCom->Initialize(dx);

	SceneManager::GetInstance()->SetObject3dCom(object3dCom.get());
}

void Game::InitializeModelResources()
{
	auto* dx = engine_->GetDirectXCom();

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(object3dCom.get(), object3d_->LoadObjFile("Resources", "plane.obj"));

	//パイプラインステートの生成に失敗した場合はエラー
	assert(SUCCEEDED(dx->GetHr()));

	//モデル読み込み
	modelData = object3d_->LoadObjFile("Resources", "plane.obj");

	// Model を作成して初期化（Model が自分で頂点リソースを作る）
	modelCom_ = std::make_unique<ModelCom>();
	modelCom_->Initialize(dx);
	model_ = std::make_unique<Model>();
	model_->Initialize(modelCom_.get(), "Resources", "plane.obj");
}

void Game::InitializeSceneResources()
{
	auto* dx = engine_->GetDirectXCom();

	materialManager_ = std::make_unique<MaterialManager>();
	materialManager_->Initialize(dx);
	SceneManager::GetInstance()->SetMaterialManager(materialManager_.get());

	light = std::make_unique<Light>();
	light->Initialize(dx);
	SceneManager::GetInstance()->SetLight(light.get());

	camera_ = std::make_unique<Camera>();
	camera_->Initialize(dx);
	// 斜め見下ろしカメラ設定（Escape from Dakkof スタイル）
	// カメラを斜め上後方に配置し、X軸で約45度下向きに傾ける
	camera_->SetTranslate({ 0.0f, 20.0f, -20.0f });
	camera_->SetRotate({ 0.785f, 0.0f, 0.0f });
	SceneManager::GetInstance()->SetCamera(camera_.get());

	skyboxCom_ = std::make_unique<SkyboxCom>(logStream, dx);
	skyboxCom_->Initialize();
	skybox_ = std::make_unique<SkyBox>();
	skybox_->Initialize(dx, camera_.get());
	SceneManager::GetInstance()->SetSkyboxCom(skyboxCom_.get());
	SceneManager::GetInstance()->SetSkyBox(skybox_.get());

	particleManager = std::make_unique<ParticleManager>(logStream, dx);
	particleManager->Initialize(camera_.get());
	SceneManager::GetInstance()->SetParticleManager(particleManager.get());

	// Load example particle textures so they are available early
	TextureManager::GetInstance()->Load("Resources/uvChecker.png");
	// Load circle texture used by GamePlayScene for alternate particle appearance
	TextureManager::GetInstance()->Load("Resources/CG4/circle2.png");

}

void Game::InitializeAudioAndInput()
{
	auto* window = engine_ ? engine_->GetWindowAPI() : nullptr;


	//音声読み込み
	audioManager_ = std::make_unique<AudioManager>(logStream);
	audioManager_->Initialize();
	SceneManager::GetInstance()->SetAudioManager(audioManager_.get());


	inputManager.Initialize(window);
	// initialize mouse input
	mouseInput.Initialize(window);


	debugCamera_.Initialize(window);
}

void Game::DrawObjects(const RenderContext& ctx)
{
	if (ctx.textureHandle.ptr != 0)
	{
		ctx.commandList->SetGraphicsRootDescriptorTable(2, ctx.textureHandle);
	}

	if (ctx.light)
	{
		ctx.commandList->SetGraphicsRootConstantBufferView(3, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
	}
	else
	{
		ctx.commandList->SetGraphicsRootConstantBufferView(3, 0);
	}

	if (ctx.camera && ctx.camera->GetCameraResource())
	{
		ctx.commandList->SetGraphicsRootConstantBufferView(4, ctx.camera->GetCameraResource()->GetGPUVirtualAddress());
	}
	else
	{
		Logger::Log(logStream, "Warning: camera GPU resource not available when drawing object.\n");
		return;
	}

	object3d_->Draw(ctx.commandList);

	if (drawObject)
	{
		ctx.commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
	}
}

void Game::DrawSprites(const RenderContext& ctx)
{
	if (!drawSprite) return;


	SpriteManager* sm = engine_ ? engine_->GetSpriteManager() : nullptr;
    if (sm)
	{
		// external sprites updated separately for performance
		sm->DrawAll(ctx, &debugCamera_, &sprites, false);
	}
}

void Game::DrawParticles(const RenderContext& ctx)
{
	particleRenderer_.Draw(ctx, particleManager.get(), model_.get(), UINT(modelData.vertices.size()));
}

RenderContext Game::PrepareRenderContext()
{
	RenderContext ctx{};
	auto* dx = engine_ ? engine_->GetDirectXCom() : nullptr;
	auto* window = engine_ ? engine_->GetWindowAPI() : nullptr;

	ctx.commandList = dx ? dx->GetCommandList().Get() : nullptr;
	ctx.windowAPI = window;
	ctx.camera = camera_.get();
	ctx.light = light.get();

	uint32_t chosenIndex = useMonsterBall ? textureIndexModelTex : textureIndexUvChecker;
	if (chosenIndex != TextureManager::kInvalidTextureIndex)
	{
		ctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(chosenIndex);
	}
	else
	{
		Logger::Log(logStream, "Warning: invalid texture index when preparing render context for drawing.\n");
		ctx.textureHandle = {};
	}

	ctx.materialGPUAddress = (materialManager_ && materialManager_->GetMaterialResource())
		? materialManager_->GetMaterialResource()->GetGPUVirtualAddress()
		: 0;

	return ctx;
}


