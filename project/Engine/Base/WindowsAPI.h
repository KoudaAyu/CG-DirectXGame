#pragma once

#include<Windows.h>
#include <cstdint>

#include"imgui.h"
#include"imgui_impl_dx12.h"
#include"imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lParam);

class WindowAPI
{
public:
	void Initialize();
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	void Show();

	void Finalize();

	//メッセージ処理
	bool ProcessMassage();

	HWND GetHwnd() const { return hwnd_; }
	HINSTANCE GetHInstance() const { return wc.hInstance; }

	const int32_t GetClientWidth() const { return kClientWidth; }
	const int32_t GetClientHeight() const { return kClientHeight; }

private:

	//ウィンドウ関係
	WNDCLASS wc{};

	HWND hwnd_ = nullptr;

	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;
};