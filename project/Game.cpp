#include"Game.h"
#include "DebugUI.h"

#include <combaseapi.h>
#include <sstream>
#include <iomanip>

#include"RootParam.h"

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
	windowAPI = std::make_unique<WindowAPI>();
	windowAPI->Initialize();

	directXCom = std::make_unique<DirectXCom>(windowAPI.get(), logStream);
	directXCom->DebugLayer();
	//ウィンドウを表示する
	windowAPI->Show();
	directXCom->Initialize();

	SceneManager::GetInstance()->Initialize(directXCom.get());

	TextureManager::GetInstance()->Initialize();
	TextureManager::GetInstance()->SetDirectXCom(directXCom.get());
	
	spriteCom = std::make_unique<SpriteCom>(logStream, directXCom.get());
	spriteCom->Initialize();

	spriteCom->CreateGraphicsPipeline();

	SceneManager::GetInstance()->SetSpriteCom(spriteCom.get());

	spriteManager_ = std::make_unique<SpriteManager>();
	spriteManager_->Initialize(spriteCom.get(), "Resources/uvChecker.png", 5);

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
}


void Game::Finalize()
{


	SceneManager::Destroy();

#ifdef USE_IMGUI
	//ImGui終了処理 is delegated to ImGuiManager
	if (imguiManager)
	{
		imguiManager->Finalize();
	}
#endif


	Logger::Log(logStream, "Application terminating.");

	std::wstring wstringValue = L"Hello, DirectX!";
	Logger::Log(logStream, StringUtil::ConvertString(std::format(L"WSTRING{}\n", wstringValue)));

	//出力ウィンドウへの文字出力
	OutputDebugStringA("Hello, DirectX!\n");

	// サウンドの終了処理
	if (audioManager_)
	{
		SceneManager::GetInstance()->SetAudioManager(nullptr);
		audioManager_->Finalize();
		audioManager_.reset();
	}

	TextureManager::GetInstance()->Finalize();

	camera_->Finalize();

	if (directXCom)
	{
		HANDLE h = directXCom->GetFenceEvent();
		if (h && h != INVALID_HANDLE_VALUE)
		{
			CloseHandle(h);
		}
	}
	directXCom.reset();

	windowAPI->Finalize();


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

	SceneManager::GetInstance()->Draw();

	spriteCom->SetupDraw(directXCom->GetCommandList().Get());

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

	object3d_->Draw(directXCom->GetCommandList().Get());

	{
		uint32_t chosenIndex = useMonsterBall ? textureIndexModelTex : textureIndexUvChecker;
		D3D12_GPU_DESCRIPTOR_HANDLE chosenHandle{};
		if (chosenIndex != TextureManager::kInvalidTextureIndex)
		{
			chosenHandle = TextureManager::GetInstance()->GetSrvHandleGPU(chosenIndex);
		}
		directXCom->GetCommandList()->SetGraphicsRootDescriptorTable(2, chosenHandle);
	}
	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(3, light->GetDirectionalLightResource()->GetGPUVirtualAddress());
	if (camera_ && camera_->GetCameraResource())
	{
		directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(4, camera_->GetCameraResource()->GetGPUVirtualAddress());
	}
	else
	{
		directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(4, 0);
		Logger::Log(logStream, "Warning: camera GPU resource not available when drawing object.\n");
	}
	if (drawObject)
	{
		directXCom->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
	}

	

	if (drawSprite)
	{
		// 既存の SpriteManager 管理スプライト
		spriteManager_->Draw();

		// API 経由で Game::sprites に格納したスプライトも描画
		for (auto& s : sprites)
		{
			if (!s) { continue; }
			// 個別スプライト用の行列更新
			s->Update(windowAPI.get(), &debugCamera_);
			s->Draw();
		}
	}

	particleManager->SetupDraw(directXCom->GetCommandList().Get());
	//Objectの描画

	{
		uint32_t chosenIndex = useMonsterBall ? textureIndexModelTex : textureIndexUvChecker;
		D3D12_GPU_DESCRIPTOR_HANDLE chosenHandle{};
		if (chosenIndex != TextureManager::kInvalidTextureIndex)
		{
			chosenHandle = TextureManager::GetInstance()->GetSrvHandleGPU(chosenIndex);
		}
		std::ostringstream oss;
		oss << "Game::Draw - particle texture index=" << std::dec << chosenIndex
		    << " handle=0x" << std::hex << (unsigned long long)chosenHandle.ptr << std::dec << "\n";
		OutputDebugStringA(oss.str().c_str());
	}

    model_->Bind(directXCom->GetCommandList().Get());

  
    particleManager->BindResources(directXCom->GetCommandList().Get(), materialManager_->GetMaterialResource()->GetGPUVirtualAddress());

    uint32_t chosenIndex = useMonsterBall ? textureIndexModelTex : textureIndexUvChecker;
    D3D12_GPU_DESCRIPTOR_HANDLE chosenHandle{};
    if (chosenIndex != TextureManager::kInvalidTextureIndex)
    {
        chosenHandle = TextureManager::GetInstance()->GetSrvHandleGPU(chosenIndex);
    }
    directXCom->GetCommandList()->SetGraphicsRootDescriptorTable(RootParam::Particle::kTextureTable, chosenHandle);

   
    directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(RootParam::Particle::kLight, light->GetDirectionalLightResource()->GetGPUVirtualAddress());
    directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(RootParam::Particle::kCamera, camera_ && camera_->GetCameraResource() ? camera_->GetCameraResource()->GetGPUVirtualAddress() : 0);

	
	if (particleManager->GetNumInstance() > 0)
	{
		directXCom->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), particleManager->GetNumInstance(), 0, 0);
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