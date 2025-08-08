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

	Texture();
	~Texture();

	static uint32_t Load(
		const Microsoft::WRL::ComPtr<ID3D12Device>& device,
		const std::string& filePath);

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

	void ReleaseAllResources();

private:
	static Microsoft::WRL::ComPtr<ID3D12Resource> resource_;      // 最終的なGPU上のテクスチャリソース
	static Microsoft::WRL::ComPtr<ID3D12Resource> uploadHeap_;    // データアップロードのための一時的なGPUメモリ領域
	static DirectX::TexMetadata metadata_;                         // 読み込んだテクスチャのサイズやフォーマットなどの情報
	inline static D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_{};             // このテクスチャを使うためのSRVの設定情報

	static uint32_t nextHandle_;
	static std::unordered_map<uint32_t, Microsoft::WRL::ComPtr<ID3D12Resource>> resources_;


};

