#include"Game.h"

#include <combaseapi.h>
#include <sstream>
#include <iomanip>

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

	SceneManager::GetInstance()->SetDirectXCom(directXCom.get());

	TextureManager::GetInstance()->Initialize();
	TextureManager::GetInstance()->SetDirectXCom(directXCom.get());
	// 作業ディレクトリが project/ である前提の相対パスを使う
	//TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");

	spriteCom = std::make_unique<SpriteCom>(logStream, directXCom.get());
	spriteCom->Initialize();

	spriteCom->CreateGraphicsPipeline();

	spriteManager_ = std::make_unique<SpriteManager>();
	spriteManager_->Initialize(spriteCom.get(), "Resources/uvChecker.png", 5);

	// 既存の手動テクスチャ読み込みはそのまま利用（Sphere用）

	object3dCom = std::make_unique<Object3dCom>(logStream);
	object3dCom->Initialize(GetDirectXCom());

	// Make shared engine resources available to scenes
	SceneManager::GetInstance()->SetObject3dCom(object3dCom.get());

#pragma region 最初のシーンの初期化
	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(object3dCom.get());

	
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
		Sprite* sprite = Sprite::Create(spriteCom.get(), textureHandle, { 100.0f,100.0f });
		// 必要に応じて管理用コンテナへ登録
		sprites.emplace_back(sprite);
	}

	uint32_t srvIndexUvChecker = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
	uint32_t srvIndexModelTex = TextureManager::GetInstance()->Load(modelData.material.textureFilePath);
	textureSrvHandleGPU = TextureManager::GetInstance()->GetSrvHandleGPU(srvIndexUvChecker);
	textureSrvHandleGPU2 = TextureManager::GetInstance()->GetSrvHandleGPU(srvIndexModelTex);

	
	{
		std::ostringstream oss;
		oss << "Game::Initialize - srvIndexUvChecker=" << std::dec << srvIndexUvChecker << " srvIndexModelTex=" << srvIndexModelTex << "\n";
		oss << "Game::Initialize - textureSrvHandleGPU=0x" << std::hex << (unsigned long long)textureSrvHandleGPU.ptr << " textureSrvHandleGPU2=0x" << (unsigned long long)textureSrvHandleGPU2.ptr << std::dec << "\n";
		// Also log directXCom handle computation to cross-check
		auto directHandle1 = directXCom->GetSRVHandleGPU(srvIndexUvChecker);
		auto directHandle2 = directXCom->GetSRVHandleGPU(srvIndexModelTex);
		oss << "DirectXCom::GetSRVHandleGPU computed: handle1=0x" << std::hex << (unsigned long long)directHandle1.ptr << " handle2=0x" << (unsigned long long)directHandle2.ptr << std::dec << "\n";
		if (srvIndexUvChecker == srvIndexModelTex)
		{
			oss << "Warning: srvIndexUvChecker == srvIndexModelTex (duplicate index)\n";
		}
		OutputDebugStringA(oss.str().c_str());
	}

	//音声読み込み
	audioManager_ = std::make_unique<AudioManager>(logStream);
	audioManager_->Initialize();


	inputManager.Initialize(windowAPI.get());


	debugCamera_.Initialize(windowAPI.get());

	object3dCom->SetDefaultCamera(camera_.get());

	imguiManager = std::make_unique<ImGuiManager>();
	imguiManager->Initialize(windowAPI.get(), directXCom.get());

	SceneRegistration::RegisterScenes();
	SceneManager::GetInstance()->SetCamera(camera_.get());
	SceneManager::GetInstance()->ChangeScene("TITLE");
}


