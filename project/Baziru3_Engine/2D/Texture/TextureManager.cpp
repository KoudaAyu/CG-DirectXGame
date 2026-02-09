#include"TextureManager.h"
#include <cassert>
#include <filesystem>

TextureManager* TextureManager::instance_ = nullptr;

TextureManager* TextureManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new TextureManager();
	}
	return instance_;
}

void TextureManager::Finalize()
{
	delete instance_;
	instance_ = nullptr;
}

void TextureManager::Initialize()
{
	textureDatas_.reserve(DirectXCom::kMaxSRVCount);
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

	//テクスチャデータを追加
	textureDatas_.resize(textureDatas_.size() + 1);
	//追加したテクスチャデータの参照を取得
	TextureData& textureData = textureDatas_.back();

	textureData.filePath_ = filePath;
	textureData.metadata_ = mipImages.GetMetadata();
	textureData.resource_ =
		directXCom_->CreateTextureResource(textureData.metadata_);

	//テクスチャデータの要素数番号をSRVのインデックスとする
	uint32_t srvIndex = static_cast<uint32_t>(textureDatas_.size() - 1) + kSRVIndexTop;

	//テクスチャ枚数上限チェック
	assert(textureDatas_.size() + kSRVIndexTop <= DirectXCom::kMaxSRVCount);

	textureData.srvHandleCPU_ =
		directXCom_->GetSRVHandleCPU(srvIndex);

	textureData.srvHandleGPU_ =
		directXCom_->GetSRVHandleGPU(srvIndex);

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
	//for (size_t i = 0; i < textureDatas_.size(); ++i)
	//{
	//	if (textureDatas_[i].filePath_ == filePath)
	//	{
	//		return static_cast<int32_t>(i);
	//	}
	//}
	//return -1; // 見つからなかった

	//読み込み済みテクスチャデータを検索
	auto it = std::find_if(textureDatas_.begin(), textureDatas_.end(),
		[&filePath](const TextureData& data)
		{
			return data.filePath_ == filePath;
		});

	if (it != textureDatas_.end())

	{
		uint32_t textureIndex = static_cast<uint32_t>(std::distance(textureDatas_.begin(), it));
		return textureIndex;
	}

	assert(0);
	return 0;
}

