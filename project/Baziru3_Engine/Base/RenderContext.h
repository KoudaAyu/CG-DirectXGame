#pragma once
#include<cstdint>
#include<d3d12.h>

struct ID3D12GraphicsCommandList;
class WindowAPI;
class Camera;
class Light;

struct RenderContext
{
	ID3D12GraphicsCommandList* commandList = nullptr;
	WindowAPI* windowAPI = nullptr;
	Camera* camera = nullptr;
	Light* light = nullptr;
	D3D12_GPU_DESCRIPTOR_HANDLE environmentTextureHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle{};
	D3D12_GPU_VIRTUAL_ADDRESS materialGPUAddress = 0;
};
