#pragma once

#include<wrl.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>


class DirectXCom
{
public:
	void Initialize();

	void DebugLayer();

private:
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
};