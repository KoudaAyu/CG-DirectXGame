#include "GameScene.h"

void GameScene::Initialize(const Microsoft::WRL::ComPtr<ID3D12Device>& device)
{
	// 初期化処理をここに記述
	textureHandle_ = Texture::Load(device.Get(),  "Resources/monsterBall.png");
	texture_.CreateDepthStencilTextureResource(device.Get(), static_cast<int32_t>(1200.0f), static_cast<int32_t>(600.0f));
	
}

void GameScene::Update()
{
	// 更新処理をここに記述
}	

void GameScene::Draw()
{
	// 描画処理をここに記述
}