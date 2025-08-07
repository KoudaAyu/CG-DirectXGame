#pragma once
#include"Buffer.h"
#include"Struct.h"
class DirectionalLightManager
{
public:
	void Initialize(const Microsoft::WRL::ComPtr<ID3D12Device>& device,Buffer buffer);
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetDirectionalLightResource() const { return directionalLight; }
	Microsoft::WRL::ComPtr<ID3D12Resource>& GetDirectionalLightResource() 
	{ 
		return directionalLight; 
	}
	const DirectionalLight* GetDirectionalLightData() const
	{
		return directionalLightData;
	}
	DirectionalLight* GetDirectionalLightData()
	{
		return directionalLightData;
	}

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLight = nullptr;
	// MapしてGPUリソースのCPU側の書き込み可能ポインタを取得する
	DirectionalLight* directionalLightData = nullptr;
};
