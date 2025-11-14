#pragma once
#include "DirectXCom.h"
#include "StringUtil.h"

class TextureManager
{
public:
	//シングルトンインスタンスを取得
	static TextureManager* GetInstance();

	//終了
	void Finalize();


	void Initialize();

	void LoadTexture(const std::string& filePath);

	// DirectXCom を設定するためのセッターを追加
	void SetDirectXCom(DirectXCom* directXCom) { directXCom_ = directXCom; }

private:
	static TextureManager* instance_;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(TextureManager&) = delete;

	// テクスチャ1枚分のデータ
	struct TextureData
	{
		std::string filePath_;
		DirectX::TexMetadata metadata_;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
		uint32_t srvIndex_;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_{};
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};
	};

	std::vector<TextureData> textureDatas_;

	DirectXCom* directXCom_ = nullptr;
};