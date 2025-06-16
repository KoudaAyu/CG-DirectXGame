#include<Windows.h>

#include<chrono> //時間を扱うライブラリ
#include<cstdint>
#include<filesystem> //ファイルやディレクトリに関する操作を行うライブラリ
#include<format> //文字列のフォーマットを行うライブラリ
#include<fstream> //ファイルにかいたり読んだりするライブラリ
#include<string> //文字列を扱うライブラリ

std::wstring ConvertString(const std::string& str)
{
	if (str.empty())
	{
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0)
	{
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

std::string ConvertString(const std::wstring& str)
{
	if (str.empty())
	{
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0)
	{
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}

void Log(std::ostream& os, const std::string& message)
{
	os << message << std::endl;
	OutputDebugStringA(message.c_str()); //出力ウィンドウに文字を出力
}

//ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
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

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	//ログファイル関係
	//ログのディレクトリを用意
	std::filesystem::create_directories("logs");

	//現在時刻を取得(UTC時刻)
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

	//ログファイルの名前にコンマ何秒はいらないため、削って秒にする
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
	nowSecound = std::chrono::time_point_cast<std::chrono::seconds>(now);

	//日本時間(PCの設定時間に変換)
	std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSecound };

	//formatを使って年月日_時分秒の形式にする
	std::string datString = std::format("{:%Y%m%d_%H%M%S}", localTime);

	//時刻を使ってファイル名を決定
	std::string logFilePath = std::string("logs/") + datString + ".log";

	//ファイルを作って書き込み準備
	std::ofstream logStream(logFilePath);

	

	//ウィンドウ関係
	WNDCLASS wc{};

	//ウィンドウプロシージャ
	wc.lpfnWndProc = WindowProc;

	//ウィンドウクラス名
	wc.lpszClassName = L"MyWindowClass";

	//インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);

	//カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	//ウィンドウクラスを登録する
	RegisterClass(&wc);

	//クライアント領域のサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;

	//ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };

	//クライアント領域をもとに実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

	//ウィンドウの生成
	HWND hwnd = CreateWindow(
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

	//ウィンドウを表示する
	ShowWindow(hwnd, SW_SHOW);

	MSG msg{};
	//ウィンドウのxボタンが押されるまでループ
	while (msg.message != WM_QUIT)
	{
		//Windowに目セージが来ていたら最優先で処理される
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg); //メッセージを変換
			DispatchMessage(&msg); //メッセージをウィンドウプロシージャに送る
		}
		else
		{
			//ゲームの処理
		}
	}

	Log(logStream, "Application terminating.");

	std::wstring wstringValue = L"Hello, DirectX!";
	Log(logStream, ConvertString(std::format(L"WSTRING{}\n", wstringValue)));
	

	//出力ウィンドウへの文字出力
	OutputDebugStringA("Hello, DirextX!\n");

	return 0;
}

