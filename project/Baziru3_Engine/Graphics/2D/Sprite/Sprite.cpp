#include"Sprite.h"
#include"SpriteCom.h"
#include"TextureManager.h"
#include"Log.h"
#include "Baziru3_Engine/Core/Base/Allocator/ConstantBufferAllocator.h"
#include<cassert>
#include <iostream>



Sprite::Sprite()
{
}

std::unique_ptr<Sprite> Sprite::Create(SpriteCom* spriteCom, const Sprite::Transform& transform, const std::string& texturePath, TextureManager* textureManager)
{
    if (!spriteCom) return nullptr;

    TextureManager* tm = textureManager ? textureManager : TextureManager::GetInstance();
    uint32_t texIndex = tm->Load(texturePath);
    if (texIndex == TextureManager::kInvalidTextureIndex)
    {
        Logger::Log(std::string("Sprite::Create - failed to load texture: ") + texturePath);
        return nullptr;
    }

    Vector2 pos{ transform.translate.x, transform.translate.y };
    auto sp = Sprite::Create(spriteCom, texIndex, pos, tm);
    if (!sp) return nullptr;

    Vector2 baseSize = sp->GetSize();
    sp->SetSize({ baseSize.x * transform.scale.x, baseSize.y * transform.scale.y });
    sp->SetRotation(transform.rotate.z);

    return sp;
}

Sprite::~Sprite()
{
   
    Finalize();
}



void Sprite::Initialize(SpriteCom* spriteCom, const std::string& textureFilePath, TextureManager* textureManager)
{
	spriteCom_ = spriteCom;
	assert(spriteCom);
	dxCommon_ = spriteCom->GetDxCommon();
	assert(dxCommon_);

	textureManager_ = textureManager ? textureManager : TextureManager::GetInstance();

	CreateVertexBufferView();
	CreateIndexBufferView();

	CreateVertexData();
	ReflectionProcessing();
	CreateIndexData();

    materialData_.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白
    materialData_.enableLighting = false;
    materialData_.specularModel = 0;
    materialData_.shininess = 16.0f;
    materialData_.uvTransform = MakeIdentity4x4();
	
	// 単位行列を書き込んでおく
	transformationMatrixDataSprite_.WVP = MakeIdentity4x4();
	transformationMatrixDataSprite_.World = MakeIdentity4x4();
	
	uint32_t index = textureManager_->Load(textureFilePath);

	assert(index != TextureManager::kInvalidTextureIndex);

	textureIndex_ = index;

	textureHandleGPU_ = textureManager_->GetSrvHandleGPU(textureIndex_);
	
	AdjustTextureSize();
}

std::unique_ptr<Sprite> Sprite::Create(SpriteCom* spriteCom, uint32_t textureHandle, const Vector2& position, TextureManager* textureManager)
{
    if (!spriteCom) return nullptr;

    auto sp = std::make_unique<Sprite>();

   
    sp->spriteCom_ = spriteCom;
    sp->dxCommon_ = spriteCom->GetDxCommon();
    if (!sp->dxCommon_)
    {
        Logger::Log(std::cout, std::string("Sprite::Create - invalid DirectXCom pointer\n"));
        return nullptr;
    }

	// GPUリソースとビューの作成
    sp->CreateVertexBufferView();
    sp->CreateIndexBufferView();
    sp->CreateVertexData();
    sp->ReflectionProcessing();
    sp->CreateIndexData();

    // マテリアルリソース初期化
    sp->materialData_.color = { 1.0f,1.0f,1.0f,1.0f };
    sp->materialData_.enableLighting = false;
    sp->materialData_.specularModel = 0;
    sp->materialData_.shininess = 16.0f;
    sp->materialData_.uvTransform = MakeIdentity4x4();

	// TransformationMatrix初期化
    sp->transformationMatrixDataSprite_.WVP = MakeIdentity4x4();
    sp->transformationMatrixDataSprite_.World = MakeIdentity4x4();

	// Textureのセット
	sp->textureManager_ = textureManager ? textureManager : TextureManager::GetInstance();
    sp->textureIndex_ = textureHandle;
    sp->textureHandleGPU_ = sp->textureManager_->GetSrvHandleGPU(sp->textureIndex_);

	// positionのセット
    sp->SetPosition(position);

	// AnchorPointを中心にしている場合、テクスチャサイズに基づいてSpriteのサイズを調整
    sp->AdjustTextureSize();

    return sp;
}

