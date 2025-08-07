#include "ViewportManager.h"
#pragma comment(lib,"d3d12.lib")
void ViewportManager::Initialize(Window window)
{
	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = static_cast<float>(window.GetClientWidth());
	viewport.Height = static_cast<float>(window.GetClientHeight());
	viewport.TopLeftX = 0.0f; //左上のX座標
	viewport.TopLeftY = 0.0f; //左上のY座標
	viewport.MinDepth = 0.0f; //最小の深度
	viewport.MaxDepth = 1.0f; //最大の深度
}
