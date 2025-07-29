#pragma once
#include<cassert>
#include <dxgi1_6.h>
#include<wrl.h>

#include"DebugLog.h"
#include"StringUtil.h"

class Graphic
{
public:
	void GraphicCreateDXGIFactory();
	void SelectAdapter();
	void SelectDevice(Microsoft::WRL::ComPtr<ID3D12Device>& device);
	void CreateCommandQueue(Microsoft::WRL::ComPtr<ID3D12Device>& device);

	static const D3D_FEATURE_LEVEL featureLevels[];
	static const size_t featureLevelsCount;

	static const char* featureLevelNames[];
	static const size_t featureLevelNamesCount;

	HRESULT GetHRESULT() const { return hr; }
	Microsoft::WRL::ComPtr<IDXGIFactory7> GetDXGIFactory() const
	{
		return dxgiFactory;
	}
	Microsoft::WRL::ComPtr<IDXGIAdapter4> GetUseAdapter() const
	{
		return useAdapter;
	}
	const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& GetCommandQueue() const
	{
		return commandQueue;
	}

private:
	HRESULT hr;
	Debug debug;

	//DXGIファクトリーの生成
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;

	//使用するアダプタ用の変数。最初にnullptrを入れる
	Microsoft::WRL::ComPtr<IDXGIAdapter4>useAdapter = nullptr;

	//コマンドキューの生成
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
};
