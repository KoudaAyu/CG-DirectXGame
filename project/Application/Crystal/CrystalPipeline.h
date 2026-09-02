#pragma once
//======================================================================
// CrystalPipeline.h
//
// クリスタル(水晶)表現用の D3D12 パイプライン一式。
//   - ルートシグネチャ
//   - 背面(内部発光)パス用 PSO
//   - 前面(屈折+フレネル)パス用 PSO
//
// 既存のレンダラー内から、Initialize() でPSOを作成し、
// フレーム毎に DrawBackfacePass() → (SceneColorのコピー) → DrawFrontfacePass()
// の順に呼び出して使う。
//======================================================================

#include <d3d12.h>
#include <dxgiformat.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class CrystalPipeline
{
public:
	// sceneColorFormat : クリスタルを描き込む対象のRTVフォーマット（例: DXGI_FORMAT_R16G16B16A16_FLOAT）
	// depthFormat      : 深度テストに使うDSVフォーマット（例: DXGI_FORMAT_D32_FLOAT）
	void Initialize(ID3D12Device* device, DXGI_FORMAT sceneColorFormat, DXGI_FORMAT depthFormat);

	// --- 背面(裏側)パス ---------------------------------------------
	// CullFront で裏面のみラスタライズし、内部発光を加算合成で書き込む。
	// この時点では屈折サンプリングは行わないため t0 のバインドは不要。
	// 呼び出し前に SceneColor を RENDER_TARGET 状態にしておくこと。
	void DrawBackfacePass(
		ID3D12GraphicsCommandList* cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS  frameCB,
		D3D12_GPU_VIRTUAL_ADDRESS  materialCB,
		D3D12_GPU_VIRTUAL_ADDRESS  objectCB);

	// --- 前面(表側)パス ---------------------------------------------
	// CullBack で表面のみラスタライズ。sceneColorCopySRV には、
	// 背面パス完了後の SceneColor を CopyResource でコピーしたテクスチャの
	// SRV(GPUディスクリプタハンドル) を渡す。
	void DrawFrontfacePass(
		ID3D12GraphicsCommandList* cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS  frameCB,
		D3D12_GPU_VIRTUAL_ADDRESS  materialCB,
		D3D12_GPU_VIRTUAL_ADDRESS  objectCB,
		D3D12_GPU_DESCRIPTOR_HANDLE sceneColorCopySRV);

	ID3D12RootSignature* GetRootSignature() const { return m_rootSignature.Get(); }

private:
	void CreateRootSignature(ID3D12Device* device);
	void CreatePSOs(ID3D12Device* device, DXGI_FORMAT sceneColorFormat, DXGI_FORMAT depthFormat);

	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_psoBackface;
	ComPtr<ID3D12PipelineState> m_psoFrontface;
};
