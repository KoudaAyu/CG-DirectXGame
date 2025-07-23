#pragma once

#include"Dx12.h"
#include"DebugCamera.h"
#include"Struct.h"
#include"Texture.h"
#include"Matrix4x4.h"

class Sprite
{
public:
	void Initialize(ID3D12Device* device);
	void Updata(DebugCamera debugCamera_);
	void Draw();

	void Create(Texture texture_,Vector2 size);

private:
	DebugCamera debugCamera_;

	Texture texture;

	Transform transformSprite;

	TransformationMatrix* transformationMatrixDataSprite = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Device> device;

	//頂点バッファビューを生成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite;

	//Sprite用のTransformationMatrix用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite;
};