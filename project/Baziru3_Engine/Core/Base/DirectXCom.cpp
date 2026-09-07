#include "DirectXCom.h"

#include<cassert>
#include <format>
#include <thread>
#include <vector>
#include <immintrin.h>

#include"d3dx12.h"
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "winmm.lib")

#include"Log.h"
#include"StringUtil.h"

#include"ImGuiManager.h"
#include "Baziru3_Engine/Core/Base/Allocator/ConstantBufferAllocator.h"
#include "Baziru3_Engine/Core/Base/Allocator/StackAllocator.h"
#include "Baziru3_Engine/Graphics/Graphics/GpuProfiler.h"

using namespace Microsoft::WRL;

DirectXCom::DirectXCom(WindowAPI* windowAPI, std::ostream& logStream)
	: windowAPI(windowAPI), logStream(logStream)
{
}

DirectXCom::~DirectXCom()
{
	timeEndPeriod(1);
}

void DirectXCom::InitializeFixFPS()
{
	timeBeginPeriod(1);
	refrence_ = std::chrono::steady_clock::now();
}

void DirectXCom::UpdateFixFPS()
{
	if (!enableFpsLimit_ || targetFps_ <= 0.0f)
	{
		refrence_ = std::chrono::steady_clock::now();
		return;
	}

	// 目標フレーム時間（マイクロ秒: 60FPS時 約16666µs）
	const int64_t targetMicroseconds = static_cast<int64_t>(1000000.0 / static_cast<double>(targetFps_));
	const std::chrono::microseconds targetDuration(targetMicroseconds);

	// 次のフレーム目標時刻（累積方式でジッターを完全吸収）
	auto targetTime = refrence_ + targetDuration;
	auto now = std::chrono::steady_clock::now();

	// 目標時刻まで高精度待機
	if (now < targetTime)
	{
		// 残り時間が 1.5ms 以上ある場合はスリープで CPU 負荷を抑制
		while (std::chrono::duration_cast<std::chrono::microseconds>(targetTime - std::chrono::steady_clock::now()).count() > 1500)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		// 残り時間はマイクロ秒スピンロックで極限の 60.000 FPS 精度を達成
		while (std::chrono::steady_clock::now() < targetTime)
		{
			_mm_pause();
		}
	}

	// 累積タイマー基準点更新
	now = std::chrono::steady_clock::now();
	// 極端な遅延（2フレーム以上遅延）が発生した場合は基準点を再同期
	if (now - targetTime > targetDuration * 2)
	{
		refrence_ = now;
	}
	else
	{
		refrence_ = targetTime;
	}
}

void DirectXCom::Initialize()
{
	assert(windowAPI);
	this->windowAPI = windowAPI;

	InitializeFixFPS();

	GraphicCreateDXGIFactory();
	SelectAdapter();
	CreateDevice();
	SetupD3D12InfoQueue();
	InitializeCommandList();
	CreateSwapChain();
	CreateDescriptorHeaps();
	InitializeRenderTargetView();
	InitializeDepthStencilView();
	CreateFence();
	CreateViewportRect();
	CerateScissorRect();
	CreateDxcCompiler();
	InitializeImGui();

	// 定数バッファアロケーターの生成・初期化
	cbAllocator_ = std::make_unique<ConstantBufferAllocator>(this);
	cbAllocator_->Initialize();

	// スタックアロケーターの生成・初期化
	stackAllocator_ = std::make_unique<StackAllocator>();
	stackAllocator_->Initialize(16 * 1024 * 1024); // 16MB

	// GPUプロファイラーの初期化
	GpuProfiler::GetInstance()->Initialize(device.Get(), commandQueue.Get());
}

