#pragma once
#include<cassert>
#include <dxgi1_6.h>
#include<wrl.h>

#include"BlendManager.h"
#include"DebugLog.h"
#include"DescriptorHeap.h"
#include"InputLayoutManage.h"
#include"RasterizerManager.h"
#include"RootSignatureManager.h"
#include"ShaderCompile.h"
#include"StringUtil.h"
#include"Window.h"

class Graphic
{
public:
	void GraphicCreateDXGIFactory();
	void SelectAdapter();
	void SelectDevice(Microsoft::WRL::ComPtr<ID3D12Device>& device);
	void CreateCommandQueue(Microsoft::WRL::ComPtr<ID3D12Device>& device);
	void CreateCommandAllocator(Microsoft::WRL::ComPtr<ID3D12Device>& device);
	void CreateCommandList(Microsoft::WRL::ComPtr<ID3D12Device>& device);
	void CreateSwapChain(Window window);
	void CreateDescriptorHeaps(Microsoft::WRL::ComPtr<ID3D12Device>& device);
	void GetSwapChainResources();
	void CreateRenderTargetViews(Microsoft::WRL::ComPtr<ID3D12Device>& device);
	void CreateGraphicPipelineStateDesc(const Microsoft::WRL::ComPtr<IDxcBlob>& vertexShaderBlob,
		const Microsoft::WRL::ComPtr<IDxcBlob>& pixelShaderBlob, D3D12_INPUT_LAYOUT_DESC inputLayoutDesc,
		D3D12_BLEND_DESC blendDesc, D3D12_RASTERIZER_DESC rasterizerDesc);

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
	const Microsoft::WRL::ComPtr<ID3D12CommandAllocator>& GetCommandAllocator() const
	{
		return  commandAllocator;
	}
	const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& GetCommandList() const
	{
		return  commandList;
	}

	const Microsoft::WRL::ComPtr<IDXGISwapChain4> GetSwapChain() const
	{
		return swapChain;
	}

	
	const DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc() const { return swapChainDesc; }

	const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetRtvDescriptorHeap() const
	{
		return rtvDescriptorHeap;
	}

	const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetSrvDescriptorHeap() const
	{
		return srvDescriptorHeap;
	}

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& GetSrvDescriptorHeap()
	{
		return srvDescriptorHeap;
	}

	const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDsvDescriptorHeap() const
	{
		return dsvDescriptorHeap;
	}

	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetSwapChainResource(int index) const
	{
		return swapChainResources[index];
	}

	const D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc() const { return rtvDesc; }

	// RTV開始ハンドルを取得（const）
	const D3D12_CPU_DESCRIPTOR_HANDLE GetRtvStartHandle() const { return rtvStartHandle; }

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetRtvHandles(int index) const
	{
		return rtvHandles[index];
	}

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC& GetGraphicPipelineStateDesc() const
	{
		return graphicPipelineStateDesc;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC& GetGraphicPipelineStateDesc()
	{
		return graphicPipelineStateDesc;
	}

private:
	BlendManager blendManager;
	Debug debug;
	HRESULT hr;
	
	RasterizerManager rasterizerManager;
	RootSignatureManager rootSignatureManager;
	

	Window window;

	//DXGIファクトリーの生成
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;

	//使用するアダプタ用の変数。最初にnullptrを入れる
	Microsoft::WRL::ComPtr<IDXGIAdapter4>useAdapter = nullptr;

	//コマンドキューの生成
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;

	//コマンドアロケーターを生成する
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;

	//コマンドリストの生成
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;

	//スワップチェーンを生成する
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain = nullptr;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap = nullptr;

	//SwapChainからResorrceを取得する
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2] = { nullptr, nullptr };

	//RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};

	//ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle;

	//RTVを2つ作るのでディスクリプタを2つ用意する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicPipelineStateDesc{};
};
