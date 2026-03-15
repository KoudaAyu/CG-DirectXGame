#include "ModelManager.h"

namespace {
    static std::unique_ptr<ModelManager>& ModelManagerStorage()
    {
        static std::unique_ptr<ModelManager> instance;
        return instance;
    }
}

ModelManager* ModelManager::GetInstance()
{
    auto& instance = ModelManagerStorage();
    if (!instance)
    {
        instance.reset(new ModelManager());
    }
    return instance.get();
}

void ModelManager::Destroy()
{
    ModelManagerStorage().reset();
}

void ModelManager::Initialize(DirectXCom* dxCommon)
{
    dxCommon_ = dxCommon;
    modelCom_ = std::make_unique<ModelCom>();
    modelCom_->Initialize(dxCommon_);
}

Model* ModelManager::LoadModel(const std::string& filepath)
{
    // すでに読み込まれている場合は保持しているポインタを返す
    auto it = models_.find(filepath);
    if (it != models_.end())
    {
        return it->second.get();
    }

    //モデルの生成とファイル読み込み、初期化
    std::unique_ptr<Model> model = std::make_unique<Model>();
    // ファイルパスがdirectory/filenameの形式と仮定して分割して渡す
    // シンプルにfilepathの最後の"/"以降をfilenameとする
    size_t pos = filepath.find_last_of('/');
    size_t posBack = filepath.find_last_of('\\');
    if (posBack != std::string::npos && (posBack > pos)) pos = posBack;

    std::string directory;
    std::string filename;
    if (pos == std::string::npos)
    {
        directory = "";
        filename = filepath;
    }
    else
    {
        directory = filepath.substr(0, pos);
        filename = filepath.substr(pos + 1);
    }

    assert(modelCom_);
    model->Initialize(modelCom_.get(), directory, filename);

    //モデルをmapコンテナに格納する
    Model* ret = model.get();
    models_.insert(std::make_pair(filepath, std::move(model)));
    return ret;
}

Model* ModelManager::FindModel(const std::string& filepath)
{
    //読み込み済みモデルを検索
    if (models_.contains(filepath))
    {
        //読み込みモデルを戻り値としてreturn
        return models_.at(filepath).get();
    }

    //見つからなかった場合はnullptrをreturn
    return nullptr;
}
