#include "KeyInput.h"

#include<cassert>

KeyInput::KeyInput()
{
}

KeyInput::~KeyInput()
{
}

void KeyInput::Initialize(WindowAPI* windowAPI)
{

	/*HRESULT hr;*/

	 // DirectInputの初期化
	HRESULT result = DirectInput8Create(windowAPI->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(result) && directInput != nullptr); // 失敗時停止

	 //キーボードデバイスの生成
	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(result) && directInput != nullptr); // 失敗時停止

	//入力データ形式のセット
	result = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(result) && keyboard != nullptr);

	
	//排他制御レベルのセット
	result = keyboard->SetCooperativeLevel(windowAPI->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result) && keyboard != nullptr);


}

void KeyInput::Update()
{

	memcpy(keyPre, key, sizeof(key));

	//キーボード情報の取得開始
	keyboard->Acquire();
	//全キーの入力状態を取得する
	
	keyboard->GetDeviceState(sizeof(key), key);

	if (TriggerKey(DIK_0))
	{
		OutputDebugStringA("Hit 0\n");
	}
}

bool KeyInput::PushKey(BYTE keyNumber)
{
	if (key[keyNumber])
	{
		return true;
	}

	return false;
}

bool KeyInput::TriggerKey(BYTE keyNumber)
{
	// 前回は押されていない、今回押された場合のみ true
	if (!keyPre[keyNumber] && key[keyNumber])
	{
		return true;
	}
	return false;
}
