#include "RasterizerManager.h"
#pragma comment(lib,"d3d12.lib")
void RasterizerManager::RasterizerSetting()
{

	//裏面(時計回り)を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;


}
