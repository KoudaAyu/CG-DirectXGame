#pragma once

#include <Windows.h>
#include<string>

class StringUtil
{
public:
	static std::wstring ConvertString(const std::string& str);
	static std::string ConvertString(const std::wstring& str);
};

