#include"Game.h"
#include "DebugUI.h"

#include <combaseapi.h>

#include <sstream>
#include <iomanip>

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

	engine_ = std::make_unique<EngineContext>();

	if (!engine_->Initialize(logStream, InitConfig{}))
	{
		Logger::Log(logStream, "EngineContext initialization failed. Check previous logs for details.\n");
		return;
	}
	
	if (!engine_->GetDirectXCom())
	{
		Logger::Log(logStream, "Error: EngineContext initialized but DirectXCom is null.\n");
		return;
	}


	{
		auto dx = engine_->GetDirectXCom();
		std::ostringstream oss;
		oss << "Diagnostics: DirectXCom=" << std::hex << (uintptr_t)dx;
		oss << " device=" << (uintptr_t)(dx ? dx->GetDevice().Get() : nullptr);
		oss << " commandList=" << (uintptr_t)(dx ? dx->GetCommandList().Get() : nullptr) << std::dec << "\n";
		Logger::Log(logStream, oss.str());
	}


	SceneManager::GetInstance()->Initialize(engine_->GetDirectXCom());


	// 既存の手動テクスチャ読み込みはそのまま利用（Sphere用）

	object3dCom = std::make_unique<Object3dCom>(logStream);
	object3dCom->Initialize(GetDirectXCom());


	SceneManager::GetInstance()->SetObject3dCom(object3dCom.get());

#pragma region 最初のシーンの初期化
	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(object3dCom.get(), object3d_->LoadObjFile("Resources", "plane.obj"));
#pragma endregion 最初のシーン終了
	//パイプラインステートの生成に失敗した場合はエラー
	assert(SUCCEEDED(GetDirectXCom()->GetHr()));

	//モデル読み込み
	modelData = object3d_->LoadObjFile("Resources", "plane.obj");

	// Model を作成して初期化（Model が自分で頂点リソースを作る）
	modelCom_ = std::make_unique<ModelCom>();
	modelCom_->Initialize(GetDirectXCom());
	model_ = std::make_unique<Model>();
	model_->Initialize(modelCom_.get(), "Resources", "plane.obj");



	materialManager_ = std::make_unique<MaterialManager>();
	materialManager_->Initialize(GetDirectXCom());
	SceneManager::GetInstance()->SetMaterialManager(materialManager_.get());

	light = std::make_unique<Light>();
	light->Initialize(GetDirectXCom());
	SceneManager::GetInstance()->SetLight(light.get());

	camera_ = std::make_unique<Camera>();
	camera_->Initialize(GetDirectXCom());
	SceneManager::GetInstance()->SetCamera(camera_.get());

	skyboxCom_ = std::make_unique<SkyboxCom>(logStream, GetDirectXCom());
	skyboxCom_->Initialize();
	skybox_ = std::make_unique<SkyBox>();
	skybox_->Initialize(GetDirectXCom(), camera_.get());

	particleManager = std::make_unique<ParticleManager>(logStream, GetDirectXCom());
	particleManager->Initialize(camera_.get());
	SceneManager::GetInstance()->SetParticleManager(particleManager.get());

	//Transform変数を作る
	Sprite::Transform transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	//Sphere用
	transformObject = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	// ユーザー指定の API 形式でのテクスチャ読み込み＆スプライト生成例
	{
		uint32_t textureHandle = TextureManager::GetInstance()->Load("Resources/uvChecker.png");

		auto sp = std::make_unique<Sprite>();
		sp->Initialize(GetSpriteCom(), "Resources/uvChecker.png");

		sprites.emplace_back(std::move(sp));
	}


	//音声読み込み
	audioManager_ = std::make_unique<AudioManager>(logStream);
	audioManager_->Initialize();
	SceneManager::GetInstance()->SetAudioManager(audioManager_.get());


	inputManager.Initialize(GetWindowAPI());


	debugCamera_.Initialize(GetWindowAPI());

	object3dCom->SetDefaultCamera(camera_.get());

	imguiManager = std::make_unique<ImGuiManager>();
	imguiManager->Initialize(GetWindowAPI(), GetDirectXCom());


	SpriteManager* uiSpriteManager = engine_ && engine_->GetSpriteManager() ? engine_->GetSpriteManager() : spriteManager_.get();
	debugUI = std::make_unique<DebugUI>(materialManager_.get(), uiSpriteManager, camera_.get(), &transformObject, &useMonsterBall, &drawObject, &drawSprite);
	debugUI->Initialize();

	SceneRegistration::RegisterScenes();
	SceneManager::GetInstance()->ChangeScene("TITLE");

	textureIndexUvChecker = TextureManager::GetInstance()->Load("Resources/uvChecker.png"); // Load UV Checker texture
	textureIndexModelTex = TextureManager::GetInstance()->Load(modelData.material.textureFilePath); // Load model texture
	textureIndexSkybox_ = TextureManager::GetInstance()->Load("Resources/CG4/dds/CG4_test.dds"); // Load skybox texture
}


