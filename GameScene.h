#pragma once
#include <cstdint>
#include"Graphic.h"
#include"Texture.h"
class GameScene
{
public:

	// 初期化
	void Initialize(const Microsoft::WRL::ComPtr<ID3D12Device>& device);
	// 更新処理
	void Update();
	// 描画処理
	void Draw();
private:
	uint32_t textureHandle_ = 0;
	Texture texture_; // テクスチャオブジェクト
	Graphic graphic_; // グラフィックオブジェクト
};