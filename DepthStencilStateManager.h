#pragma once
#include "Graphic.h"
#include<cassert>
class DepthStencilStateManager
{
public:
	void DepthStencilStateSetting(const Microsoft::WRL::ComPtr<ID3D12Device>& device, HRESULT hr,
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& graphicPipelineStateDesc,const Microsoft::WRL::ComPtr<IDxcBlob>& vertexShaderBlob,
		const Microsoft::WRL::ComPtr<IDxcBlob>& pixelShaderBlob);

	const Microsoft::WRL::ComPtr<ID3D12PipelineState> GetGraphicPipelineState() const
	{
		return graphicPipelineState;
	}
private:
	Graphic graphic;
	//DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicPipelineState = nullptr;
};
