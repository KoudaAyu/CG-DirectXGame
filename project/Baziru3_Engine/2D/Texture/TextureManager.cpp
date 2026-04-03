#include"TextureManager.h"
#include <cassert>
#include <filesystem>
#include <format>
#include <Windows.h>
#include <sstream>
#include <iomanip>
#include "AudioManager.h"

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
        // std::make_unique cannot access a private constructor from here because
        // access checking for templates happens in the template's scope.
        // Allocate manually so the private ctor can be used by this member function.
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
	indexToFilePath_.reserve(DirectXCom::kMaxSRVCount);
}

void TextureManager::LoadTexture(const std::string& filePath)
{
    Load(filePath);
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
	// 逆引きマップでファイルパスを探し、そこからメタデータを取得する
	auto it = indexToFilePath_.find(index);
	if (it != indexToFilePath_.end()) {
		auto it2 = textureDates_.find(it->second);
		if (it2 != textureDates_.end()) {
			return it2->second.metadata_;
		}
	}

	assert(0);
	static DirectX::TexMetadata dummy{};
	return dummy;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t index) const
{
	if (directXCom_)
	{
		return directXCom_->GetSRVHandleGPU(index);
	}


	auto it = indexToFilePath_.find(index);
	if (it != indexToFilePath_.end()) {
		auto it2 = textureDates_.find(it->second);
		if (it2 != textureDates_.end()) {
			return it2->second.srvHandleGPU_;
		}
	}

	assert(0);
	D3D12_GPU_DESCRIPTOR_HANDLE dummy{};
	return dummy;
}

uint32_t TextureManager::Load(const std::string& filePath)
{
	//無効入力
	if (filePath.empty())
	{
		return kInvalidTextureIndex;
	}
	//未初期化チェック
	if (!directXCom_)
	{
		return kInvalidTextureIndex;
	}

	//すでに読み込まれているならインデックスを返す
	auto it = textureDates_.find(filePath);

	if (it != textureDates_.end())
	{
		return it->second.srvIndex_;
	}

	//SRVManagerの初期化
	if (!srvManager_)
	{
		srvManager_ = std::make_unique<SRVManager>();
		srvManager_->Initialize(directXCom_);
		{
			std::ostringstream oss;
			oss << "TextureManager::Load - created srvManager (this=0x" << std::hex << (unsigned long long)(uintptr_t)srvManager_.get() << ")\n";
			OutputDebugStringA(oss.str().c_str());
		}
	}

	//画像読み込み、ミニマップ生成
	DirectX::ScratchImage mipImages = directXCom_->LoadTexture(filePath);

	const DirectX::TexMetadata& mateData = mipImages.GetMetadata();

	//テクスチャリソースの生成(GPU上のID3D12Resourceを作る)
	auto textureResource = directXCom_->CreateTextureResource(mateData);

	//テクスチャデータのアップロード(CPU側ののイメージをGPUテクスチャへ転送)
	// store intermediate resource inside TextureData to keep it alive until GPU consumes it
	TextureData& textureData = textureDates_[filePath];
	textureData.filePath_ = filePath;
	textureData.metadata_ = mateData;
	textureData.resource_ = textureResource;

	textureData.uploadIntermediate_ = directXCom_->UploadTextureData(
		textureData.resource_, mipImages, directXCom_->GetDevice(), directXCom_->GetCommandList()
	);

	// Ensure upload commands are executed and completed on GPU before returning
	directXCom_->ExecuteAndWaitForGPU();

	//SRVの割り当てと先性
	uint32_t srvIndex = srvManager_->Allocate();
	auto cpuHandle = srvManager_->GetCPUDescriptorHandle(srvIndex);
	auto gpuHandle = srvManager_->GetGPUDescriptorHandle(srvIndex);

	//SRV作成
	srvManager_->CreateSRVForTexture2D(srvIndex,textureData.resource_.Get(),mateData.format,(UINT)mateData.mipLevels);

	//TextureDataの格納と逆引き登録(将来的な参照を可能にする)
	textureData.srvIndex_ = srvIndex;
	textureData.srvHandleCPU_ = cpuHandle;
	textureData.srvHandleGPU_ = gpuHandle;
	indexToFilePath_.emplace(srvIndex, filePath);

	// Debug log with srvManager pointer
	{
		std::ostringstream oss;
		oss << "TextureManager::Load - file='" << filePath << "' srvIndex=" << srvIndex << " srvMgr=0x" << std::hex << (unsigned long long)(uintptr_t)srvManager_.get() << " gpu.ptr=0x" << (unsigned long long)textureData.srvHandleGPU_.ptr << std::dec << "\n";
		OutputDebugStringA(oss.str().c_str());
	}

	//戻り値を返す
	return srvIndex;
}

void TextureManager::SetDirectXCom(DirectXCom* directXCom)
{
	directXCom_ = directXCom;

	if (srvManager_)
	{
		srvManager_.reset();
	}
	
	srvManager_ = std::make_unique<SRVManager>();
	srvManager_->Initialize(directXCom_);
}