void Sprite::Update()
{
	WindowAPI* windowAPI = dxCommon_ ? dxCommon_->GetWindowAPI() : nullptr;
	assert(windowAPI);

	const DirectX::TexMetadata& metadata = textureManager_->GetMetadata(textureIndex_);

	float tex_left = textureLeftTop_.x / static_cast<float>(metadata.width);
	float tex_right = (textureLeftTop_.x + textureSize_.x) / static_cast<float>(metadata.width);
	float tex_top = textureLeftTop_.y / static_cast<float>(metadata.height);
	float tex_bottom = (textureLeftTop_.y + textureSize_.y) / static_cast<float>(metadata.height);

	vertexData_[0].texcoord = { tex_left,tex_bottom }; // 左下
	vertexData_[1].texcoord = { tex_left,tex_top };   // 左上
	vertexData_[2].texcoord = { tex_right,tex_bottom }; // 右下
	vertexData_[3].texcoord = { tex_right,tex_top };   // 右上

	float left = 0.0f - anchorPoint_.x;
	float right = 1.0f - anchorPoint_.x;
	float top = 0.0f - anchorPoint_.y;
	float bottom = 1.0f - anchorPoint_.y;

	vertexData_[0].position = { left,bottom,0.0f,1.0f }; // 左下
	vertexData_[1].position = { left,top,0.0f,1.0f };   // 左上
	vertexData_[2].position = { right,bottom,0.0f,1.0f }; // 右下
	vertexData_[3].position = { right,top,0.0f,1.0f };   // 右上

	//spriteの座標、回転、拡縮関係
	transform_.translate = { position_.x, position_.y, 0.0f };
	transform_.rotate = { 0.0f,0.0f,rotation_ };
	transform_.scale = { size_.x, size_.y,1.0f };


	Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
	Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(windowAPI->GetClientWidth()), float(windowAPI->GetClientHeight()), 0.0f, 100.0f);
	Matrix4x4 worldViewProjectionmatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
	transformationMatrixDataSprite_.WVP = worldViewProjectionmatrixSprite; 
	transformationMatrixDataSprite_.World = worldMatrixSprite;

	if (uvDirty_)
	{
		RecalculateUVMatrix();
		uvDirty_ = false;
	}
}

void Sprite::Draw(ID3D12GraphicsCommandList* commandList)
{
	ID3D12GraphicsCommandList* cmdList = commandList ? commandList : dxCommon_->GetCommandList().Get();
	
	cmdList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite_);
	cmdList->IASetIndexBuffer(&indexBufferViewSprite_);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 定数バッファアロケーターから領域を切り出す
	auto* cbAllocator = dxCommon_->GetCBAllocator();
	assert(cbAllocator);

	auto matAlloc = cbAllocator->Allocate(sizeof(Material));
	std::memcpy(matAlloc.cpuAddress, &materialData_, sizeof(Material));

	auto transAlloc = cbAllocator->Allocate(sizeof(TransformationMatrix));
	std::memcpy(transAlloc.cpuAddress, &transformationMatrixDataSprite_, sizeof(TransformationMatrix));

	cmdList->SetGraphicsRootConstantBufferView(0, matAlloc.gpuAddress);
	cmdList->SetGraphicsRootConstantBufferView(1, transAlloc.gpuAddress);
	if (textureHandleGPU_.ptr != 0) {
		cmdList->SetGraphicsRootDescriptorTable(2, textureHandleGPU_);
	}
	if (directionalLightResource_) {
		cmdList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());
	}
	else {
		cmdList->SetGraphicsRootConstantBufferView(3, transAlloc.gpuAddress);
	}
	cmdList->SetGraphicsRootConstantBufferView(4, transAlloc.gpuAddress);

	// draw quad
	cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::Finalize()
{
   
    if (vertexMap_) {
        vertexMap_.releaseWithWrittenRange(sizeof(VertexData) * 6);
        vertexData_ = nullptr;
    }
	// indexResourceSprite_ は既に Unmap 済み
}

