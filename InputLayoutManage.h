#pragma once
#include <d3d12.h>
class InputLayoutManager
{
public:
	void CreateInputLayer();
	const D3D12_INPUT_ELEMENT_DESC* GetInputElementDescs() const
	{
		return inputElementDescs;
	}
	size_t GetInputElementDescCount() const
	{
		return sizeof(inputElementDescs) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	}
	const D3D12_INPUT_LAYOUT_DESC& GetInputLayoutDesc() const
	{
		return inputLayoutDesc;
	}
	D3D12_INPUT_LAYOUT_DESC& GetInputLayoutDesc()
	{
		return inputLayoutDesc;
	}

private:
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
};
