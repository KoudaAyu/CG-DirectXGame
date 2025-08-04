#include "BlendManager.h"
#pragma comment(lib,"d3d12.lib")

void BlendManager::CreateBlend()
{
	//すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
}