void Sprite::CreateIndexBufferView()
{
	indexResourceSprite_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), sizeof(uint32_t) * 6);
	//頂点バッファービューを生成する
	//リソースの先頭アドレスから使う
	indexBufferViewSprite_.BufferLocation = indexResourceSprite_->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点6つ分のサイズ
	indexBufferViewSprite_.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferViewSprite_.Format = DXGI_FORMAT_R32_UINT;
}

void Sprite::CreateVertexBufferView()
{
	vertexResourceSprite_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), sizeof(VertexData) * 6);
	//頂点バッファビューを生成する
	//リソースの先頭のアドレスから使う
	vertexBufferViewSprite_.BufferLocation = vertexResourceSprite_->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite_.SizeInBytes = sizeof(VertexData) * 6;
	//1頂点当たりのサイズ
	vertexBufferViewSprite_.StrideInBytes = sizeof(VertexData);
}

void Sprite::CreateVertexData()
{
    // Persistently map vertex buffer for sprite lifetime
    vertexMap_.reset(vertexResourceSprite_);
    vertexData_ = vertexMap_.get();
    vertexData_[0].position = { 0.0f,360.0f,0.0f,1.0f }; // 左下
    vertexData_[1].position = { 0.0f,0.0f,0.0f,1.0f };   // 左上
    vertexData_[2].position = { 640.0f,360.0f,0.0f,1.0f }; // 右下
    vertexData_[3].position = { 640.0f,0.0f,0.0f,1.0f };   // 右上

    vertexData_[0].texcoord = { 0.0f,1.0f };
    vertexData_[1].texcoord = { 0.0f,0.0f };
    vertexData_[2].texcoord = { 1.0f,1.0f };
    vertexData_[3].texcoord = { 1.0f,0.0f };
}

void Sprite::CreateIndexData()
{
    // インデックスリソースにデータを書き込む（スコープマップを使って即時Unmap）
    {
        Baziru3::ScopedMap<uint32_t> scopedIndexMap(indexResourceSprite_);
        uint32_t* idx = scopedIndexMap.get();
        idx[0] = 0; // 左下
        idx[1] = 1; // 左上
        idx[2] = 2; // 右下
        idx[3] = 2; // 右下
        idx[4] = 1; // 左上
        idx[5] = 3; // 右上
    }
}

void Sprite::ReflectionProcessing()
{
	//頂点リソースにデータを書き込む
	//左下
	vertexData_[0].position = { 0.0f,1.0f,0.0f,1.0f };
	vertexData_[0].texcoord = { 0.0f,1.0f };
	vertexData_[0].normal = { 0.0f,0.0f,-1.0f };
	//左上
	vertexData_[1].position = { 0.0f,0.0f,0.0f,1.0f };
	vertexData_[1].texcoord = { 0.0f,0.0f };
	vertexData_[1].normal = { 0.0f,0.0f,-1.0f };
	//右下
	vertexData_[2].position = { 1.0f,1.0f,0.0f,1.0f };
	vertexData_[2].texcoord = { 1.0f,1.0f };
	vertexData_[2].normal = { 0.0f,0.0f,-1.0f };
	//右上
	vertexData_[3].position = { 1.0f,0.0f,0.0f,1.0f };
	vertexData_[3].texcoord = { 1.0f,0.0f };
	vertexData_[3].normal = { 0.0f,0.0f,-1.0f };


}

void Sprite::AdjustTextureSize()
{
	const DirectX::TexMetadata& metadata = textureManager_->GetMetadata(textureIndex_);
	
	textureSize_.x = static_cast<float>(metadata.width);
	textureSize_.y = static_cast<float>(metadata.height);

	size_ = textureSize_;
}


void Sprite::SetUVParams(const Vector3& scale, float rotZ, const Vector3& translate)
{
	uvParams_.scale = scale;
	uvParams_.rotate.z = rotZ; 
	uvParams_.translate = translate;
	uvDirty_ = true;
}


void Sprite::RecalculateUVMatrix()
{
	Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvParams_.scale);
	uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvParams_.rotate.z));
	uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvParams_.translate));

	materialData_.uvTransform = uvTransformMatrix;
}
