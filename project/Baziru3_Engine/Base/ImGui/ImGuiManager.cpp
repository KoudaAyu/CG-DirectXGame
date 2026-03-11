#include"ImGuiManager.h"
#include"WindowsAPI.h"
#include"DirectXCom.h"
#include"SRVManager.h"
#include <memory>

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#endif

static std::unique_ptr<SRVManager> g_srvManager;

ImGuiManager::~ImGuiManager()
{
#ifdef USE_IMGUI
   
    g_srvManager.reset();
#endif

}

void ImGuiManager::Initialize([[maybe_unused]]WindowAPI* windowAPI, [[maybe_unused]]DirectXCom* dxCommon)
{
#ifdef USE_IMGUI



	this->windowAPI = windowAPI;
	this->dxCommon = dxCommon;

	// ImGuiのコンテキストを作成
	CreateContext();

	// ImGuiのスタイルを設定
	StyleColorsDark();

	ImGui_ImplWin32_Init(windowAPI->GetHwnd());

	//DirectX12用の初期化情報
	ImGui_ImplDX12_InitInfo initInfo = {};


	if (!g_srvManager)
	{
		g_srvManager = std::make_unique<SRVManager>();
		g_srvManager->Initialize(dxCommon);
	}

	//初期化情報の設定
	initInfo.Device = dxCommon->GetDevice().Get();
	initInfo.CommandQueue = dxCommon->GetCommandQueue().Get();
	initInfo.NumFramesInFlight = dxCommon->GetSwapChainDesc().BufferCount;
	initInfo.RTVFormat = dxCommon->GetRtvDesc().Format;
	initInfo.DSVFormat = dxCommon->GetDsvDesc().Format;
	initInfo.SrvDescriptorHeap = dxCommon->GetSrvDescriptorHeap().Get();

	//SRV確保用関数の設定
	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle,
	D3D12_GPU_DESCRIPTOR_HANDLE* out_put_handle)
		{
			// SRVManager からインデックスを割り当て、CPU/GPU ハンドルを返す
			uint32_t srvIndex = g_srvManager->Allocate();
			*out_cpu_handle = g_srvManager->GetCPUDescriptorHandle(srvIndex);
			*out_put_handle = g_srvManager->GetGPUDescriptorHandle(srvIndex);
		};

	//SRV解放用関数の設定
	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
	D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
		{
			//SRVManagerに解放機能を作っていないため、ここでは何も記載しない。
		};

	ImGui_ImplDX12_Init(&initInfo);

	// フォントテクスチャ等のデバイス側オブジェクトを作成
	ImGui_ImplDX12_CreateDeviceObjects();
	initialized = true;
#endif 

}

void ImGuiManager::Update()
{
#ifdef USE_IMGUI
	if(!initialized) return;
	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX12_NewFrame();
	ImGui::NewFrame();
#endif
}

void ImGuiManager::CreateContext()
{
#ifdef USE_IMGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	if (io.Fonts->Fonts.empty())
	{
		io.Fonts->AddFontDefault();
	}
	
#endif
}

void ImGuiManager::StyleColorsDark()
{
#ifdef USE_IMGUI
	ImGui::StyleColorsDark();
#endif
}
