#include"Sprite.h"
#include"SpriteCom.h"
#include"TextureManager.h"
#include<cassert>

// Instance data layout used for per-sprite instance SRV
struct InstanceDataLayout {
    Matrix4x4 WVP;
    Matrix4x4 World;
    float alpha;
    float padding[3];
};



void Sprite::Initialize(SpriteCom* spriteCom, std::string textureFilePath)
{
	this->spriteCom = spriteCom;
	assert(spriteCom);
	this->dxCommon = spriteCom->GetDxCommon();
	assert(dxCommon);


	CreateVertexBufferView();
	CreateIndexBufferView();

	CreateVertexData();
	ReflectionProcessing();
	CreateIndexData();

	materialResourceSprite = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(Material));
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白（テクスチャ色をそのまま出す用）
	materialData->enableLighting = false;
	materialData->uvTransform = MakeIdentity4x4();
	// Note: keep mapped for lifetime so caller can update directly
	// materialResourceSprite->Unmap(0, nullptr);

	//Sprite用のTransformationMatrix用のリソースを作る
	transformationMatrixResourceSprite = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(TransformationMatrix));
	//データを書き込む
	//書き込むためのアドレス取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	//単位行列を書き込んでおく
	transformationMatrixDataSprite->WVP = MakeIdentity4x4();
	transformationMatrixDataSprite->World = MakeIdentity4x4();
	
	textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);

	AdjustTextureSize();

	// NOTE: Do not create a per-sprite instance SRV here. The application (main) creates a shared
	// instance buffer SRV at descriptor slot 4 for instanced draws. Creating another SRV at the
	// same slot here overwrote that SRV and reduced instance count to 1.
	// Previous code created instanceBufferResourceSprite and an SRV at index 4; it has been removed.

	// After AdjustTextureSize(); create per-sprite instance buffer and SRV at a safe index
	// Use a descriptor index unlikely used by others
	const uint32_t kSpriteInstanceSrvIndex = 30;
	instanceBufferResourceSprite = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(InstanceDataLayout));
	// Map persistently
	instanceBufferResourceSprite->Map(0, nullptr, &instanceDataPtr);
	InstanceDataLayout* instPtr = reinterpret_cast<InstanceDataLayout*>(instanceDataPtr);
	instPtr->WVP = MakeIdentity4x4();
	instPtr->World = MakeIdentity4x4();
	instPtr->alpha = 1.0f;

	// Create SRV at a non-conflicting descriptor slot
	D3D12_SHADER_RESOURCE_VIEW_DESC instSrvDesc{};
	instSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instSrvDesc.Buffer.FirstElement = 0;
	instSrvDesc.Buffer.NumElements = 1;
	instSrvDesc.Buffer.StructureByteStride = sizeof(InstanceDataLayout);

	D3D12_CPU_DESCRIPTOR_HANDLE instCpu = dxCommon->GetCPUDescroptirHandle(dxCommon->GetSrvDescriptorHeap(), dxCommon->GetDescriptorSizeSRV(), kSpriteInstanceSrvIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE instGpu = dxCommon->GetGPUDescriptorHandle(dxCommon->GetSrvDescriptorHeap(), dxCommon->GetDescriptorSizeSRV(), kSpriteInstanceSrvIndex);
	dxCommon->GetDevice()->CreateShaderResourceView(instanceBufferResourceSprite.Get(), &instSrvDesc, instCpu);
	instanceSrvHandleGPU = instGpu;
}

