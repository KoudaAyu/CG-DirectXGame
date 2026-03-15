#pragma once
#include "DirectXCom.h"
#include "StringUtil.h"
#include "SrvManager.h"

#include<unordered_map>
#include <memory>

class TextureManager
{
public:
	//シングルトンインスタンスを取得
	static TextureManager* GetInstance();
	static void Destroy();

	//終了
	void Finalize();


	void Initialize();

	void LoadTexture(const std::string& filePath);

	// DirectXCom を設定するためのセッターを追加
	void SetDirectXCom(DirectXCom* directXCom) { directXCom_ = directXCom; }

	// 追加: SRVManager へのアクセサ（SRV インデックス管理を一元化するため）
	SRVManager* GetSRVManager() const { return srvManager_.get(); }

	// 追加: ファイルパスからSRVインデックスを取得（見つからなければ -1）
	uint32_t GetTextureIndexByFilePath(const std::string& filePath) const;

	//メタデータ取得 (オーバーロード: ファイルパスまたはインデックスで取得可能)
	const DirectX::TexMetadata& GetMetadata(const std::string& filePath) const
	{
		auto it = textureDates_.find(filePath);
		assert(it != textureDates_.end());
		return it->second.metadata_;
	}
	const DirectX::TexMetadata& GetMetadata(uint32_t index) const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath) const
	{
		auto it = textureDates_.find(filePath);
		assert(it != textureDates_.end());
		return it->second.srvHandleGPU_;
	}
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t index) const;

private:
	std::unique_ptr<SRVManager> srvManager_ = nullptr;

	TextureManager() = default;
public:
	~TextureManager() = default; // Made public so unique_ptr can delete
private:
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

	//テクスチャデータ
	std::unordered_map<std::string,TextureData> textureDates_;

	DirectXCom* directXCom_ = nullptr;

	//SRVインデックスの開始番号
	uint32_t kSRVIndexTop = 1;

	
};