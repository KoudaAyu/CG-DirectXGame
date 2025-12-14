#pragma once

#include<unordered_map>
#include <iterator>
#include "DirectXCom.h"
#include "SRVManager.h"
#include "StringUtil.h"


class TextureManager
{
public:
	//シングルトンインスタンスを取得
	static TextureManager* GetInstance();

	//終了
	void Finalize();


	void Initialize(DirectXCom* dxCommon, SRVManager* SrvManager);

	void LoadTexture(const std::string& filePath);

	// DirectXCom を設定するためのセッターを追加
	void SetDirectXCom(DirectXCom* directXCom) { directXCom_ = directXCom; }

	// 追加: ファイルパスからSRVインデックスを取得（見つからなければ -1）
	uint32_t GetTextureIndexByFilePath(const std::string& filePath) const;

	//メタデータ取得
	const DirectX::TexMetadata& GetMetadata(uint32_t index) const
	{
		assert(index < textureDatas.size());
		auto it = textureDatas.begin();
		std::advance(it, index);
		return it->second.metadata_;
	}

public:
	const DirectX::TexMetadata& GetMetadata(const std::string& filePath) const
	{
		assert(textureDatas.contains(filePath));
		return textureDatas.at(filePath).metadata_;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t index) const
	{
		assert(index < textureDatas.size());
		auto it = textureDatas.begin();
		std::advance(it, index);
		return it->second.srvHandleGPU_;
	}

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


	DirectXCom* directXCom_ = nullptr;

	SRVManager* srvManager_ = nullptr;

	//SRVインデックスの開始番号
	uint32_t kSRVIndexTop = 1;

	std::unordered_map<std::string, TextureData> textureDatas;
};