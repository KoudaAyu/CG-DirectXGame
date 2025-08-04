#include<Windows.h>

//自作h
#include"AtIndex.h"
#include"BlendManager.h"
#include"DebugCamera.h"
#include"DebugLog.h"
#include"DescriptorHeap.h"
#include"GameScene.h"
#include"Graphic.h"
#include"InputLayoutManage.h"
#include"KeyInput.h"
#include"Matrix4x4.h"
#include"Model.h"
#include"RasterizerManager.h"
#include"Renderer.h"
#include"RootSignatureManager.h"
#include"Sound.h"
#include"StringUtil.h"
#include"ShaderCompile.h"
#include"Texture.h"
#include"UVTransform.h"
#include"Vector.h"
#include"Window.h"

#include<chrono> //時間を扱うライブラリ
#include<cstdint>
#include<filesystem> //ファイルやディレクトリに関する操作を行うライブラリ
#include<format> //文字列のフォーマットを行うライブラリ
#include<fstream> //ファイルにかいたり読んだりするライブラリ
#include<string> //文字列を扱うライブラリ
#include<strsafe.h>

#include<d3d12.h>
#include<dxgi1_6.h>
#include<cassert>
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
//Comptr
#include<wrl.h>

//Debug用
#include<dbghelp.h>
#pragma comment(lib,"Dbghelp.lib")

//ファイル関係 / サウンド関係
#include<sstream>
#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")


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

//Imgui使用のため
#include"externals/imgui/imgui.h"
#include"externals/imgui/imgui_impl_dx12.h"
#include"externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lParam);

//リソースリークチェック
struct D3DResourceLeakChecker
{
	~D3DResourceLeakChecker()
	{
		Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
		{
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);

		}
	}
};

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception)
{
	//時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下に出力
	SYSTEMTIME time;
	GetSystemTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	//processId(このexeのId)とクラッシュ(例外)の発生したthreadIdを取得
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();

	//設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{};
	minidumpInformation.ThreadId = threadId; //クラッシュしたスレッドのID
	minidumpInformation.ExceptionPointers = exception; //例外情報
	minidumpInformation.ClientPointers = TRUE; //クライアントポインタを使用する

	//Dumpの出力を行う。MiniDumpNormalは最小限の情報を出力する
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

	//ほかに関連付けられているSEH例外ハンドラがあったら実行。通常時はプロセスを終了
	return EXCEPTION_EXECUTE_HANDLER;
}


