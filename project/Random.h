#pragma once
#include <random>
#include <numbers>

class Random
{
private:

	//乱数生成エンジン
	static std::random_device seedGenerator_;
	//メルセンヌ・ツイスターエンジン(64bit版)
	static std::mt19937_64 randomEngine_;

public:

	static void SeedEngine();
	static float GeneratorFloat(float min, float max);
};
#pragma once