#include "Random.h"

//静的メンバの実体と初期化
std::random_device Random::seedGenerator_;
std::mt19937_64 Random::randomEngine_;

void Random::SeedEngine() { randomEngine_.seed(seedGenerator_()); }

float Random::GeneratorFloat(float min, float max)
{
	std::uniform_real_distribution<float> distribution(min, max);

	//乱数を返す
	return distribution(randomEngine_);
}