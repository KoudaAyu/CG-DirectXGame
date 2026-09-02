#pragma once

#include "AbstractSceneFactory.h"
#include <unordered_map>
#include <functional>
#include <memory>

class SceneFactory : public AbstractSceneFactory
{
public:
    /// <summary>
    /// シーン生成
    /// </summary>
    /// <param name="sceneName">シーン名</param>
    /// <returns>生成したシーン</returns>
    std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) override;

    void Register(const std::string& sceneName, std::function<std::unique_ptr<BaseScene>()> creator) override;
    std::vector<std::string> GetRegisteredSceneNames() const override;

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<BaseScene>()>> creators_;
};

