#include"Model.h"
#include "BufferUtil.h"
#include<cassert>
#include<fstream>
#include<sstream>
#include<cstring>
#include "TextureManager.h"




void Model::Initialize(ModelCom* modelCom, const std::string& directoryPath, const std::string& filename)
{
    modelCom_ = modelCom;

    //Modelの読み込み
    modelData_ = LoadObjFile(directoryPath, filename);

    //頂点データとマテリアルの初期化
    VertexResource();
    MaterialResource();

    // テクスチャロードはパスが有効なときのみ実行
    if (!modelData_.material.textureFilePath.empty())
    {
        // TextureManagerにDirectXコンテキストが渡っていることを前提にする
        TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
        modelData_.material.textureIndex =
            TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData_.material.textureFilePath);
    }
}

void Model::Update()
{
    //今の所特に更新処理は無し
}

void Model::Bind(ID3D12GraphicsCommandList* commandList)
{
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // インデックスバッファがある場合はバインド
    if (indexResource && !modelData_.indices.empty())
    {
        commandList->IASetIndexBuffer(&indexBufferView);
    }

}

void Model::Draw()
{

    if (!modelCom_ || !modelCom_->GetDirectXCom())
    {
        return;
    }

    DirectXCom* dxCommon = modelCom_->GetDirectXCom();
    auto commandList = dxCommon->GetCommandList();
    if (!commandList)
    {
        return;
    }


    if (modelData_.vertices.empty())
    {
        return;
    }


    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // インデックスがある場合はインデックス描画
    if (!modelData_.indices.empty() && indexResource)
    {
        commandList->IASetIndexBuffer(&indexBufferView);
        UINT indexCount = static_cast<UINT>(modelData_.indices.size());
        commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
    }
    else
    {
        UINT vertexCount = static_cast<UINT>(modelData_.vertices.size());
        commandList->DrawInstanced(vertexCount, 1, 0, 0);
    }
}



Model::ModelData Model::LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
    //中で必要になる変数の宣言
    Model::ModelData modelData;//構築するデータ
    std::vector<Vector4>positions;//位置
    std::vector<Vector3>normals;//法線
    std::vector<Vector2>texcoords;//テクスチャ座標
    std::string line;//ファイルから読み込んだ1行を格納するもの

    //ファイルを開く
    std::ifstream file(directoryPath + "/" + filename);//ファイルを開く
    assert(file.is_open());//ファイルが開けなかったら停止

    //実際にファイルを読み込む。その後modelDataを構築する
    while (std::getline(file, line))
    {
        std::string identifile;
        std::istringstream s(line);
        s >> identifile; //先頭の識別子を取得

        //identifileに応じた処理
        if (identifile == "v")
        {
            Vector4 position;
            s >> position.x >> position.y >> position.z;
            // position.x *= -1.0f; // X反転は Object3d 側の挙動と合わせるため無効化
            position.w = 1.0f;
            positions.push_back(position);//位置を格納
        }
        else if (identifile == "vt")
        {
            Vector2 texcoord;
            s >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y; //Y軸を反転する
            texcoords.push_back(texcoord);//テクスチャ座標を格納
        }
        else if (identifile == "vn")
        {
            Vector3 normal;
            s >> normal.x >> normal.y >> normal.z;
            // normal.x *= -1.0f; // X反転は無効化
            normals.push_back(normal);//法線を格納
        }
        else if (identifile == "f")
        {
            //面は三角形限定。その他は未対応
            Sprite::VertexData triangle[3];

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
            }
            // Object3d 側と同じワインディング順で追加
            modelData.vertices.push_back(triangle[0]);
            modelData.vertices.push_back(triangle[1]);
            modelData.vertices.push_back(triangle[2]);
        }
        else if (identifile == "mtllib")
        {
            //materialTemplateLibraryファイルの名前を取得する
            std::string materialFilename;
            s >> materialFilename; //マテリアルファイル名を取得
            //基本的にobjファイルを同じ階層にmtlファイルがあるので、ディレクトリ名とファイル名を渡す
            modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
        }

    }

    // Extract skin/bone data using Assimp when available (OBJ doesn't contain bones)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(directoryPath + "/" + filename,
            aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

        if (scene && scene->HasMeshes())
        {
            for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
            {
                const aiMesh* mesh = scene->mMeshes[meshIndex];
                if (!mesh || !mesh->HasBones())
                    continue;

                for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
                {
                    const aiBone* bone = mesh->mBones[boneIndex];
                    if (!bone)
                        continue;

                    std::string jointName = bone->mName.C_Str();
                    JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

                    aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix;
                    bindPoseMatrixAssimp.Inverse();

                    aiVector3D scale, translate;
                    aiQuaternion rotate;
                    bindPoseMatrixAssimp.Decompose(scale, rotate, translate);

                    Matrix4x4 bindPoseMatrix = MakeAffineMatrix({ scale.x, scale.y, scale.z },
                        { rotate.x, -rotate.y, -rotate.z, rotate.w },
                        { -translate.x, translate.y, translate.z });

                    jointWeightData.inverseBindPoseMatrix = Inverse(bindPoseMatrix);

                    for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
                    {
                        jointWeightData.vertexWeights.push_back({ bone->mWeights[weightIndex].mWeight, bone->mWeights[weightIndex].mVertexId });
                    }
                }
            }
        }
    }

    return modelData;
}

