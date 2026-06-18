#include "Light.h"

void Light::Initialize(DirectXCom* dxCommon)
{
	directXCom = dxCommon;

	directionalLight = directXCom->CreateBufferResource(directXCom->GetDevice().Get(), sizeof(Object3d::DirectionalLight));

	// MapしてGPUリソースのCPU側の書き込み可能ポインタを取得する

	directionalLight->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// directionalLightDataに値を書き込む
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;

	// 書き込み完了後はUnmapを呼ぶ
	directionalLight->Unmap(0, nullptr);
}