void DirectXCom::Finalize()
{
	// GPUプロファイラーの解放
	GpuProfiler::GetInstance()->Finalize();

	// GPUが処理を終えるのを待つ
	if (commandQueue)
	{
		if (commandList && commandAllocator && fence && fenceEvent)
		{
			// 終了/実行/待機/リセットを実。
			ExecuteAndWaitForGPU();
		}
		else if (fence && fenceEvent)
		{
			fenceValue++;
			commandQueue->Signal(fence.Get(), fenceValue);
			if(fence->GetCompletedValue() < fenceValue)
			{
				fence->SetEventOnCompletion(fenceValue, fenceEvent);
				WaitForSingleObject(fenceEvent, INFINITE);
			}
		}
	}

	// スワップチェーンのリソースを解放する前に、コマンドリストとコマンドアロケーターをリセットしておく
	for (auto& res : swapChainResources) res.Reset();

	rtvDescriptorHeap_.Finalize();
	srvDescriptorHeap_.Finalize();
	dsvDescriptorHeap_.Finalize();
	descriptorHeap.Reset();
	depthStencilResource.Reset();
	commandList.Reset();
	commandAllocator.Reset();
	commandQueue.Reset();

	// フェンスを解放しハンドルを閉じる
	if (fenceEvent)
	{
		CloseHandle(fenceEvent);
		fenceEvent = nullptr;
	}
	fence.Reset();

	dxcCompiler.Reset();
	dxcUtils.Reset();
	includeHandler.Reset();
	swapChain.Reset();
	useAdapter.Reset();
	device.Reset();
	dxgiFactory.Reset();

	infoQueue.Reset();
	debugController.Reset();

	if (cbAllocator_)
	{
		cbAllocator_->Finalize();
		cbAllocator_.reset();
	}

	if (stackAllocator_)
	{
		stackAllocator_.reset();
	}
}

void DirectXCom::DebugLayer()
{
#ifdef _DEBUG
	// 特定のNVIDIA GPU/ドライバ環境でD3D12 Debug Layerが原因のDevice Removal (0x887A0005) が発生するため一時的に無効化
	/*
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(FALSE);
	}
	*/
#endif
}

//DXGIファクトリーの生成
void DirectXCom::GraphicCreateDXGIFactory()
{

	//HRESULTはWindoes系のエラーコード
	//関数が成功したか同課をSUCCEEDEDマクロで判断する
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	assert(SUCCEEDED(hr));
}

//使用するアダプタ用の変数。最初にnullptrを入れる
void DirectXCom::SelectAdapter()
{


	//いい順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i)
	{

		//アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));//ここで止まった場合一大事

		//ソフトウェアアダプタでなければ採用する
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
		{
			Logger::Log(logStream, std::format("Using adapter: {}\n", StringUtil::ConvertString(adapterDesc.Description)));
			break;
		}
		useAdapter = nullptr; //ソフトウェアアダプタの場合は見なかったことにするためしないのでnullptr
	}

	//アダプターが見つからなかった場合はエラー
	assert(useAdapter != nullptr);
}

void DirectXCom::CreateDevice()
{
	//機能レベルを順に試していく
	for (size_t i = 0; i < featureLevelNamesCount; ++i)
	{
		//採用したアダプタでデバイスを作成
		hr = D3D12CreateDevice(
			useAdapter.Get(),
			featureLevels[i],
			IID_PPV_ARGS(&device)
		);

		//指定した昨日レベルでデバイスが生成できたか確認
		if (SUCCEEDED(hr))
		{
			//生成出来たのでログ出力を行う
			Logger::Log(logStream, std::format("Feature Level: {}\n", featureLevelNames[i]));
			break; //ループを抜ける
		}


	}

	//デバイスの生成に失敗し起動できない
	assert(device != nullptr);
	Logger::Log(logStream, std::format("Complete create D3D12Device!"));//初期起動完了のLogを出す
}

void DirectXCom::SetupD3D12InfoQueue()
{
#ifdef _DEBUG

	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
	{
		//重大なエラーの時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);

		//エラーの時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

		//警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);



		//抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] =
		{
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
			D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND
		};

		D3D12_MESSAGE_SEVERITY serverities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds); //抑制するメッセージの数
		filter.DenyList.pIDList = denyIds; //抑制するメッセージのID
		filter.DenyList.NumSeverities = _countof(serverities); //抑制するメッセージの重要度の数
		filter.DenyList.pSeverityList = serverities; //抑制するメッセージの重要度

		infoQueue->PushStorageFilter(&filter); //フィルターを適用する
	}
#endif
}

void DirectXCom::InitializeCommandList()
{
	CreateCommandAllocator();
	CreateCommandList();
	CreateCommandQueue();

	// ワーカー用の作成
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&workerCommandAllocator_));
	assert(SUCCEEDED(hr));
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, workerCommandAllocator_.Get(), nullptr, IID_PPV_ARGS(&workerCommandList_));
	assert(SUCCEEDED(hr));
	// 初期状態は Close しておく
	workerCommandList_->Close();
}

