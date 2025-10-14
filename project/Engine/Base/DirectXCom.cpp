#include "DirectXCom.h"

#include<cassert>
#include <format>

#include"d3dx12.h"
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib,"dxgi.lib")

#include"Log.h"
#include"StringUtil.h"

using namespace Microsoft::WRL;

DirectXCom::DirectXCom(WindowAPI* windowAPI, std::ostream& logStream)
	: windowAPI(windowAPI), logStream(logStream)
{
}

DirectXCom::~DirectXCom()
{
}

void DirectXCom::Initialize()
{
	assert(windowAPI);
	this->windowAPI = windowAPI;

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
}

void DirectXCom::DebugLayer()
{
#ifdef _DEBUG


	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		//デバックレイヤーを有効化する
		debugController->EnableDebugLayer();
		//更にGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
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
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);



		//抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] =
		{
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		D3D12_MESSAGE_SEVERITY serverities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds); //抑制するメッセージの数
		filter.DenyList.pIDList = denyIds; //抑制するメッセージのID
		filter.DenyList.NumSeverities = _countof(serverities); //抑制するメッセージの重要度の数
		filter.DenyList.pSeverityList = serverities; //抑制するメッセージの重要度

		infoQueue->PushStorageFilter(&filter); //フィルターを適用する

		//解放
		infoQueue->Release();
	}
#endif
}

void DirectXCom::InitializeCommandList()
{
	CreateCommandAllocator();
	CreateCommandList();
	CreateCommandQueue();
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


	//RTV用のヒープでディスクリプタの数は2。RTVはShader内でふれるものではないため、ShaderVisibleはfalse
	rtvDescriptorHeap = CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	//SRV用のヒープでディスクリプタの数は128。SRTはShader内で触れるものなので、ShaderVisibleはtrue
	srvDescriptorHeap = CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, true);
	//DSV用のヒープでディスクリプタの数は1。DSVはShader内で触れるものではないため、ShaderVisibleはfalse
	dsvDescriptorHeap = CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

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
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();


	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (int i = 0; i < 2; ++i)
	{
		rtvHandles[i].ptr = rtvStartHandle.ptr + descriptorSize * i;
		device->CreateRenderTargetView(swapChainResources[i].Get(), &rtvDesc, rtvHandles[i]);
	}

}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCom::GetCPUDescroptirHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCom::GetGPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index)
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
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
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

	//dxcCompilerを初期化
	hr = (DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils)));
	assert(SUCCEEDED(hr));
	hr = (DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler)));
	assert(SUCCEEDED(hr));

	//現時点ではincludeしないが、includeに対応する為の設定を行う
	hr = (dxcUtils->CreateDefaultIncludeHandler(&includeHandler));
	assert(SUCCEEDED(hr));
}

void DirectXCom::InitializeImGui()
{
	//Imguiの初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(windowAPI->GetHwnd());
	ImGui_ImplDX12_Init(
		device.Get(),
		swapChainDesc.BufferCount,
		rtvDesc.Format,
		srvDescriptorHeap.Get(),
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
}

void DirectXCom::PreDraw()
{
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
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);

	float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };//RGBAの値。青っぽい色
	commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	//描画用のDescriptorHeapの設定
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap[] = { srvDescriptorHeap.Get() };
	commandList->SetDescriptorHeaps(1, descriptorHeap->GetAddressOf());

	//コマンドを積む
	commandList->RSSetViewports(1, &viewport); //ビューポートを設定
	commandList->RSSetScissorRects(1, &scissorRect); //シザー矩形を設定



}

void DirectXCom::PostDraw()
{
	backBufferIndex = swapChain->GetCurrentBackBufferIndex();
	//画面に描く処理は終わり画面に映すので、状態を遷移
		//RenderTargetからPresentにする
	barrier.Transition.StateBefore = (D3D12_RESOURCE_STATE_RENDER_TARGET);
	barrier.Transition.StateAfter = (D3D12_RESOURCE_STATE_PRESENT);
	//TransitionBarrierを張る
	commandList->ResourceBarrier(1, &barrier);

	//コマンドリストの内容を下記率させる。すべてのコマンドを積んでからCloseする
	hr = (commandList->Close());
	//コマンドリストのCloseに失敗した場合はエラー
	assert(SUCCEEDED(hr));

	//GUPにコマンドリストの実行を行わせる
	ID3D12CommandList* commandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(1, commandLists);
	//GUPとOSに画面の交換を要求する
	swapChain->Present(1, 0);

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

	LPCWSTR arguments[] = {
		filePath.c_str(), //コンパイルするファイルのパス
		L"-E", L"main", //エントリーポイントの指定。基本的にmain以外にはしない
		L"-T", profile, //ShaderProfileの設定
		L"-Zi",L"Qembed_debug",
		L"-Od", //最適化を行わない
		L"-Zpr", //メモリレイアウトは行優先
	};

	//実際にShaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcResult> shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer, //コンパイルするシェーダーの内容
		arguments, //コンパイル時の引数
		_countof(arguments), //引数の数
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
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(device, intermediateSize);
	UpdateSubresources(commandList.Get(), texture.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());
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
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_DEFAULT_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	//ミニマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
		DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	//ミニマップ付きのデータを返す
	return mipImages;
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
