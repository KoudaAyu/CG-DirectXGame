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

	//uvTransform行列の初期化
	materialData->uvTransform = MakeIdentity4x4();
}

void UVTransform::Draw(ID3D12GraphicsCommandList* commandList)
{
	commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
}
