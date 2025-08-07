#include "ScissorManager.h"
#pragma comment(lib,"d3d12.lib")
void ScissorManager::Initialize(Window window)
{
	
	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0; //左上のX座標
	scissorRect.right = window.GetClientWidth(); //右下のX座標
	scissorRect.top = 0; //左上のY座標
	scissorRect.bottom = window.GetClientHeight(); //右下のY座標

}
