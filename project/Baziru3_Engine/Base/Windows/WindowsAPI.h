#pragma once

#include<Windows.h>
#include <cstdint>


class WindowAPI
{
public:
	void Initialize();
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	void Show();

	void Finalize();

	//メッセージ処理
	bool ProcessMessage();

	HWND GetHwnd() const { return hwnd_; }
	HINSTANCE GetHInstance() const { return wc.hInstance; }

	// クライアントサイズ取得を静的にして、オブジェクト不要で参照できるようにする
	static int32_t GetClientWidth() { return kClientWidth; }
	static int32_t GetClientHeight() { return kClientHeight; }

	
	
private:

	//ウィンドウ関係
	WNDCLASS wc{};

	HWND hwnd_ = nullptr;

	// クライアントサイズ定数（静的）
	static constexpr int32_t kClientWidth = 1280;
	static constexpr int32_t kClientHeight = 720;
};