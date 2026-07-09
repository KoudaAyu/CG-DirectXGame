#include "Sphere.h"
#include"Object3dCom.h"
#include"MaterialManager.h"
#include"Camera.h"
#include"Light.h"
#include "SceneManager.h"
#include "TextureManager.h"

#include <algorithm>

std::vector<Sphere*> Sphere::instances_;

Sphere::Sphere()
{
	instances_.push_back(this);
}

Sphere::~Sphere()
{
	auto it = std::find(instances_.begin(), instances_.end(), this);
	if (it != instances_.end())
	{
		instances_.erase(it);
	}

	if (transformationMatrixResourceSphere && transformationMatrixDataSphere)
	{
		D3D12_RANGE written = { 0, sizeof(TransformationMatrix) };
		transformationMatrixResourceSphere->Unmap(0, &written);
		transformationMatrixDataSphere = nullptr;
	}
}

void Sphere::Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom, MaterialManager* materialManager, Light* light, Camera* camera)
{
	directXCom_ = dxCommon;
	assert(directXCom_ != nullptr);

	object3dCom_ = object3dCom;
	assert(object3dCom_ != nullptr);

	materialManager_ = materialManager;
	assert(materialManager_ != nullptr);

	light_ = light;
	assert(light_ != nullptr);

	camera_ = camera;
	assert(camera_ != nullptr);

	// --- 頂点データを埋める ---
	for (uint32_t lat = 0; lat <= kSubdivision; ++lat)
	{
		// 緯度 (theta): -π/2 (下端) から π/2 (上端) まで
		float theta = -DirectX::XM_PIDIV2 + DirectX::XM_PI * (float(lat) / kSubdivision);
		for (uint32_t lon = 0; lon <= kSubdivision; ++lon)
		{
			// 経度 (phi): 0 (東端) から 2π (一周) まで
			float phi = DirectX::XM_2PI * (float(lon) / kSubdivision);
			uint32_t idx = lat * (kSubdivision + 1) + lon; // 1次元配列内のインデックス

			// 球面座標からデカルト座標への変換
			vertexData[idx].position.x = cos(theta) * cos(phi);
			vertexData[idx].position.y = sin(theta);
			vertexData[idx].position.z = cos(theta) * sin(phi);
			vertexData[idx].position.w = 1.0f; // 同次座標

			// テクスチャ座標 (UV)
			// U: 経度に比例 (0.0 から 1.0)
			vertexData[idx].texcoord.x = float(lon) / kSubdivision;
			if (lat == 0 || lat == kSubdivision)
			{
				vertexData[idx].texcoord.x = 0.5f;
			}
			// V: 緯度に比例 (1.0 から 0.0、上向きが正になるように反転)
			vertexData[idx].texcoord.y = 1.0f - float(lat) / kSubdivision;

			// 法線ベクトル (原点から頂点へのベクトルがそのまま法線となる)
			vertexData[idx].normal = {
				vertexData[idx].position.x,
				vertexData[idx].position.y,
				vertexData[idx].position.z
			};
		}
	}

	// --- 頂点バッファを作成・アップロード ---
	vertexResourceSphere = directXCom_->CreateBufferResource(directXCom_->GetDevice().Get(), sizeof(Sprite::VertexData) * kVertexCount);
	Sprite::VertexData* mappedVertex = nullptr;
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertex));
	memcpy(mappedVertex, vertexData.data(), sizeof(Sprite::VertexData) * kVertexCount);
	vertexResourceSphere->Unmap(0, nullptr);

	std::vector<uint32_t> indexData;
	indexData.resize(kIndexCount);
	uint32_t idx = 0; // ここを元のままの変数名に戻しました

	for (uint32_t lat = 0; lat < kSubdivision; ++lat)
	{
		for (uint32_t lon = 0; lon < kSubdivision; ++lon)
		{

			uint32_t v0 = lat * (kSubdivision + 1) + lon;             // 左上 (A)
			uint32_t v1 = v0 + 1;                                      // 右上 (C)
			uint32_t v2 = v0 + (kSubdivision + 1);                     // 左下 (B)
			uint32_t v3 = v2 + 1;                                      // 右下 (D)

			// 四角形を2つの三角形で表現する
			// 1つ目の三角形: v0, v2, v1 (A, B, C)
			// DirectXでは通常、右手座標系で反時計回り（CCW）が表
			indexData[idx++] = v0; // A
			indexData[idx++] = v2; // B
			indexData[idx++] = v1; // C

			// 2つ目の三角形: v2, v3, v1 (B, D, C)
			indexData[idx++] = v2; // B
			indexData[idx++] = v3; // D
			indexData[idx++] = v1; // C
		}
	}

	// --- インデックスバッファを作成・アップロード ---
	indexResourceSphere = directXCom_->CreateBufferResource(directXCom_->GetDevice().Get(), sizeof(uint32_t) * kIndexCount);
	uint32_t* mappedIndex = nullptr;
	indexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndex));
	memcpy(mappedIndex, indexData.data(), sizeof(uint32_t) * kIndexCount);
	indexResourceSphere->Unmap(0, nullptr);

	// --- バッファビュー設定 ---

	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();
	vertexBufferViewSphere.SizeInBytes = sizeof(Sprite::VertexData) * kVertexCount;
	vertexBufferViewSphere.StrideInBytes = sizeof(Sprite::VertexData);


	indexBufferViewSphere.BufferLocation = indexResourceSphere->GetGPUVirtualAddress();
	indexBufferViewSphere.SizeInBytes = sizeof(uint32_t) * kIndexCount;
	indexBufferViewSphere.Format = DXGI_FORMAT_R32_UINT;


	//WVP用のリソースを作る。　Matrix4x4 1つのサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = directXCom_->CreateBufferResource(directXCom_->GetDevice().Get(), sizeof(TransformationMatrix));
	//データを書き込む
	TransformationMatrix* wvpData = nullptr;
	//書き込む為のアドレス取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	//単位行列を書き込む
	wvpData->World = MakeIdentity4x4();
	wvpData->WVP = MakeIdentity4x4();

	transformationMatrixResourceSphere = directXCom_->CreateBufferResource(directXCom_->GetDevice().Get(), sizeof(TransformationMatrix));

	// データを書き込むためのポインタを取得

	transformationMatrixResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSphere));
	transformationMatrixDataSphere->WVP = MakeIdentity4x4();
	transformationMatrixDataSphere->World = MakeIdentity4x4();


}

