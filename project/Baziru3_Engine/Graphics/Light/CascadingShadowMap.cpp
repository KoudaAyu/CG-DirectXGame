#include "CascadingShadowMap.h"
#include <cassert>
#include <algorithm>
#include <cmath>

namespace BaziruEngine::Graphics {

namespace {

inline Vector3 Cross(const Vector3& a, const Vector3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline float Dot(const Vector3& a, const Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float LengthSq(const Vector3& v)
{
    return Dot(v, v);
}

inline Vector3 Normalize(const Vector3& v)
{
    float len = std::sqrt(LengthSq(v));
    if (len > 1e-5f)
    {
        return { v.x / len, v.y / len, v.z / len };
    }
    return { 0.0f, 0.0f, 0.0f };
}

inline Matrix4x4 MultiplyMatrix(const Matrix4x4& m1, const Matrix4x4& m2)
{
    Matrix4x4 result{};
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            result.m[i][j] = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                result.m[i][j] += m1.m[i][k] * m2.m[k][j];
            }
        }
    }
    return result;
}

inline Matrix4x4 MakeLookAtMatrix(const Vector3& eye, const Vector3& target, const Vector3& up)
{
    Vector3 zAxis = Normalize({ target.x - eye.x, target.y - eye.y, target.z - eye.z });
    if (LengthSq(zAxis) < 1e-5f) zAxis = { 0.0f, 0.0f, 1.0f };

    Vector3 xAxis = Normalize(Cross(up, zAxis));
    if (LengthSq(xAxis) < 1e-5f) xAxis = { 1.0f, 0.0f, 0.0f };

    Vector3 yAxis = Cross(zAxis, xAxis);

    Matrix4x4 result = MakeIdentity4x4();
    result.m[0][0] = xAxis.x; result.m[0][1] = yAxis.x; result.m[0][2] = zAxis.x;
    result.m[1][0] = xAxis.y; result.m[1][1] = yAxis.y; result.m[1][2] = zAxis.y;
    result.m[2][0] = xAxis.z; result.m[2][1] = yAxis.z; result.m[2][2] = zAxis.z;
    result.m[3][0] = -Dot(xAxis, eye);
    result.m[3][1] = -Dot(yAxis, eye);
    result.m[3][2] = -Dot(zAxis, eye);
    return result;
}

} // namespace

void CascadingShadowMap::Initialize(DirectXCom* dxCommon)
{
    assert(dxCommon);
    dxCommon_ = dxCommon;

    CreateShadowMapResources();
    CreateDescriptorHeaps();

    // 定数バッファの生成
    shadowParamBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), sizeof(ShadowParamForGPU));
    HRESULT hr = shadowParamBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&shadowParamData_));
    assert(SUCCEEDED(hr));
}

void CascadingShadowMap::CreateShadowMapResources()
{
    ID3D12Device* device = dxCommon_->GetDevice().Get();

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resDesc.Alignment = 0;
    resDesc.Width = kShadowMapResolution;
    resDesc.Height = kShadowMapResolution;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    resDesc.SampleDesc.Count = 1;
    resDesc.SampleDesc.Quality = 0;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    for (uint32_t i = 0; i < kCascadeCount; ++i)
    {
        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&shadowMaps_[i])
        );
        assert(SUCCEEDED(hr));
    }
}

