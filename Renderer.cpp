#include "Renderer.h"
#include<cassert>

#pragma comment(lib,"d3d12.lib")

void Renderer::CreateFence(Microsoft::WRL::ComPtr<ID3D12Device>& device, HRESULT hr)
{
	fenceValue = 0;
	
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	//フェンスの生成に失敗した場合はエラー
	assert(SUCCEEDED(hr));

	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	//フェンスイベントの生成に失敗した場合はエラー
	assert(fenceEvent != nullptr);
}
