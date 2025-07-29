#pragma once
#include <windows.h>
#include <cstdint>

#include"externals/imgui/imgui.h"
#include"externals/imgui/imgui_impl_dx12.h"
#include"externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lParam);


class Window
{
public:

	void Initialize();

	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	void Show();

	HWND GetHwnd() const { return hwnd_; }

	int32_t GetClientWidth() const { return kClientWidth; }
	int32_t GetClientHeight() const { return kClientHeight; }
private:
	HWND hwnd_ = nullptr;

	//クライアント領域のサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;
};