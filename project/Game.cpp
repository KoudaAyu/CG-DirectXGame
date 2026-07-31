#include"Game.h"
#include "DebugUI.h"
#include "Baziru3_Engine/Core/Base/Pipeline/PipelineStateManager.h"
#include <future>

#include <combaseapi.h>


#include <sstream>
#include <iomanip>

#include "Baziru3_Engine\Graphics\Graphics\SceneRenderRequests.h"
#include"RenderContext.h"
#include"RootParam.h"
#include"Baziru3_Engine/Core/Base/SubsystemFactory.h"


#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#endif
#include "Baziru3_Engine/Graphics/Graphics/GpuProfiler.h"

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

	offScreenRendering_ = std::make_unique<OffScreenRendering>(logStream, dx);
	offScreenRendering_->Initialize();

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
	debugUI = std::make_unique<DebugUI>(materialManager_.get(), uiSpriteManager, camera_.get(), &transformObject, &useMonsterBall, &drawObject, &drawSprite);
	debugUI->Initialize();

	fadeApplication_ = std::make_unique<Fade>();
	fadeApplication_->Initialize(spriteCom, window);
	SceneManager::GetInstance()->SetFadeApplication(fadeApplication_.get());

	SceneRegistration::RegisterScenes();
	SceneManager::GetInstance()->ChangeScene("TITLE");

	textureIndexUvChecker = TextureManager::GetInstance()->Load("Resources/uvChecker.png"); // Load UV Checker texture
	textureIndexModelTex = TextureManager::GetInstance()->Load(modelData.material.textureFilePath); // Load model texture
	textureIndexSkybox_ = TextureManager::GetInstance()->Load("Resources/CG4/dds/CG4_test.dds"); // Load skybox texture
   SceneManager::GetInstance()->SetSkyboxTextureIndex(textureIndexSkybox_);

	// OffScreenRendering の初期化
	offScreenRendering_ = std::make_unique<OffScreenRendering>(logStream, dx);
	offScreenRendering_->Initialize(0, 0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, { 1.0f, 0.0f, 0.0f, 1.0f });

	if (debugUI)
	{
		debugUI->SetOffScreenRendering(offScreenRendering_.get());
	}

	// 遅延していたアップロードバッファの解放とGPU同期待ちを一括実行
	TextureManager::GetInstance()->ReleaseUploadBuffers();
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

	if (SceneManager::GetInstance())
	{
		SceneManager::GetInstance()->SetFadeApplication(nullptr);
	}

	if (fadeApplication_)
	{
		fadeApplication_->Finalize();
		fadeApplication_.reset();
	}

	if (offScreenRendering_)
	{
		offScreenRendering_->Finalize();
		offScreenRendering_.reset();
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
		SceneManager::GetInstance()->SetSkinningObject3dCom(nullptr);
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
	if (skinningObject3dCom) { skinningObject3dCom.reset(); }

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

	// 7) Ensure TextureManager releases GPU resources (delegated to engine_->Finalize())

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

	if (engine_)
	{
		PipelineStateManager::GetInstance()->Update(engine_->GetDirectXCom());
	}

	if (audioManager_)
	{
		audioManager_->Update();
	}

	if (fadeApplication_)
	{
		fadeApplication_->Update();
	}


    //ImGuiにここからフレームが始まる趣旨をつたえる
    // ※ シーンのUpdate()内でImGuiを使えるよう、SceneManager::Update()より前に呼ぶ
    imguiManager->Update();

    // Update scenes and engine subsystems. Use fixed timestep here (same as scenes expect).
    SceneManager::GetInstance()->Update(1.0f / 60.0f);



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
#endif // USE_IMGUI





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
            cur->Update();
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

	if (!dx) return;

	// PreDraw backbuffer transition and clear
	dx->PreDraw();

	// Begin rendering to off-screen buffer
	if (offScreenRendering_)
	{
		offScreenRendering_->Begin(dx->GetCommandList().Get());
	}

	if (object3dCom) object3dCom->PreDraw();

	RenderContext ctx = PrepareRenderContext();
	SceneRenderRequests renderRequests{};

	if (camera_ && camera_->GetCameraGpuAddress() != 0)
	{
		dx->GetCommandList()->SetGraphicsRootConstantBufferView(4, camera_->GetCameraGpuAddress());
	}
	else
	{
		Logger::Log(logStream, "Warning: camera GPU resource not available before SceneManager draw.\n");
	}

	// スプライトの更新処理を一括でメインスレッドで行う（サブスレッドでのGPUメモリ書き込み競合を回避するため）
	SpriteManager* sm = engine_ ? engine_->GetSpriteManager() : nullptr;
	if (sm)
	{
		sm->Update();
	}
	for (auto& sp : sprites)
	{
		if (sp) sp->Update();
	}

	// === [サブスレッド] Sprite 描画コマンドの並列記録を開始 ===
	RenderContext workerCtx = ctx;
	workerCtx.commandList = dx->GetWorkerCommandList().Get();

	auto spriteFuture = std::async(std::launch::async, [this, workerCtx, dx]() {
		dx->GetWorkerCommandAllocator()->Reset();
		dx->GetWorkerCommandList()->Reset(dx->GetWorkerCommandAllocator().Get(), nullptr);

		// パイプラインステート、RTV/DSV、ビューポート、記述子ヒープの設定
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{};
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
		if (offScreenRendering_)
		{
			dsvHandle = offScreenRendering_->GetDsvHandle();
			rtvHandle = offScreenRendering_->GetRtvHandle();
		}
		else
		{
			UINT backBufferIndex = dx->GetSwapChain()->GetCurrentBackBufferIndex();
			dsvHandle = dx->GetDsvHeap().GetCPUDescriptorHandle(0);
			rtvHandle = dx->GetRtvHandle(backBufferIndex);
		}
		workerCtx.commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

		ID3D12DescriptorHeap* descriptorHeaps[] = { dx->GetSrvDescriptorHeap().Get() };
		workerCtx.commandList->SetDescriptorHeaps(1, descriptorHeaps);

		workerCtx.commandList->RSSetViewports(1, &dx->GetViewport());
		workerCtx.commandList->RSSetScissorRects(1, &dx->GetScissorRect());

		GpuProfiler::GetInstance()->BeginProfile(workerCtx.commandList, "Sprite Draw");
		DrawSprites(workerCtx);
		GpuProfiler::GetInstance()->EndProfile(workerCtx.commandList, "Sprite Draw");

		dx->GetWorkerCommandList()->Close();
	});

	// === [メインスレッド] 3Dオブジェクト等の描画コマンド記録 ===

	// 1. Scene Drawの計測
	GpuProfiler::GetInstance()->BeginProfile(dx->GetCommandList().Get(), "Scene Draw");

	if (SceneManager::GetInstance())
	{
		SceneManager::GetInstance()->DrawSkybox(ctx.commandList);
	}

	if (SceneManager::GetInstance())
	{
		SceneManager::GetInstance()->Draw(renderRequests);
	}

	sphereRenderer_.Draw(ctx, renderRequests);

	if (!renderRequests.sceneDrawn && drawObject)
	{
		if (object3dCom && object3d_)
		{
			object3dCom->Draw(object3d_.get(), ctx, modelData, drawObject);
		}
	}
	GpuProfiler::GetInstance()->EndProfile(dx->GetCommandList().Get(), "Scene Draw");

	// 3. Particle Drawの計測 (通常のパーティクル描画)
	GpuProfiler::GetInstance()->BeginProfile(dx->GetCommandList().Get(), "Particle Draw");
	if (renderRequests.sceneDrawn)
	{
		DrawParticles(ctx);
	}
	GpuProfiler::GetInstance()->EndProfile(dx->GetCommandList().Get(), "Particle Draw");

	// === 前半のコマンドリスト記録を終了し、GPUに即時提出（オフスクリーン3D描画の確定） ===
	dx->GetCommandList()->Close();
	ID3D12CommandList* mainLists1[] = { dx->GetCommandList().Get() };
	dx->GetCommandQueue()->ExecuteCommandLists(1, mainLists1);

	// === [サブスレッド] Sprite 描画コマンド記録完了を同期的に待機し、提出 ===
	spriteFuture.get();
	ID3D12CommandList* workerLists[] = { dx->GetWorkerCommandList().Get() };
	dx->GetCommandQueue()->ExecuteCommandLists(1, workerLists);

	// === 後半のコマンド記録（ポストプロセス以降）の開始 ===
	// アロケーターはリセットせず、コマンドリストのみをリセットして記録を再開
	dx->GetCommandList()->Reset(dx->GetCommandAllocator().Get(), nullptr);

	// ビューポート、記述子ヒープ、ターゲットの再設定（リセットによりクリアされるため）
	ID3D12DescriptorHeap* descriptorHeaps[] = { dx->GetSrvDescriptorHeap().Get() };
	dx->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
	dx->GetCommandList()->RSSetViewports(1, &dx->GetViewport());
	dx->GetCommandList()->RSSetScissorRects(1, &dx->GetScissorRect());

	// End off-screen rendering
	if (offScreenRendering_)
	{
		offScreenRendering_->End(dx->GetCommandList().Get());

		// Restore main render target (backbuffer)
		offScreenRendering_->SetMainRenderTarget(dx->GetCommandList().Get());

		// Set camera's inverse projection matrix
		if (camera_)
		{
			offScreenRendering_->SetProjectionInverse(Inverse(camera_->GetProjectionMatrix()));
		}

		// 4. PostProcess (OffScreen Rendering) Drawの計測
		GpuProfiler::GetInstance()->BeginProfile(dx->GetCommandList().Get(), "PostProcess Draw");
		offScreenRendering_->DrawToBackBuffer(dx->GetCommandList().Get());
		GpuProfiler::GetInstance()->EndProfile(dx->GetCommandList().Get(), "PostProcess Draw");
	}

	if (fadeApplication_)
	{
		fadeApplication_->Draw();
	}

	// Draw object debug logs if necessary
	{
		// Removed debug print to reduce game loop log spam
	}

#ifdef USE_IMGUI
	if (imguiManager) imguiManager->Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx->GetCommandList().Get());
#endif

	dx->PostDraw();
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

	// Skinning対応の Object3dCom も初期化
	skinningObject3dCom = std::make_unique<SkinningObject3dCom>(logStream);
	skinningObject3dCom->Initialize(dx);

	SceneManager::GetInstance()->SetObject3dCom(object3dCom.get());
	SceneManager::GetInstance()->SetSkinningObject3dCom(skinningObject3dCom.get());
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

	if (ctx.camera && ctx.camera->GetCameraGpuAddress() != 0)
	{
		ctx.commandList->SetGraphicsRootConstantBufferView(4, ctx.camera->GetCameraGpuAddress());
	}
	else
	{
		Logger::Log(logStream, "Warning: camera GPU resource not available when drawing object.\n");
		return;
	}

	object3d_->Draw(ctx);

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