//コマンドアロケーターを生成する
void DirectXCom::CreateCommandAllocator()
{
	hr = (device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));

	//コマンドアロケーターの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));
}

//コマンドリストの生成
void DirectXCom::CreateCommandList()
{
	hr = (device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
	//コマンドリストの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));
}

//コマンドキューの生成
void DirectXCom::CreateCommandQueue()
{
	hr = (device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue)));
	//コマンドキューの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));
}

//スワップチェーンを生成する
void DirectXCom::CreateSwapChain()
{


	swapChainDesc.Width = windowAPI->GetClientWidth(); //ウィンドウの幅
	swapChainDesc.Height = windowAPI->GetClientHeight(); //ウィンドウの高さ
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //色の形式
	swapChainDesc.SampleDesc.Count = 1; //マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; //レンダリングターゲットとして使用
	swapChainDesc.BufferCount = 2; //ダブルバッファリング
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; //モニターに映ったら描画を破棄

	//コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr = (dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), windowAPI->GetHwnd(), &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf())));
	//スワップチェーンの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));
}

void DirectXCom::CreateUnroaderedAccessView(const Microsoft::WRL::ComPtr<ID3D12Resource>& resource,
	UINT NumElements, UINT structureByteStride,D3D12_CPU_DESCRIPTOR_HANDLE uavCpuDescriptorHandle)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN; //Format
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER; //View Dimension
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = NumElements; //要素数
	uavDesc.Buffer.CounterOffsetInBytes = 0; //カウンターオフセット
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE; //フラグ
	uavDesc.Buffer.StructureByteStride = structureByteStride; //ストライド

	device->CreateUnorderedAccessView(resource.Get(), nullptr, &uavDesc, uavCpuDescriptorHandle);
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCom::CreateDepthStencilTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, int32_t width, int32_t height)
{
	//生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;//Textureの幅
	resourceDesc.Height = height;//textureの高さ
	resourceDesc.MipLevels = 1;//mipmapの数
	resourceDesc.DepthOrArraySize = 1;//奥行き or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//TextureのFormat
	resourceDesc.SampleDesc.Count = 1;//サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//Textureの次元数。普段使っているのは2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;//DepthStencilとして使う通知

	//2. 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//VRAM上に作る

	//深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;//1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//フォーマット。Resourceと合わせる

	//3. Resourceを生成する
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,//Heapの設定
		D3D12_HEAP_FLAG_NONE,//Heapの特殊な設定
		&resourceDesc,//Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE,//深度値を書き込む状態にしておく
		&depthClearValue,//Clear最適値。
		IID_PPV_ARGS(&resource));//作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

//DescriptorHeapの作成関数
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCom::CreateDescriptorHeap(const Microsoft::WRL::ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	return descriptorHeap;
}

void DirectXCom::CreateDescriptorHeaps()
{

	const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);


	rtvDescriptorHeap_.Initialize(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	srvDescriptorHeap_.Initialize(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMacSRVCount, true);
	dsvDescriptorHeap_.Initialize(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

}

void DirectXCom::InitializeRenderTargetView()
{
	//SwapChainからResorrceを取得する
	hr = (swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0])));
	//うまく取得できなければエラー
	assert(SUCCEEDED(hr));
	hr = (swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1])));
	//うまく取得できなければエラー
	assert(SUCCEEDED(hr));

	//RTVの設定
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; //出力結果をSRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; //2Dテクスチャとして書き込む
	//ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap_.GetCPUDescriptorHandle(0);


	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (int i = 0; i < 2; ++i)
	{
		rtvHandles[i].ptr = rtvStartHandle.ptr + descriptorSize * i;
		device->CreateRenderTargetView(swapChainResources[i].Get(), &rtvDesc, rtvHandles[i]);
	}

}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCom::GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCom::GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

