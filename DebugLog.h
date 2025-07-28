#pragma once
#include <fstream>
#include <filesystem>
#include <iostream>
#include <windows.h>

class DebugLog
{
public:
	void Initialize();

	static void Log(std::ostream& os, const std::string& message);
	
	void Info(const std::string& message);

	std::ofstream& GetStream() { return logStream; }

private:
	std::ofstream logStream;
};
