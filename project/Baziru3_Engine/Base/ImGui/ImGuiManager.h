#pragma once

#include <memory>

class WindowAPI;
class DirectXCom;
class SRVManager; // forward declare

class ImGuiManager
{
public:
	ImGuiManager() = default;
	~ImGuiManager();

	void Initialize(WindowAPI* windowAPI, DirectXCom* dxCommon);
	void Update();
	void CreateContext();
	void StyleColorsDark();
	void Finalize();

	bool IsInitialized() const { return initialized; }

private:
	WindowAPI* windowAPI = nullptr;
	DirectXCom* dxCommon = nullptr;
	bool initialized = false;

	// Own an SRVManager only if we create one here. If using TextureManager's SRVManager this remains null.
	std::unique_ptr<SRVManager> srvManagerOwned_ = nullptr;
	// Pointer to the SRVManager in use (either owned or external)
	SRVManager* srvManager_ = nullptr;

	// Static pointer used by the C-style callback required by ImGui_ImplDX12.
	// Stored as a static so a non-capturing function/lambda can access it.
	static SRVManager* s_srvManagerForCallback;
};