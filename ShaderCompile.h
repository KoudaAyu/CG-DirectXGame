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
	

};