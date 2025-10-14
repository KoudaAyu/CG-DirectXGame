#include "TextureManager.h"
#include"DirectXCom.h"
#include"StringUtil.h"

TextureManager* TextureManager::instance = nullptr;

TextureManager* TextureManager::GetInstance()
{
    if (instance == nullptr)
    {
        instance = new TextureManager();
    }
    return instance;
}

void TextureManager::Initialize()
{
    //SRVの数と同数
	textureDatas.resize(DirectXCom::GetKMaXSRVCount());
}

void TextureManager::LoadTexture(const std::string& filePath)
{
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

	//textureデータを追加
	textureDatas.resize(textureDatas.size() + 1);
	//追加したtextureデータの参照
	TextureData& textureData = textureDatas.back();
	textureData.filePath = filePath;
	textureData.metadata = mipImages.GetMetadata();
	//TextureResourceを作る
	
}

void TextureManager::Finalize()
{
    delete instance;
	instance = nullptr;
}
