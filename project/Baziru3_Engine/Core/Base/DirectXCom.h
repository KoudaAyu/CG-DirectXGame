#pragma once

#include<wrl.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <ostream>

#include <array>
#include <memory>
#include <chrono>
#include "DirectXTex.h"

class ConstantBufferAllocator;
class StackAllocator;


#include"WindowsAPI.h"
#include "Baziru3_Engine/Core/Base/Srv/DescriptorHeap.h"

/**
 * @brief DirectX12の共通処理を管理するクラス
 */
class DirectXCom
{
public:

	//機能レベルとログの出力用の文字列
	static const D3D_FEATURE_LEVEL featureLevels[];
	static const size_t featureLevelsCount;

	static const char* featureLevelNames[];
	static const size_t featureLevelNamesCount;

	// 公開SRV最大数定数
	static constexpr uint32_t kMaxSRVCount = 8192;
	static constexpr uint32_t GetMaxSRVCount() { return kMaxSRVCount; }

	DirectXCom(WindowAPI* windowAPI, std::ostream& logStream);
	~DirectXCom();

	void InitializeFixFPS();

	void UpdateFixFPS();

	/**
	 * @brief DirectX12のシステム全体を初期化します
	 */
	void Initialize();

	/**
	 * @brief DirectX12のシステムを終了し、確保した全リソースを解放します
	 */
	void Finalize();

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

	void CreateUnroaderedAccessView(const Microsoft::WRL::ComPtr<ID3D12Resource>& resource,
		UINT NumElements, UINT structureByteStride, D3D12_CPU_DESCRIPTOR_HANDLE uavCpuDescriptorHandle);

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>  CreateDescriptorHeap(
		const Microsoft::WRL::ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);


	void CreateDescriptorHeaps();

	void InitializeRenderTargetView();

    // 引数のディスクリプタヒープは変更しないため const 参照を受け取る
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
		uint32_t descriptorSize, uint32_t index);

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
		uint32_t descriptorSize, uint32_t index);

	void InitializeDepthStencilView();

	void CreateFence();

	void CreateViewportRect();

	void CerateScissorRect();

	void CreateDxcCompiler();

	void InitializeImGui();

	/**
	 * @brief 描画前処理を行い、レンダーターゲットをクリアして書き込み可能状態にします
	 */
	void PreDraw();

	/**
	 * @brief 描画後処理を行い、コマンドリストをクローズ・実行して画面をフリップします
	 */
	void PostDraw();

	void ExecuteAndWaitForGPU();

  

	/**
	 * @brief HLSLシェーダーファイルをコンパイルしてバイナリデータを取得します
	 * @param filePath シェーダーファイルへのパス
	 * @param profile シェーダープロファイル (例: L"vs_6_0")
	 * @param dxcUtils DXCユーティリティ
	 * @param dxcCompiler DXCコンパイラ
	 * @param includeHandler インクルードハンドラ
	 * @param logStream ログ出力用ストリーム
	 * @return コンパイルされたシェーダーバイナリ
	 */
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
		const std::wstring& filePath,
		const wchar_t* profile,
		Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils,
		Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler,
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler,
		std::ostream& logStream);

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device,
		size_t sizeInBytes);

	[[nodiscard]]
	Microsoft::WRL::ComPtr<ID3D12Resource>UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages, const Microsoft::WRL::ComPtr<ID3D12Device>& device,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList);

	DirectX::ScratchImage LoadTexture(const std::string& filePath);

	Microsoft::WRL::ComPtr<ID3D12Resource>CreateTextureResource(const DirectX::TexMetadata& metadata);

