#pragma once

#include<string>
#include<dxgi1_6.h>
#include<d3d12.h>

#include "externals/DirectXTex/DirectXTex.h"
#include"externals/DirectXTex/d3dx12.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")



#include"Buffer.h"
#include"StringUtil.h"

class Texture
{
public:

	void Load();

	static DirectX::ScratchImage LoadTexture(const std::string& filePath);

	//TextureResourceを作る
	static Microsoft::WRL::ComPtr<ID3D12Resource>CreateTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device,
		const DirectX::TexMetadata& metadata);

	[[nodiscard]]
	static Microsoft::WRL::ComPtr<ID3D12Resource>UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
		const DirectX::ScratchImage& mipImages, const Microsoft::WRL::ComPtr<ID3D12Device>& device,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList);

	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device,
		int32_t width, int32_t height);



private:
};

