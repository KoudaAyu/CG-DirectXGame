#pragma once

#include <d3d12.h>
#include <ostream>
#include <wrl.h>

class DirectXCom;

enum BlendMode
{
	//!< ブレンドなし
	kBlendMode_None,

	//!< αブレンド
	kBlendMode_Normal,

	//!< 加算ブレンド
	kBlendMode_Add,

	//!< 減算ブレンド
	kBlendMode_Sub,

	//!< 乗算ブレンド
	kBlendMode_Mul,

	//!< スクリーンブレンド
	kBlendMode_Screen,

	//利用禁止
	kCountOfBlendMode,
};

class SpriteCom
{
public:
	SpriteCom(std::ostream& logStream, DirectXCom* dxCommon);
	~SpriteCom();

	void Initialize();
	void Finalize();

	void SetupDraw(ID3D12GraphicsCommandList* commandList);
    
	// Runtime blend mode control
	void SetBlendMode(BlendMode mode);
	BlendMode GetBlendMode() const { return currentBlendMode; }

	DirectXCom* GetDxCommon() { return dxCommon; }

	const Microsoft::WRL::ComPtr<ID3D12RootSignature>& GetRootSignature() const;
	const Microsoft::WRL::ComPtr<ID3D12PipelineState>& GetPipelineState() const;

private:
	DirectXCom* dxCommon = nullptr;
	BlendMode currentBlendMode = kBlendMode_Normal;
	std::ostream& logStream;
};