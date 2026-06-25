#include "Object3d.h"
#include "Object3dCom.h"
#include "Matrix4x4.h"
#include "RootParam.h"
#include "TextureManager.h"
#include "Skeleton.h"
#include "SkinCluster.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include<cassert>
#include<fstream> // 追加: mtlファイル読み込み
#include<sstream> // 追加: 行分解用
#include<cstring> // 追加: memcpy 用

namespace
{
	void AppendAssimpMeshToModelData(const aiMesh* mesh, Object3d::ModelData& modelData)
	{
		if (!mesh)
		{
			return;
		}

		// 法線やテクスチャ座標が必要な場合は存在を確認してください
		// （Assimp のインポート時のフラグで提供されるはずです）。
		// まずメッシュの頂点を追加し、インデックス用のベースオフセットを記憶します。
		uint32_t baseIndex = static_cast<uint32_t>(modelData.vertices.size());
		modelData.vertices.resize(baseIndex + mesh->mNumVertices);

		for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
		{
			Sprite::VertexData vertex{};

			if (mesh->HasPositions())
			{
				const aiVector3D& position = mesh->mVertices[v];
				vertex.position = { position.x, position.y, position.z, 1.0f };
			}

			if (mesh->HasTextureCoords(0))
			{
				const aiVector3D& texcoord = mesh->mTextureCoords[0][v];
				vertex.texcoord = { texcoord.x, texcoord.y };
			}

			if (mesh->HasNormals())
			{
				const aiVector3D& normal = mesh->mNormals[v];
				vertex.normal = { normal.x, normal.y, normal.z };
			}

			modelData.vertices[baseIndex + v] = vertex;
		}

		// ベースオフセットを使って面（フェース）をインデックスとして追加する
		for (uint32_t fi = 0; fi < mesh->mNumFaces; ++fi)
		{
			const aiFace& face = mesh->mFaces[fi];
			// 三角形化された面を想定
			if (face.mNumIndices == 3)
			{
				modelData.indices.push_back(baseIndex + face.mIndices[0]);
				modelData.indices.push_back(baseIndex + face.mIndices[1]);
				modelData.indices.push_back(baseIndex + face.mIndices[2]);
			}
			else
			{
				// 三角形でない場合はスキップするか、適切に処理してください
				for (uint32_t k = 0; k < face.mNumIndices; ++k)
				{
					modelData.indices.push_back(baseIndex + face.mIndices[k]);
				}
			}
		}
	}

	void AppendAssimpNodeMeshes(const aiNode* node, const aiScene* scene, Object3d::ModelData& modelData)
	{
		if (!node || !scene)
		{
			return;
		}

		for (uint32_t meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex)
		{
			const aiMesh* mesh = scene->mMeshes[node->mMeshes[meshIndex]];
			AppendAssimpMeshToModelData(mesh, modelData);
		}

		for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
		{
			AppendAssimpNodeMeshes(node->mChildren[childIndex], scene, modelData);
		}
	}

	void LoadAssimpMaterial(const aiScene* scene, const std::string& directoryPath, Object3d::ModelData& modelData)
	{
		if (!scene || scene->mNumMaterials == 0)
		{
			return;
		}

		const aiMaterial* material = scene->mMaterials[0];
		aiString texturePath;
		if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) == aiReturn_SUCCESS ||
			material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == aiReturn_SUCCESS)
		{
			modelData.material.textureFilePath = directoryPath + "/" + texturePath.C_Str();
		}
	}
}



void Object3d::Initialize(Object3dCom* object3dCom, const ModelData& modelData)
{
	object3dCom_ = object3dCom;
	modelData_ = modelData;
	if (object3dCom_)
	{
		camera_ = object3dCom_->GetDefaultCamera();
	}

	VertexResource();
	MaterialResource();
	TransformationMatrixResource();
	DirectionalLightResource();

	// テクスチャロードはパスが有効なときのみ実行
	if (!modelData_.material.textureFilePath.empty())
	{
		uint32_t index = TextureManager::GetInstance()->Load(modelData_.material.textureFilePath);
		if (index != TextureManager::kInvalidTextureIndex)
		{
			modelData_.material.textureIndex = index;
		}
		else
		{
			OutputDebugStringA(("Texture load failed: " + modelData_.material.textureFilePath + "\n").c_str());
		}
	}

	//Transform変数の生成
	transform = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
	cameraTransform = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,-10.0f} };
}

void Object3d::UpdateModelData(const ModelData& modelData)
{
	modelData_ = modelData;
	// 既存のリソースを安全に破棄
	vertexResource.Reset();
	indexResource.Reset();
	// 再生成とアップロード
	VertexResource();
}