void Sprite::Update(WindowAPI* windowAPI, DebugCamera* debugCamera_)
{

	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetadata(textureIndex);

	float tex_left = textureLeftTop.x / static_cast<float>(metadata.width);
	float tex_right = (textureLeftTop.x + textureSize.x) / static_cast<float>(metadata.width);
	float tex_top = textureLeftTop.y / static_cast<float>(metadata.height);
	float tex_bottom = (textureLeftTop.y + textureSize.y) / static_cast<float>(metadata.height);

	vertexData[0].texcoord = { tex_left, tex_bottom }; // 左下
	vertexData[1].texcoord = { tex_left, tex_top };   // 左上
	vertexData[2].texcoord = { tex_right, tex_bottom }; // 右下
	vertexData[3].texcoord = { tex_right, tex_top };   // 右上

	// If no camera passed -> UI / screen-space. Place vertices in pixel coordinates.
	if (debugCamera_ == nullptr)
	{
		// Anchor point is fraction [0..1] where (0,0) is top-left of sprite.
		float left = position.x - anchorPoint.x * size.x;
		float top = position.y - anchorPoint.y * size.y;
		float right = left + size.x;
		float bottom = top + size.y;

		// Note: In this coordinate system Y increases downward (screen coords)
		vertexData[0].position = { left, bottom, 0.0f, 1.0f }; // 左下
		vertexData[1].position = { left, top, 0.0f, 1.0f };   // 左上
		vertexData[2].position = { right, bottom, 0.0f, 1.0f }; // 右下
		vertexData[3].position = { right, top, 0.0f, 1.0f };   // 右上

		// For UI, compute orthographic projection that maps pixel coords to clip space
		Matrix4x4 worldMatrixSprite = MakeIdentity4x4();
		Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
		Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(windowAPI->GetClientWidth()), float(windowAPI->GetClientHeight()), 0.0f, 100.0f);

		Matrix4x4 worldViewProjectionmatrixSprite = Multiply(projectionMatrixSprite, Multiply(viewMatrixSprite, worldMatrixSprite));
		transformationMatrixDataSprite->WVP = worldViewProjectionmatrixSprite;
		transformationMatrixDataSprite->World = worldMatrixSprite;

		// Debug output will show WVP translation reflecting pixel coords
		char dbg[128];
		sprintf_s(dbg, "UI WVP trans=(%.2f, %.2f, %.2f)\n",
			transformationMatrixDataSprite->WVP.m[3][0],
			transformationMatrixDataSprite->WVP.m[3][1],
			transformationMatrixDataSprite->WVP.m[3][2]);
		OutputDebugStringA(dbg);

		return;
	}

	// -- world-space sprites (use camera) --
	float left = 0.0f - anchorPoint.x;
	float right = 1.0f - anchorPoint.x;
	float top = 0.0f - anchorPoint.y;
	float bottom = 1.0f - anchorPoint.y;

	vertexData[0].position = { left,bottom,0.0f,1.0f }; // 左下
	vertexData[1].position = { left,top,0.0f,1.0f };   // 左上
	vertexData[2].position = { right,bottom,0.0f,1.0f }; // 右下
	vertexData[3].position = { right,top,0.0f,1.0f };   // 右上

	//spriteの座標、回転、拡縮関係
	transform.translate = { position.x, position.y, 0.0f };
	transform.rotate = { 0.0f,0.0f,rotation };
	transform.scale = { size.x, size.y,1.0f };


	Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
	Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(windowAPI->GetClientWidth()), float(windowAPI->GetClientHeight()), 0.0f, 100.0f);

	// If a debug camera is supplied, use its view matrix
	const Matrix4x4& usedView = debugCamera_->GetViewMatrix();

	// Correct transform order: projection * view * world
	Matrix4x4 worldViewProjectionmatrixSprite = Multiply(projectionMatrixSprite, Multiply(usedView, worldMatrixSprite));
	transformationMatrixDataSprite->WVP = worldViewProjectionmatrixSprite; 
	transformationMatrixDataSprite->World = worldMatrixSprite;

	// Debug: print WVP translation component
	{
		char dbg[128];
		sprintf_s(dbg, "WVP trans = (%.2f, %.2f, %.2f)\\n",
			transformationMatrixDataSprite->WVP.m[3][0],
			transformationMatrixDataSprite->WVP.m[3][1],
			transformationMatrixDataSprite->WVP.m[3][2]);
		OutputDebugStringA(dbg);
	}

	// Write instance data so Particle.VS can use it when drawing this sprite
	if (instanceDataPtr)
	{
		InstanceDataLayout* inst = reinterpret_cast<InstanceDataLayout*>(instanceDataPtr);
		inst->WVP = transformationMatrixDataSprite->WVP;
		inst->World = transformationMatrixDataSprite->World;
		inst->alpha = 1.0f;
	}
}

