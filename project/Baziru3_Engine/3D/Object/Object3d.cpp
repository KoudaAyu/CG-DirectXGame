#include"Object3d.h"
#include"Object3dCom.h"
#include"Matrix4x4.h"
#include<cassert>
#include<fstream> // 追加: mtlファイル読み込み
#include<sstream> // 追加: 行分解用



void Object3d::Initialize(Object3dCom* object3dCom)
{
	object3dCom_ = object3dCom;
    // object3dCom_ が未設定の場合は何もしない(デフォルトカメラは取得しない)
    if (object3dCom_)
    {
        camera_ = object3dCom_->GetDefaultCamera();
    }
}

void Object3d::Update()
{
    Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.GetScale(), transform_.GetRotate(), transform_.GetTranslate());
    Matrix4x4 worldViewProjectionMatrix;
    if (camera_)
    {
        const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
        worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
    }
    else
    {
        worldViewProjectionMatrix = worldMatrix;
    }

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
	//中で必要になる変数の宣言
	Object3d::ModelData modelData;//構築するデータ
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
			position.x *= -1.0f; //X軸を反転する
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
			normal.x *= -1.0f; //X軸を反転する
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
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
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

	return modelData;
}