void Game::Finalize()
{


	SceneManager::Destroy();

#ifdef USE_IMGUI
	//ImGui終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif


	Logger::Log(logStream, "Application terminating.");

	std::wstring wstringValue = L"Hello, DirectX!";
	Logger::Log(logStream, StringUtil::ConvertString(std::format(L"WSTRING{}\n", wstringValue)));

	//出力ウィンドウへの文字出力
	OutputDebugStringA("Hello, DirectX!\n");

	// サウンドの終了処理
	if (audioManager_)
	{
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

	ImGui::ShowDemoWindow();

	ImGui::Begin("Windows");


	ImGui::ColorEdit4("Material Color", &materialManager_->GetMaterialDataColor().x);
	//ImGui::DragFloat("Light Intensity", &directionalLightData->intensity, 0.01f, 0.0f, 10.0f);

	// Sprite position window: size (500,100), sliders (x,y) with initial (100,100) and format integer 4 digits, decimal 1
	ImGui::SetNextWindowSize(ImVec2(500.0f, 100.0f), ImGuiCond_Once);
	ImGui::Begin("Sprite Position");
	// Slider range chosen to allow 4 integer digits and 1 decimal place
	//ImGui::SliderFloat2("Position (X,Y)", &uiSpritePosition.x, 0.0f, 9999.9f, "%4.1f");
	ImGui::End();

	ImGui::Checkbox("useMonsterBall", &useMonsterBall);
	ImGui::Checkbox("LightSprite Flag", (bool*)&materialManager_->GetMaterialDataEnableLighting());
	for (auto& spritePtr : spriteManager_->GetSprites())
	{
		ImGui::Checkbox("LightObject Flag", (bool*)&spritePtr->GetMaterialDataSprite()->enableLighting);
	}
	ImGui::Checkbox("DrawObject", &drawObject);
	ImGui::Checkbox("DrawSprite", &drawSprite);
	//ImGui::Checkbox("DrawSphere", &drawSphere);
	//ImGui::DragFloat3("LightDirection", &directionalLightData->direction.x, 0.01f, -10.0f, 10.0f);

	ImGui::DragFloat3("Object Rotate", &transformObject.rotate.x, 0.01f, -10.0f, 10.0f);

	//ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
	//ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
	//ImGui::DragFloat("UVRotate", &uvTransformSprite.rotate.z, 0.01f);

	// --- ここから追加：マテリアル調整 ---
	if (ImGui::CollapsingHeader("Material"))
	{
		// ライティングのON/OFF切り替え
		// boolからint32_tへ変換して代入
		bool enableLock = (materialManager_->GetMaterialDataEnableLighting() != 0);
		if (ImGui::Checkbox("Enable Lighting", &enableLock))
		{
			materialManager_->GetMaterialDataEnableLighting() = enableLock ? 1 : 0;
		}

		// Shininess（テカリ具合）のスライダー
		// 0.1 ～ 100.0 くらいの範囲で調整できるようにします
		ImGui::SliderFloat("Shininess", &materialManager_->GetMaterialDataShininess(), 0.1f, 100.0f);

		// 色の調整もできるようにするとモンスターボールの赤が調整しやすいです
		ImGui::ColorEdit4("Material Color", &materialManager_->GetMaterialDataColor().x);
	}

	// --- ここまで追加 ---

	if (ImGui::Button("Reset Camera"))
	{
		camera_->SetRotate({ 0.0f, 0.0f, 0.0f });
		camera_->SetTranslate({ 0.0f, 0.0f, -5.0f });
	}

	ImGui::End();

	ImGui::Begin("Material Settings"); // ウィンドウ名は既存のものに合わせてください

	// ライティングの有効化フラグ
	bool enableLighting = (materialManager_->GetMaterialDataEnableLighting() != 0);
	if (ImGui::Checkbox("Enable Lighting", &enableLighting))
	{
		materialManager_->GetMaterialDataEnableLighting() = enableLighting ? 1 : 0;
	}

	// テカリ具合 (shininess)
	// 0.1 ～ 100.0 くらいの範囲で調整できるようにします
	ImGui::DragFloat("Shininess", &materialManager_->GetMaterialDataShininess(), 0.5f, 0.1f, 100.0f);

	// 光の色 (DirectionalLight)
	/*if (ImGui::CollapsingHeader("Light"))
	{
		ImGui::ColorEdit4("Light Color", &directionalLightData->color.x);
		ImGui::DragFloat3("Light Direction", &directionalLightData->direction.x, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat("Intensity", &directionalLightData->intensity, 0.01f, 0.0f, 5.0f);
	}*/

	ImGui::End();

	// --- main.cpp ---
	ImGui::Begin("Settings");

	// モンスターボールのテクスチャ切り替え（既存のコードがある場合）
	ImGui::Checkbox("Use Monster Ball", &useMonsterBall);

	ImGui::Separator(); // 区切り線

	// --- マテリアル（質感）の設定 ---
	if (ImGui::CollapsingHeader("Material"))
	{

		bool enable = (materialManager_->GetMaterialDataEnableLighting() != 0);
		if (ImGui::Checkbox("Enable Lighting", &enable))
		{
			materialManager_->GetMaterialDataEnableLighting() = enable ? 1 : 0;
		}


		ImGui::SliderFloat("Shininess", &materialManager_->GetMaterialDataShininess(), 0.1f, 100.0f);

		ImGui::ColorEdit4("Color", &materialManager_->GetMaterialDataColor().x);
	}

	// --- 平行光源の設定 ---
	/*if (ImGui::CollapsingHeader("Directional Light"))
	{
		ImGui::DragFloat3("Direction", &directionalLightData->direction.x, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat("Intensity", &directionalLightData->intensity, 0.01f, 0.0f, 5.0f);
	}*/

	ImGui::End();

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

	// Debug: log handle before object draw
	{
		std::ostringstream oss;
		oss << "Game::Draw - object texture handle (selected) = 0x" << std::hex << (unsigned long long)(useMonsterBall ? textureSrvHandleGPU2.ptr : textureSrvHandleGPU.ptr) << std::dec << "\n";
		OutputDebugStringA(oss.str().c_str());
	}

	directXCom->GetCommandList()->IASetVertexBuffers(0, 1, &model_->GetVertexBufferView());
	// Note: object draw uses non-indexed DrawInstanced, so do not set an index buffer here
	directXCom->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialManager_->GetMaterialResource()->GetGPUVirtualAddress());
	// Use Object3d WVP updated above
	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(1, object3d_->GetTransformationMatrixResource()->GetGPUVirtualAddress());
	directXCom->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(3, light->GetDirectionalLightResource()->GetGPUVirtualAddress());
	if (camera_ && camera_->GetCameraResource())
	{
		directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(4, camera_->GetCameraResource()->GetGPUVirtualAddress());
	}
	else
	{
		// Bind a null GPU address to avoid crash and log the issue
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

	
	directXCom->GetCommandList()->SetGraphicsRootSignature(particleManager->GetRootSignature().Get());
	directXCom->GetCommandList()->SetPipelineState(particleManager->GetPipelineState().Get()); // パイプラインステートを設定
	//Objectの描画

	// Debug: log handle before particle draw
	{
		std::ostringstream oss;
		oss << "Game::Draw - particle texture handle (selected) = 0x" << std::hex << (unsigned long long)(useMonsterBall ? textureSrvHandleGPU2.ptr : textureSrvHandleGPU.ptr) << std::dec << "\n";
		OutputDebugStringA(oss.str().c_str());
	}

	directXCom->GetCommandList()->IASetVertexBuffers(0, 1, &model_->GetVertexBufferView());
	// Particle draw uses non-indexed DrawInstanced, so do not set an index buffer here
	directXCom->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialManager_->GetMaterialResource()->GetGPUVirtualAddress());
	// Do NOT set a CBV at root parameter 1 because particle manager declares parameter 1 as a descriptor table.
	// Set descriptor table for instance data (root parameter 1)
	directXCom->GetCommandList()->SetGraphicsRootDescriptorTable(1, particleManager->GetInstancingSrvHandleGPU());
	// Set descriptor table for texture (root parameter 2)
	directXCom->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(3, light->GetDirectionalLightResource()->GetGPUVirtualAddress());
	if (camera_ && camera_->GetCameraResource())
	{
		directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(4, camera_->GetCameraResource()->GetGPUVirtualAddress());
	}
	else
	{
		directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(4, 0);
		Logger::Log(logStream, "Warning: camera GPU resource not available when drawing particles.\n");
	}

	/*commandList->DrawIndexedInstanced(kIndexCount, 1, 0, 0, 0);*/
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