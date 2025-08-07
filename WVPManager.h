#pragma once
#include<d3d12.h>
#include <wrl.h>
#include"Buffer.h"
#include"Struct.h"
class WVPManager
{
public:
	void Initialize(const Microsoft::WRL::ComPtr<ID3D12Device>& device,Buffer buffer);
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = nullptr;
	//データを書き込む
	TransformationMatrix* wvpData = nullptr;
};