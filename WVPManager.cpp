#include "WVPManager.h"
#pragma comment(lib,"d3d12.lib")
void WVPManager::Initialize(const Microsoft::WRL::ComPtr<ID3D12Device>& device,Buffer buffer)
{
	//WVP用のリソースを作る。　Matrix4x4 1つのサイズを用意する
	wvpResource = buffer.CreateBufferResource(device.Get(), sizeof(TransformationMatrix));

	//書き込む為のアドレス取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	//単位行列を書き込む
	wvpData->World = MakeIdentity4x4();
	wvpData->WVP = MakeIdentity4x4();
}
