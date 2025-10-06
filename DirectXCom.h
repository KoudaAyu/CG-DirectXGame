#pragma once

#include<wrl.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>


class DirectXCom
{
public:
	void Initialize();

	void DebugLayer();

	void CreateDXGIFactory();

	Microsoft::WRL::ComPtr<ID3D12Device>& GetDevice()
	{
		return device;
	}


private:
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;

};