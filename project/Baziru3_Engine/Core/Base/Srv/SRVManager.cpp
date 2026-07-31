#include "SRVManager.h"
#include <cassert>
#include <sstream>
#include <Windows.h>

void SRVManager::Initialize(DirectXCom* directXCom)
{
	assert(directXCom);
	directXCom_ = directXCom;

	// 0, 1, 2番目を予約（アロケートしておく）
	// (SRVManagerが管理する予約領域)
	directXCom_->GetSrvHeap().AllocateAt(0);
	directXCom_->GetSrvHeap().AllocateAt(1);
	directXCom_->GetSrvHeap().AllocateAt(2);

	{
		std::ostringstream oss;
		oss << "SRVManager::Initialize (this=0x" << std::hex << (unsigned long long)(uintptr_t)this << ")\n";
		OutputDebugStringA(oss.str().c_str());
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE SRVManager::GetCPUDescriptorHandle(uint32_t index)
{
	return directXCom_->GetSRVHandleCPU(index);
}

D3D12_GPU_DESCRIPTOR_HANDLE SRVManager::GetGPUDescriptorHandle(uint32_t index)
{
	return directXCom_->GetSRVHandleGPU(index);
}

void SRVManager::CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLeveles)
{
	assert(srvIndex < kMaxSRVCount);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = MipLeveles;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	directXCom_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex)
	);
}

void SRVManager::CreateSRVForTexture2D(uint32_t index, ID3D12Resource* resource, const DirectX::TexMetadata& meta)
{
	assert(directXCom_);
	assert(index < kMaxSRVCount);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = meta.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (meta.IsCubemap() || (meta.dimension == DirectX::TEX_DIMENSION_TEXTURE2D && meta.arraySize >= 6))
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = UINT(meta.mipLevels);
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	}
	else
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = UINT(meta.mipLevels);
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	}

	directXCom_->GetDevice()->CreateShaderResourceView(resource, &srvDesc, GetCPUDescriptorHandle(index));
}

void SRVManager::CreateSRVForStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
{
	assert(srvIndex < kMaxSRVCount);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = numElements;
	srvDesc.Buffer.StructureByteStride = structureByteStride;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	directXCom_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex)
	);
}

void SRVManager::PreDraw()
{
	ID3D12DescriptorHeap* descriptorHeaps[] = { directXCom_->GetSrvDescriptorHeap().Get() };
	directXCom_->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
}

uint32_t SRVManager::Allocate()
{
	return directXCom_->GetSrvHeap().Allocate();
}

uint32_t SRVManager::AllocateAt(uint32_t index)
{
	return directXCom_->GetSrvHeap().AllocateAt(index);
}

void SRVManager::Free(uint32_t index)
{
	if (directXCom_)
	{
		directXCom_->GetSrvHeap().Free(index);
	}
}
