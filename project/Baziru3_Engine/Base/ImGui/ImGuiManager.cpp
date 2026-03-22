#include"ImGuiManager.h"
#include"WindowsAPI.h"
#include"DirectXCom.h"
#include"SRVManager.h"
#include"../../2D/Texture/TextureManager.h"
#include <memory>

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#endif

// g_srvManagerOwned owns an SRVManager only if ImGui creates one itself.
static std::unique_ptr<SRVManager> g_srvManagerOwned;
// g_srvManager points to the SRVManager to use (either owned or from TextureManager)
static SRVManager* g_srvManager = nullptr;

ImGuiManager::~ImGuiManager()
{
    // Ensure finalization if not already done
#ifdef USE_IMGUI
    if (initialized)
    {
        Finalize();
    }
    // Only destroy owned manager; if we're using TextureManager's SRVManager do not free it
    g_srvManagerOwned.reset();
    g_srvManager = nullptr;
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

	// Prefer using TextureManager's SRVManager if available to avoid duplicate SRVManagers
	if (TextureManager::GetInstance())
	{
		SRVManager* texSrvMgr = TextureManager::GetInstance()->GetSRVManager();
		if (texSrvMgr)
		{
			g_srvManager = texSrvMgr;
			// log selection
			{
				std::ostringstream oss;
				oss << "ImGuiManager::Initialize - using TextureManager's SRVManager (this=0x" << std::hex << (unsigned long long)(uintptr_t)g_srvManager << ")\n";
				OutputDebugStringA(oss.str().c_str());
			}
		}
		else
		{
			// If TextureManager exists but has not yet created its SRVManager, request it to initialize one
			TextureManager::GetInstance()->SetDirectXCom(dxCommon);
			texSrvMgr = TextureManager::GetInstance()->GetSRVManager();
			if (texSrvMgr)
			{
				g_srvManager = texSrvMgr;
				std::ostringstream oss;
				oss << "ImGuiManager::Initialize - forced TextureManager to create SRVManager (this=0x" << std::hex << (unsigned long long)(uintptr_t)g_srvManager << ")\n";
				OutputDebugStringA(oss.str().c_str());
			}
		}
	}

	if (!g_srvManager)
	{
		// Create and initialize our own SRVManager
		g_srvManagerOwned = std::make_unique<SRVManager>();
		g_srvManagerOwned->Initialize(dxCommon);
		g_srvManager = g_srvManagerOwned.get();
		// log creation
		{
			std::ostringstream oss;
			oss << "ImGuiManager::Initialize - created owned SRVManager (this=0x" << std::hex << (unsigned long long)(uintptr_t)g_srvManager << ")\n";
			OutputDebugStringA(oss.str().c_str());
		}
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
			//SRVManager::Free is available but requires index; ImGui does not provide index here.
			//We don't free here. If necessary, implement handle->index mapping in SRVManager.
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

void ImGuiManager::Finalize()
{
#ifdef USE_IMGUI
	if (!initialized) return;
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	initialized = false;
#endif
}
