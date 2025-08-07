#include "DepthStencilStateManager.h"

void DepthStencilStateManager::DepthStencilStateSetting(const Microsoft::WRL::ComPtr<ID3D12Device>& device, 
	HRESULT hr,D3D12_GRAPHICS_PIPELINE_STATE_DESC& graphicPipelineStateDesc,const Microsoft::WRL::ComPtr<IDxcBlob>& vertexShaderBlob,
	const Microsoft::WRL::ComPtr<IDxcBlob>& pixelShaderBlob)
{
	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	//書き込み
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqua。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	//DepthStencilの設定
	graphicPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	//実際に生成
	
	hr = device->CreateGraphicsPipelineState(&graphicPipelineStateDesc,
		IID_PPV_ARGS(&graphicPipelineState));

	assert(vertexShaderBlob && "頂点シェーダーの読み込み失敗！");
	assert(pixelShaderBlob && "ピクセルシェーダーの読み込み失敗！");

	//パイプラインステートの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));
}
