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
	if (filePath.empty()) {
		return;
	}
	// DirectXコンテキスト未設定なら何もしない
	if (!directXCom_) {
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

	// SRV を SRVManager から確保する（もし設定されていれば）
	if (srvManager_)
	{
		// SRV確保前に、SRVヒープの上限に達しないか確認する
		if (textureDatas.size() + kSRVIndexTop <= DirectXCom::kMaxSRVCount)
		{
			textureData.srvIndex_ = srvManager_->Allocate();
			textureData.srvHandleCPU_ = srvManager_->GetSRVHandleCPU(textureData.srvIndex_);
			textureData.srvHandleGPU_ = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex_);
		}
		else
		{
			// SRV確保できない場合は致命的なのでアサート
			assert(false && "TextureManager::LoadTexture - SRV allocation would exceed maximum count");
			return;
		}
	}

	//テクスチャデータの要素数番号をSRVのインデックスとする
	uint32_t srvIndex = static_cast<uint32_t>(textureDatas.size() - 1) + kSRVIndexTop;

	//テクスチャ枚数上限チェック
	assert(textureDatas.size() + kSRVIndexTop <= DirectXCom::kMaxSRVCount);

	// 従来の DirectXCom 経由のハンドル取得は上書きされるが、srvManager_ がない場合に使う
	if (!srvManager_)
	{
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

	// SRV を作成
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
