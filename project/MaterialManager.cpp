#include "MaterialManager.h"
#include"DirectXCom.h"
#include "GpuResourceUtils.h"

void  MaterialManager::Initialize(DirectXCom* directXCom)
{
	this->directXCom = directXCom;
	materialResource = GpuResourceUtils::CreateMappedBuffer(directXCom, materialData);
	GpuResourceUtils::InitializeMaterial(materialData);
}
