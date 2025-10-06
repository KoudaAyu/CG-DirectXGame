#pragma once

#include<wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <ostream>

#include <array>

class DirectXCom
{
public:

	//機能レベルとログの出力用の文字列
	static const D3D_FEATURE_LEVEL featureLevels[];
	static const size_t featureLevelsCount;

	static const char* featureLevelNames[];
	static const size_t featureLevelNamesCount;

	DirectXCom(std::ostream& logStream);
	~DirectXCom();

	void Initialize();

	void DebugLayer();

	void GraphicCreateDXGIFactory();

	void SelectAdapter();

	void CreateDevice();

	void SetupD3D12InfoQueue();

	Microsoft::WRL::ComPtr<ID3D12Device>& GetDevice()
	{
		return device;
	}
	Microsoft::WRL::ComPtr<IDXGIFactory7>& GetDxgiFactory()
	{
		return dxgiFactory;
	}
	Microsoft::WRL::ComPtr<IDXGIAdapter4>& GetUseAdapter()
	{
		return useAdapter;
	}


	HRESULT GetHr() const { return hr; }
	void SetHr(HRESULT value) { hr = value; }


private:
	HRESULT hr;

	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;
	Microsoft::WRL::ComPtr<IDXGIAdapter4>useAdapter = nullptr;

	ID3D12InfoQueue* infoQueue = nullptr;

	std::ostream& logStream;

	

};