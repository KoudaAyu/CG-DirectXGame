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
	uploadBuffers_.clear();
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
	if (textureDates_.contains(filePath))
	{
		return textureDates_.at(filePath).srvIndex_;
	}

	// 未登録の場合は最初の有効なテクスチャまたは無効インデックスを返す
	if (!textureDates_.empty())
	{
		return textureDates_.begin()->second.srvIndex_;
	}
	return kInvalidTextureIndex;
}

const DirectX::TexMetadata& TextureManager::GetMetadata(uint32_t index) const
{
	if (indexToFilePath_.contains(index))
	{
		const std::string& filePath = indexToFilePath_.at(index);
		if (textureDates_.contains(filePath))
		{
			return textureDates_.at(filePath).metadata_;
		}
	}

	static DirectX::TexMetadata dummy{};
	return dummy;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t index) const
{
	if (directXCom_)
	{
		return directXCom_->GetSRVHandleGPU(index);
	}

	if (indexToFilePath_.contains(index))
	{
		const std::string& filePath = indexToFilePath_.at(index);
		if (textureDates_.contains(filePath))
		{
			return textureDates_.at(filePath).srvHandleGPU_;
		}
	}

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

	{
		std::ostringstream oss;
		oss << "[Debug TM] Load START file=" << filePath 
		    << " directXCom_=" << std::hex << (uintptr_t)directXCom_
		    << " srvMgr->dx=" << (uintptr_t)(srvManager_ ? srvManager_->GetDirectXCom() : nullptr) << std::dec << "\n";
		OutputDebugStringA(oss.str().c_str());
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
	{
		std::ostringstream oss;
		oss << "TextureManager::Load metadata - file='" << filePath
			<< "' width=" << mateData.width
			<< " height=" << mateData.height
			<< " arraySize=" << mateData.arraySize
			<< " mipLevels=" << mateData.mipLevels
			<< " isCubemap=" << (mateData.IsCubemap() ? "true" : "false")
			<< " dimension=" << static_cast<int>(mateData.dimension) << "\n";
		OutputDebugStringA(oss.str().c_str());
	}

	//テクスチャリソースの生成(GPU上のID3D12Resourceを作る)
	auto textureResource = directXCom_->CreateTextureResource(mateData);

	//テクスチャデータのアップロード(CPU側ののイメージをGPUテクスチャへ転送)
	//テクスチャデータのアップロード(CPU側ののイメージをGPUテクスチャへ転送)
	
	TextureData& textureData = textureDates_[filePath];
	textureData.filePath_ = filePath;
	textureData.metadata_ = mateData;
	textureData.resource_ = textureResource;

	{
		auto intermediate = directXCom_->UploadTextureData(
			textureData.resource_, mipImages, directXCom_->GetDevice(), directXCom_->GetCommandList()
		);
		uploadBuffers_.push_back(intermediate);
	}

	//SRVの割り当てと先性
	uint32_t srvIndex = srvManager_->Allocate();
	auto cpuHandle = srvManager_->GetCPUDescriptorHandle(srvIndex);
	auto gpuHandle = srvManager_->GetGPUDescriptorHandle(srvIndex);

	//SRV作成
	srvManager_->CreateSRVForTexture2D(srvIndex, textureData.resource_.Get(), mateData);

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

	{
		std::ostringstream oss;
		oss << "[Debug TM] Load END file=" << filePath 
		    << " directXCom_=" << std::hex << (uintptr_t)directXCom_
		    << " srvMgr->dx=" << (uintptr_t)(srvManager_ ? srvManager_->GetDirectXCom() : nullptr) << std::dec << "\n";
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

void TextureManager::ReleaseUploadBuffers()
{
	if (directXCom_ && !uploadBuffers_.empty())
	{
		directXCom_->ExecuteAndWaitForGPU();
		uploadBuffers_.clear();
	}
}

