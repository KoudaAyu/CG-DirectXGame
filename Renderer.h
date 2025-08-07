#pragma once
#include<wrl.h>
#include <cstdint>

#include <d3d12.h>
#include <windows.h>


class Renderer
{
public:
	void CreateFence(const Microsoft::WRL::ComPtr<ID3D12Device>& device, HRESULT hr);

	const MSG GetMSG() const { return msg; }

	MSG& GetMSG() { return msg; }

	const Microsoft::WRL::ComPtr<ID3D12Fence>& GetFence() const
	{
		return fence;
	}

	const uint64_t GetFenceValue() const { return fenceValue; }
	uint64_t& GetFenceValue()  { return fenceValue; }

	const HANDLE GetFenceEvent() const { return fenceEvent; }

private:
	MSG msg{};

	//初期値0でFenceを作る
	Microsoft::WRL::ComPtr<ID3D12Fence> fence = nullptr;

	uint64_t fenceValue = 0;

	HANDLE fenceEvent = nullptr;
};
