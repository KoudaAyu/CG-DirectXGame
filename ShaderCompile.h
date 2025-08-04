#pragma once

#include<cassert>

#include<wrl.h>

#include"DebugLog.h"
#include"StringUtil.h"

#include <dxcapi.h>

class ShaderCompile
{
public:
	static Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
		//CompilerするShaderファイルへのパス
		const std::wstring& filePath,
		//Compilerに使用するProfile
		const wchar_t* profile,
		//初期化で生成したもの3つ
		Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils,
		Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler,
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler,
		std::ofstream* logStream);

	void CreateDxcCompiler(HRESULT hr);

	void CreateIncludeHandler(HRESULT hr);

	void CreateVertexShaderBlob();

	void CreatePixelShaderBlob();

	const Microsoft::WRL::ComPtr<IDxcUtils>& GetDxcUtils() const
	{
		return dxcUtils;
	}
	const Microsoft::WRL::ComPtr<IDxcCompiler3>& GetDxcCompiler() const
	{
		return dxcCompiler;
	}
	const Microsoft::WRL::ComPtr<IDxcIncludeHandler>& GetIncludeHandler() const
	{
		return includeHandler;
	}
	const Microsoft::WRL::ComPtr<IDxcBlob>& GetVertexShaderBlob() const
	{
		return vertexShaderBlob;
	}
	const Microsoft::WRL::ComPtr<IDxcBlob>& GetPixelShaderBlob() const
	{
		return pixelShaderBlob;
	}	

private:
	Debug debug;

	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils = nullptr;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler = nullptr;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler>includeHandler = nullptr;
	//Shaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = nullptr;
};