#include "KeyInput.h"

#include<cassert>

KeyInput::KeyInput()
{
}

KeyInput::~KeyInput()
{
}

void KeyInput::Initialize(HINSTANCE hInstance, HWND hwnd)
{

	/*HRESULT hr;*/

	 // DirectInputの初期化
	HRESULT result = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	/* assert(SUCCEEDED(hr));*/

	 //キーボードデバイスの生成
	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	/* assert(SUCCEEDED(hr));*/

	 //入力データ形式のセット
	result = keyboard->SetDataFormat(&c_dfDIKeyboard); //標準形式
	/* assert(SUCCEEDED(hr));*/

	 //排他制御レベルのセット
	result = keyboard->SetCooperativeLevel(hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	/* assert(SUCCEEDED(hr));*/
}

void KeyInput::Update()
{

	//キーボード情報の取得開始
	keyboard->Acquire();
	//全キーの入力状態を取得する
	
	keyboard->GetDeviceState(sizeof(keyState_), keyState_);

	if (keyState_[DIK_0])
	{
		OutputDebugStringA("Hit 0\n");
	}
}
