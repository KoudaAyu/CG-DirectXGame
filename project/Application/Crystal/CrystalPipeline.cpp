//======================================================================
// CrystalPipeline.cpp
//======================================================================

#include "CrystalPipeline.h"

#include<d3dx12.h>
#include <dxcapi.h>           // DXC (DirectX Shader Compiler)
#include <stdexcept>
#include <vector>

namespace
{
	//------------------------------------------------------------------
	// DXC を使って HLSL ファイルを Shader Model 6.0 でコンパイルする簡易ヘルパー。
	// 実運用では結果をキャッシュしたり、ホットリロードに対応させるとよい。
	//------------------------------------------------------------------
	ComPtr<IDxcBlob> CompileShaderDXC(const std::wstring& path,
									   const std::wstring& entryPoint,
									   const std::wstring& target)
	{
		static ComPtr<IDxcUtils>          s_utils;
		static ComPtr<IDxcCompiler3>      s_compiler;
		static ComPtr<IDxcIncludeHandler> s_includeHandler;

		if (!s_utils)
		{
			DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&s_utils));
			DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&s_compiler));
			s_utils->CreateDefaultIncludeHandler(&s_includeHandler);
		}

		ComPtr<IDxcBlobEncoding> source;
		if (FAILED(s_utils->LoadFile(path.c_str(), nullptr, &source)))
		{
			throw std::runtime_error("Failed to load shader file");
		}

		std::vector<LPCWSTR> args =
		{
			path.c_str(),
			L"-E", entryPoint.c_str(),
			L"-T", target.c_str(),
			L"-HV", L"2021",
#if defined(_DEBUG)
			L"-Zi", L"-Od",
#else
			L"-O3",
#endif
		};

		DxcBuffer srcBuffer{};
		srcBuffer.Ptr      = source->GetBufferPointer();
		srcBuffer.Size     = source->GetBufferSize();
		srcBuffer.Encoding = DXC_CP_ACP;

		ComPtr<IDxcResult> result;
		s_compiler->Compile(&srcBuffer, args.data(), static_cast<UINT32>(args.size()),
							 s_includeHandler.Get(), IID_PPV_ARGS(&result));

		ComPtr<IDxcBlobUtf8> errors;
		result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
		if (errors && errors->GetStringLength() > 0)
		{
			OutputDebugStringA(errors->GetStringPointer());
		}

		HRESULT status = S_OK;
		result->GetStatus(&status);
		if (FAILED(status))
		{
			throw std::runtime_error("Shader compilation failed");
		}

		ComPtr<IDxcBlob> shaderBlob;
		result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
		return shaderBlob;
	}
}

void CrystalPipeline::Initialize(ID3D12Device* device, DXGI_FORMAT sceneColorFormat, DXGI_FORMAT depthFormat)
{
	CreateRootSignature(device);
	CreatePSOs(device, sceneColorFormat, depthFormat);
}

void CrystalPipeline::CreateRootSignature(ID3D12Device* device)
{
	// t0 : SceneColorCopy (屈折サンプリング用の背景コピー)
	CD3DX12_DESCRIPTOR_RANGE1 srvRange{};
	srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 params[5]{};
	params[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);    // b0 FrameCB
	params[1].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);  // b1 CrystalMaterialCB
	params[2].InitAsConstants(1, 2, 0, D3D12_SHADER_VISIBILITY_PIXEL);                                         // b2 PassCB (背面/前面フラグ, 1 DWORD)
	params[3].InitAsConstantBufferView(3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX); // b3 ObjectCB
	params[4].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);                              // t0 SceneColorCopy

	CD3DX12_STATIC_SAMPLER_DESC sampler(
		0,                                    // s0
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
	desc.Init_1_1(_countof(params), params, 1, &sampler,
				  D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> sigBlob{}, errBlob{};
	HRESULT hr = D3D12SerializeVersionedRootSignature(&desc, &sigBlob, &errBlob);
	if (FAILED(hr))
	{
		if (errBlob)
		{
			OutputDebugStringA(static_cast<const char*>(errBlob->GetBufferPointer()));
		}
		throw std::runtime_error("Failed to serialize root signature");
	}

	hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
									  IID_PPV_ARGS(&m_rootSignature));
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create root signature");
	}
}

