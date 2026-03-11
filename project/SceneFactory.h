#pragma once

#include"AbstractSceneFactory.h"
#include<unordered_map>
#include<functional>

class SceneFactory : public AbstractSceneFactory
{
public:
    /// <summary>
    /// シーン生成
    /// </summary>
    /// <param name="sceneName">シーン名</param>
    /// <returns>生成したシーン</returns>
    BaseScene* CreateScene(const std::string& sceneName) override;

    void Register(const std::string& sceneName, std::function<BaseScene*()> creator) override;

private:
    std::unordered_map<std::string, std::function<BaseScene* ()>> creators_;
};

