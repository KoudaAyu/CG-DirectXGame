#pragma once
#include<map>
#include <string>
#include <memory>
#include"DirectXCom.h"
#include "Model.h"
#include"ModelCom.h"
class ModelManager
{

public:
	// シングルトン取得・破棄
	static ModelManager* GetInstance();
	~ModelManager() = default;
	static void Destroy();

	void Initialize(DirectXCom* dxCommon);

	/// <summary>
	/// モデルファイルの読み込み
	/// </summary>
	/// <param name="filepath">モデルのファイルパス</param>
	Model* LoadModel(const std::string& filepath);

	/// <summary>
	/// モデルの検索
	/// </summary>
	/// <param name="filepath">モデルのファイルパス</param>
	/// <returns>モデル</returns>
	Model* FindModel(const std::string& filepath);

private:
	ModelManager() = default;

	// コピー・ムーブを禁止してインスタンスの複製を封印
	ModelManager(const ModelManager&) = delete;
	ModelManager& operator=(const ModelManager&) = delete;
	ModelManager(ModelManager&&) = delete;
	ModelManager& operator=(ModelManager&&) = delete;

	std::map<std::string, std::unique_ptr<Model>> models_;

	DirectXCom* dxCommon_ = nullptr;
	std::unique_ptr<ModelCom> modelCom_ = nullptr;
};
