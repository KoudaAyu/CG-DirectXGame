#include "DirectionalLight.h"

void DirectionalLight::Initialize(ID3D12Device* device)
{
	directionalLight = Buffer::CreateBufferResource(device, sizeof(DirectionalLight));

	// MapしてGPUリソースのCPU側の書き込み可能ポインタを取得する
	directionalLight->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// directionalLightDataに値を書き込む
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;



	// 書き込み完了後はUnmapを呼ぶ
	directionalLight->Unmap(0, nullptr);
}

void DirectionalLight::Draw(ID3D12GraphicsCommandList* commandList)
{
	commandList->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());
}

void DirectionalLight::DrawSprite(ID3D12GraphicsCommandList* commandList)
{
	commandList->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());
}
