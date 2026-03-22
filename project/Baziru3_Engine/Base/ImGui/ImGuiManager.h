#pragma once


class WindowAPI;
class DirectXCom;

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
};