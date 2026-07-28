#include "MaterialManager.h"
#include"DirectXCom.h"

void  MaterialManager::Initialize(DirectXCom* directXCom)
{
	this->directXCom = directXCom;
	//マテリアル用のリソースを作る
	materialResource = directXCom->CreateBufferResource(directXCom->GetDevice().Get(), sizeof(Material));
	//マテリアルにデータを書き込む

    // Initialize host-side material (used by UI)
    hostMaterial_.color = {1.0f, 1.0f, 1.0f, 1.0f};
    hostMaterial_.enableLighting = 0;
    hostMaterial_.specularModel = 0; // Blinn-Phong
    hostMaterial_.reflectionFactor = 0.5f;
    hostMaterial_.fresnelF0 = 0.04f;
    hostMaterial_.shininess = 16.0f;
    hostMaterial_.uvTransform = MakeIdentity4x4();

    // Mapしたら書き込みして Unmap -> 生ポインタを残さない
    Material* gpuPtr = nullptr;
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&gpuPtr));
    if (gpuPtr)
    {
        *gpuPtr = hostMaterial_;
        materialResource->Unmap(0, nullptr);
    }
}

void MaterialManager::Finalize()
{
    // materialData は Initialize() 内で Unmap() したなら nullptr にしておくべき
    materialData = nullptr;

    // GPUリソースを解放
    if (materialResource)
    {
        materialResource.Reset();
    }

    // 依存ポインタをクリア
    directXCom = nullptr;
}