void Object3d::SetupAnimation(const Animation* animation, const Skeleton& skeleton, const Model::ModelData& modelData)
{
	if (!object3dCom_) return;
	DirectXCom* dx = object3dCom_->GetDirectXCom();
	if (!dx) return;
	SRVManager* srvManager = TextureManager::GetInstance()->GetSRVManager();
	if (!srvManager) return;

	animator_.SetAnimation(animation);
	skeleton_ = skeleton;

	if (!skeleton_.joints.empty())
	{
		skeleton_.Update();
		skinCluster_ = skinClusterLender_.CreateSkinCluster(
			dx->GetDevice(),
			skeleton_,
			modelData,
			dx->GetSrvDescriptorHeap(),
			dx->GetDescriptorSizeSRV(),
			*dx,
			*srvManager);
		skinClusterInitialized_ = true;
	}
}

void Object3d::Update()
{
	// 毎フレーム、Object3dCom から最新のカメラを取得（初期化後に設定されたケースへ対応）
	if (object3dCom_)
	{
		camera_ = object3dCom_->GetDefaultCamera();
	}

	// ステップ1: アニメーション時間を進める
	// ステップ2: 骨ごとのLocal情報を更新する
	if (animator_.HasAnimation())
	{
		animator_.Update(deltaTime_);
		// ステップ3: SkeletonSpaceの情報を更新する
		animator_.ApplyTo(skeleton_);
	}

	if (!skeleton_.joints.empty())
	{
		// ステップ3: 現在の骨ごとのLocal情報を基にSkeletonSpaceの情報を更新する
		skeleton_.Update();
	}

	// ステップ4: SkinClusterのMatrixPaletteを更新する
	if (skinClusterInitialized_)
	{
		skinClusterLender_.Update(skinCluster_, skeleton_);
	}

	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.GetScale(), transform.GetRotate(), transform.GetTranslate());

	Matrix4x4 viewMatrix;
	Matrix4x4 projectionMatrix;

	if (camera_)
	{
		// カメラが有効な場合はそれを使う
		viewMatrix = camera_->GetViewMatrix();
		projectionMatrix = camera_->GetProjectionMatrix();
	}
	else
	{
		// フォールバック: 内部の簡易カメラで計算
		Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.GetScale(), cameraTransform.GetRotate(), cameraTransform.GetTranslate());
		viewMatrix = Inverse(cameraMatrix);
		projectionMatrix = MakePerspectiveFovMatrix(0.45f, 1.0f, 0.1f, 100.0f);
	}

	// WVP を更新
	transformationMatrixData_->WVP = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	transformationMatrixData_->World = worldMatrix;
	// WorldInverseTranspose を計算して格納（法線変換用）
	transformationMatrixData_->WorldInverseTranspose = Transpose(Inverse(worldMatrix));
}

void Object3d::Draw(ID3D12GraphicsCommandList* commandList)
{
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   if (indexResource && !modelData_.indices.empty())
	{
		commandList->IASetIndexBuffer(&indexBufferView_);
	}
	commandList->SetGraphicsRootConstantBufferView(RootParam::Object3D::kMaterial, materialResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(RootParam::Object3D::kTransform, transformationMatrixResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(RootParam::Object3D::kLight, directionalLightResource->GetGPUVirtualAddress());
   if (indexResource && !modelData_.indices.empty())
	{
		commandList->DrawIndexedInstanced(static_cast<UINT>(modelData_.indices.size()), 1, 0, 0, 0);
	}
	else
	{
		commandList->DrawInstanced(static_cast<UINT>(modelData_.vertices.size()), 1, 0, 0);
	}
}

Object3d::MaterialData Object3d::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
	//中で必要になる変数の宣言
	Object3d::MaterialData materlialData;//構築するデータ
	std::string line;//ファイルから読み込んだ1行を格納するもの
	std::ifstream file(directoryPath + "/" + filename);//ファイルを開く
	if (!file.is_open())
	{
		OutputDebugStringA(("Warning: Material template file " + directoryPath + "/" + filename + " could not be opened.\n").c_str());
		return materlialData; // ファイルが開けなかった場合は空のデータを返す（安全対策）
	}

	//MaterialDataを構築
	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; //先頭の識別子を取得

		//identifierに応じた処理
		if (identifier == "map_Kd")
		{
			std::string textureFilename;
			s >> textureFilename; //テクスチャファイル名を取得
			//連結してファイルパスにする
			materlialData.textureFilePath = directoryPath + "/" + textureFilename;
		}

	}


	return materlialData;
}

