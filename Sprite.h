#pragma once
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
	void Update();
	void Draw();

private:
	SpriteCom* spriteCom = nullptr;
};
