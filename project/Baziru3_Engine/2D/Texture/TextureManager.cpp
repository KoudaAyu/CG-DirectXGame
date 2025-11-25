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
	if (filePath.empty()) {
		return;
	}
	// DirectXコンテキスト未設定なら何もしない
	if (!directXCom_) {
		return;
	}
	// 既に読み込み済みの場合は何もしない
	if (GetTextureIndexByFilePath(filePath) >= 0) {
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

	// SRV割り当て: 0 は ImGui 用に予約する
	const uint32_t descriptorSize = directXCom_->GetDescriptorSizeSRV();
	const uint32_t heapReservedForImGui = 1u;
	// ベクタ上のインデックス
	uint32_t vecIndex = static_cast<uint32_t>(textureDatas_.size() - 1);
	uint32_t srvIndex = heapReservedForImGui + vecIndex;
	textureData.srvIndex_ = srvIndex;

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

int32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath) const
{
	for (size_t i = 0; i < textureDatas_.size(); ++i)
	{
		if (textureDatas_[i].filePath_ == filePath)
		{
			// SRVインデックスを返す
			return static_cast<int32_t>(textureDatas_[i].srvIndex_);
		}
	}
	return -1; // 見つからなかった
}
