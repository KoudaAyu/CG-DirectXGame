#include"TextureManager.h"
#include <cassert>
#include <filesystem>

namespace {
    static std::unique_ptr<TextureManager>& TextureManagerStorage()
    {
        static std::unique_ptr<TextureManager> instance;
        return instance;
    }
}

TextureManager* TextureManager::GetInstance()
{
    auto& instance = TextureManagerStorage();
    if (instance == nullptr)
    {
        instance.reset(new TextureManager());
    }
    return instance.get();
}

void TextureManager::Destroy()
{
    TextureManagerStorage().reset();
}

void TextureManager::Finalize()
{

	srvManager_.reset();
	TextureManagerStorage().reset();
}

void TextureManager::Initialize()
{
	textureDates_.reserve(DirectXCom::kMaxSRVCount);
}

void TextureManager::LoadTexture(const std::string& filePath)
{
	// 無効なパスは無視
	if (filePath.empty())
	{
		return;
	}
	// DirectXコンテキスト未設定なら何もしない
	if (!directXCom_)
	{
		return;
	}
	if(textureDates_.contains(filePath))
	{
		return;
	}

	if (!srvManager_)
	{
		
		srvManager_ = std::make_unique<SRVManager>();
	
		if (directXCom_)
		{
			srvManager_->Initialize(directXCom_);
		}
		else
		{
			return;
		}
	}

	//テクスチャファイルを読み込んでプログラムで使えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = StringUtil::ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_DEFAULT_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	//ミニマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
		DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	// テクスチャデータを追加 (unordered_mapなので resize は使えない)
	// operator[] で要素を作成して参照を取得する
	TextureData& textureData = textureDates_[filePath];

	textureData.srvIndex_ = srvManager_->Allocate();
	textureData.srvHandleCPU_ = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex_);
	textureData.srvHandleGPU_ = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex_);

	textureData.filePath_ = filePath;
	textureData.metadata_ = mipImages.GetMetadata();
	textureData.resource_ =
		directXCom_->CreateTextureResource(textureData.metadata_);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = textureData.metadata_.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(textureData.metadata_.mipLevels);

	directXCom_->GetDevice()->CreateShaderResourceView(
		textureData.resource_.Get(), &srvDesc, textureData.srvHandleCPU_);
}

uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath) const
{
	// unordered_map ではキーで検索する
	auto it = textureDates_.find(filePath);
	if (it != textureDates_.end())
	{
		return it->second.srvIndex_;
	}

	assert(0);
	return 0;
}

const DirectX::TexMetadata& TextureManager::GetMetadata(uint32_t index) const
{
	// index から対応する TextureData を探す
	for (const auto& pair : textureDates_)
	{
		if (pair.second.srvIndex_ == index) {
			return pair.second.metadata_;
		}
	}

	assert(0);
	static DirectX::TexMetadata dummy{};
	return dummy;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t index) const
{
	for (const auto& pair : textureDates_)
	{
		if (pair.second.srvIndex_ == index) {
			return pair.second.srvHandleGPU_;
		}
	}

	assert(0);
	D3D12_GPU_DESCRIPTOR_HANDLE dummy{};
	return dummy;
}

