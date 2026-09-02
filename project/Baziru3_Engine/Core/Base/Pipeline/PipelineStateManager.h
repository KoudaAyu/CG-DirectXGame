#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <unordered_map>
#include <string>
#include <memory>
#include <filesystem>

class DirectXCom;

/**
 * @brief パイプラインステートオブジェクト（PSO）とルートシグネチャを一元管理・キャッシュするクラス
 */
class PipelineStateManager
{
public:
	struct ShaderFileInfo
	{
		std::wstring filePath;
		std::filesystem::file_time_type lastWriteTime;
	};

	static PipelineStateManager* GetInstance();

	void Initialize(DirectXCom* dxCommon);
	void Finalize();
	void Update(DirectXCom* dxCommon);

	const Microsoft::WRL::ComPtr<ID3D12PipelineState>& GetPipelineState(const std::string& name) const;
	const Microsoft::WRL::ComPtr<ID3D12RootSignature>& GetRootSignature(const std::string& name) const;

private:
	PipelineStateManager() = default;
	~PipelineStateManager() = default;
	PipelineStateManager(const PipelineStateManager&) = delete;
	PipelineStateManager& operator=(const PipelineStateManager&) = delete;

	void CreateSpritePipelines(DirectXCom* dxCommon);
	void CreateObject3dPipelines(DirectXCom* dxCommon);
	void CreateSlimePipelines(DirectXCom* dxCommon);

private:
	void LoadPipelineLibrary(ID3D12Device* device);
	void SavePipelineLibrary();

private:
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStates_;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSignatures_;
	std::unordered_map<std::string, ShaderFileInfo> watchedShaders_;
	Microsoft::WRL::ComPtr<ID3D12PipelineLibrary> pipelineLibrary_ = nullptr;
};
