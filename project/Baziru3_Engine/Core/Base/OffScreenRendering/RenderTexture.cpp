#include "RenderTexture.h"

Microsoft::WRL::ComPtr<ID3D12Resource> RenderTexture::CreateRenderTargetTexture(const Microsoft::WRL::ComPtr<ID3D12Device>& device, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor)
{
    // RenderTargetとして利用可能にする
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_HEAP_PROPERTIES heapProperties = {};
    // VRAM上に作成
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	// クリアカラーの設定
    D3D12_CLEAR_VALUE clearValue;
	clearValue.Format = format;
	clearValue.Color[0] = clearColor.x;
	clearValue.Color[1] = clearColor.y;
	clearValue.Color[2] = clearColor.z;
	clearValue.Color[3] = clearColor.w;

    // Resourceの作成
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET, // これから描画することを前提としたTextureなのでRenderTarget歳て使うところから始める
        &clearValue, // Clear最適値。ClearRenderTargetをこの色でClearするようにする。
		IID_PPV_ARGS(&resource_));

    return resource_;
}
