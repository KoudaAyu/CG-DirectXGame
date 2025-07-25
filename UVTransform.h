#pragma once

#include<d3d12.h>
#include<wrl.h>

#include"Buffer.h"
#include"Matrix4x4.h"
#include"Struct.h"


class UVTransform
{
public:

	void Initialize(ID3D12Device* device);
	void Update();
	void Draw(ID3D12GraphicsCommandList* commandList);

	Material* GetMaterialData() { return materialData; }

private:

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	//マテリアルにデータを書き込む
	Material* materialData = nullptr;
	// データを設定（赤色 RGBA: 1,0,0,1）
	Vector4 temp{};

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
};