void Game::Finalize()
{
	SceneManager::Destroy();


#ifdef USE_IMGUI
	if (imguiManager) { imguiManager->Finalize(); imguiManager.reset(); }
#endif


	debugUI.reset();


	if (audioManager_)
	{
		SceneManager::GetInstance()->SetAudioManager(nullptr);
		audioManager_->Finalize();
		audioManager_.reset();
	}

	SceneManager::GetInstance()->SetParticleManager(nullptr);
	particleManager.reset();

	SceneManager::GetInstance()->SetSpriteCom(nullptr);
	// SpriteCom/SpriteManager are owned by EngineContext now; do not reset here.

	// Finalize and release any game-owned sprites that reference GPU resources
	for (auto &sp : sprites)
	{
		if (sp)
		{
			sp->Finalize();
		}
	}
	sprites.clear();

	// Ensure TextureManager releases its GPU resources before engine teardown
	TextureManager::GetInstance()->Finalize();

	SceneManager::GetInstance()->SetMaterialManager(nullptr);
	materialManager_.reset();

	model_.reset();
	modelCom_.reset();

	skybox_.reset();
	skyboxCom_.reset();

	object3d_.reset();
	object3dCom.reset();

	SceneManager::GetInstance()->SetCamera(nullptr);
	camera_->Finalize();
	camera_.reset();

	SceneManager::GetInstance()->SetLight(nullptr);
	light.reset();

	if (directXCom)
	{
	}

   
    if (engine_)
    {
        engine_->Finalize();
        engine_.reset();
    }

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
	ImGui::Render();
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

	if (GetDirectXCom()) GetDirectXCom()->PreDraw();

	if (object3dCom) object3dCom->PreDraw();

	RenderContext ctx{};
	ctx.commandList = GetDirectXCom() ? GetDirectXCom()->GetCommandList().Get() : nullptr;
	ctx.windowAPI = GetWindowAPI();
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
	ctx.materialGPUAddress = materialManager_->GetMaterialResource() ? materialManager_->GetMaterialResource()->GetGPUVirtualAddress() : 0;

	if (camera_ && camera_->GetCameraResource() && GetDirectXCom())
	{
		GetDirectXCom()->GetCommandList()->SetGraphicsRootConstantBufferView(4, camera_->GetCameraResource()->GetGPUVirtualAddress());
	}
	else
	{
		Logger::Log(logStream, "Warning: camera GPU resource not available before SceneManager draw.\n");
	}

	if (skybox_ && skyboxCom_ && textureIndexSkybox_ != TextureManager::kInvalidTextureIndex)
	{
		skyboxCom_->SetupDraw(ctx.commandList);
		skybox_->Draw(ctx.commandList, TextureManager::GetInstance()->GetSrvHandleGPU(textureIndexSkybox_));
	}

	if (object3dCom) object3dCom->PreDraw();

	if (SceneManager::GetInstance())
	{
		SceneManager::GetInstance()->Draw();
	}



	if (drawObject)
	{
		if (object3dCom && object3d_)
		{
			object3dCom->Draw(object3d_.get(), ctx, modelData, drawObject);
		}
	}
	DrawSprites(ctx);
	DrawParticles(ctx);

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
	if (GetDirectXCom()) ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GetDirectXCom()->GetCommandList().Get());
#endif

	if (GetDirectXCom()) GetDirectXCom()->PostDraw();
}

bool Game::IsQuitRequested()
{
	auto dx = GetDirectXCom();
	if (dx)
	{
		return (dx->GetMsg().message == WM_QUIT);
	}
	return false;
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

	
	SpriteManager* sm = engine_ ? engine_->GetSpriteManager() : spriteManager_.get();
	if (sm)
	{
		sm->DrawAll(ctx, &debugCamera_, &sprites);
	}
}

void Game::DrawParticles(const RenderContext& ctx)
{

	particleManager->SetupDraw(ctx.commandList);


	model_->Bind(ctx.commandList);

	particleManager->BindResources(ctx.commandList, ctx.materialGPUAddress);


	if (ctx.textureHandle.ptr != 0)
	{
		ctx.commandList->SetGraphicsRootDescriptorTable(RootParam::Particle::kTextureTable, ctx.textureHandle);
	}


	if (ctx.light)
	{
		ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kLight, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
	}
	else
	{
		ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kLight, 0);
	}
	ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kCamera,
		ctx.camera && ctx.camera->GetCameraResource() ? ctx.camera->GetCameraResource()->GetGPUVirtualAddress() : 0);

	if (particleManager->GetNumInstance() > 0)
	{
		ctx.commandList->DrawInstanced(UINT(modelData.vertices.size()), particleManager->GetNumInstance(), 0, 0);
	}
}
