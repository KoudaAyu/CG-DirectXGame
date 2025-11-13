#pragma once
#include"Camera.h"
#include"Object3dCom.h"
#include"Transform.h"
class Object3d
{
public:

	void Initialize();

	void Update();

public:
	void SetCamera(Camera* camera)
	{
		camera_ = camera;
	}
	Camera* GetCamera() const
	{
		return camera_;
	}
private:
	Camera* camera_ = nullptr;
	Object3dCom* object3dCom_ = nullptr;
	Transform transform_;
};