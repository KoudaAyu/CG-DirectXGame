#pragma once
#include <random>
#include <numbers>

class Random
{
private:

	//乱数生成エンジン
	static std::random_device seedGenerator_;
	
	static std::mt19937_64 randomEngine_;

public:

	static void SeedEngine();
	static float GeneratorFloat(float min, float max);
};