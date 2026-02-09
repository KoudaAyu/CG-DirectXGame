#pragma once
#include <string>
#include <ostream>
#include <fstream>
#include"StringUtil.h"

namespace Logger
{
	void Log(std::ostream& os, const std::string& message);
}

class Log
{
public:
	void Initialize();

	std::ostream& GetLogStream() { return logStream; }

private:
	std::ofstream logStream;
};