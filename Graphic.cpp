#include "Graphic.h"

void Graphic::GraphicCreateDXGIFactory()
{
	//HRESULTはWindoes系のエラーコード
	//関数が成功したか同課をSUCCEEDEDマクロで判断する
	hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	assert(SUCCEEDED(hr));
}

void Graphic::SelectAdapter()
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
			Debug::Log(debug.GetStream(), std::format("Using adapter: {}\n", StringUtil::ConvertString(adapterDesc.Description)));
			break;
		}
		useAdapter = nullptr; //ソフトウェアアダプタの場合は見なかったことにするためしないのでnullptr
	}

	//アダプターが見つからなかった場合はエラー
	assert(useAdapter != nullptr);

}

void Graphic::SelectDevice(Microsoft::WRL::ComPtr<ID3D12Device>& device)
{
	//機能レベルを順に試していく
	for (size_t i = 0; i < Graphic::featureLevelsCount; ++i)
	{
		//採用したアダプタでデバイスを作成
		hr = D3D12CreateDevice(
			useAdapter.Get(), //アダプタ
			Graphic::featureLevels[i], //機能レベル
			IID_PPV_ARGS(&device) //デバイスのポインタ
		);

		//指定した機能レベルでデバイスが生成できたか確認
		if (SUCCEEDED(hr))
		{
			//生成出来たのでログ出力を行う
			Debug::Log(debug.GetStream(), std::format("Feature Level: {}\n", Graphic::featureLevelNames[i]));
			break; //ループを抜ける
		}


	}

	//デバイスの生成に失敗し起動できない
	assert(device != nullptr);
	Debug::Log(debug.GetStream(), std::format("Complete create D3D12Device!"));//初期起動完了のLogを出す
}

void Graphic::CreateCommandQueue(Microsoft::WRL::ComPtr<ID3D12Device>& device)
{
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
	//コマンドキューの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));
}

void Graphic::CreateCommandAllocator(Microsoft::WRL::ComPtr<ID3D12Device>& device)
{
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));

	//コマンドアロケーターの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));
}

void Graphic::CreateCommandList(Microsoft::WRL::ComPtr<ID3D12Device>& device)
{
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));

	//コマンドリストの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));
}

void Graphic::CreateSwapChain(Window window)
{


	swapChainDesc.Width = window.GetClientWidth(); //ウィンドウの幅
	swapChainDesc.Height = window.GetClientHeight(); //ウィンドウの高さ
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //色の形式
	swapChainDesc.SampleDesc.Count = 1; //マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; //レンダリングターゲットとして使用
	swapChainDesc.BufferCount = 2; //ダブルバッファリング
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; //モニターに映ったら描画を破棄

	//コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), window.GetHwnd(), &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));
	
	assert(window.GetHwnd() != nullptr);
	//スワップチェーンの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));
}

void Graphic::CreateDescriptorHeaps(Microsoft::WRL::ComPtr<ID3D12Device>& device)
{
	//RTV用のヒープでディスクリプタの数は2。RTVはShader内でふれるものではないため、ShaderVisibleはfalse
	rtvDescriptorHeap = DescriptorHeap::CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	//SRV用のヒープでディスクリプタの数は128。SRTはShader内で触れるものなので、ShaderVisibleはtrue
	srvDescriptorHeap = DescriptorHeap::CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	//DSV用のヒープでディスクリプタの数は1。DSVはShader内で触れるものではないため、ShaderVisibleはfalse
	dsvDescriptorHeap = DescriptorHeap::CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
}

void Graphic::GetSwapChainResources()
{
	
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	//うまく取得できなければエラー
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	//うまく取得できなければエラー
	assert(SUCCEEDED(hr));
}

void Graphic::CreateRenderTargetViews(Microsoft::WRL::ComPtr<ID3D12Device>& device)
{
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; //出力結果をSRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; //2Dテクスチャとして書き込む
	rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	
	//まず1つ目を作る。1つ目は最初のところ。こちらで場所指定する必要あり
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc, rtvHandles[0]);
	//2つ目は1つ目の後ろに作る
	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	//2つ目を作る
	device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc, rtvHandles[1]);
}

const D3D_FEATURE_LEVEL Graphic::featureLevels[] = {
	D3D_FEATURE_LEVEL_12_2,
	D3D_FEATURE_LEVEL_12_1,
	D3D_FEATURE_LEVEL_12_0,
};

const size_t Graphic::featureLevelsCount = sizeof(Graphic::featureLevels) / sizeof(Graphic::featureLevels[0]);



const char* Graphic::featureLevelNames[] = {
	"12.2",
	"12.1",
	"12.0",
};

const size_t Graphic::featureLevelNamesCount = sizeof(Graphic::featureLevelNames) / sizeof(Graphic::featureLevelNames[0]);