//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	D3DResourceLeakChecker leakChecker; //リソースリークチェック用のオブジェクト

	CoInitializeEx(0, COINIT_MULTITHREADED);

	//誰も補足しなかった場合に(Unhandled)、補足する関数を登録
	SetUnhandledExceptionFilter(ExportDump);

	HRESULT hr;

	Microsoft::WRL::ComPtr<ID3D12Device> device;

	Debug debug;
	debug.Initialize();

	Window window;
	window.Initialize();

	
	debug.EnableDebugLayer();

	window.Show();

	Graphic graphic;
	graphic.GraphicCreateDXGIFactory();

	graphic.SelectAdapter();

	graphic.SelectDevice(device);

	debug.SetupInfoQueue(device);

	graphic.CreateCommandQueue(device);
	
	graphic.CreateCommandAllocator(device);
	
	graphic.CreateCommandList(device);
	
	graphic.CreateSwapChain(window);

	graphic.CreateDescriptorHeaps(device);
	
	graphic.GetSwapChainResources();
	
	graphic.CreateRenderTargetViews(device);
	
	Renderer renderer;
	renderer.CreateFence(device, graphic.GetHRESULT());

	ShaderCompile shaderCompile;

	shaderCompile.CreateDxcCompiler(graphic.GetHRESULT());

	shaderCompile.CreateIncludeHandler(graphic.GetHRESULT());

	RootSignatureManager rootSignatureManager;
	rootSignatureManager.CreateDescriptionRootSignature();
	rootSignatureManager.CreateDescriptorRange();
	
	rootSignatureManager.CreateRootParemeter();
	rootSignatureManager.CreateStaticSamplers();
	rootSignatureManager.CreateBlob(device,graphic.GetHRESULT());
	
	//InputLayer
	InputLayoutManager inputLayer;
	inputLayer.CreateInputLayer();

	BlendManager blendManager;
	blendManager.CreateBlend();

	RasterizerManager rasterizerManager;
	rasterizerManager.RasterizerSetting();

	shaderCompile.CreateVertexShaderBlob();
	shaderCompile.CreatePixelShaderBlob();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicPipelineStateDesc{};
	graphicPipelineStateDesc.pRootSignature = rootSignatureManager.GetRootSignature().Get(); //ルートシグネチャ
	graphicPipelineStateDesc.InputLayout = inputLayer.GetInputLayoutDesc(); //入力レイアウト
	graphicPipelineStateDesc.VS = { shaderCompile.GetVertexShaderBlob()->GetBufferPointer(),
	shaderCompile.GetVertexShaderBlob()->GetBufferSize() }; //頂点シェーダーの設定
	graphicPipelineStateDesc.PS = { shaderCompile.GetPixelShaderBlob()->GetBufferPointer(),
		shaderCompile.GetPixelShaderBlob()->GetBufferSize() }; //ピクセルシェーダーの設定
	graphicPipelineStateDesc.BlendState = blendManager.GetBlendDesc(); //ブレンドステートの設定
	graphicPipelineStateDesc.RasterizerState = rasterizerManager.GetRasterizerDesc(); //ラスタライザーステートの設定
	//書き込むRTVの情報
	graphicPipelineStateDesc.NumRenderTargets = 1;
	graphicPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; //RTVのフォーマット
	//利用するトロポジ(形状)のタイプ。三角形
	graphicPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//どのように画面に色を打ち込むか設定(気にしなくていい？)
	graphicPipelineStateDesc.SampleDesc.Count = 1; //マルチサンプルしない
	graphicPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; //サンプルマスクはデフォルト

	//DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	//書き込み
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqua。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	//DepthStencilの設定
	graphic.GetGraphicPipelineStateDesc().DepthStencilState = depthStencilDesc;
	graphic.GetGraphicPipelineStateDesc().DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	//実際に生成
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicPipelineStateDesc,
		IID_PPV_ARGS(&graphicPipelineState));

	assert(shaderCompile.GetVertexShaderBlob() && "頂点シェーダーの読み込み失敗！");
	assert(shaderCompile.GetPixelShaderBlob() && "ピクセルシェーダーの読み込み失敗！");

	//パイプラインステートの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));

	// 球体
	const uint32_t kSubdivision = 16; // 16分割

	// 経度分割1つ分の角度
	const float kLonEvery = DirectX::XM_2PI / float(kSubdivision);
	// 緯度分割1つ分の角度
	const float kLatEvery = DirectX::XM_PI / float(kSubdivision);

	// 頂点数・インデックス数
	// 緯度方向と経度方向の両端に重複する頂点があるため、+1が必要
	const uint32_t kVertexCount = (kSubdivision + 1) * (kSubdivision + 1);
	const uint32_t kIndexCount = kSubdivision * kSubdivision * 6; // 各四角形に三角形2つ、各三角形に頂点3つで 2*3=6

	// 頂点配列を確保
	VertexData* vertexData = new VertexData[kVertexCount];

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
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSphere = Buffer::CreateBufferResource(device.Get(), sizeof(VertexData) * kVertexCount);
	VertexData* mappedVertex = nullptr;
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertex));
	memcpy(mappedVertex, vertexData, sizeof(VertexData) * kVertexCount);
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
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSphere = Buffer::CreateBufferResource(device.Get(), sizeof(uint32_t) * kIndexCount);
	uint32_t* mappedIndex = nullptr;
	indexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndex));
	memcpy(mappedIndex, indexData, sizeof(uint32_t) * kIndexCount);
	indexResourceSphere->Unmap(0, nullptr);

	// --- バッファビュー設定 ---
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};
	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();
	vertexBufferViewSphere.SizeInBytes = sizeof(VertexData) * kVertexCount;
	vertexBufferViewSphere.StrideInBytes = sizeof(VertexData);

	D3D12_INDEX_BUFFER_VIEW indexBufferViewSphere{};
	indexBufferViewSphere.BufferLocation = indexResourceSphere->GetGPUVirtualAddress();
	indexBufferViewSphere.SizeInBytes = sizeof(uint32_t) * kIndexCount;
	indexBufferViewSphere.Format = DXGI_FORMAT_R32_UINT;

	VertexData* mapped = nullptr;
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	memcpy(mapped, vertexData, sizeof(VertexData) * kVertexCount);
	vertexResourceSphere->Unmap(0, nullptr);



	//Sprite用の頂点Resource
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = Buffer::CreateBufferResource(device.Get(), sizeof(VertexData) * 6);
	//頂点バッファビューを生成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	//リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
	//1頂点当たりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);


	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = Buffer::CreateBufferResource(device.Get(), sizeof(uint32_t) * 6);
	//頂点バッファービューを生成する
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	//リソースの先頭アドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	//インデックスリソースにデータを書き込む
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0; // 左下
	indexDataSprite[1] = 1; // 左上
	indexDataSprite[2] = 2; // 右下
	indexDataSprite[3] = 2; // 右下
	indexDataSprite[4] = 1; // 左上
	indexDataSprite[5] = 3; // 右上

	indexResourceSprite->Unmap(0, nullptr);

	//Sprite用
	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
	//一枚目の三角形
	vertexDataSprite[0].position = { 0.0f,360.0f,0.0f,1.0f }; // 左下
	vertexDataSprite[1].position = { 0.0f,0.0f,0.0f,1.0f };   // 左上
	vertexDataSprite[2].position = { 640.0f,360.0f,0.0f,1.0f }; // 右下
	vertexDataSprite[3].position = { 640.0f,0.0f,0.0f,1.0f };   // 右上

	vertexDataSprite[0].texcoord = { 0.0f,1.0f };
	vertexDataSprite[1].texcoord = { 0.0f,0.0f };
	vertexDataSprite[2].texcoord = { 1.0f,1.0f };
	vertexDataSprite[3].texcoord = { 1.0f,0.0f };

	//ビューポート
	D3D12_VIEWPORT viewport{};
	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = static_cast<float>(window.GetClientWidth());
	viewport.Height = static_cast<float>(window.GetClientHeight());
	viewport.TopLeftX = 0.0f; //左上のX座標
	viewport.TopLeftY = 0.0f; //左上のY座標
	viewport.MinDepth = 0.0f; //最小の深度
	viewport.MaxDepth = 1.0f; //最大の深度

	//シザー矩形
	D3D12_RECT scissorRect{};
	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0; //左上のX座標
	scissorRect.right = window.GetClientWidth(); //右下のX座標
	scissorRect.top = 0; //左上のY座標
	scissorRect.bottom = window.GetClientHeight(); //右下のY座標

	UVTransform uvTransforms;
	uvTransforms.Initialize(device.Get());
	

	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLight = Buffer::CreateBufferResource(device.Get(), sizeof(DirectionalLight));

	// MapしてGPUリソースのCPU側の書き込み可能ポインタを取得する
	DirectionalLight* directionalLightData = nullptr;
	directionalLight->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// directionalLightDataに値を書き込む
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;

	// 書き込み完了後はUnmapを呼ぶ
	directionalLight->Unmap(0, nullptr);

	//WVP用のリソースを作る。　Matrix4x4 1つのサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = Buffer::CreateBufferResource(device.Get(), sizeof(TransformationMatrix));
	//データを書き込む
	TransformationMatrix* wvpData = nullptr;
	//書き込む為のアドレス取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	//単位行列を書き込む
	wvpData->World = MakeIdentity4x4();
	wvpData->WVP = MakeIdentity4x4();

	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSphere = Buffer::CreateBufferResource(device.Get(), sizeof(TransformationMatrix));

	// データを書き込むためのポインタを取得
	TransformationMatrix* transformationMatrixDataSphere = nullptr;
	transformationMatrixResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSphere));
	transformationMatrixDataSphere->WVP = MakeIdentity4x4();
	transformationMatrixDataSphere->World = MakeIdentity4x4();
	// 書き込みが完了したので、マップを解除
	transformationMatrixResourceSphere->Unmap(0, nullptr);

	//Sprite用のTransformationMatrix用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = Buffer::CreateBufferResource(device.Get(), sizeof(TransformationMatrix));
	//データを書き込む
	TransformationMatrix* transformationMatrixDataSprite = nullptr;
	//書き込むためのアドレス取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	//単位行列を書き込んでおく
	transformationMatrixDataSprite->WVP = MakeIdentity4x4();
	transformationMatrixDataSprite->World = MakeIdentity4x4();
	transformationMatrixResourceSprite->Unmap(0, nullptr);

	//Transform変数を作る
	Transform transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	//Sprite用
	Transform transformSprite{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	//Sphere用
	Transform transformSphere{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	Transform cameraTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} };

	float fovY = 0.45f;  // 資料通り
	float aspectRatio = static_cast<float>(window.GetClientWidth()) / static_cast<float>(window.GetClientHeight());
	float nearZ = 0.1f;
	float farZ = 100.0f;

	//Textureを読んで転送する
	DirectX::ScratchImage mipImages = Texture::LoadTexture("./Resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = Texture::CreateTextureResource(device, metadata);
	//DepthStecilTextureをウィンドウのサイズで生成
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource = Texture::CreateDepthStencilTextureResource(device.Get(), window.GetClientWidth(), window.GetClientHeight());

	Model model;
	model.Initialize(device.Get());

	//2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = Texture::LoadTexture(model.GetTextureFilePath());
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 = Texture::CreateTextureResource(device, metadata2);

	//DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;//2dTexture
	//DSVHeapの先頭にDSVを作る
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, graphic.GetDsvDescriptorHeap()->GetCPUDescriptorHandleForHeapStart());


	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = Texture::UploadTextureData(textureResource, mipImages, device.Get(), graphic.GetCommandList());
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2 = Texture::UploadTextureData(textureResource2, mipImages2, device.Get(), graphic.GetCommandList());

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

	//DescriptorSizeを取得しておく
	const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	//SRVを生成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = graphic.GetSrvDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = graphic.GetSrvDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = AtIndex::GetCPUDescriptorHandle(graphic.GetSrvDescriptorHeap(), descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = AtIndex::GetGPUDescriptorHandle(graphic.GetSrvDescriptorHeap(), descriptorSizeSRV, 2);

	//先頭はImGuiに使用しているためその次を使う
	textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//SRVの生成
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);
	//2つ目
	device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

	//SRVの切り替え
	bool useMonsterBall = true;
	//Sphereの描画切り替え
	bool drawSphere = true;
	bool drawSprite = false;

	KeyInput inputManager;
	inputManager.Initialize(hInstance, window.GetHwnd());

	DebugCamera debugCamera_;
	debugCamera_.Initialize(hInstance, window.GetHwnd());

	GameScene gameScene;
	gameScene.Initialize();

	Sound sound;
	sound.Initialize();
	sound.Load();
	sound.Play();


	//Imguiの初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(window.GetHwnd());
	ImGui_ImplDX12_Init(
		device.Get(),
		graphic.GetSwapChainDesc().BufferCount,
		graphic.GetRtvDesc().Format,
		graphic.GetSrvDescriptorHeap().Get(),
		graphic.GetSrvDescriptorHeap()->GetCPUDescriptorHandleForHeapStart(),
		graphic.GetSrvDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());

	//ウィンドウのxボタンが押されるまでループ
	while (renderer.GetMSG().message != WM_QUIT)
	{
		//Windowに目セージが来ていたら最優先で処理される
		if (PeekMessage(&renderer.GetMSG(), NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&renderer.GetMSG()); //メッセージを変換
			DispatchMessage(&renderer.GetMSG()); //メッセージをウィンドウプロシージャに送る
		}
		else
		{
			//Imguiにここからフレームが始まる趣旨をつたえる
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			debugCamera_.Update();

			//ゲームの処理
			/*transformSphere.rotate.y += 0.01f;*/
			Matrix4x4 worldMatrix = MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);
			Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
			Matrix4x4 viewMatrix = Inverse(cameraMatrix);
			Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(fovY, aspectRatio, nearZ, farZ);
			//WVPMatrixを作る
			Matrix4x4 worldViewProjectMatrix = Multiply(worldMatrix, Multiply(debugCamera_.GetViewMatrix(), projectionMatrix));
			transformationMatrixDataSphere->WVP = worldViewProjectMatrix;
			transformationMatrixDataSphere->World = worldMatrix;

			//Sprite用のworldViewProjectMatrix
			Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
			Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(window.GetClientWidth()), float(window.GetClientHeight()), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionmatrixSprite = Multiply(worldMatrixSprite, Multiply(debugCamera_.GetViewMatrix(), projectionMatrixSprite));
			transformationMatrixDataSprite->WVP = worldViewProjectionmatrixSprite;
			transformationMatrixDataSprite->World = worldMatrixSprite;

			uvTransforms.Update();

			//開発用UIの処理、実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換え
			ImGui::ShowDemoWindow();

			ImGui::Begin("Windows");

			ImGui::Checkbox("useMonsterBall", &useMonsterBall);
			ImGui::Checkbox("LightSprite Flag", (bool*)& uvTransforms.GetMaterialData()->enableLighting);
			ImGui::Checkbox("LightSphere Flag", (bool*)&uvTransforms.GetMaterialDataSprite()->enableLighting);

			ImGui::Checkbox("DrawSphere", &drawSphere);
			ImGui::Checkbox("DrawSprite", &drawSprite);
			ImGui::DragFloat3("LightDirection", &directionalLightData->direction.x, 0.01f);

			ImGui::DragFloat3("Sphere Rotate", &transformSphere.rotate.x);

			ImGui::DragFloat2("UVTranslate", &uvTransforms.GetUVTranslateSprite().x, 0.01f, -10.0f, 10.0f);
			ImGui::DragFloat2("UVScale", &uvTransforms.GetUVScaleSprite().x, 0.01f, -10.0f, 10.0f);
			ImGui::DragFloat("UVRotate", &uvTransforms.GetUVRotateSprite().z, 0.01f);

			ImGui::End();

			//ImGui内部コマンドを生成する
			ImGui::Render();

			inputManager.Update();

			//これから書き込むバックバッファのインデックスを取得する
			UINT backBufferIndex = graphic.GetSwapChain()->GetCurrentBackBufferIndex();

			//TransitionBarrierの設定
			D3D12_RESOURCE_BARRIER barrier{};
			//今回のバリアはTransition
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			//Noneにしておく
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			//バリアを張る対象のリソース。現在のバックバッファに対し行う
			barrier.Transition.pResource = graphic.GetSwapChainResource(backBufferIndex).Get();
			//遷移前(現在)のResourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			//遷移後のResourceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			//TransitionBarrierを張る
			graphic.GetCommandList()->ResourceBarrier(1, &barrier);



			//描画先のRTVとDSVを設定する
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = graphic.GetDsvDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
			graphic.GetCommandList()->OMSetRenderTargets(1, &graphic.GetRtvHandles(backBufferIndex), false, &dsvHandle);
			float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };//RGBAの値。青っぽい色
			graphic.GetCommandList()->ClearRenderTargetView(graphic.GetRtvHandles(backBufferIndex), clearColor, 0, nullptr);

			graphic.GetCommandList()->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			//描画用のDescriptorHeapの設定
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap[] = { graphic.GetSrvDescriptorHeap().Get() };
			graphic.GetCommandList()->SetDescriptorHeaps(1, descriptorHeap->GetAddressOf());

			//コマンドを積む
			graphic.GetCommandList()->RSSetViewports(1, &viewport); //ビューポートを設定
			graphic.GetCommandList()->RSSetScissorRects(1, &scissorRect); //シザー矩形を設定
			//RootSignatureを設定。PSOに設定しているけれど別途設定が必要
			graphic.GetCommandList()->SetGraphicsRootSignature(rootSignatureManager.GetRootSignature().Get());
			graphic.GetCommandList()->SetPipelineState(graphicPipelineState.Get()); //パイプラインステートを設定
			//Sphereの描画

			graphic.GetCommandList()->IASetVertexBuffers(0, 1, &model.GetVertexBufferView());
			graphic.GetCommandList()->IASetIndexBuffer(&indexBufferViewSphere);
			graphic.GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			uvTransforms.Draw(graphic.GetCommandList().Get());
			graphic.GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSphere->GetGPUVirtualAddress());
			graphic.GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
			graphic.GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());
			if (drawSphere)
			{
				model.Draw(graphic.GetCommandList().Get());
			}

			if (drawSprite)
			{
				graphic.GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
				graphic.GetCommandList()->IASetIndexBuffer(&indexBufferViewSprite);
				graphic.GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				uvTransforms.DrawSprite(graphic.GetCommandList().Get());
				graphic.GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
				graphic.GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
				graphic.GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());

				graphic.GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
			}


			//実際のcommandListのImGuiの描画コマンドを積む
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), graphic.GetCommandList().Get());

			//画面に描く処理は終わり画面に映すので、状態を遷移
			//RenderTargetからPresentにする
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			//TransitionBarrierを張る
			graphic.GetCommandList()->ResourceBarrier(1, &barrier);



			//コマンドリストの内容を下記率させる。すべてのコマンドを積んでからCloseする
			hr = graphic.GetCommandList()->Close();
			//コマンドリストのCloseに失敗した場合はエラー
			assert(SUCCEEDED(hr));

			//GUPにコマンドリストの実行を行わせる
			ID3D12CommandList* commandLists[] = { graphic.GetCommandList().Get() };
			graphic.GetCommandQueue()->ExecuteCommandLists(1, commandLists);
			//GUPとOSに画面の交換を要求する
			graphic.GetSwapChain()->Present(1, 0);

			//Fenceの値を更新
			renderer.GetFenceValue()++;
			//GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
			graphic.GetCommandQueue()->Signal(renderer.GetFence().Get(), renderer.GetFenceValue());

			//Fenceの値が指定したSignalの値にたどり着いているか確認する
			//GetCompletedValueの初期値はFence作成時に渡した初期値
			if (renderer.GetFence()->GetCompletedValue() < renderer.GetFenceValue())
			{
				//指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
				renderer.GetFence()->SetEventOnCompletion(renderer.GetFenceValue(), renderer.GetFenceEvent());

				//イベントを待つ
				WaitForSingleObject(renderer.GetFenceEvent(), INFINITE);
			}


			//次フレーム用のコマンドリストを用意
			hr = graphic.GetCommandAllocator()->Reset();
			//コマンドアロケーターのリセットに失敗した場合はエラー
			assert(SUCCEEDED(hr));
			//コマンドリストをリセットする
			hr = graphic.GetCommandList()->Reset(graphic.GetCommandAllocator().Get(), nullptr);
			//コマンドリストのリセットに失敗した場合はエラー
			assert(SUCCEEDED(hr));


		}
	}

	//ImGui終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	//COMの終了処理
	CoUninitialize();

	Debug::Log(debug.GetStream(), "Application terminating.");

	std::wstring wstringValue = L"Hello, DirectX!";
	Debug::Log(debug.GetStream(), StringUtil::ConvertString(std::format(L"WSTRING{}\n", wstringValue)));


	//出力ウィンドウへの文字出力
	OutputDebugStringA("Hello, DirextX!\n");

	CloseHandle(renderer.GetFenceEvent());

	

	sound.~Sound();


	delete[] vertexData;
	delete[] indexData;
	// --- ウィンドウ解放 ---
	CloseWindow(window.GetHwnd()); //ウィンドウの解放

	return 0;
}

