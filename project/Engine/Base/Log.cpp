#include "Log.h"
#include <windows.h>
#include <iostream>

void Logger::Log(std::ostream& os, const std::string& message)
{
	os << message << std::endl;
	OutputDebugStringA(message.c_str()); //出力ウィンドウに文字を出力
}