public:
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
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>& GetWorkerCommandAllocator()
	{
		return workerCommandAllocator_;
	}
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& GetWorkerCommandList()
	{
		return workerCommandList_;
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
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetRtvDescriptorHeap() const
	{
		return rtvDescriptorHeap_.GetHeap();
	}
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetSrvDescriptorHeap() const
	{
		return srvDescriptorHeap_.GetHeap();
	}
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDsvDescriptorHeap() const
	{
		return dsvDescriptorHeap_.GetHeap();
	}
	DescriptorHeap& GetSrvHeap() { return srvDescriptorHeap_; }
	DescriptorHeap& GetRtvHeap() { return rtvDescriptorHeap_; }
	DescriptorHeap& GetDsvHeap() { return dsvDescriptorHeap_; }
	uint32_t GetDescriptorSizeSRV() const
	{
		return device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	uint32_t GetDescriptorSizeRTV() const
	{
		return device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}
	uint32_t GetDescriptorSizeDSV() const
	{
		return device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	}
	const D3D12_RENDER_TARGET_VIEW_DESC& GetRtvDesc() const
	{
		return rtvDesc;
	}
	const D3D12_DEPTH_STENCIL_VIEW_DESC& GetDsvDesc() const
	{
		return dsvDesc;
	}
	const Microsoft::WRL::ComPtr<ID3D12Resource>* GetSwapChainResources() const
	{
		return swapChainResources;
	}
	const D3D12_CPU_DESCRIPTOR_HANDLE* GetRtvHandles() const
	{
		return rtvHandles;
	}
	uint64_t GetFenceValue() const { return fenceValue; }
	void SetFenceValue(uint64_t value) { fenceValue = value; }
	const Microsoft::WRL::ComPtr<ID3D12Fence>& GetFence() const
	{
		return fence;
	}
	HANDLE GetFenceEvent() const { return fenceEvent; }
	const D3D12_VIEWPORT& GetViewport() const { return viewport; }
	const D3D12_RECT& GetScissorRect() const { return scissorRect; }
	const Microsoft::WRL::ComPtr<IDxcUtils>& GetDxcUtils() const 
	{ 
		return dxcUtils; 
	}
	const Microsoft::WRL::ComPtr<IDxcCompiler3>& GetDxcCompiler() const {
		return dxcCompiler; 
	}
	const Microsoft::WRL::ComPtr<IDxcIncludeHandler>& GetIncludeHandler() const {
		return includeHandler; 
	}
	const D3D12_RESOURCE_BARRIER& GetBarrier() const { 
		return barrier; 
	}
	void SetBarrier(const D3D12_RESOURCE_BARRIER& value) { barrier = value; }
	void SetBarrierStateBefore(D3D12_RESOURCE_STATES state) { barrier.Transition.StateBefore = state; }
	void SetBarrierStateAfter(D3D12_RESOURCE_STATES state) { barrier.Transition.StateAfter = state; }

	HRESULT GetHr() const { return hr; }
	void SetHr(HRESULT value) { hr = value; }

	MSG& GetMsg() { return msg; }


	static const uint32_t GetKMaXSRVCount()
	{
		return kMacSRVCount;
	}

	size_t GetSwapChainResourcesNum() const
	{
		return std::size(swapChainResources);
	}

	// 新規追加: SRV用CPU/GPUディスクリプタハンドル取得
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandleCPU(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandleGPU(uint32_t index);

	


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
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];//RTVを2つ作るのでディスクリプタを2つ用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2] = { nullptr, nullptr };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>  descriptorHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource = nullptr;
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	MSG msg{};
	Microsoft::WRL::ComPtr<ID3D12Fence> fence = nullptr;
	uint64_t fenceValue = 0;//初期値0でFenceを作る
	HANDLE fenceEvent;
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils = nullptr;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler = nullptr;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler>includeHandler = nullptr;
	D3D12_RESOURCE_BARRIER barrier{};
	UINT backBufferIndex;

	// ワーカー用（並列コマンド記録用）
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> workerCommandAllocator_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> workerCommandList_ = nullptr;

	//DescriptorSizeを取得しておく
	
	DescriptorHeap rtvDescriptorHeap_;
	DescriptorHeap srvDescriptorHeap_;
	DescriptorHeap dsvDescriptorHeap_;


	uint32_t descriptorSize_;

	std::ostream& logStream;

	WindowAPI* windowAPI = nullptr;
	std::unique_ptr<ConstantBufferAllocator> cbAllocator_ = nullptr;
	std::unique_ptr<StackAllocator> stackAllocator_ = nullptr;

public:
    // Provide access to WindowAPI for callers that need window information
    WindowAPI* GetWindowAPI() const { return windowAPI; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(UINT index) const { return rtvHandles[index]; }
	void PrintDebugMessages();
	ConstantBufferAllocator* GetCBAllocator() const { return cbAllocator_.get(); }
	StackAllocator* GetStackAllocator() const { return stackAllocator_.get(); }


	// 最大SRV数(Texture枚数)
	static const uint32_t kMacSRVCount = 8192;

	// 高精度 60FPS フレームリミッター用
	std::chrono::steady_clock::time_point refrence_;
	float targetFps_ = 60.0f;
	bool enableFpsLimit_ = true;

public:
	void SetTargetFPS(float fps) { targetFps_ = (fps > 1.0f) ? fps : 60.0f; }
	float GetTargetFPS() const { return targetFps_; }
	void SetEnableFPSLimit(bool enable) { enableFpsLimit_ = enable; }
	bool IsEnableFPSLimit() const { return enableFpsLimit_; }
};