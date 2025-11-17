#include "Model.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <cstring>

void Model::Initialize(ModelCom* modelCom)
{
	modelCom_ = modelCom;
}

void Model::Initialize(ModelCom* modelCom, const std::string& objDirectory, const std::string& objFilename)
{
	modelCom_ = modelCom;
	// 読み込みとGPUバッファ生成
	LoadFromObj(objDirectory, objFilename);

	//頂点データの初期化

}

// .mtl 読み込み
Model::MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
	MaterialData materialData; // 構築するデータ
	std::string line;
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string ident;
		std::istringstream s(line);
		s >> ident; // 先頭の識別子

		if (ident == "map_Kd")
		{
			std::string textureFilename;
			s >> textureFilename;
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	return materialData;
}

// .obj 読み込み
Model::ModelData Model::LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
	ModelData modelData; // 構築するデータ
	std::vector<Vector4> positions; // 位置
	std::vector<Vector3> normals;   // 法線
	std::vector<Vector2> texcoords; // テクスチャ座標
	std::string line;               // 1行

	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string ident;
		std::istringstream s(line);
		s >> ident;

		if (ident == "v")
		{
			Vector4 p{};
			s >> p.x >> p.y >> p.z;
			p.x *= -1.0f; // X反転
			p.w = 1.0f;
			positions.push_back(p);
		}
		else if (ident == "vt")
		{
			Vector2 uv{};
			s >> uv.x >> uv.y;
			uv.y = 1.0f - uv.y; // Y反転
			texcoords.push_back(uv);
		}
		else if (ident == "vn")
		{
			Vector3 n{};
			s >> n.x >> n.y >> n.z;
			n.x *= -1.0f; // X反転
			normals.push_back(n);
		}
		else if (ident == "f")
		{
			Sprite::VertexData tri[3]{};
			for (int32_t i = 0; i < 3; ++i)
			{
				std::string vertexDef;
				s >> vertexDef;
				std::istringstream v(vertexDef);
				uint32_t idx[3]{};
				for (int32_t e = 0; e < 3; ++e)
				{
					std::string token;
					std::getline(v, token, '/');
					idx[e] = static_cast<uint32_t>(std::stoi(token));
				}
				const Vector4& p = positions[idx[0] - 1];
				const Vector2& uv = texcoords[idx[1] - 1];
				const Vector3& n = normals[idx[2] - 1];
				tri[i] = { p, uv, n };
			}
			// 反時計->時計順に入れ替え
			modelData.vertices.push_back(tri[2]);
			modelData.vertices.push_back(tri[1]);
			modelData.vertices.push_back(tri[0]);
		}
		else if (ident == "mtllib")
		{
			std::string mtlFilename;
			s >> mtlFilename;
			modelData.material = LoadMaterialTemplateFile(directoryPath, mtlFilename);
		}
	}

	return modelData;
}

bool Model::LoadFromObj(const std::string& directoryPath, const std::string& filename)
{
	modelData_ = LoadObjFile(directoryPath, filename);

	// GPU バッファ生成
	CreateVertexBufferFromModel();
	CreateMaterialBuffer();

	return !modelData_.vertices.empty();
}

void Model::CreateVertexBufferFromModel()
{
	if (!modelCom_ || !modelCom_->GetDxCommon()) { return; }
	DirectXCom* dx = modelCom_->GetDxCommon();

	size_t vertexCount = modelData_.vertices.size();
	if (vertexCount == 0) { vertexCount = 1; }
	size_t bufferSize = sizeof(Sprite::VertexData) * vertexCount;

	vertexResource = dx->CreateBufferResource(dx->GetDevice().Get(), bufferSize);
	vertexBufferView_.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = static_cast<UINT>(bufferSize);
	vertexBufferView_.StrideInBytes = sizeof(Sprite::VertexData);

	// 書き込み
	Sprite::VertexData* mapped = nullptr;
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	if (!modelData_.vertices.empty())
	{
		std::memcpy(mapped, modelData_.vertices.data(), sizeof(Sprite::VertexData) * modelData_.vertices.size());
	}
	vertexResource->Unmap(0, nullptr);
}

void Model::CreateMaterialBuffer()
{
	if (!modelCom_ || !modelCom_->GetDxCommon()) { return; }
	DirectXCom* dx = modelCom_->GetDxCommon();

	materialResource = dx->CreateBufferResource(dx->GetDevice().Get(), sizeof(Material));
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = false;
	materialData_->uvTransform = MakeIdentity4x4();
	materialResource->Unmap(0, nullptr);
}
