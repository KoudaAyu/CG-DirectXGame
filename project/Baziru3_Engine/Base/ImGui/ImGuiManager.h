#pragma once


class WindowAPI;
class DirectXCom;

class ImGuiManager
{
public:
	ImGuiManager() = default;
	~ImGuiManager();

	void Initialize(WindowAPI* windowAPI, DirectXCom* dxCommon);
	void CreateContext();
	void StyleColorsDark();

private:
	WindowAPI* windowAPI = nullptr;
	DirectXCom* dxCommon = nullptr;
};