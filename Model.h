#pragma once

#include<Windows.h>

#include<cassert>
#include<d3d12.h>
#include <dxgi1_6.h> 
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#include<fstream>
#include<sstream>
#include<vector>
#include<wrl.h>

#include"Buffer.h"
#include"externals/DirectXTex/d3dx12.h"
#include"Struct.h"
#include"Vector.h"

class Model
{
public:
	Model();
	~Model();
	void Initialize(ID3D12Device* device);
	void Update();
	void Draw(ID3D12GraphicsCommandList* commandList);

	MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

	const std::string& GetTextureFilePath() const { return modelData.material.textureFilePath; }
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const
	{
		return vertexBufferView;
	}

private:
	ModelData modelData;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceModel;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;

};
