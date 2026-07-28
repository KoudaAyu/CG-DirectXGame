#pragma once
#include <cstdint>
#include <d3d12.h>

struct ID3D12GraphicsCommandList;
class WindowAPI;
class Camera;
class Light;

class RenderContext
{
public:
	RenderContext() = default;
	RenderContext(ID3D12GraphicsCommandList* cmdList, WindowAPI* winAPI, Camera* cam, Light* lt);
	~RenderContext() = default;

	// API固有の操作をカプセル化（移植性のための抽象レイヤー）
	void IASetVertexBuffers(UINT startSlot, UINT numViews, const D3D12_VERTEX_BUFFER_VIEW* pViews) const;
	void IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* pView) const;
	void IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology) const;
	void SetGraphicsRootConstantBufferView(UINT rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS bufferLocation) const;
	void SetGraphicsRootDescriptorTable(UINT rootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE baseDescriptor) const;
	void DrawIndexedInstanced(UINT indexCountPerInstance, UINT instanceCount, UINT startIndexLocation, INT baseVertexLocation, UINT startInstanceLocation) const;
	void DrawInstanced(UINT vertexCountPerInstance, UINT instanceCount, UINT startVertexLocation, UINT startInstanceLocation) const;

	// ゲッター
	ID3D12GraphicsCommandList* GetRawCommandList() const { return commandList; }
	WindowAPI* GetWindowAPI() const { return windowAPI; }
	Camera* GetCamera() const { return camera; }
	Light* GetLight() const { return light; }

	void SetTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { textureHandle = handle; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetTextureHandle() const { return textureHandle; }

	void SetMaterialGPUAddress(D3D12_GPU_VIRTUAL_ADDRESS address) { materialGPUAddress = address; }
	D3D12_GPU_VIRTUAL_ADDRESS GetMaterialGPUAddress() const { return materialGPUAddress; }

public:
	ID3D12GraphicsCommandList* commandList = nullptr;
	WindowAPI* windowAPI = nullptr;
	Camera* camera = nullptr;
	Light* light = nullptr;
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle{};
	D3D12_GPU_VIRTUAL_ADDRESS materialGPUAddress = 0;
};
