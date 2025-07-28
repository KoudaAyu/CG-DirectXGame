//#pragma once
//#include<cassert>
//#include <d3d12.h>	
//#include <dxgi1_6.h>
//#include<wrl.h>
//
//#include"DescriptorHeap.h"
//#include"Window.h"
//class SwapChain
//{
//public:
//	void Initialize(Microsoft::WRL::ComPtr<ID3D12Device>& device,
//		IDXGIFactory7* dxgiFactory,
//		ID3D12CommandQueue* commandQueue,
//		HWND hwnd);
//	void Present(UINT syncInterval = 1, UINT flags = 0)
//	{
//		assert(swapChain != nullptr);
//		swapChain->Present(syncInterval, flags);
//	}
//	Microsoft::WRL::ComPtr<IDXGISwapChain4>& GetSwapChain() { return swapChain; }
//	DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc() { return swapChainDesc; }
//	Microsoft::WRL::ComPtr<ID3D12Resource>& GetSwapChainResource(size_t index)
//	{
//		assert(index < 2); // 範囲チェック
//		return swapChainResources[index];
//	}
//	D3D12_RENDER_TARGET_VIEW_DESC& GetRtvDesc() { return rtvDesc; }
//	D3D12_CPU_DESCRIPTOR_HANDLE& GetRtvHandle(int index)
//	{
//		assert(index >= 0 && index < 2);
//		return rtvHandles[index];
//	}
//	
//
//	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDsvDescriptorHeap() { return dsvDescriptorHeap; }
//	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& GetSrvDescriptorHeap() { return srvDescriptorHeap; }
//private:
//
//	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;
//	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
//	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2];
//	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
//	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
//
//
//	//RTV用のヒープでディスクリプタの数は2。RTVはShader内でふれるものではないため、ShaderVisibleはfalse
//	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
//	//SRV用のヒープでディスクリプタの数は128。SRTはShader内で触れるものなので、ShaderVisibleはtrue
//	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap;
//
//	//DSV用のヒープでディスクリプタの数は1。DSVはShader内で触れるものではないため、ShaderVisibleはfalse
//	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;
//
//};
