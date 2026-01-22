#pragma once
#include"Transform.h"

class PointLight
{
	struct PointLightData
	{
		Vector4 color;//!< ライトの色
		Vector3 position;//!< ライトの位置
		float intensity;//!< 輝度
	};
};
