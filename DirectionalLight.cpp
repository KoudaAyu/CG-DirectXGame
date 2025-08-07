#include "DirectionalLight.h"

void DirectionalLightManager::Initialize(const Microsoft::WRL::ComPtr<ID3D12Device>& device, Buffer buffer)
{
	directionalLight = Buffer::CreateBufferResource(device.Get(), sizeof(DirectionalLight));

	directionalLight->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// directionalLightDataに値を書き込む
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;

	// 書き込み完了後はUnmapを呼ぶ
	directionalLight->Unmap(0, nullptr);
}
