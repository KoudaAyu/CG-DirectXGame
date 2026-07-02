#include "SceneFactory.h"
#include "Log.h"

#include <format>
#include <iostream>

std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName)
{
    auto it = creators_.find(sceneName);

    if (it == creators_.end())
    {
        Logger::Log(std::cout, std::format("SceneFactory: unknown scene '{}'\n", sceneName));
        return nullptr;
    }

    // 登録された生成関数を呼んで返す
    return it->second();
}

void SceneFactory::Register(const std::string& sceneName, std::function<std::unique_ptr<BaseScene>()> creator)
{
    creators_[sceneName] = std::move(creator);
}
