#pragma once

#include <d3d12.h>

class SpriteCom
{
public:
	void Initialize();
	void Update();
	void Draw();

	void RootSignature();
	void InputLayer();
	void GraphicPipeline();

	const D3D12_ROOT_SIGNATURE_DESC& GetDescriptionRootSignature() const
	{
		return descriptionRootSignature;
	}
	void SetDescriptionRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc)
	{
		descriptionRootSignature = desc;
	}
	const D3D12_INPUT_LAYOUT_DESC& GetInputLayoutDesc() const
	{
		return inputLayoutDesc;
	}

private:
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
};
