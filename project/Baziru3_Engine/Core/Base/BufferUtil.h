#pragma once
#include <d3d12.h>
#include <vector>
#include <wrl.h>
#include <cassert>
#include <cstring>
#include "DirectXCom.h"

namespace BufferUtil
{
    template <typename T>
    inline Microsoft::WRL::ComPtr<ID3D12Resource> CreateVertexBuffer(
        DirectXCom* dxCommon,
        const std::vector<T>& vertices,
        D3D12_VERTEX_BUFFER_VIEW& outView)
    {
        if (!dxCommon || vertices.empty())
        {
            outView = {};
            return nullptr;
        }

        size_t sizeInBytes = vertices.size() * sizeof(T);
        auto device = dxCommon->GetDevice();
        assert(device);

        Microsoft::WRL::ComPtr<ID3D12Resource> buffer = dxCommon->CreateBufferResource(device, sizeInBytes);
        assert(buffer);

        void* mapped = nullptr;
        HRESULT hr = buffer->Map(0, nullptr, &mapped);
        assert(SUCCEEDED(hr));
        std::memcpy(mapped, vertices.data(), sizeInBytes);
        buffer->Unmap(0, nullptr);

        outView.BufferLocation = buffer->GetGPUVirtualAddress();
        outView.SizeInBytes = static_cast<UINT>(sizeInBytes);
        outView.StrideInBytes = static_cast<UINT>(sizeof(T));

        return buffer;
    }

    template <typename T>
    inline Microsoft::WRL::ComPtr<ID3D12Resource> CreateIndexBuffer(
        DirectXCom* dxCommon,
        const std::vector<T>& indices,
        D3D12_INDEX_BUFFER_VIEW& outView,
        DXGI_FORMAT format = DXGI_FORMAT_R32_UINT)
    {
        if (!dxCommon || indices.empty())
        {
            outView = {};
            return nullptr;
        }

        size_t sizeInBytes = indices.size() * sizeof(T);
        auto device = dxCommon->GetDevice();
        assert(device);

        Microsoft::WRL::ComPtr<ID3D12Resource> buffer = dxCommon->CreateBufferResource(device, sizeInBytes);
        assert(buffer);

        void* mapped = nullptr;
        HRESULT hr = buffer->Map(0, nullptr, &mapped);
        assert(SUCCEEDED(hr));
        std::memcpy(mapped, indices.data(), sizeInBytes);
        buffer->Unmap(0, nullptr);

        outView.BufferLocation = buffer->GetGPUVirtualAddress();
        outView.SizeInBytes = static_cast<UINT>(sizeInBytes);
        outView.Format = format;

        return buffer;
    }
}
