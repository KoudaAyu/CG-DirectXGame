#include"Sprite.h"
#include"SpriteCom.h"
#include"TextureManager.h"
#include<cassert>



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
	
	//Sprite用のTransformationMatrix用のリソースを作る
	transformationMatrixResourceSprite = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(TransformationMatrix));
	// データを書き込むためにMapする。TransformationMatrixは毎フレーム更新されるため、ここでも永続的にMapしておく。
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	// 単位行列を書き込んでおく
	transformationMatrixDataSprite->WVP = MakeIdentity4x4();
	transformationMatrixDataSprite->World = MakeIdentity4x4();
	
	uint32_t index = TextureManager::GetInstance()->Load(textureFilePath);

	assert(index != TextureManager::kInvalidTextureIndex);

	textureIndex = index;

	textureHandleGPU = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex);
	
	AdjustTextureSize();
}

Sprite* Sprite::Create(SpriteCom* spriteCom, uint32_t textureHandle, const Vector2& position)
{
	Sprite* sprite = new Sprite();
	// 既存 Initialize とほぼ同じ初期化を行うが、テクスチャはインデックスから設定する
	sprite->spriteCom = spriteCom;
	assert(spriteCom);
	sprite->dxCommon = spriteCom->GetDxCommon();
	assert(sprite->dxCommon);

	sprite->CreateVertexBufferView();
	sprite->CreateIndexBufferView();
	sprite->CreateVertexData();
	sprite->ReflectionProcessing();
	sprite->CreateIndexData();

	sprite->materialResourceSprite = sprite->dxCommon->CreateBufferResource(sprite->dxCommon->GetDevice().Get(), sizeof(Material));
	sprite->materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&sprite->materialData));
	sprite->materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	sprite->materialData->enableLighting = false;
	sprite->materialData->uvTransform = MakeIdentity4x4();

	sprite->transformationMatrixResourceSprite = sprite->dxCommon->CreateBufferResource(sprite->dxCommon->GetDevice().Get(), sizeof(TransformationMatrix));
	sprite->transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&sprite->transformationMatrixDataSprite));
	sprite->transformationMatrixDataSprite->WVP = MakeIdentity4x4();
	sprite->transformationMatrixDataSprite->World = MakeIdentity4x4();

	// テクスチャハンドルを直接設定
	sprite->textureHandleGPU = TextureManager::GetInstance()->GetSrvHandleGPU(textureHandle);
	sprite->textureIndex = textureHandle;

	sprite->SetPosition(position);
	sprite->AdjustTextureSize();
	return sprite;
}

void Sprite::Update(WindowAPI* windowAPI, DebugCamera* debugCamera_)
{

	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetadata(textureIndex);

	float tex_left = textureLeftTop.x / static_cast<float>(metadata.width);
	float tex_right = (textureLeftTop.x + textureSize.x) / static_cast<float>(metadata.width);
	float tex_top = textureLeftTop.y / static_cast<float>(metadata.height);
	float tex_bottom = (textureLeftTop.y + textureSize.y) / static_cast<float>(metadata.height);

	vertexData[0].texcoord = { tex_left,tex_bottom }; // 左下
	vertexData[1].texcoord = { tex_left,tex_top };   // 左上
	vertexData[2].texcoord = { tex_right,tex_bottom }; // 右下
	vertexData[3].texcoord = { tex_right,tex_top };   // 右上

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
	Matrix4x4 worldViewProjectionmatrixSprite = Multiply(worldMatrixSprite, Multiply(debugCamera_->GetViewMatrix(), projectionMatrixSprite));
	transformationMatrixDataSprite->WVP = worldViewProjectionmatrixSprite; 
	transformationMatrixDataSprite->World = worldMatrixSprite;

	if (uvDirty)
	{
		RecalculateUVMatrix();
		uvDirty = false;
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
		dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureHandleGPU);
	}
	if (directionalLightResource) {
		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
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


void Sprite::SetUVParams(const Vector3& scale, float rotZ, const Vector3& translate)
{
	uvParams.scale = scale;
	uvParams.rotate.z = rotZ; 
	uvParams.translate = translate;
	uvDirty = true;
}


void Sprite::RecalculateUVMatrix()
{
	if (!materialData) return;

	Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvParams.scale);
	uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvParams.rotate.z));
	uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvParams.translate));

	materialData->uvTransform = uvTransformMatrix;
}
