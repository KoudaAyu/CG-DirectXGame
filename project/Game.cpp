#include"Game.h"
#include "DebugUI.h"

#include <combaseapi.h>

#include <sstream>
#include <iomanip>

#include "Baziru3_Engine\Graphics\SceneRenderRequests.h"
#include"RenderContext.h"
#include"RootParam.h"
#include"SubsystemFactory.h"

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
	}

	InitializeAudioAndInput();


	object3dCom->SetDefaultCamera(camera_.get());

	imguiManager = std::make_unique<ImGuiManager>();
	imguiManager->Initialize(window, dx);


	SpriteManager* uiSpriteManager = engine_ ? engine_->GetSpriteManager() : nullptr;
	debugUI = std::make_unique<DebugUI>(materialManager_.get(), uiSpriteManager, camera_.get(), &transformObject, &useMonsterBall, &drawObject, &drawSprite);
	debugUI->Initialize();

	SceneRegistration::RegisterScenes();
	SceneManager::GetInstance()->ChangeScene("TITLE");

	textureIndexUvChecker = TextureManager::GetInstance()->Load("Resources/uvChecker.png"); // Load UV Checker texture
	textureIndexModelTex = TextureManager::GetInstance()->Load(modelData.material.textureFilePath); // Load model texture
	textureIndexSkybox_ = TextureManager::GetInstance()->Load("Resources/CG4/dds/CG4_test.dds"); // Load skybox texture

	// OffScreenRendering の初期化
	offScreenRendering_ = std::make_unique<OffScreenRendering>(logStream, dx);
	offScreenRendering_->Initialize(0, 0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, { 1.0f, 0.0f, 0.0f, 1.0f });
}


void Game::Finalize()
{
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

	// OffScreenRendering の終了処理
	if (offScreenRendering_)
	{
		offScreenRendering_->Finalize();
		offScreenRendering_.reset();
	}

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


	SceneManager::GetInstance()->Update();

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



#ifdef _DEBUG


	if (debugUI)
	{
		debugUI->Update();
	}


#endif // DEBUG

	//ImGui内部コマンドを生成する
#ifdef USE_IMGUI
	if (imguiManager) imguiManager->Render();
#endif

	inputManager.Update();

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

	if (offScreenRendering_)
	{
		offScreenRendering_->Begin(dx->GetCommandList().Get());
	}

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

	if (skybox_ && skyboxCom_ && textureIndexSkybox_ != TextureManager::kInvalidTextureIndex)
	{
		skyboxCom_->SetupDraw(ctx.commandList);
		//skybox_->Draw(ctx.commandList, TextureManager::GetInstance()->GetSrvHandleGPU(textureIndexSkybox_));
	}

	if (object3dCom) object3dCom->PreDraw();

	if (SceneManager::GetInstance())
	{
        SceneManager::GetInstance()->Draw(renderRequests);
	}
  sphereRenderer_.Draw(ctx, renderRequests);



	if (drawObject)
	{
		if (object3dCom && object3d_)
		{
			object3dCom->Draw(object3d_.get(), ctx, modelData, drawObject);
		}
	}
	//DrawSprites(ctx);
	//DrawParticles(ctx);

	//Objectの描画

	{
		uint32_t chosenIndex = useMonsterBall ? textureIndexModelTex : textureIndexUvChecker;
		D3D12_GPU_DESCRIPTOR_HANDLE chosenHandle{};
		if (chosenIndex != TextureManager::kInvalidTextureIndex)
		{
			chosenHandle = TextureManager::GetInstance()->GetSrvHandleGPU(chosenIndex);
		}
		else
		{
			Logger::Log(logStream, "Warning: invalid texture index when selecting object texture.\n");
		}

		std::ostringstream oss;
		oss << "Game::Draw - object texture index=" << std::dec << chosenIndex
			<< " handle=0x" << std::hex << (unsigned long long)chosenHandle.ptr << std::dec << "\n";
		OutputDebugStringA(oss.str().c_str());
	}

	//実際のcommandListのImGuiの描画コマンドを積む
#ifdef USE_IMGUI
	if (offScreenRendering_)
	{
		offScreenRendering_->End(dx->GetCommandList().Get());
		offScreenRendering_->SetMainRenderTarget(dx->GetCommandList().Get());
		offScreenRendering_->DrawToBackBuffer(dx->GetCommandList().Get());
	}

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
	SceneManager::GetInstance()->SetCamera(camera_.get());

	skyboxCom_ = std::make_unique<SkyboxCom>(logStream, dx);
	skyboxCom_->Initialize();
	skybox_ = std::make_unique<SkyBox>();
	skybox_->Initialize(dx, camera_.get());

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
		sm->DrawAll(ctx, &debugCamera_, &sprites);
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


