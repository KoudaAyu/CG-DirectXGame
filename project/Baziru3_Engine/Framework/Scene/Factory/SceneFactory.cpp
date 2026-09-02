#include "SceneFactory.h"
#include "Baziru3_Engine/Framework/Scene/EngineDefaultScene.h"
#include "Log.h"

#include <format>
#include <iostream>

std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName)
{
    if (sceneName == "DEFAULT" || sceneName.empty())
    {
        return std::make_unique<EngineDefaultScene>();
    }

    auto it = creators_.find(sceneName);

    if (it == creators_.end())
    {
        Logger::Log(std::cout, std::format("SceneFactory: unknown scene '{}', falling back to EngineDefaultScene\n", sceneName));
        return std::make_unique<EngineDefaultScene>();
    }

    // 登録された生成関数を呼んで返す
    return it->second();
}

void SceneFactory::Register(const std::string& sceneName, std::function<std::unique_ptr<BaseScene>()> creator)
{
    creators_[sceneName] = std::move(creator);
}

std::vector<std::string> SceneFactory::GetRegisteredSceneNames() const
{
    std::vector<std::string> names;
    names.reserve(creators_.size() + 1);
    names.push_back("DEFAULT");
    for (const auto& [name, _] : creators_)
    {
        names.push_back(name);
    }
    return names;
}
