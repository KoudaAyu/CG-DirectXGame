//#include<Windows.h>

//自作h
#include"Camera.h"
#include"DebugCamera.h"
#include"DirectXCom.h"
#include"KeyInput.h"
#include"Matrix4x4.h"
#include"Random.h"
#include"ParticleEmitter.h"
#include"Sound.h"
#include"TextureManager.h"
#include"Vector.h"
#include"WindowsAPI.h"

#include<chrono> //時間を扱うライブラリ
#include<filesystem> //ファイルやディレクトリに関する操作を行うライブラリ
#include<format> //文字列のフォーマットを行うライブラリ
#include<fstream> //ファイルにかいたり読んだりするライブラリ
#include<string> //文字列を扱うライブラリ
#include<strsafe.h>

#include<d3d12.h>
#include<dxgi1_6.h>
#include<cassert>



//Comptr
#include<wrl.h>

//Debug用
#include<dbghelp.h>
#pragma comment(lib,"Dbghelp.lib")

//ファイル関係 / サウンド関係
#include<sstream>
//#include <xaudio2.h>
//#pragma comment(lib, "xaudio2.lib")


//ReportLiveObjects
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")

//DXCの初期化
#include<dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")

//Textureの転送
#include"externals/DirectXTex/d3dx12.h"
#include<vector>

#include <DirectXMath.h>
#include<cmath>
#include "externals/DirectXTex/DirectXTex.h"

#include<numbers>
#include<list>


#include"ImGuiManager.h"

#include"Game.h"

enum BlendMode
{
	//!< ブレンドなし
	kBlendMode_None,

	//!< αブレンド
	kBlendMode_Normal,

	//!< 加算ブレンド
	kBlendMode_Add,

	//!< 減算ブレンド
	kBlendMode_Sub,

	//!< 乗算ブレンド
	kBlendMode_Mul,

	//!< スクリーンブレンド
	kBlendMode_Screen,

	//利用禁止
	kCountOfBlendMode,
};

//TextureResourceを作る
Microsoft::WRL::ComPtr<ID3D12Resource>CreateTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const DirectX::TexMetadata& metadata)
{
	//1. metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);//Textureの幅
	resourceDesc.Height = UINT(metadata.height);//Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);//mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);//奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format;//TextureのFormat
	resourceDesc.SampleDesc.Count = 1;//サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);//Textureの次元数。普段使っているのは2次元

	//2. 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//細かい設定を行う
	//heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;//WriteBackポリシーでCPUアクセス可能
	//heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;//プロセッサの近くに配列

	//3. Resourceを生成する
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,//Heapの設定
		D3D12_HEAP_FLAG_NONE,//Heapの特殊な設定
		&resourceDesc,//Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST,//初回のResourceState。Textureは基本読むだけ
		nullptr,//Clear最適値。使わないのでnullptr
		IID_PPV_ARGS(&resource));//作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