void Sprite::Draw()
{
	
	dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
	dxCommon->GetCommandList()->IASetIndexBuffer(&indexBufferViewSprite);
	dxCommon->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
	if (textureHandleGPU.ptr != 0) {
		// Use the explicitly assigned GPU descriptor handle when available
		dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureHandleGPU);
	}
	else {
		// Fallback: use TextureManager based on texture index
		dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex));
	}
	if (directionalLightResource) {
		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
	}

	// Ensure instance SRV is bound so Particle.VS reads this sprite's transform (t0)
	if (instanceSrvHandleGPU.ptr != 0)
	{
		dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(4, instanceSrvHandleGPU);
	}
	
	// draw quad
	dxCommon->GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::CreateIndexBufferView()
{
	indexResourceSprite = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(uint32_t) * 6);
	//頂点バッファービューを生成する
	//リソースの先頭アドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;
}

void Sprite::CreateVertexBufferView()
{
	vertexResourceSprite = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(VertexData) * 6);
	//頂点バッファビューを生成する
	//リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
	//1頂点当たりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);
}

void Sprite::CreateVertexData()
{
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	vertexData[0].position = { 0.0f,360.0f,0.0f,1.0f }; // 左下
	vertexData[1].position = { 0.0f,0.0f,0.0f,1.0f };   // 左上
	vertexData[2].position = { 640.0f,360.0f,0.0f,1.0f }; // 右下
	vertexData[3].position = { 640.0f,0.0f,0.0f,1.0f };   // 右上

	vertexData[0].texcoord = { 0.0f,1.0f };
	vertexData[1].texcoord = { 0.0f,0.0f };
	vertexData[2].texcoord = { 1.0f,1.0f };
	vertexData[3].texcoord = { 1.0f,0.0f };
}

void Sprite::CreateIndexData()
{
	//インデックスリソースにデータを書き込む
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0; // 左下
	indexDataSprite[1] = 1; // 左上
	indexDataSprite[2] = 2; // 右下
	indexDataSprite[3] = 2; // 右下
	indexDataSprite[4] = 1; // 左上
	indexDataSprite[5] = 3; // 右上

	indexResourceSprite->Unmap(0, nullptr);
}

void Sprite::ReflectionProcessing()
{
	//頂点リソースにデータを書き込む
	//左下
	vertexData[0].position = { 0.0f,1.0f,0.0f,1.0f };
	vertexData[0].texcoord = { 0.0f,1.0f };
	vertexData[0].normal = { 0.0f,0.0f,-1.0f };
	//左上
	vertexData[1].position = { 0.0f,0.0f,0.0f,1.0f };
	vertexData[1].texcoord = { 0.0f,0.0f };
	vertexData[1].normal = { 0.0f,0.0f,-1.0f };
	//右下
	vertexData[2].position = { 1.0f,1.0f,0.0f,1.0f };
	vertexData[2].texcoord = { 1.0f,1.0f };
	vertexData[2].normal = { 0.0f,0.0f,-1.0f };
	//右上
	vertexData[3].position = { 1.0f,0.0f,0.0f,1.0f };
	vertexData[3].texcoord = { 1.0f,0.0f };
	vertexData[3].normal = { 0.0f,0.0f,-1.0f };


}

void Sprite::AdjustTextureSize()
{
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetadata(textureIndex);
	
	textureSize.x = static_cast<float>(metadata.width);
	textureSize.y = static_cast<float>(metadata.height);

	size = textureSize;
}
