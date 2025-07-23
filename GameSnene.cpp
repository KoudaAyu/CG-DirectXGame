#include "GameScene.h"

GameScene::GameScene()
{
}

GameScene::~GameScene()
{
}

void GameScene::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	// 初期化処理をここに記述
	texture_.Load(device,commandList,"./Resources/uvChecker.png");
	sprite_.Create(texture_, { 100,50 }); // スプライトのサイズを指定
}

void GameScene::Update()
{
	// 更新処理をここに記述
}	

void GameScene::Draw()
{
	// 描画処理をここに記述
	sprite_.Draw();
}