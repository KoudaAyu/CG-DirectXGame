#pragma once

#include <cstdint>
#include"Sprite.h"
#include"Texture.h"

class GameScene
{
public:
	GameScene();
	~GameScene();

	// 初期化
	void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	// 更新処理
	void Update();
	// 描画処理
	void Draw();
	
private:
	Texture texture_; // テクスチャオブジェクト
	Sprite sprite_; // スプライトオブジェクト
	
};