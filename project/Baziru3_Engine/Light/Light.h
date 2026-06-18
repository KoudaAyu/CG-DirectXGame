#pragma once
#include <d3d12.h>
#include<wrl.h>

#include"DirectXCom.h"
#include"Object3d.h"


class Light
{
public:
	void Initialize(DirectXCom* dxCommon);
	Microsoft::WRL::ComPtr<ID3D12Resource> GetDirectionalLightResource() const { return directionalLight; }


private:
	DirectXCom* directXCom = nullptr;

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLight;
	Object3d::DirectionalLight* directionalLightData = nullptr;
};

