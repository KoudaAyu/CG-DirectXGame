#include "SkyBox.h"
#include "Baziru3_Engine/Core/Base/Allocator/ConstantBufferAllocator.h"

#include <cassert>
#include <cstring>

SkyBox::~SkyBox()
{
}

void SkyBox::Initialize(DirectXCom* directXCom, Camera* camera)
{
	assert(directXCom);
	assert(camera);

	directXCom_ = directXCom;
	camera_ = camera;

	CreateVertexData();
	CreateIndexData();
	CreateBuffers();
	UpdateTransformationMatrix();
}

void SkyBox::Update()
{
	UpdateTransformationMatrix();
}

void SkyBox::CreateVertexData()
{
	for (auto& vertex : vertexData)
	{
		vertex.texcoord = { 0.0f, 0.0f };
		vertex.normal = { 0.0f, 0.0f, 0.0f };
	}

	vertexData[0].position = { 1.0f, 1.0f, 1.0f, 1.0f };
	vertexData[1].position = { 1.0f, 1.0f, -1.0f, 1.0f };
	vertexData[2].position = { 1.0f, -1.0f, 1.0f, 1.0f };
	vertexData[3].position = { 1.0f, -1.0f, -1.0f, 1.0f };

	vertexData[4].position = { -1.0f, 1.0f, -1.0f, 1.0f };
	vertexData[5].position = { -1.0f, 1.0f, 1.0f, 1.0f };
	vertexData[6].position = { -1.0f, -1.0f, -1.0f, 1.0f };
	vertexData[7].position = { -1.0f, -1.0f, 1.0f, 1.0f };

	vertexData[8].position = { -1.0f, 1.0f, 1.0f, 1.0f };
	vertexData[9].position = { 1.0f, 1.0f, 1.0f, 1.0f };
	vertexData[10].position = { -1.0f, -1.0f, 1.0f, 1.0f };
	vertexData[11].position = { 1.0f, -1.0f, 1.0f, 1.0f };

	vertexData[12].position = { 1.0f, 1.0f, -1.0f, 1.0f };
	vertexData[13].position = { -1.0f, 1.0f, -1.0f, 1.0f };
	vertexData[14].position = { 1.0f, -1.0f, -1.0f, 1.0f };
	vertexData[15].position = { -1.0f, -1.0f, -1.0f, 1.0f };

	vertexData[16].position = { -1.0f, 1.0f, -1.0f, 1.0f };
	vertexData[17].position = { 1.0f, 1.0f, -1.0f, 1.0f };
	vertexData[18].position = { -1.0f, 1.0f, 1.0f, 1.0f };
	vertexData[19].position = { 1.0f, 1.0f, 1.0f, 1.0f };

	vertexData[20].position = { -1.0f, -1.0f, 1.0f, 1.0f };
	vertexData[21].position = { 1.0f, -1.0f, 1.0f, 1.0f };
	vertexData[22].position = { -1.0f, -1.0f, -1.0f, 1.0f };
	vertexData[23].position = { 1.0f, -1.0f, -1.0f, 1.0f };
}

void SkyBox::CreateIndexData()
{
	indexData_ = {
		0, 1, 2, 2, 1, 3,
		4, 5, 6, 6, 5, 7,
		8, 9, 10, 10, 9, 11,
		12, 13, 14, 14, 13, 15,
		16, 17, 18, 18, 17, 19,
		20, 21, 22, 22, 21, 23
	};
}

void SkyBox::CreateBuffers()
{
	vertexResource_ = directXCom_->CreateBufferResource(directXCom_->GetDevice().Get(), sizeof(Sprite::VertexData) * vertexData.size());
	indexResource_ = directXCom_->CreateBufferResource(directXCom_->GetDevice().Get(), sizeof(uint32_t) * indexData_.size());

	Sprite::VertexData* mappedVertex = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertex));
	std::memcpy(mappedVertex, vertexData.data(), sizeof(Sprite::VertexData) * vertexData.size());
	vertexResource_->Unmap(0, nullptr);

	uint32_t* mappedIndex = nullptr;
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndex));
	std::memcpy(mappedIndex, indexData_.data(), sizeof(uint32_t) * indexData_.size());
	indexResource_->Unmap(0, nullptr);

	transformationMatrixData_.World = MakeIdentity4x4();
	transformationMatrixData_.WVP = MakeIdentity4x4();
	transformationMatrixData_.WorldInverseTranspose = MakeIdentity4x4();

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(Sprite::VertexData) * vertexData.size());
	vertexBufferView_.StrideInBytes = sizeof(Sprite::VertexData);

	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indexData_.size());
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
}

void SkyBox::UpdateTransformationMatrix()
{
	if (!camera_)
	{
		return;
	}

	Vector3 cameraPosition = camera_->GetWorldPosition();
	Matrix4x4 worldMatrix = MakeAffineMatrix(Vector3{ 50.0f, 50.0f, 50.0f }, Vector3{ 0.0f, 0.0f, 0.0f }, cameraPosition);
	Matrix4x4 wvpMatrix = Multiply(worldMatrix, Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix()));

	transformationMatrixData_.World = worldMatrix;
	transformationMatrixData_.WVP = wvpMatrix;
	transformationMatrixData_.WorldInverseTranspose = MakeIdentity4x4();
}

void SkyBox::Draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle)
{
	if (!commandList)
	{
		return;
	}

	auto* cbAllocator = directXCom_->GetCBAllocator();
	assert(cbAllocator);
	auto transAlloc = cbAllocator->Allocate(sizeof(TransformationMatrix));
	std::memcpy(transAlloc.cpuAddress, &transformationMatrixData_, sizeof(TransformationMatrix));

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(0, transAlloc.gpuAddress);
	commandList->SetGraphicsRootDescriptorTable(1, textureHandle);
	commandList->DrawIndexedInstanced(static_cast<UINT>(indexData_.size()), 1, 0, 0, 0);
}
