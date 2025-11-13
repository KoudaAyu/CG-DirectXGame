#pragma once
#include"Camera.h"

class Object3dCom
{
	public:
		void Initialize();
		void Update();

public:
	void SetDefaultCamera(Camera* camera)
	{
		defaultCamera_ = camera;
	}
	Camera* GetDefaultCamera() const
	{
		return defaultCamera_;
	}

private:
	Camera* defaultCamera_ = nullptr;
};