void CrystalPipeline::CreatePSOs(ID3D12Device* device, DXGI_FORMAT sceneColorFormat, DXGI_FORMAT depthFormat)
{
	ComPtr<IDxcBlob> vsBlob = CompileShaderDXC(L"Resources/shaders/Crystal/Crystal.hlsl", L"VSMain", L"vs_6_0");
	ComPtr<IDxcBlob> psBlob = CompileShaderDXC(L"Resources/shaders/Crystal/Crystal.hlsl", L"PSMain", L"ps_6_0");

	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature        = m_rootSignature.Get();
	psoDesc.VS                    = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS                    = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	psoDesc.InputLayout           = { inputLayout, _countof(inputLayout) };
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets      = 1;
	psoDesc.RTVFormats[0]         = sceneColorFormat;
	psoDesc.DSVFormat             = depthFormat;
	psoDesc.SampleDesc            = { 1, 0 };
	psoDesc.SampleMask            = UINT_MAX;

	// 深度テストのみ行い、書き込みはしない（半透明描画の定石）
	D3D12_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable    = TRUE;
	depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthFunc      = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	depthDesc.StencilEnable  = FALSE;
	psoDesc.DepthStencilState = depthDesc;

	// ---- 背面パス：裏面のみ描画し、内部発光を加算合成 ----
	{
		D3D12_RASTERIZER_DESC raster = {};
		raster.FillMode        = D3D12_FILL_MODE_SOLID;
		raster.CullMode        = D3D12_CULL_MODE_FRONT; // 表面をカリング=裏面のみ描画
		raster.DepthClipEnable = TRUE;
		psoDesc.RasterizerState = raster;

		D3D12_BLEND_DESC blend = {};
		blend.RenderTarget[0].BlendEnable           = TRUE;
		blend.RenderTarget[0].SrcBlend              = D3D12_BLEND_ONE;
		blend.RenderTarget[0].DestBlend             = D3D12_BLEND_ONE; // 加算合成
		blend.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
		blend.RenderTarget[0].SrcBlendAlpha         = D3D12_BLEND_ONE;
		blend.RenderTarget[0].DestBlendAlpha        = D3D12_BLEND_ZERO;
		blend.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
		blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		psoDesc.BlendState = blend;

		HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_psoBackface));
		if (FAILED(hr)) throw std::runtime_error("Failed to create backface PSO");
	}

	// ---- 前面パス：表面のみ描画し、屈折+フレネルを通常αブレンド ----
	{
		D3D12_RASTERIZER_DESC raster = {};
		raster.FillMode        = D3D12_FILL_MODE_SOLID;
		raster.CullMode        = D3D12_CULL_MODE_BACK; // 裏面をカリング=表面のみ描画
		raster.DepthClipEnable = TRUE;
		psoDesc.RasterizerState = raster;

		D3D12_BLEND_DESC blend = {};
		blend.RenderTarget[0].BlendEnable           = TRUE;
		blend.RenderTarget[0].SrcBlend              = D3D12_BLEND_SRC_ALPHA;
		blend.RenderTarget[0].DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
		blend.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
		blend.RenderTarget[0].SrcBlendAlpha         = D3D12_BLEND_ONE;
		blend.RenderTarget[0].DestBlendAlpha        = D3D12_BLEND_ZERO;
		blend.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
		blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		psoDesc.BlendState = blend;

		HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_psoFrontface));
		if (FAILED(hr)) throw std::runtime_error("Failed to create frontface PSO");
	}
}

void CrystalPipeline::DrawBackfacePass(
	ID3D12GraphicsCommandList* cmdList,
	D3D12_GPU_VIRTUAL_ADDRESS  frameCB,
	D3D12_GPU_VIRTUAL_ADDRESS  materialCB,
	D3D12_GPU_VIRTUAL_ADDRESS  objectCB)
{
	cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
	cmdList->SetPipelineState(m_psoBackface.Get());

	cmdList->SetGraphicsRootConstantBufferView(0, frameCB);
	cmdList->SetGraphicsRootConstantBufferView(1, materialCB);

	const UINT isBackfacePass = 1;
	cmdList->SetGraphicsRoot32BitConstants(2, 1, &isBackfacePass, 0);

	cmdList->SetGraphicsRootConstantBufferView(3, objectCB);
	// t0 (SceneColorCopy) は背面パスでは未使用のためバインド不要

	// ここで呼び出し側が cmdList->IASetVertexBuffers / IASetIndexBuffer /
	// DrawIndexedInstanced(...) を呼んでクリスタルメッシュを描画する
}

void CrystalPipeline::DrawFrontfacePass(
	ID3D12GraphicsCommandList* cmdList,
	D3D12_GPU_VIRTUAL_ADDRESS  frameCB,
	D3D12_GPU_VIRTUAL_ADDRESS  materialCB,
	D3D12_GPU_VIRTUAL_ADDRESS  objectCB,
	D3D12_GPU_DESCRIPTOR_HANDLE sceneColorCopySRV)
{
	cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
	cmdList->SetPipelineState(m_psoFrontface.Get());

	cmdList->SetGraphicsRootConstantBufferView(0, frameCB);
	cmdList->SetGraphicsRootConstantBufferView(1, materialCB);

	const UINT isBackfacePass = 0;
	cmdList->SetGraphicsRoot32BitConstants(2, 1, &isBackfacePass, 0);

	cmdList->SetGraphicsRootConstantBufferView(3, objectCB);
	cmdList->SetGraphicsRootDescriptorTable(4, sceneColorCopySRV);

	// ここで呼び出し側が cmdList->IASetVertexBuffers / IASetIndexBuffer /
	// DrawIndexedInstanced(...) を呼んでクリスタルメッシュを描画する
}
