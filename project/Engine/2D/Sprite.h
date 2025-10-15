#pragma once

#include"DirectXCom.h"
#include"Matrix4x4.h"
#include"Vector.h"

class SpriteCom;

class Sprite
{
public:

	struct VertexData
	{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};



	void Initialize(SpriteCom* spriteCom);



private:
	DirectXCom* dxCommon = nullptr;
	SpriteCom* spriteCom = nullptr;

};