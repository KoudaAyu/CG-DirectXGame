#include"ParticleManager.h"

void ParticleManager::Initialize(DirectXCom* dxCommon, Camera* camera)
{
	this->dxCommon = dxCommon;
	this->camera = camera;
	RootSignature();
	CreateResource(); 
	CreateSRV();


	for (uint32_t index = 0; index < kNumInstance; ++index)
	{
		particleTransform[index].scale = { 1.0f,1.0f,1.0f };
		particleTransform[index].rotate = { 0.0f,0.0f,0.0f };
		particleTransform[index].translate = { index * 0.1f,index * 0.1f,index * 0.1f };

	}

	
}

void ParticleManager::Update()
{
	Matrix4x4 viewProjectionMatrix = MakeIdentity4x4();
	if (camera)
	{
		viewProjectionMatrix = camera->GetViewProjectionMatrix();
	}

	for (uint32_t index = 0; index < kNumInstance; ++index)
	{
		Matrix4x4 worldMatrix =
			MakeAffineMatrix(particleTransform[index].scale, particleTransform[index].rotate, particleTransform[index].translate);
		Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
		if (instancingData)
		{
			instancingData[index].WVP = worldViewProjectionMatrix;
			instancingData[index].World = worldMatrix;
		}
	}
}

void Draw()
{
	/*dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU);*/
}

void ParticleManager::RootSignature()
{
	
	descriptorRangeForInstancing[0].BaseShaderRegister = 0; //0から始まる
	descriptorRangeForInstancing[0].NumDescriptors = 1; //数は1
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; //SEVを使う
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; //DescriptorTableを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; //VertexShaderで使う
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing; //Tableの中身の配列を指定
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing); //Tableで利用
}

void ParticleManager::CreateResource()
{
	instancingResource =
		dxCommon->CreateBufferResource(dxCommon->GetDevice(), sizeof(TransformationMatrix) * kNumInstance);
	
	instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&instancingData));

	for (uint32_t index = 0; index < kNumInstance; ++index)
	{
		instancingData[index].WVP = MakeIdentity4x4();
		instancingData[index].World = MakeIdentity4x4();
	}
}

void ParticleManager::CreateSRV()
{
	
	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = kNumInstance;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(TransformationMatrix);
	instancingSrvHandleCPU = dxCommon->GetCPUDescroptirHandle(dxCommon->GetSrvDescriptorHeap(), dxCommon->GetDescriptorSizeSRV(), 3);
	instancingSrvHandleGPU = dxCommon->GetGPUDescriptorHandle(dxCommon->GetSrvDescriptorHeap(), dxCommon->GetDescriptorSizeSRV(), 3);
	dxCommon->GetDevice()->CreateShaderResourceView(instancingResource.Get(), &instancingSrvDesc, instancingSrvHandleCPU);
}