Object3d::ModelData Object3d::LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
	//中で必要になる変数の宣言
	Object3d::ModelData modelData;//構築するデータ
	std::vector<Vector4>positions;//位置
	std::vector<Vector3>normals;//法線
	std::vector<Vector2>texcoords;//テクスチャ座標
	std::string line;//ファイルから読み込んだ1行を格納するもの

	//ファイルを開く
	std::ifstream file(directoryPath + "/" + filename);//ファイルを開く
	if (!file.is_open())
	{
		OutputDebugStringA(("Warning: Obj file " + directoryPath + "/" + filename + " could not be opened.\n").c_str());
		return modelData; // ファイルが開けなかった場合は空のデータを返す（安全対策）
	}

	//実際にファイルを読み込む。その後modelDataを構築する
	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; //先頭の識別子を取得

		//identifierに応じた処理
		if (identifier == "v")
		{
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			// position.x *= -1.0f; // X反転を無効化: テクスチャ向きが変わる原因になる
			position.w = 1.0f;
			positions.push_back(position);//位置を格納
		}
		else if (identifier == "vt")
		{
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			// OBJのVTは左下原点のことが多いので、DirectXのテクスチャ原点(左上)に合わせてVを反転する
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);//テクスチャ座標を格納
		}
		else if (identifier == "vn")
		{
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			// normal.x *= -1.0f; // 法線X反転を無効化
			normals.push_back(normal);//法線を格納
		}
		else if (identifier == "f")
		{
			//面は三角形限定。その他は未対応
			Sprite::VertexData triangle[3];
			uint32_t triangleIndices[3];

			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex)
			{
				std::string vertexDefinition;
				s >> vertexDefinition; //頂点の定義を取得

				//頂点の要素へのIndexは、位置、UV、法線の順で入っているため、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element)
				{
					std::string index;
					std::getline(v, index, '/'); //スラッシュで区切って要素を取得
					elementIndices[element] = std::stoi(index);
				}
				//要素へのIndexから実際の用をの値を取得して、頂点を構築する
				Vector4 position = positions[elementIndices[0] - 1]; //OBJファイルは1始まりなので-1する
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				triangle[faceVertex] = { position, texcoord, normal };
               triangleIndices[faceVertex] = static_cast<uint32_t>(modelData.vertices.size()) + static_cast<uint32_t>(faceVertex);
			}
			// OBJの元の頂点順を維持して追加（入れ替えない）
			modelData.vertices.push_back(triangle[0]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[2]);
           modelData.indices.push_back(triangleIndices[0]);
			modelData.indices.push_back(triangleIndices[1]);
			modelData.indices.push_back(triangleIndices[2]);
		}
		else if (identifier == "mtllib")
		{
			//materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename; //マテリアルファイル名を取得
			//基本的にobjファイルを同じ階層にmtlファイルがあるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}

	}

	return modelData;
}

Object3d::ModelData Object3d::LoadModelFile(const std::string& directoryPath, const std::string& filename)
{
	Object3d::ModelData modelData;
	Assimp::Importer importer;

	const std::string fullPath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(fullPath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);
	if (!scene || !scene->mRootNode)
	{
		OutputDebugStringA(("Warning: Assimp failed to load model file " + fullPath + "\n").c_str());
		return modelData; // ロード失敗時は空のデータを返す（安全対策）
	}

	AppendAssimpNodeMeshes(scene->mRootNode, scene, modelData);
	LoadAssimpMaterial(scene, directoryPath, modelData);

	return modelData;
}


void Object3d::VertexResource()
{
	//VertexResourceの生成
	if (object3dCom_ && object3dCom_->GetDirectXCom())
	{
		DirectXCom* dxCommon = object3dCom_->GetDirectXCom();
		// 現在保持している頂点数に応じたサイズで確保（未設定の場合は最小1頂点分）
		size_t vertexCount = modelData_.vertices.size();
		if (vertexCount == 0) { vertexCount = 1; }
		size_t bufferSize = sizeof(Sprite::VertexData) * vertexCount;

		vertexResource = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), bufferSize);

		// VertexBufferView を設定（値の設定のみ）
		vertexBufferView_.BufferLocation = vertexResource->GetGPUVirtualAddress();
		vertexBufferView_.SizeInBytes = static_cast<UINT>(bufferSize);
		vertexBufferView_.StrideInBytes = sizeof(Sprite::VertexData);

		// 書き込み用アドレスを取得してメンバーに保持
		vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

		// 可能なら読み込んだモデル頂点をアップロード
		if (!modelData_.vertices.empty())
		{
			std::memcpy(vertexData_, modelData_.vertices.data(), sizeof(Sprite::VertexData) * modelData_.vertices.size());
		}

		D3D12_RANGE written = { 0, static_cast<SIZE_T>(bufferSize) };
		vertexResource->Unmap(0, &written);
		vertexData_ = nullptr;

		if (!modelData_.indices.empty())
		{
			size_t indexBufferSize = sizeof(uint32_t) * modelData_.indices.size();
			indexResource = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), indexBufferSize);
			uint32_t* mappedIndex = nullptr;
			indexResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndex));
			std::memcpy(mappedIndex, modelData_.indices.data(), indexBufferSize);
			D3D12_RANGE indexWritten = { 0, static_cast<SIZE_T>(indexBufferSize) };
			indexResource->Unmap(0, &indexWritten);

			indexBufferView_.BufferLocation = indexResource->GetGPUVirtualAddress();
			indexBufferView_.SizeInBytes = static_cast<UINT>(indexBufferSize);
			indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
		}

	}
}

