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

void TextureManager::Initialize(DirectXCom* dxCommon, SRVManager* SrvManager)
{
	// DirectXCom を保存
	assert(dxCommon != nullptr);
	directXCom_ = dxCommon;



	srvManager_ = SrvManager;

	// SRVヒープの最大数に合わせてテクスチャコンテナを予約
	textureDatas.reserve(DirectXCom::kMaxSRVCount);
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
	// 既に読み込まれているかチェック
	if (textureDatas.contains(filePath))
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

	//テクスチャデータを追加 (unordered_map に挿入/生成)
	TextureData& textureData = textureDatas[filePath];

	textureData.filePath_ = filePath;
	textureData.metadata_ = mipImages.GetMetadata();
	textureData.resource_ =
		directXCom_->CreateTextureResource(textureData.metadata_);

	// SRV を確保してハンドルを設定
	if (srvManager_)
	{
		uint32_t alloc = srvManager_->Allocate();
		if (alloc == UINT32_MAX)
		{
			// Allocation failed: assert in debug and return early in release
			assert(false && "TextureManager::LoadTexture - SRV allocation failed");
			// Remove the inserted entry to keep state consistent
			textureDatas.erase(filePath);
			return;
		}

		textureData.srvIndex_ = alloc;
		textureData.srvHandleCPU_ = srvManager_->GetSRVHandleCPU(alloc);
		textureData.srvHandleGPU_ = srvManager_->GetGPUDescriptorHandle(alloc);
	}
	else
	{
		// Fallback when no SRVManager: compute index based on insertion order + top offset
		uint32_t srvIndex = static_cast<uint32_t>(textureDatas.size() - 1) + kSRVIndexTop;
		// Range check
		assert(srvIndex <= DirectXCom::kMaxSRVCount && "TextureManager::LoadTexture - srvIndex out of range");
		textureData.srvIndex_ = srvIndex;
		textureData.srvHandleCPU_ =
			directXCom_->GetSRVHandleCPU(srvIndex);

		textureData.srvHandleGPU_ =
			directXCom_->GetSRVHandleGPU(srvIndex);
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = textureData.metadata_.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(textureData.metadata_.mipLevels);

	// SRV を作成（ハンドルは既に安全に設定されているはず）
	directXCom_->GetDevice()->CreateShaderResourceView(
		textureData.resource_.Get(), &srvDesc, textureData.srvHandleCPU_);


}

uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath) const
{
	uint32_t index = 0;
	for (auto it = textureDatas.begin(); it != textureDatas.end(); ++it, ++index)
	{
		if (it->first == filePath)
		{
			return index;
		}
	}
	return UINT32_MAX; // 見つからなかった
}