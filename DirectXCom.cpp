#include "DirectXCom.h"

#include<cassert>
#include <format>

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

#include"Log.h"
#include"StringUtil.h"

using namespace Microsoft::WRL;

DirectXCom::DirectXCom(std::ostream& logStream)
	: logStream(logStream)
{
}

DirectXCom::~DirectXCom()
{
}

void DirectXCom::Initialize()
{
	GraphicCreateDXGIFactory();
	SelectAdapter();
	CreateDevice();
	SetupD3D12InfoQueue();
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
