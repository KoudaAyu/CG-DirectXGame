#include "Sprite.h"
using namespace Dx12;

void Sprite::Initialize(ID3D12Device* device)
{
	this->device = device;

	//Sprite用の頂点Resource
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 4);

	//リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;
	//1頂点当たりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	//Sprite用
	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
	//一枚目の三角形
	vertexDataSprite[0].position = { 0.0f,360.0f,0.0f,1.0f }; // 左下
	vertexDataSprite[1].position = { 0.0f,0.0f,0.0f,1.0f };   // 左上
	vertexDataSprite[2].position = { 640.0f,360.0f,0.0f,1.0f }; // 右下
	vertexDataSprite[3].position = { 640.0f,0.0f,0.0f,1.0f };   // 右上

	vertexDataSprite[0].texcoord = { 0.0f,1.0f };
	vertexDataSprite[1].texcoord = { 0.0f,0.0f };
	vertexDataSprite[2].texcoord = { 1.0f,1.0f };
	vertexDataSprite[3].texcoord = { 1.0f,0.0f };

	
	//データを書き込む
	transformationMatrixResourceSprite = CreateBufferResource(device, sizeof(TransformationMatrix));

	//書き込むためのアドレス取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	//単位行列を書き込んでおく
	transformationMatrixDataSprite->WVP = MakeIdentity4x4();
	transformationMatrixDataSprite->World = MakeIdentity4x4();
	transformationMatrixResourceSprite->Unmap(0, nullptr);

	//Sprite用
	transformSprite = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
}

void Sprite::Updata(DebugCamera debugCamera_)
{
	//Sprite用のworldViewProjectMatrix
	Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
	Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
	Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
	Matrix4x4 worldViewProjectionmatrixSprite = Multiply(worldMatrixSprite, Multiply(debugCamera_.GetViewMatrix(), projectionMatrixSprite));
	transformationMatrixDataSprite->WVP = worldViewProjectionmatrixSprite;
	transformationMatrixDataSprite->World = worldMatrixSprite;

}

void Sprite::Draw()
{
	commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
	commandList->IASetIndexBuffer(&indexBufferViewSprite);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
	commandList->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());

	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::Create(Texture texture_, Vector2 size)
{
	texture = texture_;

	//頂点バッファに書き込む頂点データ
	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

	// サイズを反映した頂点座標（左下原点の例）
	vertexDataSprite[0].position = { 0.0f, size.y, 0.0f, 1.0f }; // 左下
	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };   // 左上
	vertexDataSprite[2].position = { size.x, size.y, 0.0f, 1.0f }; // 右下
	vertexDataSprite[3].position = { size.x, 0.0f, 0.0f, 1.0f };   // 右上

	// UV座標はそのまま
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };
	vertexDataSprite[3].texcoord = { 1.0f, 0.0f };

	vertexResourceSprite->Unmap(0, nullptr);

	// テクスチャのSRVハンドル準備などがあればここで行う

	// transformSpriteのscaleなど初期化するならここでもOK
	transformSprite.scale = { 1.0f, 1.0f, 1.0f };
}
