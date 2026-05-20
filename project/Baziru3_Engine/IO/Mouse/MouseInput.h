#pragma once

#define DIRECTIONPUT_VECTOR         0x0800
#include <dinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#include <Windows.h>
#include <wrl.h>
#include <cassert>

#include "WindowsAPI.h"

using Microsoft::WRL::ComPtr;

class MouseInput
{
public:
    MouseInput();
    ~MouseInput();

    void Initialize(WindowAPI* windowAPI);
    void Update();

    // ボタン押下
    bool PushButton(int button) const;
    // 押した瞬間
    bool TriggerButton(int button) const;

    // 相対移動量
    int GetMoveX() const;
    int GetMoveY() const;

    // クライアント座標 (内部で累積・クランプされる)
    int GetX() const { return posX; }
    int GetY() const { return posY; }

    WindowAPI* GetWindowAPI() const { return windowAPI; }

private:
    ComPtr<IDirectInput8> directInput;
    ComPtr<IDirectInputDevice8> mouse;

    DIMOUSESTATE2 mouseState{};
    DIMOUSESTATE2 mouseStatePrev{};

    int posX = 0;
    int posY = 0;

    // 禁止
    MouseInput(const MouseInput&) = delete;
    MouseInput& operator=(const MouseInput&) = delete;

    WindowAPI* windowAPI = nullptr;
};
