
#include "Sphere.h"

Sphere::Sphere()
{
}

Sphere::~Sphere()
{
	delete[] vertexData;
}

void Sphere::Initialize(const Microsoft::WRL::ComPtr<ID3D12Device>& device,Buffer buffer)
{
	vertexData = new VertexData[kVertexCount];

	// --- 頂点データを埋める ---  
	for (uint32_t lat = 0; lat <= kSubdivision; ++lat)
	{
		float theta = -DirectX::XM_PIDIV2 + DirectX::XM_PI * (float(lat) / kSubdivision);
		for (uint32_t lon = 0; lon <= kSubdivision; ++lon)
		{
			float phi = DirectX::XM_2PI * (float(lon) / kSubdivision);
			uint32_t idx = lat * (kSubdivision + 1) + lon;

			if (idx >= kVertexCount)
			{
				continue;
			}

			vertexData[idx].position.x = static_cast<float>(cos(theta) * cos(phi));
			vertexData[idx].position.y = static_cast<float>(sin(theta));
			vertexData[idx].position.z = static_cast<float>(cos(theta) * sin(phi));
			vertexData[idx].position.w = 1.0f;

			vertexData[idx].texcoord.x = float(lon) / kSubdivision;
			vertexData[idx].texcoord.y = 1.0f - float(lat) / kSubdivision;

			vertexData[idx].normal = {
				vertexData[idx].position.x,
				vertexData[idx].position.y,
				vertexData[idx].position.z
			};
		}
	}

	vertexResourceSphere = buffer.CreateBufferResource(device.Get(), sizeof(VertexData) * kVertexCount);
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertex));
	memcpy(mappedVertex, vertexData, sizeof(VertexData) * kVertexCount);
	vertexResourceSphere->Unmap(0, nullptr);

	uint32_t* indexData = new uint32_t[kIndexCount];
	uint32_t idx = 0;

	// Initialize indexData to avoid uninitialized memory usage
	memset(indexData, 0, sizeof(uint32_t) * kIndexCount);

	for (uint32_t lat = 0; lat < kSubdivision; ++lat)
	{
		for (uint32_t lon = 0; lon < kSubdivision; ++lon)
		{
			uint32_t v0 = lat * (kSubdivision + 1) + lon;
			uint32_t v1 = v0 + 1;
			uint32_t v2 = v0 + (kSubdivision + 1);
			uint32_t v3 = v2 + 1;

			if (idx + 6 > kIndexCount)
			{
				continue;
			}

			indexData[idx++] = v0;
			indexData[idx++] = v2;
			indexData[idx++] = v1;

			indexData[idx++] = v2;
			indexData[idx++] = v3;
			indexData[idx++] = v1;
		}
	}

	indexResourceSphere = buffer.CreateBufferResource(device.Get(), sizeof(uint32_t) * kIndexCount);
	indexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndex));
	memcpy(mappedIndex, indexData, sizeof(uint32_t) * kIndexCount);
	indexResourceSphere->Unmap(0, nullptr);

	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();
	vertexBufferViewSphere.SizeInBytes = sizeof(VertexData) * kVertexCount;
	vertexBufferViewSphere.StrideInBytes = sizeof(VertexData);

	indexBufferViewSphere.BufferLocation = indexResourceSphere->GetGPUVirtualAddress();
	indexBufferViewSphere.SizeInBytes = sizeof(uint32_t) * kIndexCount;
	indexBufferViewSphere.Format = DXGI_FORMAT_R32_UINT;

	delete[] indexData; // Free allocated memory

	CreateMapping();

	transformationMatrixResourceSphere = buffer.CreateBufferResource(device.Get(), sizeof(TransformationMatrix));

	
	transformationMatrixResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSphere));
	transformationMatrixDataSphere->WVP = MakeIdentity4x4();
	transformationMatrixDataSphere->World = MakeIdentity4x4();
	// 書き込みが完了したので、マップを解除
	transformationMatrixResourceSphere->Unmap(0, nullptr);
}

void Sphere::CreateMapping()
{
	
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	memcpy(mapped, vertexData, sizeof(VertexData) * kVertexCount);
	vertexResourceSphere->Unmap(0, nullptr);
}
