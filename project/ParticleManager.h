#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <cstdint>
#include<wrl.h>
#include"DirectXCom.h"
#include"Sprite.h"
#include"Matrix4x4.h"
#include"Camera.h"

class ParticleManager
{
public:
	void Initialize(DirectXCom* dxCommon, Camera* camera);
	void Update();
	void Draw();

private:
	void RootSignature();
	void CreateResource();
	void CreateSRV();

private:
	DirectXCom* dxCommon = nullptr;
	Camera* camera = nullptr;

	static constexpr uint32_t kNumInstance = 10;

	Sprite::Transform particleTransform[kNumInstance];
private:
	TransformationMatrix* instancingData = nullptr;

	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;

	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU;
};