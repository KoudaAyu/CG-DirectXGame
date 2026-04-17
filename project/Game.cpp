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

	InitConfig cfg;
	auto res = SubsystemFactory::InitializeAll(logStream, cfg);
	if (!res.success)
	{
		Logger::Log(logStream, "Engine init failed: " + res.errorMessage + "\n");
		return;
	}

	// 所有権を受け取る
	windowAPI = std::move(res.windowAPI);
	directXCom = std::move(res.directXCom);
	spriteCom = std::move(res.spriteCom);
	spriteManager_ = std::move(res.spriteManager);

	SceneManager::GetInstance()->Initialize(directXCom.get());

	
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
	modelCom_->Initialize(directXCom.get());
	model_ = std::make_unique<Model>();
	model_->Initialize(modelCom_.get(), "Resources", "plane.obj");



	materialManager_ = std::make_unique<MaterialManager>();
	materialManager_->Initialize(directXCom.get());
	SceneManager::GetInstance()->SetMaterialManager(materialManager_.get());

	light = std::make_unique<Light>();
	light->Initialize(directXCom.get());
	SceneManager::GetInstance()->SetLight(light.get());

	camera_ = std::make_unique<Camera>();
	camera_->Initialize(directXCom.get());
	SceneManager::GetInstance()->SetCamera(camera_.get());

	skyboxCom_ = std::make_unique<SkyboxCom>(logStream, directXCom.get());
	skyboxCom_->Initialize();
	skybox_ = std::make_unique<SkyBox>();
	skybox_->Initialize(directXCom.get(), camera_.get());

	particleManager = std::make_unique<ParticleManager>(logStream, directXCom.get());
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
		sp->Initialize(spriteCom.get(), "Resources/uvChecker.png");
		
		sprites.emplace_back(std::move(sp));
	}

	
	//音声読み込み
	audioManager_ = std::make_unique<AudioManager>(logStream);
	audioManager_->Initialize();
	SceneManager::GetInstance()->SetAudioManager(audioManager_.get());


	inputManager.Initialize(windowAPI.get());


	debugCamera_.Initialize(windowAPI.get());

	object3dCom->SetDefaultCamera(camera_.get());

	imguiManager = std::make_unique<ImGuiManager>();
	imguiManager->Initialize(windowAPI.get(), directXCom.get());

	debugUI = std::make_unique<DebugUI>(materialManager_.get(), spriteManager_.get(), camera_.get(), &transformObject, &useMonsterBall, &drawObject, &drawSprite);
	debugUI->Initialize();

	SceneRegistration::RegisterScenes();
	SceneManager::GetInstance()->ChangeScene("TITLE");

	textureIndexUvChecker = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
	textureIndexModelTex = TextureManager::GetInstance()->Load(modelData.material.textureFilePath);
	textureIndexSkybox_ = TextureManager::GetInstance()->Load("Resources/CG4/dds/CG4_test.dds");
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
	spriteManager_.reset();
	spriteCom.reset();

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
		HANDLE h = directXCom->GetFenceEvent();
		if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
	}
	directXCom.reset();

	if (windowAPI)
	{
		windowAPI->Finalize();
		windowAPI.reset();
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
	directXCom->PreDraw();

	object3dCom->PreDraw();

    RenderContext ctx{};
    ctx.commandList = directXCom->GetCommandList().Get();
    ctx.windowAPI = windowAPI.get();
    ctx.camera = camera_.get();
    ctx.light = light.get();
	if (textureIndexSkybox_ != TextureManager::kInvalidTextureIndex)
	{
		ctx.environmentTextureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndexSkybox_);
	}
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

    if (camera_ && camera_->GetCameraResource())
    {
		directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(RootParam::Object3D::kCamera, camera_->GetCameraResource()->GetGPUVirtualAddress());
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

	object3dCom->PreDraw();

    SceneManager::GetInstance()->Draw();

	

	if (drawObject)
	{
		object3dCom->Draw(object3d_.get(), ctx, modelData, drawObject);
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
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), directXCom->GetCommandList().Get());
#endif

	directXCom->PostDraw();
}

bool Game::IsQuitRequested()
{
	if (directXCom)
	{
		return (directXCom->GetMsg().message == WM_QUIT);
	}
	return false;
}

void Game::DrawObjects(const RenderContext& ctx)
{
	if (ctx.environmentTextureHandle.ptr != 0)
	{
		ctx.commandList->SetGraphicsRootDescriptorTable(RootParam::Object3D::kEnvironmentTextureTable, ctx.environmentTextureHandle);
	}

    if (ctx.textureHandle.ptr != 0)
    {
		ctx.commandList->SetGraphicsRootDescriptorTable(RootParam::Object3D::kTextureTable, ctx.textureHandle);
    }

    if (ctx.light)
    {
		ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Object3D::kLight, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
    }
    else
    {
		ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Object3D::kLight, 0);
    }

    if (ctx.camera && ctx.camera->GetCameraResource())
    {
		ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Object3D::kCamera, ctx.camera->GetCameraResource()->GetGPUVirtualAddress());
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
    if (drawSprite)
    {
        spriteManager_->DrawAll(ctx, &debugCamera_, &sprites);
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
