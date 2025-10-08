#include "SpriteCom.h"

void SpriteCom::Initialize()
{
	RootSignature();
}

void SpriteCom::Update()
{
}

void SpriteCom::Draw()
{
}

void SpriteCom::RootSignature()
{
	//RootSignatureの作成
	descriptionRootSignature.Flags =
	D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; //入力アセンブラーでの使用を許可
}

void SpriteCom::InputLayer()
{
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION"; //セマンティック名
	inputElementDescs[0].SemanticIndex = 0; //セマンティックインデックス
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT; //頂点のフォーマット
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	
	inputLayoutDesc.pInputElementDescs = inputElementDescs; //入力要素の配列
	inputLayoutDesc.NumElements = _countof(inputElementDescs); //入力要素の数
}

void SpriteCom::GraphicPipeline()
{
	RootSignature();
}
