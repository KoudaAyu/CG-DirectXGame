#include "UVTransform.h"

void UVTransform::Initialize(ID3D12Device* device)
{
	//マテリアル用のリソースを作る
	materialResource = Buffer::CreateBufferResource(device, sizeof(Material));
	
	//書き込む為のアドレス取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	
	temp.x = 1.0f;
	temp.y = 1.0f;
	temp.z = 1.0f;
	temp.w = 1.0f;

	materialData->color = temp;
	materialData->enableLighting = false;
	materialResource->Unmap(0, nullptr);

	materialResourceSprite = Buffer::CreateBufferResource(device, sizeof(Material));
	
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白（テクスチャ色をそのまま出す用）
	materialDataSprite->enableLighting = false;
	materialResourceSprite->Unmap(0, nullptr);

	//uvTrandform用の変数
	uvTransformSprite = {
	{1.0f, 1.0f, 1.0f},
	{0.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, 0.0f}
	};

	//uvTransform行列の初期化
	materialData->uvTransform = MakeIdentity4x4();
	materialDataSprite->uvTransform = MakeIdentity4x4();

	
}

void UVTransform::Update()
{
	//UVTransform用
	Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
	uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
	uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
	materialDataSprite->uvTransform = uvTransformMatrix;
}

void UVTransform::Draw(ID3D12GraphicsCommandList* commandList)
{
	commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
}

void UVTransform::DrawSprite(ID3D12GraphicsCommandList* commandList)
{
	commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
}
