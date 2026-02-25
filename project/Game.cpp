#include"Game.h"

#include <combaseapi.h>


void Game::Initialize()
{
	Framework::Initialize();

	crashDump.Install();
	log.Initialize();
	windowAPI = new WindowAPI();
	windowAPI->Initialize();

	directXCom = new DirectXCom(windowAPI, logStream);
	directXCom->DebugLayer();
	//ウィンドウを表示する
	windowAPI->Show();
	directXCom->Initialize();

	// TextureManager は SRV ヒープに依存するので DX 初期化の後に初期化
	TextureManager::GetInstance()->Initialize();
	TextureManager::GetInstance()->SetDirectXCom(directXCom);
	// 作業ディレクトリが project/ である前提の相対パスを使う
	TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");

	spriteCom = new SpriteCom(logStream, directXCom);
	spriteCom->Initialize();


	for (uint32_t i = 0; i < 5; ++i)
	{
		Sprite* sprite = new Sprite();
		// Resources フォルダ直下の uvChecker.png を指定
		sprite->Initialize(spriteCom, "Resources/uvChecker.png");
		sprites.push_back(sprite);
	}

	spriteCom->CreateGraphicsPipeline();

	// 既存の手動テクスチャ読み込みはそのまま利用（Sphere用）

	object3dCom = new Object3dCom(logStream);
	object3dCom->Initialize(GetDirectXCom());

#pragma region 最初のシーンの初期化
	object3d = new Object3d();
	object3d->Initialize(object3dCom);

	particleManager = new ParticleManager(logStream, GetDirectXCom());
	particleManager->Initialize();
#pragma endregion 最初のシーン終了

	//パイプラインステートの生成に失敗した場合はエラー
	assert(SUCCEEDED(GetDirectXCom()->GetHr()));

	sphere = new Sphere();
	sphere->Initialize(directXCom);

	//モデル読み込み
	modelData = object3d->LoadObjFile("Resources", "plane.obj");
	//頂点リソースを作る
	vertexResourceModel = directXCom->CreateBufferResource(directXCom->GetDevice().Get(), sizeof(Sprite::VertexData) * modelData.vertices.size());
	//頂点バッファービューを作成末う

	vertexBufferView.BufferLocation = vertexResourceModel->GetGPUVirtualAddress();//リソースの先頭のアドレスから使う
	vertexBufferView.SizeInBytes = UINT(sizeof(Sprite::VertexData) * modelData.vertices.size()); //使用するリソースのサイズは頂点のサイズ
	vertexBufferView.StrideInBytes = sizeof(Sprite::VertexData); //1頂点当たりのサイズ
	//頂点リソースにデータを書き込む
	Sprite::VertexData* vertexDataModel = nullptr;
	vertexResourceModel->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataModel));
	std::memcpy(vertexDataModel, modelData.vertices.data(), sizeof(Sprite::VertexData) * modelData.vertices.size());//頂点データをリソースにコピー
	//マテリアル用のリソースを作る
	materialResource = directXCom->CreateBufferResource(directXCom->GetDevice().Get(), sizeof(Sprite::Material));
	//マテリアルにデータを書き込む
	
	//書き込む為のアドレス取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// データを設定（赤色 RGBA: 1,0,0,1）
	Vector4 temp{};
	temp.x = 1.0f;
	temp.y = 1.0f;
	temp.z = 1.0f;
	temp.w = 1.0f;
	materialData->color = temp;
	materialData->enableLighting = false;
	materialResource->Unmap(0, nullptr);

	 directionalLight = directXCom->CreateBufferResource(directXCom->GetDevice().Get(), sizeof(Object3d::DirectionalLight));

	// MapしてGPUリソースのCPU側の書き込み可能ポインタを取得する
	
	directionalLight->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// directionalLightDataに値を書き込む
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;



	// 書き込み完了後はUnmapを呼ぶ
	directionalLight->Unmap(0, nullptr);

	// --- カメラ用のリソース作成を追加 ---
	cameraResource = directXCom->CreateBufferResource(directXCom->GetDevice().Get(), sizeof(CameraForGPU));
	
	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
	// 初期値を設定
	cameraData->worldPosition = { 0.0f, 0.0f, -10.0f };


	//WVP用のリソースを作る。　Matrix4x4 1つのサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = directXCom->CreateBufferResource(directXCom->GetDevice().Get(), sizeof(TransformationMatrix));
	//データを書き込む
	TransformationMatrix* wvpData = nullptr;
	//書き込む為のアドレス取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	//単位行列を書き込む
	wvpData->World = MakeIdentity4x4();
	wvpData->WVP = MakeIdentity4x4();

	transformationMatrixResourceSphere = directXCom->CreateBufferResource(directXCom->GetDevice().Get(), sizeof(TransformationMatrix));

	// データを書き込むためのポインタを取得
	
	transformationMatrixResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSphere));
	transformationMatrixDataSphere->WVP = MakeIdentity4x4();
	transformationMatrixDataSphere->World = MakeIdentity4x4();
	// NOTE: Keep this buffer mapped for the program lifetime so we can update per-frame without remapping.

	//Transform変数を作る
	Sprite::Transform transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };


	//Sphere用
	transformObject = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	cameraTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} };

	//uvTrandform用の変数
	uvTransformSprite = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	//uvTransform行列の初期化
	materialData->uvTransform = MakeIdentity4x4();


	 fovY = 0.45f;  // 資料通り
	 aspectRatio = static_cast<float>(windowAPI->GetClientWidth()) / static_cast<float>(windowAPI->GetClientHeight());
	 nearZ = 0.1f;
	 farZ = 100.0f;

	//Textureを読んで転送する
	DirectX::ScratchImage mipImages = directXCom->LoadTexture("./Resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	textureResource = directXCom->CreateTextureResource(metadata);

	//2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = directXCom->LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	textureResource2 = directXCom->CreateTextureResource(metadata2);

	intermediateResource = directXCom->UploadTextureData(textureResource, mipImages, directXCom->GetDevice().Get(), directXCom->GetCommandList());
	intermediateResource2 = directXCom->UploadTextureData(textureResource2, mipImages2, directXCom->GetDevice().Get(), directXCom->GetCommandList());

	//metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	//二つ目。metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);


	//SRVを生成するDescriptorHeapの場所を決める
	textureSrvHandleCPU = directXCom->GetSrvDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
	textureSrvHandleGPU = directXCom->GetSrvDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();

	textureSrvHandleCPU2 = directXCom->GetCPUDescroptirHandle(directXCom->GetSrvDescriptorHeap(), directXCom->GetDescriptorSizeSRV(), 2);
	textureSrvHandleGPU2 = directXCom->GetGPUDescriptorHandle(directXCom->GetSrvDescriptorHeap(), directXCom->GetDescriptorSizeSRV(), 2);
	//先頭はImGuiに使用しているためその次を使う
	textureSrvHandleCPU.ptr += directXCom->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += directXCom->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//SRVの生成
	directXCom->GetDevice()->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);
	//2つ目
	directXCom->GetDevice()->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);
	


	//音声読み込み
	sound_ = new Sound();
	sound_->Initialize();
	sound_->SoundLoadFile("Resources/Alarm01.wav");
	sound_->SoundPlayWave();

	
	inputManager.Initialize(windowAPI);

	
	debugCamera_.Initialize(windowAPI);

	camera = new Camera();
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-10.0f });
	object3dCom->SetDefaultCamera(camera);


	//ビューポート
	
	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = static_cast<float>(windowAPI->GetClientWidth());
	viewport.Height = static_cast<float>(windowAPI->GetClientHeight());
	viewport.TopLeftX = 0.0f; //左上のX座標
	viewport.TopLeftY = 0.0f; //左上のY座標
	viewport.MinDepth = 0.0f; //最小の深度
	viewport.MaxDepth = 1.0f; //最大の深度

	//シザー矩形
	
	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0; //左上のX座標
	scissorRect.right = windowAPI->GetClientWidth(); //右下のX座標
	scissorRect.top = 0; //左上のY座標
	scissorRect.bottom = windowAPI->GetClientHeight(); //右下のY座標


	transformSphere = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	

	
	
	emitter.transform.SetTranslate({ 0.0f,0.0f,0.0f });
	emitter.transform.SetRotate({ 0.0f,0.0f,0.0f });
	emitter.transform.SetScale({ 1.0f,1.0f,1.0f });

	emitter.count = 3; // 初期値
	emitter.frequency = 0.5f;
	emitter.frequencyTime = 0.0f;

	Random::SeedEngine();
	
	for (uint32_t index = 0; index < kNumMaxInstances; ++index)
	{
		particles.push_back(particleManager->MakeNewParticles(randomEngine, emitter.transform.GetTranslate()));
	}




	//TransformationMatrix gTransformationMatrices[10];

	instancingResource =
		directXCom->CreateBufferResource(directXCom->GetDevice(), sizeof(ParticleManager::ParticleForGPU) * kNumMaxInstances);
	
	instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&instanceData));


	{
		uint32_t writeIndex = 0;
		for (const auto& p : particles)
		{
			if (writeIndex >= kNumMaxInstances) { break; }
			instanceData[writeIndex].WVP = MakeIdentity4x4();
			instanceData[writeIndex].World = MakeIdentity4x4();
			instanceData[writeIndex].color = p.color;
			++writeIndex;
		}

		for (; writeIndex < kNumMaxInstances; ++writeIndex)
		{
			instanceData[writeIndex].WVP = MakeIdentity4x4();
			instanceData[writeIndex].World = MakeIdentity4x4();
			instanceData[writeIndex].color = { 0,0,0,0 };
		}
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.NumElements = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = kNumMaxInstances;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(ParticleManager::ParticleForGPU);
	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU = directXCom->GetCPUDescroptirHandle(directXCom->GetSrvDescriptorHeap(), directXCom->GetDescriptorSizeSRV(), 3);
	instancingSrvHandleGPU = directXCom->GetGPUDescriptorHandle(directXCom->GetSrvDescriptorHeap(), directXCom->GetDescriptorSizeSRV(), 3);
	directXCom->GetDevice()->CreateShaderResourceView(instancingResource.Get(), &instancingSrvDesc, instancingSrvHandleCPU);



	

	
	imguiManager = new ImGuiManager();
	imguiManager->Initialize(windowAPI, directXCom);


}
void Game::Finalize()
{
	//ImGui終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();


	Logger::Log(logStream, "Application terminating.");

	std::wstring wstringValue = L"Hello, DirectX!";
	Logger::Log(logStream, StringUtil::ConvertString(std::format(L"WSTRING{}\n", wstringValue)));

	//出力ウィンドウへの文字出力
	OutputDebugStringA("Hello, DirextX!\n");

	// サウンドの終了処理
	if (sound_)
	{
		sound_->Finalize();
		delete sound_;
		sound_ = nullptr;
	}

	delete imguiManager;

	for (auto* sprite : sprites)
	{
		delete sprite;
	}
	sprites.clear();
	delete particleManager;
	delete object3d;
	delete camera;

	TextureManager::GetInstance()->Finalize();


	delete sphere;

	delete object3dCom;
	delete spriteCom;

	CloseHandle(directXCom->GetFenceEvent());
	delete directXCom;

	windowAPI->Finalize();
	delete windowAPI;

	Framework::Finalize();
}
void Game::Update()
{ 
	Framework::Update();

	//if (windowAPI->ProcessMassage())
	//{
	//	//ゲームループ抜ける
	//	break;
	//}


	//Imguiにここからフレームが始まる趣旨をつたえる
	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX12_NewFrame();
	ImGui::NewFrame();

	debugCamera_.Update();

	camera->Update();

	cameraData->worldPosition = camera->GetWorldPosition();

	// Apply ImGui rotation to the object3d transform
	object3d->SetRotate(transformObject.rotate);
	object3d->Update();


	for (auto* sprite : sprites)
	{
		sprite->SetPosition({ 0.0f,0.0f });
		sprite->Update(windowAPI, &debugCamera_);
	}

	//UVTransform用
	Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
	uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
	uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
	for (auto* sprite : sprites)
	{
		sprite->SetUVTransform(uvTransformMatrix);
	}

	directionalLight->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	transformSphere.rotate.y += 0.01f;
	Matrix4x4 worldMatrix = MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);
	Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
	Matrix4x4 viewMatrix = Inverse(cameraMatrix);
	Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(fovY, aspectRatio, nearZ, farZ);
	//WVPMatrixを作る
	Matrix4x4 worldViewProjectMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	transformationMatrixDataSphere->WVP = worldViewProjectMatrix;
	transformationMatrixDataSphere->World = worldMatrix;
	// 法線変換用の逆転置行列も更新
	transformationMatrixDataSphere->WorldInverseTranspose = Transpose(Inverse(worldMatrix));


	numInstance = 0;

	emitter.frequencyTime += kDeltaTime;
	if (emitter.frequencyTime >= emitter.frequency)
	{
		particles.splice(particles.end(), particleEmitter.Emit(emitter, randomEngine, *particleManager));
		emitter.frequencyTime -= emitter.frequency;
	}
	{
		uint32_t writeIndex = 0;
		for (auto it = particles.begin(); it != particles.end(); ++it)
		{
			ParticleManager::Particle& p = *it;
			p.currentTime += kDeltaTime;

			if (p.currentTime >= p.lifeTime)
			{
				continue;
			}

			Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(0.0f);
			//Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);面が逆向きの場合
			Matrix4x4 billboardMatrix = Multiply(backToFrontMatrix, cameraMatrix);
			billboardMatrix.m[3][0] = 0.0f;
			billboardMatrix.m[3][1] = 0.0f;
			billboardMatrix.m[3][2] = 0.0f;

			Matrix4x4 ParticleWorldMatrix = MakeAffineMatrix(
			p.transform.GetScale(), billboardMatrix, p.transform.GetTranslate());
			Matrix4x4 ParticleViewProjectMatrix = Multiply(
				ParticleWorldMatrix, Multiply(viewMatrix, projectionMatrix));

			if (writeIndex < kNumMaxInstances)
			{
				instanceData[writeIndex].WVP = ParticleViewProjectMatrix;
				instanceData[writeIndex].World = ParticleWorldMatrix;
				instanceData[writeIndex].color = p.color;
				float alpha = 1.0f - (p.currentTime / p.lifeTime);
				instanceData[writeIndex].color.w = alpha;
				++writeIndex;
			}

			p.transform.SetTranslate(
				p.transform.GetTranslate() + p.velocity * kDeltaTime);
		}
		numInstance = writeIndex;
	}

	//開発用UIの処理、実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換え

