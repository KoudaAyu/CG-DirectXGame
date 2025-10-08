#include "Sprite.h"


void Sprite::Initialize(SpriteCom* spriteCom)
{
	this->spriteCom = spriteCom;
	DirectXCom* dxCommon = spriteCom->GetDxCommon();

	//Sprite用の頂点Resource
	vertexResourceSprite = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(Sprite::VertexData) * 6);
	//頂点バッファビューを生成する
	//リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(Sprite::VertexData) * 6;
	//1頂点当たりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(Sprite::VertexData);
	

	indexResourceSprite = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(uint32_t) * 6);
	//頂点バッファービューを生成する
	//リソースの先頭アドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;
}

void Sprite::Update()
{
}

void Sprite::Draw()
{
}
