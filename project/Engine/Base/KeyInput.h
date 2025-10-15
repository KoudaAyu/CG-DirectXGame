#pragma once

#define DIRECTIONPUT_VECTOR         0x0800 // 定数名は変更しません
#include<dinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#include <Windows.h> // HINSTANCE, HWND, HRESULT のために必要です
#include<wrl.h>
#include <cassert> 

#include"WindowsAPI.h"

// ZeroMemory のために必要です

using Microsoft::WRL::ComPtr;

class KeyInput
{
public:
    // コンストラクタ: メンバー変数を初期化
    KeyInput();

    // デストラクタ: DirectInput リソースを解放（非常に重要！）
    ~KeyInput();

    // 初期化メソッド
    void Initialize(WindowAPI* windowAPI);

    // 更新メソッド
    void Update();

    /// <summary>
    /// キーの押下を検出
    /// </summary>
    /// <param name="keyNumber">キー番号</param>
    /// <returns>押されているかどうか</returns>
    bool PushKey(BYTE keyNumber);

	/// <summary>
	/// 前フレームと現在のフレーム検出
	/// </summary>
	/// <param name="keyNumber"></param>
	/// <returns></returns>
	bool TriggerKey(BYTE keyNumber);

    // キーの状態を外部から取得するためのメソッド
    // 実装はヘッダーファイルに inline で書いても、.cpp に書いても良い
    bool IsKeyPressed(int dik_code) const;

    WindowAPI* GetWindowAPI() const { return windowAPI; }

private:
    // DirectInput オブジェクトのポインタ
    ComPtr<IDirectInput8> directInput ;
    // キーボードデバイスのポインタ
    ComPtr<IDirectInputDevice8> keyboard;
    // ★追加: 全キーの現在の状態を保持する配列
    BYTE key[256] = {};
    BYTE keyPre[256] = {};
    // コピーコンストラクタと代入演算子を禁止 (リソース管理クラスでは一般的)
    KeyInput(const KeyInput&) = delete;
    KeyInput& operator=(const KeyInput&) = delete;

    WindowAPI* windowAPI = nullptr;
};

// IsKeyPressed メソッドのインライン実装 (ヘッダーファイルに直接書く場合)
inline bool KeyInput::IsKeyPressed(int dik_code) const
{
    // DIK_CODE の範囲チェック
    if (dik_code >= 0 && dik_code < 256)
    {
        // 最上位ビット (0x80) が立っていれば、キーが押されている状態
        return (key[dik_code] & 0x80) != 0;
    }
    return false; // 無効な DIK_CODE
}