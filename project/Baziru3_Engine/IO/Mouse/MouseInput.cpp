#include "MouseInput.h"
#include <cassert>

MouseInput::MouseInput()
{
}

MouseInput::~MouseInput()
{
    if (mouse)
    {
        mouse->Unacquire();
        mouse.Reset();
    }
    directInput.Reset();
}

void MouseInput::Initialize(WindowAPI* windowAPI)
{
    this->windowAPI = windowAPI;

    HRESULT result = DirectInput8Create(windowAPI->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
    assert(SUCCEEDED(result) && directInput != nullptr);

    result = directInput->CreateDevice(GUID_SysMouse, &mouse, NULL);
    assert(SUCCEEDED(result) && mouse != nullptr);

    // マウスのデータ形式 (DIMOUSESTATE2 を使用)
    result = mouse->SetDataFormat(&c_dfDIMouse2);
    assert(SUCCEEDED(result) && mouse != nullptr);

    // 協調レベル
    result = mouse->SetCooperativeLevel(windowAPI->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    assert(SUCCEEDED(result) && mouse != nullptr);

    // 初期取得
    mouse->Acquire();

    // 初期のカーソル位置を取得して内部座標を初期化する
    POINT p{};
    if (GetCursorPos(&p))
    {
        if (ScreenToClient(windowAPI->GetHwnd(), &p))
        {
            posX = p.x;
            posY = p.y;
        }
    }
}

void MouseInput::Update()
{
    // 前フレームの状態を保存
    mouseStatePrev = mouseState;

    if (!mouse)
    {
        return;
    }

    HRESULT hr = mouse->GetDeviceState(sizeof(DIMOUSESTATE2), &mouseState);
    if (FAILED(hr))
    {
        // 取得失敗なら再取得を試みる
        hr = mouse->Acquire();
        // 再取得後もう一度試す
        hr = mouse->GetDeviceState(sizeof(DIMOUSESTATE2), &mouseState);
        if (FAILED(hr))
        {
            // 取得できない場合はゼロで埋める
            ZeroMemory(&mouseState, sizeof(mouseState));
        }
    }

    // 実際のカーソルに合わせるため、OS の絶対座標を取得して反映を試みる
    POINT p{};
    if (GetCursorPos(&p))
    {
        if (windowAPI && ScreenToClient(windowAPI->GetHwnd(), &p))
        {
            posX = p.x;
            posY = p.y;
        }
    }
    else
    {
        // GetCursorPos が失敗した場合は相対移動の累積にフォールバック
        posX += mouseState.lX;
        posY += mouseState.lY;
    }

    // クライアント領域内にクランプ
    int w = WindowAPI::GetClientWidth();
    int h = WindowAPI::GetClientHeight();
    if (posX < 0) posX = 0;
    if (posY < 0) posY = 0;
    if (posX > w) posX = w;
    if (posY > h) posY = h;
}

bool MouseInput::PushButton(int button) const
{
    if (button < 0 || button >= 8) return false;
    return (mouseState.rgbButtons[button] & 0x80) != 0;
}

bool MouseInput::TriggerButton(int button) const
{
    if (button < 0 || button >= 8) return false;
    return !(mouseStatePrev.rgbButtons[button] & 0x80) && (mouseState.rgbButtons[button] & 0x80);
}

int MouseInput::GetMoveX() const
{
    return mouseState.lX;
}

int MouseInput::GetMoveY() const
{
    return mouseState.lY;
}

Vector2 MouseInput::GetScaledPosition() const
{
    Vector2 pos{ static_cast<float>(posX), static_cast<float>(posY) };
    if (windowAPI)
    {
        RECT rc{};
        if (GetClientRect(windowAPI->GetHwnd(), &rc))
        {
            float clientW = static_cast<float>(rc.right - rc.left);
            float clientH = static_cast<float>(rc.bottom - rc.top);
            if (clientW > 0.0f && clientH > 0.0f)
            {
                // ウィンドウの実サイズに対する仮想バックバッファサイズの比率でスケーリング
                float sx = static_cast<float>(windowAPI->GetClientWidth()) / clientW;
                float sy = static_cast<float>(windowAPI->GetClientHeight()) / clientH;
                pos.x *= sx;
                pos.y *= sy;
            }
        }
    }
    return pos;
}
