#pragma once
#include <memory>
#include <string>
#include <ostream>

struct InitConfig
{
	std::string resourcesPath = "Resources";
	bool enableImGui = true;
};


class DirectXCom;
class SpriteCom;
class SpriteManager;
class WindowAPI;

struct SubsystemResult
{
	bool success = false;
	std::string errorMessage;

	//所有権を動かすためのunique_ptr
	std::unique_ptr<DirectXCom> directXCom;
	std::unique_ptr<SpriteCom> spriteCom;
	std::unique_ptr<SpriteManager> spriteManager;
	std::unique_ptr<WindowAPI> windowAPI;
};

class SubsystemFactory
{
public:
	static SubsystemResult InitializeAll(std::ostream& logStream, const InitConfig& cfg);
	static void FinalizeAll(SubsystemResult& r, std::ostream& logStream);

};