void DirectXCom::InitializeDepthStencilView()
{
	//DepthStecilTextureをウィンドウのサイズで生成
	depthStencilResource = CreateDepthStencilTextureResource(device.Get(), windowAPI->GetClientWidth(), windowAPI->GetClientHeight());

	//DepthStecilTextureをウィンドウのサイズで生成
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;//2dTexture
	//DSVHeapの先頭にDSVを作る
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvDescriptorHeap_.GetCPUDescriptorHandle(0));
}

void DirectXCom::CreateFence()
{

	hr = (device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	//フェンスの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));

	fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	//フェンスイベントの生成に失敗した場合はエラー
	assert(fenceEvent != nullptr);
}

//ビューポート
void DirectXCom::CreateViewportRect()
{

	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = static_cast<float>(windowAPI->GetClientWidth());
	viewport.Height = static_cast<float>(windowAPI->GetClientHeight());
	viewport.TopLeftX = 0.0f; //左上のX座標
	viewport.TopLeftY = 0.0f; //左上のY座標
	viewport.MinDepth = 0.0f; //最小の深度
	viewport.MaxDepth = 1.0f; //最大の深度
}

//シザー矩形
void DirectXCom::CerateScissorRect()
{

	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0; //左上のX座標
	scissorRect.right = windowAPI->GetClientWidth(); //右下のX座標
	scissorRect.top = 0; //左上のY座標
	scissorRect.bottom = windowAPI->GetClientHeight(); //右下のY座標
}

void DirectXCom::CreateDxcCompiler()
{
	// COMが未初期化だった場合に対する安全初期化ガード
	HRESULT comHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	(void)comHr;

	// dxcompiler.dll から直接 DxcCreateInstance を取得して呼び出す（COMレジストリ依存を回避）
	HMODULE hDxc = LoadLibraryA("dxcompiler.dll");
	DxcCreateInstanceProc pfnDxcCreateInstance = nullptr;
	if (hDxc)
	{
		pfnDxcCreateInstance = reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(hDxc, "DxcCreateInstance"));
	}

	auto callDxcCreate = [&](REFCLSID rclsid, REFIID riid, LPVOID* ppv) -> HRESULT {
		if (pfnDxcCreateInstance)
		{
			return pfnDxcCreateInstance(rclsid, riid, ppv);
		}
		return DxcCreateInstance(rclsid, riid, ppv);
	};

	// 1. DxcUtilsの初期化
	hr = callDxcCreate(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	if (FAILED(hr))
	{
		char msg[512];
		sprintf_s(msg, "DxcCreateInstance(CLSID_DxcUtils) failed!\nHRESULT: 0x%08X\n\nPossible cause:\n- dxcompiler.dll is missing or outdated in exe directory\n- Windows SDK is missing or incompatible", hr);
		MessageBoxA(nullptr, msg, "DirectX Shader Compiler Error", MB_OK | MB_ICONERROR);
		assert(SUCCEEDED(hr));
	}

	// 2. DxcCompilerの初期化
	hr = callDxcCreate(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	if (FAILED(hr))
	{
		char msg[512];
		sprintf_s(msg, "DxcCreateInstance(CLSID_DxcCompiler) failed!\nHRESULT: 0x%08X", hr);
		MessageBoxA(nullptr, msg, "DirectX Shader Compiler Error", MB_OK | MB_ICONERROR);
		assert(SUCCEEDED(hr));
	}

	// 3. 現時点ではincludeしないが、includeに対応する為の設定を行う
	hr = (dxcUtils->CreateDefaultIncludeHandler(&includeHandler));
	assert(SUCCEEDED(hr));
}

void DirectXCom::InitializeImGui()
{
	// ImGui initialization is handled by ImGuiManager to avoid double-initialization.
	// Keep this function empty so DirectXCom does not initialize ImGui.
}

void DirectXCom::PreDraw()
{
	if (cbAllocator_)
	{
		cbAllocator_->BeginFrame();
	}

	// GPUプロファイラーのフレーム開始
	GpuProfiler::GetInstance()->BeginFrame(commandList.Get());

	if (stackAllocator_)
	{
		stackAllocator_->Reset();
	}

	//これから書き込むバックバッファのインデックスを取得する
	backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	//リソースバリアで書き込み可能に
	//TransitionBarrierの設定
	//今回のバリアはTransition
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//Noneにしておく
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	//バリアを張る対象のリソース。現在のバックバッファに対し行う
	barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	//遷移前(現在)のResourceState
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	//遷移後のResourceState
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	//TransitionBarrierを張る
	commandList->ResourceBarrier(1, &barrier);

	//描画先のRTVとDSVを設定する
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_.GetCPUDescriptorHandle(0);
	commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);

	float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };//RGBAの値。青っぽい色
	commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	//描画用のDescriptorHeapの設定
	ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap_.GetHeap() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	//コマンドを積む
	commandList->RSSetViewports(1, &viewport); //ビューポートを設定
	commandList->RSSetScissorRects(1, &scissorRect); //シザー矩形を設定



}

