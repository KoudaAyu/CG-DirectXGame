#include"Sprite.h"
#include"SpriteCom.h"

#include<cassert>

void Sprite::Initialize(SpriteCom* spriteCom)
{
	this->spriteCom = spriteCom;
	assert(spriteCom);
	this->dxCommon = spriteCom->GetDxCommon();
	assert(dxCommon);
	

	CreateVertexBufferView();
	CreateIndexBufferView();

	CreateVertexData();
	CreateIndexData();

	materialResourceSprite = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(Material));
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白（テクスチャ色をそのまま出す用）
	materialDataSprite->enableLighting = false;
	materialDataSprite->uvTransform = MakeIdentity4x4();
	materialResourceSprite->Unmap(0, nullptr);

	//Sprite用のTransformationMatrix用のリソースを作る
	transformationMatrixResourceSprite = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(TransformationMatrix));
	//データを書き込む
	//書き込むためのアドレス取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	//単位行列を書き込んでおく
	transformationMatrixDataSprite->WVP = MakeIdentity4x4();
	transformationMatrixDataSprite->World = MakeIdentity4x4();
	transformationMatrixResourceSprite->Unmap(0, nullptr);
}

void Sprite::Update(WindowAPI* windowAPI, DebugCamera* debugCamera_)
{
	CreateVertexData();
	CreateIndexData();
	//Sprite用のworldViewProjectMatrix
	Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
	Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
	Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(windowAPI->GetClientWidth()), float(windowAPI->GetClientHeight()), 0.0f, 100.0f);
	Matrix4x4 worldViewProjectionmatrixSprite = Multiply(worldMatrixSprite, Multiply(debugCamera_->GetViewMatrix(), projectionMatrixSprite));
	transformationMatrixDataSprite->WVP = worldViewProjectionmatrixSprite; 
	transformationMatrixDataSprite->World = worldMatrixSprite;
}

void Sprite::Draw()
{
	CreateVertexBufferView();
	CreateIndexBufferView();
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());

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
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
	vertexDataSprite[0].position = { 0.0f,360.0f,0.0f,1.0f }; // 左下
	vertexDataSprite[1].position = { 0.0f,0.0f,0.0f,1.0f };   // 左上
	vertexDataSprite[2].position = { 640.0f,360.0f,0.0f,1.0f }; // 右下
	vertexDataSprite[3].position = { 640.0f,0.0f,0.0f,1.0f };   // 右上

	vertexDataSprite[0].texcoord = { 0.0f,1.0f };
	vertexDataSprite[1].texcoord = { 0.0f,0.0f };
	vertexDataSprite[2].texcoord = { 1.0f,1.0f };
	vertexDataSprite[3].texcoord = { 1.0f,0.0f };
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
