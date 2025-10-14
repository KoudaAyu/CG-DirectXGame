#pragma once
#include<string>
#include <wrl.h>
#include "externals/DirectXTex/DirectXTex.h"
#include"externals/DirectXTex/d3dx12.h"

using namespace Microsoft::WRL;

class TextureManager
{
public:
	static TextureManager* GetInstance();
	void Initialize();
	void LoadTexture(const std::string& filePath);
	void Finalize();

private:
	static TextureManager* instance;
	//コンパイラに標準のデフォルト処理を生成させる」ことを指示す。自分で {} のように空の関数本体を書く手間が省る
	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;

	//texture一枚分のデータ
	struct TextureData
	{
		std::string filePath;
		DirectX::TexMetadata metadata;
		ComPtr<ID3D12Resource> textureResource;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};
	
	//textureデータ
	std::vector<TextureData> textureDatas;

};