void DirectXCom::PostDraw()
{
	UpdateFixFPS();

	backBufferIndex = swapChain->GetCurrentBackBufferIndex();
	//画面に描く処理は終わり画面に映すので、状態を遷移
		//RenderTargetからPresentにする
	barrier.Transition.StateBefore = (D3D12_RESOURCE_STATE_RENDER_TARGET);
	barrier.Transition.StateAfter = (D3D12_RESOURCE_STATE_PRESENT);
	//TransitionBarrierを張る
	commandList->ResourceBarrier(1, &barrier);

	// GPUプロファイラーのフレーム終了（Resolveコマンドを積む）
	GpuProfiler::GetInstance()->EndFrame(commandList.Get());

	//コマンドリストの内容を下記率させる。すべてのコマンドを積んでからCloseする
	hr = (commandList->Close());
	if (FAILED(hr))
	{
		Logger::Log(logStream, std::format("DirectXCom::PostDraw - commandList->Close failed hr=0x{:08X}\n", static_cast<unsigned int>(hr)));
		// Do not attempt to execute a closed/invalid command list
		return;
	}

	//GUPにコマンドリストの実行を行わせる
	if (!commandList || !commandQueue)
	{
		Logger::Log(logStream, "DirectXCom::PostDraw - commandList or commandQueue is null. Skipping ExecuteCommandLists.\n");
		return;
	}

	PrintDebugMessages();
	ID3D12CommandList* commandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(1, commandLists);

	// Check for device removal after execute
	HRESULT deviceRemoved = device->GetDeviceRemovedReason();
	if (FAILED(deviceRemoved))
	{
		Logger::Log(logStream, std::format("DirectXCom::PostDraw - device removed or error after ExecuteCommandLists hr=0x{:08X}\n", static_cast<unsigned int>(deviceRemoved)));
	}
	// GUPとOSに画面の交換を要求する (高精度リミッターで60FPS完全固定するためVSync=0)
	swapChain->Present(0, 0);

	//Fenceの値を更新
	fenceValue++;

	//GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
	commandQueue->Signal(fence.Get(), fenceValue);

	//Fenceの値が指定したSignalの値にたどり着いているか確認する
	//GetCompletedValueの初期値はFence作成時に渡した初期値
	if (fence->GetCompletedValue() < fenceValue)
	{
		//指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
		fence->SetEventOnCompletion(fenceValue, fenceEvent);

		//イベントを待つ
		WaitForSingleObject(fenceEvent, INFINITE);
	}


	//次フレーム用のコマンドリストを用意
	hr = (commandAllocator->Reset());
	//コマンドアロケーターのリセットに失敗した場合はエラー
	assert(SUCCEEDED(hr));
	//コマンドリストをリセットする
	hr = (commandList->Reset(commandAllocator.Get(), nullptr));
	//コマンドリストのリセットに失敗した場合はエラー
	assert(SUCCEEDED(hr));

	// ワーカー用の次フレーム準備
	hr = workerCommandAllocator_->Reset();
	assert(SUCCEEDED(hr));
	hr = workerCommandList_->Reset(workerCommandAllocator_.Get(), nullptr);
	assert(SUCCEEDED(hr));
	workerCommandList_->Close();

}

