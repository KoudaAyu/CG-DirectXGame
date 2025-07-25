#pragma once
#include <iostream>
#include <windows.h>

class DebugLog
{
public:
	static void Log(std::ostream& os, const std::string& message);
};