void Sphere::Update()
{
	transform.rotate.y += 0.01f; // Y軸を中心に回転させる
	worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	viewMatrix = Inverse(camera_->GetWorldMatrix());
	WVPMatrix = Multiply(worldMatrix, Multiply(viewMatrix, camera_->GetProjectionMatrix()));
	transformationMatrixDataSphere->WVP = WVPMatrix;
	transformationMatrixDataSphere->World = worldMatrix;
	transformationMatrixDataSphere->WorldInverseTranspose = Transpose(Inverse(worldMatrix));
}

void Sphere::Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle)
{

	if (!directXCom_ || !object3dCom_ || !materialManager_ || !light_)
	{
		return;
	}

	ID3D12GraphicsCommandList* commandList = directXCom_->GetCommandList().Get();
	if (!commandList)
	{
		return;
	}

	commandList->RSSetViewports(1, &directXCom_->GetViewport());
	commandList->RSSetScissorRects(1, &directXCom_->GetScissorRect());

  commandList->SetGraphicsRootSignature(object3dCom_->GetRootSignature().Get());
	const auto& pipelineState = overlayDraw_ ? object3dCom_->GetOverlayPipelineState() : object3dCom_->GetPipelineState();
	commandList->SetPipelineState(pipelineState.Get());
	commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);
	commandList->IASetIndexBuffer(&indexBufferViewSphere);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->SetGraphicsRootConstantBufferView(0, materialManager_->GetMaterialResource()->GetGPUVirtualAddress());

	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSphere->GetGPUVirtualAddress());

	if (textureSrvHandle.ptr == 0)
	{
		return;
	}

	commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);

	commandList->SetGraphicsRootConstantBufferView(3, light_->GetDirectionalLightResource()->GetGPUVirtualAddress());

	if (camera_ && camera_->GetCameraResource())
	{
		commandList->SetGraphicsRootConstantBufferView(4, camera_->GetCameraResource()->GetGPUVirtualAddress());
	}
	else
	{
		commandList->SetGraphicsRootConstantBufferView(4, 0);
	}

	uint32_t skyboxIndex = SceneManager::GetInstance()->GetSkyboxTextureIndex();
	D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle{};
	if (skyboxIndex != TextureManager::kInvalidTextureIndex)
	{
		skyboxHandle = TextureManager::GetInstance()->GetSrvHandleGPU(skyboxIndex);
	}
	else
	{
		skyboxHandle = textureSrvHandle;
	}
	if (skyboxHandle.ptr != 0)
	{
		commandList->SetGraphicsRootDescriptorTable(5, skyboxHandle);
	}

	commandList->DrawIndexedInstanced(GetIndexCount(), 1, 0, 0, 0);
}

