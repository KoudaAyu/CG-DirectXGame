#include "WindowsAPI.h"

#include<Windows.h>

void WindowAPI::Initialize()
{


	//ウィンドウプロシージャ
	wc.lpfnWndProc = WindowAPI::WindowProc;

	//ウィンドウクラス名
	wc.lpszClassName = L"MyWindowClass";

	//インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);

	//カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	//ウィンドウクラスを登録する
	RegisterClass(&wc);



	//ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };

	//クライアント領域をもとに実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

	//ウィンドウの生成
	hwnd_ = CreateWindow(
		wc.lpszClassName, //ウィンドウクラス名
		L"DirectX Window", //ウィンドウタイトル
		WS_OVERLAPPEDWINDOW, //ウィンドウスタイル
		CW_USEDEFAULT, CW_USEDEFAULT, //位置
		wrc.right - wrc.left, wrc.bottom - wrc.top, //サイズ
		nullptr, //親ウィンドウハンドル
		nullptr, //メニューハンドル
		wc.hInstance, //インスタンスハンドル
		nullptr //追加のパラメータ
	);
}

LRESULT WindowAPI::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
	{
		return true;
	}

	//メッセージに応じてゲーム固有の処理を行う
	switch (msg)
	{
		//ウィンドウが破棄された
	case WM_DESTROY:
		//アプリケーションを終了する
		PostQuitMessage(0);
		return 0;
	}

	//標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void WindowAPI::Show()
{
	//ウィンドウを表示する
	ShowWindow(hwnd_, SW_SHOW);
}

void WindowAPI::Finalize()
{
	// --- ウィンドウ解放 ---
	CloseWindow(hwnd_); //ウィンドウの解放
	//COMの終了処理
	CoUninitialize();
}

bool WindowAPI::ProcessMassage()
{
	return false;
}
