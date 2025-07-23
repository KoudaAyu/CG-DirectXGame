#pragma once

#include <string>
#include <wrl.h> // 必要に応じて追加
#include <cassert> // assertを使用するため
#include <vector>  // vectorを使用するため

#include <d3d12.h>
#include <dxgi1_6.h>

#include "externals/DirectXTex/DirectXTex.h" 

#include "externals/DirectXTex/d3dx12.h" 

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

#include "Dx12.h"
#include "StringConverter.h"

class Texture
{
public:

	void Load(ID3D12Device* device,                  // D3D12デバイス (生ポインタが一般的)
		ID3D12GraphicsCommandList* commandList, // コマンドリスト (生ポインタが一般的)
		const std::string& filePath);

private:

	DirectX::ScratchImage LoadTexture(const std::string& filePath);
	Microsoft::WRL::ComPtr<ID3D12Resource>CreateTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device,
		const DirectX::TexMetadata& metadata);

	[[nodiscard]]
	Microsoft::WRL::ComPtr<ID3D12Resource>UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
		const DirectX::ScratchImage& mipImages,
		const Microsoft::WRL::ComPtr<ID3D12Device>* device,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList);

	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;      // 最終的なGPU上のテクスチャリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadHeap_;    // データアップロードのための一時的なGPUメモリ領域
	DirectX::TexMetadata metadata_;                         // 読み込んだテクスチャのサイズやフォーマットなどの情報
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_{};             // このテクスチャを使うためのSRVの設定情報


};