#ifdef _DEBUG

	ImGui::ShowDemoWindow();

	ImGui::Begin("Windows");


	ImGui::ColorEdit4("Material Color", &materialData->color.x);
	ImGui::DragFloat("Light Intensity", &directionalLightData->intensity, 0.01f, 0.0f, 10.0f);


	ImGui::Checkbox("useMonsterBall", &useMonsterBall);
	ImGui::Checkbox("LightSprite Flag", (bool*)&materialData->enableLighting);
	for (auto* sprite : sprites)
	{
		ImGui::Checkbox("LightObject Flag", (bool*)&sprite->GetMaterialDataSprite()->enableLighting);
	}
	ImGui::Checkbox("DrawObject", &drawObject);
	ImGui::Checkbox("DrawSprite", &drawSprite);
	ImGui::DragFloat3("LightDirection", &directionalLightData->direction.x, 0.01f, -10.0f, 10.0f);

	ImGui::DragFloat3("Object Rotate", &transformObject.rotate.x, 0.01f, -10.0f, 10.0f);

	ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
	ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
	ImGui::DragFloat("UVRotate", &uvTransformSprite.rotate.z, 0.01f);

	// --- ここから追加：マテリアル調整 ---
	if (ImGui::CollapsingHeader("Material"))
	{
		// ライティングのON/OFF切り替え
		// boolからint32_tへ変換して代入
		bool enableLock = (materialData->enableLighting != 0);
		if (ImGui::Checkbox("Enable Lighting", &enableLock))
		{
			materialData->enableLighting = enableLock ? 1 : 0;
		}

		// Shininess（テカリ具合）のスライダー
		// 0.1 ～ 100.0 くらいの範囲で調整できるようにします
		ImGui::SliderFloat("Shininess", &materialData->shininess, 0.1f, 100.0f);

		// 色の調整もできるようにするとモンスターボールの赤が調整しやすいです
		ImGui::ColorEdit4("Material Color", &materialData->color.x);
	}

	// --- ここまで追加 ---

	if (ImGui::Button("Reset Camera"))
	{
		camera->SetRotate({ 0.0f, 0.0f, 0.0f });
		camera->SetTranslate({ 0.0f, 0.0f, -5.0f });
	}

	ImGui::End();

	ImGui::Begin("Material Settings"); // ウィンドウ名は既存のものに合わせてください

	// ライティングの有効化フラグ
	bool enableLighting = (materialData->enableLighting != 0);
	if (ImGui::Checkbox("Enable Lighting", &enableLighting))
	{
		materialData->enableLighting = enableLighting ? 1 : 0;
	}

	// テカリ具合 (shininess)
	// 0.1 ～ 100.0 くらいの範囲で調整できるようにします
	ImGui::DragFloat("Shininess", &materialData->shininess, 0.5f, 0.1f, 100.0f);

	// 光の色 (DirectionalLight)
	if (ImGui::CollapsingHeader("Light"))
	{
		ImGui::ColorEdit4("Light Color", &directionalLightData->color.x);
		ImGui::DragFloat3("Light Direction", &directionalLightData->direction.x, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat("Intensity", &directionalLightData->intensity, 0.01f, 0.0f, 5.0f);
	}

	ImGui::End();

	// --- main.cpp ---
	ImGui::Begin("Settings");

	// モンスターボールのテクスチャ切り替え（既存のコードがある場合）
	ImGui::Checkbox("Use Monster Ball", &useMonsterBall);

	ImGui::Separator(); // 区切り線

	// --- マテリアル（質感）の設定 ---
	if (ImGui::CollapsingHeader("Material"))
	{
		// ライティングを有効にするかどうか
		bool enable = (materialData->enableLighting != 0);
		if (ImGui::Checkbox("Enable Lighting", &enable))
		{
			materialData->enableLighting = enable ? 1 : 0;
		}

		// ★重要：Shininessのスライダー
		// 0.1(鈍い) ～ 100.0(鋭いテカリ) までの範囲で調整
		ImGui::SliderFloat("Shininess", &materialData->shininess, 0.1f, 100.0f);

		// 色も変えれるようにしておくと便利です
		ImGui::ColorEdit4("Color", &materialData->color.x);
	}

	// --- 平行光源の設定 ---
	if (ImGui::CollapsingHeader("Directional Light"))
	{
		ImGui::DragFloat3("Direction", &directionalLightData->direction.x, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat("Intensity", &directionalLightData->intensity, 0.01f, 0.0f, 5.0f);
	}

	ImGui::End();

#endif // DEBUG

	//ImGui内部コマンドを生成する
	ImGui::Render();

	inputManager.Update();

	// ImGuiを使わずにSpaceキーでパーティクルを追加
	if (inputManager.TriggerKey(DIK_SPACE))
	{
		particles.splice(particles.end(), ParticleEmitter{}.Emit(emitter, randomEngine, *particleManager));
	}
}
void Game::Draw()
{
	directXCom->PreDraw();

	object3dCom->PreDraw();

	//RootSignatureを設定。PSOに設定しているけれど別途設定が必要
	directXCom->GetCommandList()->SetGraphicsRootSignature(spriteCom->GetRootSignature().Get());
	// Use pipeline state stored in SpriteCom
	directXCom->GetCommandList()->SetPipelineState(spriteCom->GetPipelineState().Get()); //パイプラインステートを設定
	//Objectの描画

	directXCom->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	// Note: object draw uses non-indexed DrawInstanced, so do not set an index buffer here
	directXCom->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	// Use Object3d WVP updated above
	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(1, object3d->GetTransformationMatrixResource()->GetGPUVirtualAddress());
	directXCom->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());
	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(4, cameraResource->GetGPUVirtualAddress());
	if (drawObject)
	{
		/*commandList->DrawIndexedInstanced(kIndexCount, 1, 0, 0, 0);*/
		directXCom->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
	}

	if (drawSphere)
	{

		directXCom->GetCommandList()->RSSetViewports(1, &viewport);
		directXCom->GetCommandList()->RSSetScissorRects(1, &scissorRect);

		directXCom->GetCommandList()->SetGraphicsRootSignature(object3dCom->GetRootSignature().Get());
		directXCom->GetCommandList()->SetPipelineState(object3dCom->GetPipelineState().Get());
		directXCom->GetCommandList()->IASetVertexBuffers(0, 1, &sphere->GetVertexBufferViewSphere());
		// Use sphere's index buffer for indexed draw
		directXCom->GetCommandList()->IASetIndexBuffer(&sphere->GetIndexBufferViewSphere());
		directXCom->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

		directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSphere->GetGPUVirtualAddress());
		directXCom->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);

		directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());

		directXCom->GetCommandList()->DrawIndexedInstanced(sphere->GetIndexCount(), 1, 0, 0, 0);
	}

	if (drawSprite)
	{
		for (auto* sprite : sprites)
		{
			sprite->Draw();
		}
	}

	//RootSignatureを設定。PSOに設定しているけれど別途設定が必要
	directXCom->GetCommandList()->SetGraphicsRootSignature(particleManager->GetRootSignature().Get());
	directXCom->GetCommandList()->SetPipelineState(particleManager->GetPipelineState().Get()); // パイプラインステートを設定
	//Objectの描画

	directXCom->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	// Particle draw uses non-indexed DrawInstanced, so do not set an index buffer here
	directXCom->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	// Do NOT set a CBV at root parameter 1 because particle manager declares parameter 1 as a descriptor table.
	// Set descriptor table for instance data (root parameter 1)
	directXCom->GetCommandList()->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU);
	// Set descriptor table for texture (root parameter 2)
	directXCom->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());
	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(4, cameraResource->GetGPUVirtualAddress());

	/*commandList->DrawIndexedInstanced(kIndexCount, 1, 0, 0, 0);*/
	if (numInstance > 0)
	{
		directXCom->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), numInstance, 0, 0);
	}



	//実際のcommandListのImGuiの描画コマンドを積む
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), directXCom->GetCommandList().Get());


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