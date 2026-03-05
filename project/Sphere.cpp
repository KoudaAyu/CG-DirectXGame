#include "Sphere.h"

void Sphere::Initialize(DirectXCom* dxCommon)
{
	directXCom = dxCommon;
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
	vertexResourceSphere = directXCom->CreateBufferResource(directXCom->GetDevice().Get(), sizeof(Sprite::VertexData) * kVertexCount);
	Sprite::VertexData* mappedVertex = nullptr;
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertex));
	memcpy(mappedVertex, vertexData, sizeof(Sprite::VertexData) * kVertexCount);
	vertexResourceSphere->Unmap(0, nullptr);

	uint32_t* indexData = new uint32_t[kIndexCount];
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
	indexResourceSphere = directXCom->CreateBufferResource(directXCom->GetDevice().Get(), sizeof(uint32_t) * kIndexCount);
	uint32_t* mappedIndex = nullptr;
	indexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndex));
	memcpy(mappedIndex, indexData, sizeof(uint32_t) * kIndexCount);
	indexResourceSphere->Unmap(0, nullptr);

	// --- バッファビュー設定 ---
	
	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();
	vertexBufferViewSphere.SizeInBytes = sizeof(Sprite::VertexData) * kVertexCount;
	vertexBufferViewSphere.StrideInBytes = sizeof(Sprite::VertexData);

	
	indexBufferViewSphere.BufferLocation = indexResourceSphere->GetGPUVirtualAddress();
	indexBufferViewSphere.SizeInBytes = sizeof(uint32_t) * kIndexCount;
	indexBufferViewSphere.Format = DXGI_FORMAT_R32_UINT;

	
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	memcpy(mapped, vertexData, sizeof(Sprite::VertexData) * kVertexCount);
	vertexResourceSphere->Unmap(0, nullptr);

	//WVP用のリソースを作る。　Matrix4x4 1つのサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = directXCom->CreateBufferResource(directXCom->GetDevice().Get(), sizeof(TransformationMatrix));
	//データを書き込む
	TransformationMatrix* wvpData = nullptr;
	//書き込む為のアドレス取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	//単位行列を書き込む
	wvpData->World = MakeIdentity4x4();
	wvpData->WVP = MakeIdentity4x4();

	transformationMatrixResourceSphere = directXCom->CreateBufferResource(directXCom->GetDevice().Get(), sizeof(TransformationMatrix));

	// データを書き込むためのポインタを取得

	transformationMatrixResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSphere));
	transformationMatrixDataSphere->WVP = MakeIdentity4x4();
	transformationMatrixDataSphere->World = MakeIdentity4x4();
	

}

void Sphere::Update()
{
	// 球の更新処理（必要に応じて）
}

void Sphere::Draw()
{
	/*directXCom->GetCommandList()->RSSetViewports(1, &directXCom->GetViewport());
	directXCom->GetCommandList()->RSSetScissorRects(1, &directXCom->GetScissorRect());

	directXCom->GetCommandList()->SetGraphicsRootSignature(object3dCom->GetRootSignature().Get());
	directXCom->GetCommandList()->SetPipelineState(object3dCom->GetPipelineState().Get());
	directXCom->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);
	directXCom->GetCommandList()->IASetIndexBuffer(&indexBufferViewSphere);
	directXCom->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSphere->GetGPUVirtualAddress());
	directXCom->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);

	directXCom->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());

	directXCom->GetCommandList()->DrawIndexedInstanced(GetIndexCount(), 1, 0, 0, 0);*/
}