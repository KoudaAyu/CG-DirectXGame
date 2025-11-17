#pragma once

#include"ModelCom.h"

class Model
{
public:
	void Initialize(ModelCom* modelCom);

private:
	ModelCom* modelCom_ = nullptr;
};