struct CameraForGPU
{
	Vector3 worldPosition;
};

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{

	Game game;
	game.Initialize();

	// 球体
	const uint32_t kSubdivision = 16; // 16分割

	// 経度分割1つ分の角度
	const float kLonEvery = DirectX::XM_2PI / float(kSubdivision);
	// 緯度分割1つ分の角度
	const float kLatEvery = DirectX::XM_PI / float(kSubdivision);

	// 頂点数・インデックス数
	// 緯度方向と経度方向の両端に重複する頂点があるため、+1が必要
	const uint32_t kVertexCount = (kSubdivision + 1) * (kSubdivision + 1);
	const uint32_t kIndexCount = kSubdivision * kSubdivision * 6; // 各四角形に三角形2つ、各三角形に頂 vertex 3つで 2*3=6

	// 頂点配列を確保
	Sprite::VertexData* vertexData = new Sprite::VertexData[kVertexCount];

	// --- 頂点データを埋める ---
	for (uint32_t lat = 0; lat <= kSubdivision; ++lat)
	{
		// 緯度 (theta): -π/2 (下端) から π/2 (上端) まで
		float theta = -DirectX::XM_PIDIV2 + DirectX::XM_PI * (float(lat) / kSubdivision);
		for (uint32_t lon = 0; lon <= kSubdivision; ++lon)
		{
			// 経度 (phi): 0 (東端) から 2π (一周) まで
			float phi = DirectX::XM_2PI * (float(lon) / kSubdivision);
			uint32_t idx = lat * (kSubdivision + 1) + lon; // 1次元配列内のインデックス

			// 球面座標からデカルト座標への変換
			vertexData[idx].position.x = cos(theta) * cos(phi);
			vertexData[idx].position.y = sin(theta);
			vertexData[idx].position.z = cos(theta) * sin(phi);
			vertexData[idx].position.w = 1.0f; // 同次座標

			// テクスチャ座標 (UV)
			// U: 経度に比例 (0.0 から 1.0)
			vertexData[idx].texcoord.x = float(lon) / kSubdivision;
			// V: 緯度に比例 (1.0 から 0.0、上向きが正になるように反転)
			vertexData[idx].texcoord.y = 1.0f - float(lat) / kSubdivision;

			// 法線ベクトル (原点から頂点へのベクトルがそのまま法線となる)
			vertexData[idx].normal = {
				vertexData[idx].position.x,
				vertexData[idx].position.y,
				vertexData[idx].position.z
			};
		}
	}


	// --- 頂点バッファを作成・アップロード ---
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSphere = game.GetDirectXCom()->CreateBufferResource(game.GetDirectXCom()->GetDevice().Get(), sizeof(Sprite::VertexData) * kVertexCount);
	Sprite::VertexData* mappedVertex = nullptr;
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertex));
	memcpy(mappedVertex, vertexData, sizeof(Sprite::VertexData) * kVertexCount);
	vertexResourceSphere->Unmap(0, nullptr);

	uint32_t* indexData = new uint32_t[kIndexCount];
	uint32_t idx = 0; // ここを元のままの変数名に戻しました

	for (uint32_t lat = 0; lat < kSubdivision; ++lat)
	{
		for (uint32_t lon = 0; lon < kSubdivision; ++lon)
		{

			uint32_t v0 = lat * (kSubdivision + 1) + lon;             // 左上 (A)
			uint32_t v1 = v0 + 1;                                      // 右上 (C)
			uint32_t v2 = v0 + (kSubdivision + 1);                     // 左下 (B)
			uint32_t v3 = v2 + 1;                                      // 右下 (D)

			// 四角形を2つの三角形で表現する
			// 1つ目の三角形: v0, v2, v1 (A, B, C)
			// DirectXでは通常、右手座標系で反時計回り（CCW）が表
			indexData[idx++] = v0; // A
			indexData[idx++] = v2; // B
			indexData[idx++] = v1; // C

			// 2つ目の三角形: v2, v3, v1 (B, D, C)
		indexData[idx++] = v2; // B
			indexData[idx++] = v3; // D
			indexData[idx++] = v1; // C
		}
	}

	// --- インデックスバッファを作成・アップロード ---
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSphere = game.GetDirectXCom()->CreateBufferResource(game.GetDirectXCom()->GetDevice().Get(), sizeof(uint32_t) * kIndexCount);
	uint32_t* mappedIndex = nullptr;
	indexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndex));
	memcpy(mappedIndex, indexData, sizeof(uint32_t) * kIndexCount);
	indexResourceSphere->Unmap(0, nullptr);

	// --- バッファビュー設定 ---
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};
	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();
	vertexBufferViewSphere.SizeInBytes = sizeof(Sprite::VertexData) * kVertexCount;
	vertexBufferViewSphere.StrideInBytes = sizeof(Sprite::VertexData);

	D3D12_INDEX_BUFFER_VIEW indexBufferViewObject{};
	indexBufferViewObject.BufferLocation = indexResourceSphere->GetGPUVirtualAddress();
	indexBufferViewObject.SizeInBytes = sizeof(uint32_t) * kIndexCount;
	indexBufferViewObject.Format = DXGI_FORMAT_R32_UINT;

	Sprite::VertexData* mapped = nullptr;
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	memcpy(mapped, vertexData, sizeof(Sprite::VertexData) * kVertexCount);
	vertexResourceSphere->Unmap(0, nullptr);


	//モデル読み込み
	Object3d::ModelData modelData = game.GetObject3d()->LoadObjFile("Resources", "plane.obj");
	//頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceModel = game.GetDirectXCom()->CreateBufferResource(game.GetDirectXCom()->GetDevice().Get(), sizeof(Sprite::VertexData) * modelData.vertices.size());
	//頂点バッファービューを作成末う
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	vertexBufferView.BufferLocation = vertexResourceModel->GetGPUVirtualAddress();//リソースの先頭のアドレスから使う
	vertexBufferView.SizeInBytes = UINT(sizeof(Sprite::VertexData) * modelData.vertices.size()); //使用するリソースのサイズは頂点のサイズ
	vertexBufferView.StrideInBytes = sizeof(Sprite::VertexData); //1頂点当たりのサイズ
	//頂点リソースにデータを書き込む
	Sprite::VertexData* vertexDataModel = nullptr;
	vertexResourceModel->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataModel));
	std::memcpy(vertexDataModel, modelData.vertices.data(), sizeof(Sprite::VertexData) * modelData.vertices.size());//頂点データをリソースにコピー



	//マテリアル用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = game.GetDirectXCom()->CreateBufferResource(game.GetDirectXCom()->GetDevice().Get(), sizeof(Sprite::Material));
	//マテリアルにデータを書き込む
	Sprite::Material* materialData = nullptr;
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

	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLight = game.GetDirectXCom()->CreateBufferResource(game.GetDirectXCom()->GetDevice().Get(), sizeof(Object3d::DirectionalLight));

	// MapしてGPUリソースのCPU側の書き込み可能ポインタを取得する
	Object3d::DirectionalLight* directionalLightData = nullptr;
	directionalLight->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// directionalLightDataに値を書き込む
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;



	// 書き込み完了後はUnmapを呼ぶ
	directionalLight->Unmap(0, nullptr);

	// --- カメラ用のリソース作成を追加 ---
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource = game.GetDirectXCom()->CreateBufferResource(game.GetDirectXCom()->GetDevice(), sizeof(CameraForGPU));
	CameraForGPU* cameraData = nullptr;
	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
	// 初期値を設定
	cameraData->worldPosition = { 0.0f, 0.0f, -10.0f };


	//WVP用のリソースを作る。　Matrix4x4 1つのサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = game.GetDirectXCom()->CreateBufferResource(game.GetDirectXCom()->GetDevice().Get(), sizeof(TransformationMatrix));
	//データを書き込む
	TransformationMatrix* wvpData = nullptr;
	//書き込む為のアドレス取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	//単位行列を書き込む
	wvpData->World = MakeIdentity4x4();
	wvpData->WVP = MakeIdentity4x4();

	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSphere = game.GetDirectXCom()->CreateBufferResource(game.GetDirectXCom()->GetDevice().Get(), sizeof(TransformationMatrix));

	// データを書き込むためのポインタを取得
	TransformationMatrix* transformationMatrixDataSphere = nullptr;
	transformationMatrixResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSphere));
	transformationMatrixDataSphere->WVP = MakeIdentity4x4();
	transformationMatrixDataSphere->World = MakeIdentity4x4();
	// NOTE: Keep this buffer mapped for the program lifetime so we can update per-frame without remapping.

	//Transform変数を作る
	Sprite::Transform transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };


	//Sphere用
	Sprite::Transform transformObject{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	Sprite::Transform cameraTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} };

	//uvTrandform用の変数
	Sprite::Transform uvTransformSprite = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	//uvTransform行列の初期化
	materialData->uvTransform = MakeIdentity4x4();


	float fovY = 0.45f;  // 資料通り
	float aspectRatio = static_cast<float>(game.GetWindowAPI()->GetClientWidth()) / static_cast<float>(game.GetWindowAPI()->GetClientHeight());
	float nearZ = 0.1f;
	float farZ = 100.0f;

	//Textureを読んで転送する
	DirectX::ScratchImage mipImages = game.GetDirectXCom()->LoadTexture("./Resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = CreateTextureResource(game.GetDirectXCom()->GetDevice(), metadata);

	//2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = game.GetDirectXCom()->LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 = CreateTextureResource(game.GetDirectXCom()->GetDevice(), metadata2);

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = game.GetDirectXCom()->UploadTextureData(textureResource, mipImages, game.GetDirectXCom()->GetDevice().Get(), game.GetDirectXCom()->GetCommandList());
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2 = game.GetDirectXCom()->UploadTextureData(textureResource2, mipImages2, game.GetDirectXCom()->GetDevice().Get(), game.GetDirectXCom()->GetCommandList());

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
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = game.GetDirectXCom()->GetSrvDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = game.GetDirectXCom()->GetSrvDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = game.GetDirectXCom()->GetCPUDescroptirHandle(game.GetDirectXCom()->GetSrvDescriptorHeap(), game.GetDirectXCom()->GetDescriptorSizeSRV(), 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = game.GetDirectXCom()->GetGPUDescriptorHandle(game.GetDirectXCom()->GetSrvDescriptorHeap(), game.GetDirectXCom()->GetDescriptorSizeSRV(), 2);
	//先頭はImGuiに使用しているためその次を使う
	textureSrvHandleCPU.ptr += game.GetDirectXCom()->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += game.GetDirectXCom()->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//SRVの生成
	game.GetDirectXCom()->GetDevice()->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);
	//2つ目
	game.GetDirectXCom()->GetDevice()->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);
	//SRVの切り替え
	bool useMonsterBall = true;
	//Objectの描画切り替え
	bool drawObject = false;
	bool drawSprite = false;
	bool drawSphere = false;


	//音声読み込み
	Sound* sound_ = new Sound();
	sound_->Initialize();
	sound_->SoundLoadFile("Resources/Alarm01.wav");
	sound_->SoundPlayWave();

	KeyInput inputManager;
	inputManager.Initialize(game.GetWindowAPI());

	DebugCamera debugCamera_;
	debugCamera_.Initialize(game.GetWindowAPI());

	Camera* camera = new Camera();
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-10.0f });
	game.GetObject3dCom()->SetDefaultCamera(camera);


	//ビューポート
	D3D12_VIEWPORT viewport{};
	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = static_cast<float>(game.GetWindowAPI()->GetClientWidth());
	viewport.Height = static_cast<float>(game.GetWindowAPI()->GetClientHeight());
	viewport.TopLeftX = 0.0f; //左上のX座標
	viewport.TopLeftY = 0.0f; //左上のY座標
	viewport.MinDepth = 0.0f; //最小の深度
	viewport.MaxDepth = 1.0f; //最大の深度

	//シザー矩形
	D3D12_RECT scissorRect{};
	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0; //左上のX座標
	scissorRect.right = game.GetWindowAPI()->GetClientWidth(); //右下のX座標
	scissorRect.top = 0; //左上のY座標
	scissorRect.bottom = game.GetWindowAPI()->GetClientHeight(); //右下のY座標


	Sprite::Transform transformSphere{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	const uint32_t kNumMaxInstances = 10;
	uint32_t numInstance = 0;


	std::list<ParticleManager::Particle> particles;

	ParticleEmitter particleEmitter;
	Emitter emitter{};
	emitter.transform.SetTranslate({ 0.0f,0.0f,0.0f });
	emitter.transform.SetRotate({ 0.0f,0.0f,0.0f });
	emitter.transform.SetScale({ 1.0f,1.0f,1.0f });

	emitter.count = 3; // 初期値
	emitter.frequency = 0.5f;
	emitter.frequencyTime = 0.0f;

	Random::SeedEngine();
	std::mt19937 randomEngine(std::random_device{}());
	for (uint32_t index = 0; index < kNumMaxInstances; ++index)
	{
		particles.push_back(game.GetParticleManager()->MakeNewParticles(randomEngine, emitter.transform.GetTranslate()));
	}




	//TransformationMatrix gTransformationMatrices[10];

	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource =
		game.GetDirectXCom()->CreateBufferResource(game.GetDirectXCom()->GetDevice(), sizeof(ParticleManager::ParticleForGPU) * kNumMaxInstances);
	ParticleManager::ParticleForGPU* instanceData = nullptr;
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
	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU = game.GetDirectXCom()->GetCPUDescroptirHandle(game.GetDirectXCom()->GetSrvDescriptorHeap(), game.GetDirectXCom()->GetDescriptorSizeSRV(), 3);
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU = game.GetDirectXCom()->GetGPUDescriptorHandle(game.GetDirectXCom()->GetSrvDescriptorHeap(), game.GetDirectXCom()->GetDescriptorSizeSRV(), 3);
	game.GetDirectXCom()->GetDevice()->CreateShaderResourceView(instancingResource.Get(), &instancingSrvDesc, instancingSrvHandleCPU);




	const float kDeltaTime = 1.0f / 60.0f;

	ImGuiManager* imguiManager = nullptr;
	imguiManager = new ImGuiManager();
	imguiManager->Initialize(game.GetWindowAPI(), game.GetDirectXCom());

	



	//ウィンドウのxボタンが押されるまでループ
	while (game.GetDirectXCom()->GetMsg().message != WM_QUIT)
	{
		////Windowに目セージが来ていたら最優先で処理される
		if (PeekMessage(&game.GetDirectXCom()->GetMsg(), NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&game.GetDirectXCom()->GetMsg()); //メッセージを変換
			DispatchMessage(&game.GetDirectXCom()->GetMsg()); //メッセージをウィンドウプロシージャに送る
		}

		else
		{



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
			game.GetObject3d()->SetRotate(transformObject.rotate);
			game.GetObject3d()->Update();


			for (auto* sprite : game.GetSprites())
			{
				sprite->SetPosition({ 0.0f,0.0f });
				sprite->Update(game.GetWindowAPI(), &debugCamera_);
			}

			//UVTransform用
			Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
			for (auto* sprite : game.GetSprites())
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
				particles.splice(particles.end(), particleEmitter.Emit(emitter, randomEngine, *game.GetParticleManager()));
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
			for (auto* sprite : game.GetSprites())
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
				particles.splice(particles.end(), ParticleEmitter{}.Emit(emitter, randomEngine, *game.GetParticleManager()));
			}

			game.GetDirectXCom()->PreDraw();

			game.GetObject3dCom()->PreDraw();

			//RootSignatureを設定。PSOに設定しているけれど別途設定が必要
			game.GetDirectXCom()->GetCommandList()->SetGraphicsRootSignature(game.GetSpriteCom()->GetRootSignature().Get());
			// Use pipeline state stored in SpriteCom
			game.GetDirectXCom()->GetCommandList()->SetPipelineState(game.GetSpriteCom()->GetPipelineState().Get()); //パイプラインステートを設定
			//Objectの描画

			game.GetDirectXCom()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
			game.GetDirectXCom()->GetCommandList()->IASetIndexBuffer(&indexBufferViewObject);
			game.GetDirectXCom()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			game.GetDirectXCom()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			// Use Object3d WVP updated above
			game.GetDirectXCom()->GetCommandList()->SetGraphicsRootConstantBufferView(1, game.GetObject3d()->GetTransformationMatrixResource()->GetGPUVirtualAddress());
			game.GetDirectXCom()->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
			game.GetDirectXCom()->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());
			game.GetDirectXCom()->GetCommandList()->SetGraphicsRootConstantBufferView(4, cameraResource->GetGPUVirtualAddress());
			if (drawObject)
			{
				/*commandList->DrawIndexedInstanced(kIndexCount, 1, 0, 0, 0);*/
				game.GetDirectXCom()->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
			}

			if (drawSphere)
			{

				game.GetDirectXCom()->GetCommandList()->RSSetViewports(1, &viewport);
				game.GetDirectXCom()->GetCommandList()->RSSetScissorRects(1, &scissorRect);

				game.GetDirectXCom()->GetCommandList()->SetGraphicsRootSignature(game.GetObject3dCom()->GetRootSignature().Get());
				game.GetDirectXCom()->GetCommandList()->SetPipelineState(game.GetObject3dCom()->GetPipelineState().Get());	

				game.GetDirectXCom()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);
				game.GetDirectXCom()->GetCommandList()->IASetIndexBuffer(&indexBufferViewObject);
				game.GetDirectXCom()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


				game.GetDirectXCom()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

				game.GetDirectXCom()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSphere->GetGPUVirtualAddress());

				game.GetDirectXCom()->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);

				game.GetDirectXCom()->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());

				game.GetDirectXCom()->GetCommandList()->DrawIndexedInstanced(kIndexCount, 1, 0, 0, 0);
			}

			if (drawSprite)
			{
				for (auto* sprite : game.GetSprites())
				{
					sprite->Draw();
				}
			}

			//RootSignatureを設定。PSOに設定しているけれど別途設定が必要
			game.GetDirectXCom()->GetCommandList()->SetGraphicsRootSignature(game.GetParticleManager()->GetRootSignature().Get());
			game.GetDirectXCom()->GetCommandList()->SetPipelineState(game.GetParticleManager()->GetPipelineState().Get()); // パイプラインステートを設定
			//Objectの描画

			game.GetDirectXCom()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
			game.GetDirectXCom()->GetCommandList()->IASetIndexBuffer(&indexBufferViewObject);
			game.GetDirectXCom()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			game.GetDirectXCom()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			// Do NOT set a CBV at root parameter 1 because particle manager declares parameter 1 as a descriptor table.
			// Set descriptor table for instance data (root parameter 1)
			game.GetDirectXCom()->GetCommandList()->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU);
			// Set descriptor table for texture (root parameter 2)
			game.GetDirectXCom()->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
			game.GetDirectXCom()->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());
			game.GetDirectXCom()->GetCommandList()->SetGraphicsRootConstantBufferView(4, cameraResource->GetGPUVirtualAddress());

			/*commandList->DrawIndexedInstanced(kIndexCount, 1, 0, 0, 0);*/
			if (numInstance > 0)
			{
				game.GetDirectXCom()->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), numInstance, 0, 0);
			}



			//実際のcommandListのImGuiの描画コマンドを積む
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), game.GetDirectXCom()->GetCommandList().Get());


			game.GetDirectXCom()->PostDraw();
		}

	}

	//ImGui終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();


	Logger::Log(game.logStream, "Application terminating.");

	std::wstring wstringValue = L"Hello, DirectX!";
	Logger::Log(game.logStream, StringUtil::ConvertString(std::format(L"WSTRING{}\n", wstringValue)));

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

	for (auto* sprite : game.GetSprites())
	{
		delete sprite;
	}
	game.GetSprites().clear();
	delete game.GetParticleManager();
	delete game.GetObject3d();
	delete camera;

	TextureManager::GetInstance()->Finalize();


	delete[] vertexData;
	delete[] indexData;

	delete game.GetObject3dCom();
	delete game.GetSpriteCom();

	CloseHandle(game.GetDirectXCom()->GetFenceEvent());
	delete game.GetDirectXCom();

	game.GetWindowAPI()->Finalize();
	delete game.GetWindowAPI();

	return 0;
}
