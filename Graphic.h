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

	HRESULT GetHRESULT() const { return hr; }
	Microsoft::WRL::ComPtr<IDXGIFactory7> GetDXGIFactory() const
	{
		return dxgiFactory;
	}
	Microsoft::WRL::ComPtr<IDXGIAdapter4> GetUseAdapter() const
	{
		return useAdapter;
	}

private:
	HRESULT hr;
	Debug debug;

	//DXGIファクトリーの生成
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;

	//使用するアダプタ用の変数。最初にnullptrを入れる
	Microsoft::WRL::ComPtr<IDXGIAdapter4>useAdapter = nullptr;
};
