#pragma once

#define DIRECTIONPUT_VECTOR         0x0800 // 定数名は変更しません
#include<dinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#include <Windows.h> // HINSTANCE, HWND, HRESULT のために必要です
#include <cassert>   // assert のために必要です
// ZeroMemory のために必要です

class KeyInput
{
public:
    // コンストラクタ: メンバー変数を初期化
    KeyInput();

    // デストラクタ: DirectInput リソースを解放（非常に重要！）
    ~KeyInput();

    // 初期化メソッド
    void Initialize(HINSTANCE hInstance, HWND hwnd);

    // 更新メソッド
    void Update();

    // ★追加: キーの状態を外部から取得するためのメソッド
    // 実装はヘッダーファイルに inline で書いても、.cpp に書いても良い
    bool IsKeyPressed(int dik_code) const;

private:
    // DirectInput オブジェクトのポインタ
    IDirectInput8* directInput = nullptr;
    // キーボードデバイスのポインタ
    IDirectInputDevice8* keyboard = nullptr;
    // ★追加: 全キーの現在の状態を保持する配列
    BYTE keyState_[256];

    // コピーコンストラクタと代入演算子を禁止 (リソース管理クラスでは一般的)
    KeyInput(const KeyInput&) = delete;
    KeyInput& operator=(const KeyInput&) = delete;
};

// IsKeyPressed メソッドのインライン実装 (ヘッダーファイルに直接書く場合)
inline bool KeyInput::IsKeyPressed(int dik_code) const
{
    // DIK_CODE の範囲チェック
    if (dik_code >= 0 && dik_code < 256)
    {
        // 最上位ビット (0x80) が立っていれば、キーが押されている状態
        return (keyState_[dik_code] & 0x80) != 0;
    }
    return false; // 無効な DIK_CODE
}