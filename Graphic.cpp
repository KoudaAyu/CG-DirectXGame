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