void DirectXCom::ExecuteAndWaitForGPU()
{
    // Close command list
    hr = commandList->Close();
    assert(SUCCEEDED(hr));

    // Execute
    ID3D12CommandList* lists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(1, lists);

    // Signal and wait
    fenceValue++;
    commandQueue->Signal(fence.Get(), fenceValue);
    if (fence->GetCompletedValue() < fenceValue)
    {
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    // Reset allocator and list for further recording
    hr = commandAllocator->Reset();
    assert(SUCCEEDED(hr));
    hr = commandList->Reset(commandAllocator.Get(), nullptr);
    assert(SUCCEEDED(hr));
}

Microsoft::WRL::ComPtr<IDxcBlob> DirectXCom::CompileShader(const std::wstring& filePath, const wchar_t* profile, Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils, Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler, Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler, std::ostream& logStream)
{
	//これからシェーダーをコンパイルする旨をログに出す
	Logger::Log(logStream, StringUtil::ConvertString(std::format(L"Begin CompileShader, path{},profile:{}\n", filePath, profile)));
	//hlslファイルを読み込む
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderScore = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderScore);
	//ファイルの読み込みに失敗した場合はエラー
	assert(SUCCEEDED(hr));
	//読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderScore->GetBufferPointer();
	shaderSourceBuffer.Size = shaderScore->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;//UTF8の文字コードである事を通知する

    // Build compiler arguments. Include the shader's directory in include path so
	// #include "..." resolves relative includes like "Object3d.hlsli".
	std::wstring shaderDir;
	size_t pos = filePath.find_last_of(L"/\\");
	if (pos != std::wstring::npos)
	{
		shaderDir = filePath.substr(0, pos);
	}

	// Keep strings alive for the duration of the call
	std::vector<std::wstring> argStrings;
	argStrings.push_back(filePath);
	argStrings.push_back(L"-E"); argStrings.push_back(L"main");
	argStrings.push_back(L"-T"); argStrings.push_back(profile);
	// NVIDIAドライバのクラッシュ回避のため、Debugビルド時でも-Zi,-Odを無効化し-O3（最適化）を適用
	// argStrings.push_back(L"-Zi"); argStrings.push_back(L"Qembed_debug");
	// argStrings.push_back(L"-Od");
	argStrings.push_back(L"-O3");
	argStrings.push_back(L"-Zpr");
	// カレントディレクトリ（プロジェクトルート）もインクルード検索パスに追加
	argStrings.push_back(L"-I");
	argStrings.push_back(L".");

	if (!shaderDir.empty())
	{
		argStrings.push_back(L"-I");
		argStrings.push_back(shaderDir);
	}

	std::vector<LPCWSTR> arguments;
	arguments.reserve(argStrings.size());
	for (auto& s : argStrings) arguments.push_back(s.c_str());

	//実際にShaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcResult> shaderResult = nullptr;
    hr = dxcCompiler->Compile(
		&shaderSourceBuffer, //コンパイルするシェーダーの内容
		arguments.data(), //コンパイル時の引数
		static_cast<UINT32>(arguments.size()), //引数の数
		includeHandler.Get(), //includeハンドラ
		IID_PPV_ARGS(&shaderResult) //結果を受け取るポインタ
	);

	//警告やエラーがあった場合はログに出力し停止する
	Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError = nullptr;

#pragma warning(push)
#pragma warning(disable: 6387) // C6387 警告を抑制
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
#pragma warning(pop)

	if (shaderError != nullptr && shaderError->GetStringLength() != 0)
	{
		Logger::Log(logStream, shaderError->GetStringPointer());

		//警告やエラーがあった場合は、Shaderのコンパイルに失敗したとする
		assert(SUCCEEDED(hr));
	}

	//コンパイルの結果から実行用のバイナリ部分を取得
	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	//Shaderのコンパイルに失敗した場合はエラー
	assert(SUCCEEDED(hr));

	//Shaderのコンパイルに成功したので、ログに出力する
	Logger::Log(logStream, StringUtil::ConvertString(std::format(L"Complete CompileShader, path{},profile:{}\n", filePath, profile)));


	return shaderBlob; //コンパイルしたShaderのバイナリを返す
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCom::CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, size_t sizeInBytes)
{
	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // アップロード用のヒープ

	// 頂点リソースの設定（今回は汎用的なバッファとして設定）
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; // バッファ
	resourceDesc.Width = sizeInBytes; // 指定されたサイズ
	// バッファの場合はこれらを1にする決まり
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // 行優先

	Microsoft::WRL::ComPtr<ID3D12Resource> bufferResource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, // データ書き込み用なのでREAD
		nullptr,
		IID_PPV_ARGS(&bufferResource));
	assert(SUCCEEDED(hr)); // 失敗したらassertで止める

	return bufferResource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCom::UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages, const Microsoft::WRL::ComPtr<ID3D12Device>& device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subResources;
	DirectX::PrepareUpload(device.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subResources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subResources.size()));
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(device, intermediateSize);
	UpdateSubresources(commandList.Get(), texture.Get(), intermediateResource.Get(), 0, 0, UINT(subResources.size()), subResources.data());
	//textureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);
	return intermediateResource;
}

