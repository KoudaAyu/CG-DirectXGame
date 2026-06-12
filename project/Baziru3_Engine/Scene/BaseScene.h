#pragma once

#include <cstddef>

class DirectXCom;
class SceneManager;
class Camera;
struct SceneRenderRequests;

class BaseScene
{
public:
	virtual ~BaseScene() = default;

	virtual void Initialize(DirectXCom* dxCommon, Camera* camera) = 0;
	virtual void Finalize() = 0;
	virtual void Update() = 0;
    virtual void Draw(SceneRenderRequests& renderRequests) = 0;

	virtual void SetSceneManager(SceneManager* sceneManager)
	{
		sceneManager_ = sceneManager;
	}

	virtual const char* GetSceneType() const { return "BaseScene"; }

protected:
	SceneManager* sceneManager_ = nullptr;
};

