#include "MaterialManager.h"
#include"DirectXCom.h"

void  MaterialManager::Initialize(DirectXCom* directXCom)
{
	this->directXCom = directXCom;
	//マテリアル用のリソースを作る
	materialResource = directXCom->CreateBufferResource(directXCom->GetDevice().Get(), sizeof(Material));
	//マテリアルにデータを書き込む

	//書き込む為のアドレス取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

	Vector4 temp{};
	temp.x = 1.0f;
	temp.y = 1.0f;
	temp.z = 1.0f;
	temp.w = 1.0f;
	materialData->color = temp;
	materialData->enableLighting = false;
    materialData->specularModel = 0; // デフォルトは Blinn-Phong
    materialData->shininess = 16.0f;
    materialData->environmentCoefficient = 0.2f;
    //uvTransform行列の初期化
    materialData->uvTransform = MakeIdentity4x4();
    materialResource->Unmap(0, nullptr);
}
