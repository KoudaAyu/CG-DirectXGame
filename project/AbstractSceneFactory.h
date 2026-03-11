#pragma once

#include"BaseScene.h"
#include<string>

class AbstractSceneFactory
{
public:
	//仮想デストラクタ
	virtual ~AbstractSceneFactory() = default;
	//シーンの生成
	virtual BaseScene* CreateScene(const std::string& sceneName) = 0;

	virtual void Register(const std::string& sceneName,std::function<BaseScene*()> creator) = 0;
};

