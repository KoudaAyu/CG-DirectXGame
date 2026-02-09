#include"Object3d.h"
#include"Object3dCom.h"
#include"Matrix4x4.h"
#include<cassert>
#include<fstream> // 追加: mtlファイル読み込み
#include<sstream> // 追加: 行分解用
#include<cstring> // 追加: memcpy 用

#include<assimp/Importer.hpp>
#include<assimp/scene.h>
#include<assimp/postprocess.h>



void Object3d::Initialize(Object3dCom* object3dCom)
{
	object3dCom_ = object3dCom;
	// object3dCom_ が未設定の場合は何もしない(デフォルトカメラは取得しない)
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
		// TextureManagerにDirectXコンテキストが渡っていることを前提にする
		TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
		modelData_.material.textureIndex =
			TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData_.material.textureFilePath);
	}

	//Transform変数の生成
	transform = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
	cameraTransform = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,-10.0f} };
}

void Object3d::Update()
{
	// 毎フレーム、Object3dCom から最新のカメラを取得（初期化後に設定されたケースへ対応）
	if (object3dCom_)
	{
		camera_ = object3dCom_->GetDefaultCamera();
	}

	// 優先: Object3dComに設定されたカメラを使う
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

Object3d::MaterialData Object3d::LoadMaterialTemplateFile(const std::string& direcrotyPath, const std::string& filename)
{
	//中で必要になる変数の宣言
	Object3d::MaterialData materlialData;//構築するデータ
	std::string line;//ファイルから読み込んだ1行を格納するもの
	std::ifstream file(direcrotyPath + "/" + filename);//ファイルを開く
	assert(file.is_open());//ファイルが開けなかったら停止

	//MaterialDataを構築
	while (std::getline(file, line))
	{
		std::string identifile;
		std::istringstream s(line);
		s >> identifile; //先頭の識別子を取得

		//identifileに応じた処理
		if (identifile == "map_Kd")
		{
			std::string textureFilename;
			s >> textureFilename; //テクスチャファイル名を取得
			//連結してファイルパスにする
			materlialData.textureFilePath = direcrotyPath + "/" + textureFilename;
		}

	}



	return materlialData;
}

Object3d::ModelData Object3d::LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
	Object3d::ModelData modelData;
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;

	// aiProcess_Triangulate を追加して、三角形化を確実にする
	const aiScene* scene = importer.ReadFile(filePath.c_str(),
		aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate);

	if (!scene || !scene->HasMeshes())
	{
		assert(false && "Failed to load model");
		return modelData;
	}

	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
	{
		aiMesh* mesh = scene->mMeshes[meshIndex];

		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
		{
			aiFace& face = mesh->mFaces[faceIndex];
			for (uint32_t element = 0; element < face.mNumIndices; ++element)
			{
				uint32_t vertexIndex = face.mIndices[element];

				Sprite::VertexData vertex{};

				// 位置
				aiVector3D& pos = mesh->mVertices[vertexIndex];
				vertex.position = { pos.x, pos.y, pos.z, 1.0f };
				vertex.position.x *= -1.0f; // 前のコードに合わせて反転

				// 法線
				if (mesh->HasNormals())
				{
					aiVector3D& norm = mesh->mNormals[vertexIndex];
					vertex.normal = { norm.x, norm.y, norm.z };
					vertex.normal.x *= -1.0f;
				}

				// UV
				if (mesh->HasTextureCoords(0))
				{
					aiVector3D& tex = mesh->mTextureCoords[0][vertexIndex];
					vertex.texcoord = { tex.x, tex.y };
				}

				modelData.vertices.push_back(vertex);
			}
		}
	}

	// マテリアル（テクスチャ）の取得
	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
	{
		aiMaterial* material = scene->mMaterials[materialIndex];
		aiString texturePath;
		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS)
		{
			modelData.material.textureFilePath = directoryPath + "/" + std::string(texturePath.C_Str());
		}
	}

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
		materialData_->uvTransform = MakeIdentity4x4();
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
	// Create buffer before mapping and add safety checks
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
	}
}
