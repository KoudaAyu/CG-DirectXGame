#pragma once

#include <wrl.h>   
#include <d3d12.h>  
#include <cassert>  
#include <cstdint>

namespace Dx12
{
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(
        ID3D12Device* device,
        size_t sizeInBytes
    );
}