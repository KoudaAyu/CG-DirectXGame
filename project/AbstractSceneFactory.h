#pragma once

#include "BaseScene.h"
#include <string>
#include <functional>
#include <memory>

class AbstractSceneFactory
{
public:
    //仮想デストラクタ
    virtual ~AbstractSceneFactory() = default;
  
    virtual std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) = 0;

   
    virtual void Register(const std::string& sceneName, std::function<std::unique_ptr<BaseScene>()> creator) = 0;
};