Object3d::~Object3d()
{
	// 持続的にマップされたリソースがある場合は安全にアンマップする
	// 二重アンマップを避けるため、CPU側のポインタが nullptr でない場合のみ Unmap する
	if (transformationMatrixResource && transformationMatrixData_ != nullptr)
	{
		D3D12_RANGE written = { 0, sizeof(TransformationMatrix) };
		transformationMatrixResource->Unmap(0, &written);
		transformationMatrixData_ = nullptr;
	}

	if (vertexResource && vertexData_ != nullptr)
	{
		D3D12_RANGE written = { 0, static_cast<SIZE_T>(vertexBufferView_.SizeInBytes) };
		vertexResource->Unmap(0, &written);
		vertexData_ = nullptr;
	}

	if (materialResource && materialData_ != nullptr)
	{
		D3D12_RANGE written = { 0, sizeof(Material) };
		materialResource->Unmap(0, &written);
		materialData_ = nullptr;
	}

	if (directionalLightResource && directionalLightData_ != nullptr)
	{
		D3D12_RANGE written = { 0, sizeof(DirectionalLight) };
		directionalLightResource->Unmap(0, &written);
		directionalLightData_ = nullptr;
	}
}

void Object3d::SetEnableLighting(bool enable)
{
	if (!materialResource) return;
	Material* data = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&data));
	if (data)
	{
		data->enableLighting = enable ? 1 : 0;
		D3D12_RANGE written = { 0, sizeof(Material) };
		materialResource->Unmap(0, &written);
	}
}

void Object3d::SetColor(const Vector4& color)
{
	if (!materialResource) return;
	Material* data = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&data));
	if (data)
	{
		data->color = color;
		D3D12_RANGE written = { 0, sizeof(Material) };
		materialResource->Unmap(0, &written);
	}
}

void Object3d::MaterialResource()
{
	if (object3dCom_ && object3dCom_->GetDirectXCom())
	{
		DirectXCom* dxCommon = object3dCom_->GetDirectXCom();
		// マテリアル用のリソースを作成（1個分）
		materialResource = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(Material));
		// 書き込み用アドレスを取得してメンバーに保持
		materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
		// 初期値を設定
		materialData_->color = { 1.0f,1.0f,1.0f,1.0f };
		materialData_->enableLighting = false;
		materialData_->specularModel = 0; // 0: Blinn-Phong
		materialData_->shininess = 70.0f; // デフォルトの光沢度を設定
		std::memset(materialData_->padding, 0, sizeof(materialData_->padding));
		std::memset(materialData_->padding2, 0, sizeof(materialData_->padding2));
		materialData_->uvTransform = MakeIdentity4x4();

		// マテリアルは初期化時に一度だけ書き込む想定のため、MapしたらすぐにUnmapする
		// 書き込み範囲を指定してGPUへ変更を通知する
		D3D12_RANGE writtenRange = { 0, sizeof(Material) };
		materialResource->Unmap(0, &writtenRange);

		materialData_ = nullptr;
	}
}

void Object3d::TransformationMatrixResource()
{
	if (object3dCom_ && object3dCom_->GetDirectXCom())
	{
		DirectXCom* dxCommon = object3dCom_->GetDirectXCom();
		// Transformation
		transformationMatrixResource = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(TransformationMatrix));
		// 書き込み用アドレスを取得してメンバーに保持
		transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

		transformationMatrixData_->WVP = MakeIdentity4x4();
		transformationMatrixData_->World = MakeIdentity4x4();
	}
}

void Object3d::DirectionalLightResource()
{
	// マッピング前にバッファを作成し、セーフティチェックを行う
	if (object3dCom_ && object3dCom_->GetDirectXCom())
	{
		DirectXCom* dxCommon = object3dCom_->GetDirectXCom();
		// ディレクショナルライト用のCBVリソースを作成
		directionalLightResource = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(DirectionalLight));
		// 書き込み用アドレスを取得
		directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

		// 初期値を書き込む
		directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
		directionalLightData_->intensity = 1.0f;

		// 必要に応じてアンマップ（頻繁に更新しない場合）
		directionalLightResource->Unmap(0, nullptr);
		directionalLightData_ = nullptr;
	}
}