void CascadingShadowMap::CreateDescriptorHeaps()
{
    ID3D12Device* device = dxCommon_->GetDevice().Get();

    // DSV ディスクリプタヒープ
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = kCascadeCount;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_));
    assert(SUCCEEDED(hr));

    // SRV ディスクリプタヒープ
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = kCascadeCount;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap_));
    assert(SUCCEEDED(hr));

    UINT dsvIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    UINT srvIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuBase = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuBase = srvHeap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuBase = srvHeap_->GetGPUDescriptorHandleForHeapStart();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvViewDesc{};
    dsvViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvViewDesc.Texture2D.MipSlice = 0;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvViewDesc{};
    srvViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvViewDesc.Texture2D.MostDetailedMip = 0;
    srvViewDesc.Texture2D.MipLevels = 1;

    for (uint32_t i = 0; i < kCascadeCount; ++i)
    {
        dsvCpuHandles_[i] = { dsvCpuBase.ptr + i * dsvIncrement };
        srvCpuHandles_[i] = { srvCpuBase.ptr + i * srvIncrement };
        srvGpuHandles_[i] = { srvGpuBase.ptr + i * srvIncrement };

        device->CreateDepthStencilView(shadowMaps_[i].Get(), &dsvViewDesc, dsvCpuHandles_[i]);
        device->CreateShaderResourceView(shadowMaps_[i].Get(), &srvViewDesc, srvCpuHandles_[i]);
    }
}

void CascadingShadowMap::Update(const Camera& mainCamera, const Vector3& lightDirection)
{
    Vector3 normalizedLightDir = Normalize(lightDirection);
    if (LengthSq(normalizedLightDir) < 0.001f)
    {
        normalizedLightDir = { 0.0f, -1.0f, 0.0f };
    }

    float nearZ = 0.1f;
    float farZ = 100.0f;

    for (uint32_t i = 0; i < kCascadeCount; ++i)
    {
        float splitZ = nearZ + (farZ - nearZ) * splitRatios_[i];
        cascades_[i].splitDistance = splitZ;

        // ライトカメラ行列の生成 (ビュープロジェクション構築)
        Vector3 lightPos = { mainCamera.GetTranslate().x - normalizedLightDir.x * (splitZ * 1.5f),
                             mainCamera.GetTranslate().y - normalizedLightDir.y * (splitZ * 1.5f),
                             mainCamera.GetTranslate().z - normalizedLightDir.z * (splitZ * 1.5f) };
        Matrix4x4 lightView = MakeLookAtMatrix(lightPos, mainCamera.GetTranslate(), { 0.0f, 1.0f, 0.0f });

        float orthoWidth = splitZ * 1.8f;
        float orthoHeight = splitZ * 1.8f;
        Matrix4x4 lightProj = MakeOrthographicMatrix(-orthoWidth, orthoHeight, -orthoWidth, orthoHeight, 0.1f, splitZ * 3.0f);

        cascades_[i].viewMatrix = lightView;
        cascades_[i].projMatrix = lightProj;
        cascades_[i].viewProjMatrix = MultiplyMatrix(lightView, lightProj);

        // GPUバッファに書き込み
        shadowParamData_->shadowViewProj[i] = cascades_[i].viewProjMatrix;
        shadowParamData_->cascadeSplits[i] = splitZ;
    }

    shadowParamData_->lightDirection = normalizedLightDir;
    shadowParamData_->shadowBias = 0.0005f;
}

void CascadingShadowMap::BeginRender(ID3D12GraphicsCommandList* commandList, uint32_t cascadeIndex)
{
    assert(commandList && cascadeIndex < kCascadeCount);

    // リソースバリアを DEPTH_WRITE 状態へ移行
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = shadowMaps_[cascadeIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    // ビューポートとシザー矩形の設定
    D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(kShadowMapResolution), static_cast<float>(kShadowMapResolution), 0.0f, 1.0f };
    D3D12_RECT scissorRect{ 0, 0, static_cast<LONG>(kShadowMapResolution), static_cast<LONG>(kShadowMapResolution) };
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    // DSV のクリアとバインド
    commandList->ClearDepthStencilView(dsvCpuHandles_[cascadeIndex], D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvCpuHandles_[cascadeIndex]);
}

void CascadingShadowMap::EndRender(ID3D12GraphicsCommandList* commandList, uint32_t cascadeIndex)
{
    assert(commandList && cascadeIndex < kCascadeCount);

    // リソースバリアを GENERIC_READ (Shader Resource) 状態へ戻す
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = shadowMaps_[cascadeIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
}

} // namespace BaziruEngine::Graphics
