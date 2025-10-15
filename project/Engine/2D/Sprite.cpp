#include"Sprite.h"
#include"SpriteCom.h"

#include<cassert>

void Sprite::Initialize(SpriteCom* spriteCom)
{
	this->spriteCom = spriteCom;
	assert(spriteCom);


}
