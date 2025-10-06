#pragma once

#include<wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <ostream>

#include <array>

#include"WindowsAPI.h"

class DirectXCom
{
public:

	//機能レベルとログの出力用の文字列
	static const D3D_FEATURE_LEVEL featureLevels[];
	static const size_t featureLevelsCount;

	static const char* featureLevelNames[];
	static const size_t featureLevelNamesCount;

	DirectXCom(WindowAPI* windowAPI, std::ostream& logStream);
	~DirectXCom();

	void Initialize();

	void DebugLayer();

	void GraphicCreateDXGIFactory();

	void SelectAdapter();

	void CreateDevice();

	void SetupD3D12InfoQueue();

	void InitializeCommandList();

	void CreateCommandAllocator();

	void CreateCommandList();

	void CreateCommandQueue();

	void CreateSwapChain();

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(
		const Microsoft::WRL::ComPtr<ID3D12Device>& device,
		int32_t width,
		int32_t height);

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
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>& GetCommandAllocator()
	{
		return commandAllocator;
	}
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& GetCommandList()
	{
		return commandList;
	}

	Microsoft::WRL::ComPtr<ID3D12CommandQueue>& GetCommandQueue()
	{
		return commandQueue;
	}
	Microsoft::WRL::ComPtr<IDXGISwapChain4> GetSwapChain()
	{
		return swapChain;
	}
	DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc()
	{
		return swapChainDesc;
	}


	HRESULT GetHr() const { return hr; }
	void SetHr(HRESULT value) { hr = value; }


private:
	HRESULT hr;

	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;
	Microsoft::WRL::ComPtr<IDXGIAdapter4>useAdapter = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	ID3D12InfoQueue* infoQueue = nullptr;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

	std::ostream& logStream;

	WindowAPI* windowAPI = nullptr;

};