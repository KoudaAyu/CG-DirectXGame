#pragma once

#include"Buffer.h"
#include"Struct.h"

class DirectionalLight
{
public:
	void Initialize(ID3D12Device* device);
	void Update();
	void Draw(ID3D12GraphicsCommandList* commandList);
	void DrawSprite(ID3D12GraphicsCommandList* commandList);

	Vector3& GetUVTranslateSprite() { return directionalLightData->direction; }

private:
	DirectionalLightData* directionalLightData = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLight;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
};