Model::MaterialData Model::LoadMaterialTemplateFile(const std::string& direcrotyPath, const std::string& filename)
{
    //中で必要になる変数の宣言
    Model::MaterialData materlialData;//構築するデータ
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

void Model::VertexResource()
{
    if (modelCom_ && modelCom_->GetDirectXCom())
    {
        DirectXCom* dxCommon = modelCom_->GetDirectXCom();

        if (!modelData_.vertices.empty())
        {
            vertexResource = BufferUtil::CreateVertexBuffer(dxCommon, modelData_.vertices, vertexBufferView_);
        }
        else
        {
            size_t bufferSize = sizeof(Sprite::VertexData);
            vertexResource = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), bufferSize);
            vertexBufferView_.BufferLocation = vertexResource->GetGPUVirtualAddress();
            vertexBufferView_.SizeInBytes = static_cast<UINT>(bufferSize);
            vertexBufferView_.StrideInBytes = sizeof(Sprite::VertexData);
        }

        // --- インデックスバッファの作成とアップロード ---
        if (!modelData_.indices.empty())
        {
            indexResource = BufferUtil::CreateIndexBuffer(dxCommon, modelData_.indices, indexBufferView);
        }
    }
}

void Model::MaterialResource()
{
    if (modelCom_ && modelCom_->GetDirectXCom())
    {
        DirectXCom* dxCommon = modelCom_->GetDirectXCom();
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

Model::ModelData Model::LoadModelFile(const std::string& directoryPath, const std::string& filename)
{
	Model::ModelData modelData;
	Assimp::Importer importer;

	const std::string fullPath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(fullPath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);
	assert(scene != nullptr);
	assert(scene->mRootNode != nullptr);

	// メッシュごとの頂点ベースオフセットを記録
	std::vector<uint32_t> meshBaseIndices(scene->mNumMeshes, 0);

	// ノードを再帰的にたどってメッシュを結合
	auto AppendNodeMeshes = [&](auto& self, const aiNode* node) -> void {
		if (!node) return;
		for (uint32_t i = 0; i < node->mNumMeshes; ++i)
		{
			uint32_t meshIndex = node->mMeshes[i];
			const aiMesh* mesh = scene->mMeshes[meshIndex];
			if (!mesh) continue;

			uint32_t baseIndex = static_cast<uint32_t>(modelData.vertices.size());
			meshBaseIndices[meshIndex] = baseIndex;
			modelData.vertices.resize(baseIndex + mesh->mNumVertices);

			for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
			{
				Sprite::VertexData vertex{};
				if (mesh->HasPositions())
				{
					const aiVector3D& pos = mesh->mVertices[v];
					vertex.position = { pos.x, pos.y, pos.z, 1.0f };
				}
				if (mesh->HasTextureCoords(0))
				{
					const aiVector3D& uv = mesh->mTextureCoords[0][v];
					vertex.texcoord = { uv.x, uv.y };
				}
				if (mesh->HasNormals())
				{
					const aiVector3D& n = mesh->mNormals[v];
					vertex.normal = { n.x, n.y, n.z };
				}
				modelData.vertices[baseIndex + v] = vertex;
			}

			for (uint32_t fi = 0; fi < mesh->mNumFaces; ++fi)
			{
				const aiFace& face = mesh->mFaces[fi];
				if (face.mNumIndices == 3)
				{
					modelData.indices.push_back(baseIndex + face.mIndices[0]);
					modelData.indices.push_back(baseIndex + face.mIndices[1]);
					modelData.indices.push_back(baseIndex + face.mIndices[2]);
				}
				else
				{
					for (uint32_t k = 0; k < face.mNumIndices; ++k)
						modelData.indices.push_back(baseIndex + face.mIndices[k]);
				}
			}
		}
		for (uint32_t i = 0; i < node->mNumChildren; ++i)
			self(self, node->mChildren[i]);
	};

	AppendNodeMeshes(AppendNodeMeshes, scene->mRootNode);

	// マテリアル（テクスチャパス）読み込み
	if (scene->mNumMaterials > 0)
	{
		const aiMaterial* material = scene->mMaterials[0];
		aiString texturePath;
		if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) == aiReturn_SUCCESS ||
			material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == aiReturn_SUCCESS)
		{
			modelData.material.textureFilePath = directoryPath + "/" + texturePath.C_Str();
		}
	}

	// ボーン（スキン）ウェイト読み込み
	for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
	{
		const aiMesh* mesh = scene->mMeshes[meshIndex];
		if (!mesh || !mesh->HasBones()) continue;

		uint32_t baseIndex = meshBaseIndices[meshIndex];

		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
		{
			const aiBone* bone = mesh->mBones[boneIndex];
			if (!bone) continue;

			std::string jointName = bone->mName.C_Str();
			Model::JointWeightData& jwd = modelData.skinClusterData[jointName];

			// InverseBindPoseMatrix
			aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix;
			bindPoseMatrixAssimp.Inverse();
			aiVector3D scale, translate;
			aiQuaternion rotate;
			bindPoseMatrixAssimp.Decompose(scale, rotate, translate);
			Matrix4x4 bindPoseMatrix = MakeAffineMatrix(
				{ scale.x, scale.y, scale.z },
				{ rotate.x, rotate.y, rotate.z, rotate.w },
				{ translate.x, translate.y, translate.z });
			jwd.inverseBindPoseMatrix = Inverse(bindPoseMatrix);

			// 頂点ウェイト
			for (uint32_t wi = 0; wi < bone->mNumWeights; ++wi)
			{
				jwd.vertexWeights.push_back({
					bone->mWeights[wi].mWeight,
					bone->mWeights[wi].mVertexId + baseIndex
				});
			}
		}
	}

	return modelData;
}