DirectX::ScratchImage DirectXCom::LoadTexture(const std::string& filePath)
{
	//テクスチャファイルを読み込んでプログラムで使えるようにする
    DirectX::ScratchImage image{};
    std::wstring filePathW = StringUtil::ConvertString(filePath);
    HRESULT hr = E_FAIL;

    // ファイル拡張子で読み込み方法を選択する (.dds は DirectXTex の DDS ローダーを使う)
    if (filePathW.ends_with(L".dds") || filePathW.ends_with(L".DDS"))
    {
        hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
    }
    else
    {
        // WIC で読み込む（SRGB を強制）
        hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    }

    // 読み込み失敗時はダミー（4x4 単色ホワイト）テクスチャをオンメモリー生成してフォールバック
    if (FAILED(hr))
    {
        char warnBuf[512];
        sprintf_s(warnBuf, "[DirectXCom Warning] Failed to load texture '%s' (hr=0x%08X). Generating fallback texture.\n", filePath.c_str(), static_cast<unsigned int>(hr));
        OutputDebugStringA(warnBuf);

        // 4x4 ホワイトピクセルをオンメモリ生成
        hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4, 1, 1);
        if (SUCCEEDED(hr))
        {
            const DirectX::Image* img = image.GetImage(0, 0, 0);
            if (img && img->pixels)
            {
                uint32_t* pixels = reinterpret_cast<uint32_t*>(img->pixels);
                for (size_t i = 0; i < 16; ++i)
                {
                    pixels[i] = 0xFFFFFFFF; // 純白不透明 RGBA
                }
            }
        }
    }

    // ミニマップの作成／準備
    DirectX::ScratchImage mipImages{};
    if (DirectX::IsCompressed(image.GetMetadata().format))
    {
        // 圧縮フォーマットはそのまま使う（既にミップが含まれていることが多い）
        mipImages = std::move(image);
    }
    else
    {
        // レベル 0 を指定するとフルチェーンを生成する
        hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
        if (FAILED(hr))
        {
            // MipMap生成に失敗した場合はそのまま使用
            mipImages = std::move(image);
        }
    }

    // ミニマップ付きのデータを返す
    return mipImages;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCom::CreateTextureResource(const DirectX::TexMetadata& metadata)
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

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCom::GetSRVHandleCPU(uint32_t index)
{
	return srvDescriptorHeap_.GetCPUDescriptorHandle(index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCom::GetSRVHandleGPU(uint32_t index)
{
	return srvDescriptorHeap_.GetGPUDescriptorHandle(index);
}


const char* DirectXCom::featureLevelNames[] = {
	"12.2",
	"12.1",
	"12.0",
};

const D3D_FEATURE_LEVEL DirectXCom::featureLevels[] = {
	D3D_FEATURE_LEVEL_12_2,
	D3D_FEATURE_LEVEL_12_1,
	D3D_FEATURE_LEVEL_12_0,
};

const size_t DirectXCom::featureLevelNamesCount = sizeof(DirectXCom::featureLevelNames) / sizeof(DirectXCom::featureLevelNames[0]);

void DirectXCom::PrintDebugMessages()
{
	if (!infoQueue) return;
	UINT64 messageCount = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
	for (UINT64 i = 0; i < messageCount; i++)
	{
		SIZE_T messageLength = 0;
		infoQueue->GetMessage(i, nullptr, &messageLength);
		std::vector<byte> messageBytes(messageLength);
		D3D12_MESSAGE* message = reinterpret_cast<D3D12_MESSAGE*>(messageBytes.data());
		infoQueue->GetMessage(i, message, &messageLength);
		Logger::Log(logStream, std::format("[D3D12 ERROR/WARNING] {}\n", message->pDescription));
	}
	infoQueue->ClearStoredMessages();
}
