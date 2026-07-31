#include"ImGuiManager.h"
#include"WindowsAPI.h"
#include"DirectXCom.h"
#include"SRVManager.h"
#include"../../2D/Texture/TextureManager.h"
#include <memory>
#include <sstream>
#include <Windows.h>

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#endif

// Static pointer used by the ImGui SRV allocation callbacks
SRVManager* ImGuiManager::s_srvManagerForCallback = nullptr;

ImGuiManager::~ImGuiManager()
{
    // Ensure finalization if not already done
#ifdef USE_IMGUI
    if (initialized)
    {
        Finalize();
    }
    // Only destroy the owned manager; if we're using TextureManager's SRVManager do not free it
    srvManagerOwned_.reset();
    // Clear the static callback pointer
    s_srvManagerForCallback = nullptr;
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
            srvManager_ = texSrvMgr;
            s_srvManagerForCallback = srvManager_;
            // log selection
            {
                std::ostringstream oss;
                oss << "ImGuiManager::Initialize - using TextureManager's SRVManager (this=0x" << std::hex << (unsigned long long)(uintptr_t)s_srvManagerForCallback << ")\n";
                OutputDebugStringA(oss.str().c_str());
            }
        }
        else
        {
          
            std::ostringstream oss;
            oss << "ImGuiManager::Initialize - TextureManager present but has no SRVManager; ImGuiManager will create its own SRVManager\n";
            OutputDebugStringA(oss.str().c_str());
        }
    }

    if (!srvManager_)
    {
        // Create and initialize our own SRVManager
        srvManagerOwned_ = std::make_unique<SRVManager>();
        srvManagerOwned_->Initialize(dxCommon);
        srvManager_ = srvManagerOwned_.get();
        s_srvManagerForCallback = srvManager_;
        // log creation
        {
            std::ostringstream oss;
            oss << "ImGuiManager::Initialize - created owned SRVManager (this=0x" << std::hex << (unsigned long long)(uintptr_t)s_srvManagerForCallback << ")\n";
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
            if (!ImGuiManager::s_srvManagerForCallback) {
                OutputDebugStringA("ImGuiManager::SrvDescriptorAllocFn - ERROR: SRVManager not set\n");
                out_cpu_handle->ptr = 0;
                out_put_handle->ptr = 0;
                return;
            }
            uint32_t srvIndex = ImGuiManager::s_srvManagerForCallback->Allocate();
            if (srvIndex == UINT32_MAX) {
                OutputDebugStringA("ImGuiManager::SrvDescriptorAllocFn - ERROR: Allocate returned UINT32_MAX\n");
                out_cpu_handle->ptr = 0;
                out_put_handle->ptr = 0;
                return;
            }
            *out_cpu_handle = ImGuiManager::s_srvManagerForCallback->GetCPUDescriptorHandle(srvIndex);
            *out_put_handle = ImGuiManager::s_srvManagerForCallback->GetGPUDescriptorHandle(srvIndex);
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

    // Scale mouse position events in the queue from actual window size to virtual 1280x720 size before ImGui::NewFrame processes them
    ImGuiIO& io = ImGui::GetIO();
    if (windowAPI)
    {
        HWND hwnd = windowAPI->GetHwnd();
        RECT rect;
        if (GetClientRect(hwnd, &rect))
        {
            float width = static_cast<float>(rect.right - rect.left);
            float height = static_cast<float>(rect.bottom - rect.top);
            if (width > 0.0f && height > 0.0f)
            {
                float targetWidth = static_cast<float>(WindowAPI::GetClientWidth());
                float targetHeight = static_cast<float>(WindowAPI::GetClientHeight());
                
                ImGuiContext& g = *GImGui;
                for (int n = 0; n < g.InputEventsQueue.Size; n++)
                {
                    ImGuiInputEvent& e = g.InputEventsQueue[n];
                    if (e.Type == ImGuiInputEventType_MousePos)
                    {
                        e.MousePos.PosX = (e.MousePos.PosX / width) * targetWidth;
                        e.MousePos.PosY = (e.MousePos.PosY / height) * targetHeight;
                    }
                }
            }
        }
    }

    // Set display size before NewFrame
    io.DisplaySize = ImVec2(static_cast<float>(WindowAPI::GetClientWidth()), static_cast<float>(WindowAPI::GetClientHeight()));

    ImGui::NewFrame();
#endif
}

void ImGuiManager::Render()
{
#ifdef USE_IMGUI
    if (!initialized) return;
    ImGui::Render();
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

    // Clear local and static pointers but do not destroy external SRVManager
    srvManager_ = nullptr;
    srvManagerOwned_.reset();
    s_srvManagerForCallback = nullptr;
#endif
}
