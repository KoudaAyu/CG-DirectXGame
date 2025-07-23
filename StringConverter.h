#pragma once
#include <string>
#include <windows.h> // MultiByteToWideChar, WideCharToMultiByte のために必要

class StringConverter
{
public:
    // std::string (UTF-8) から std::wstring (UTF-8) へ変換
    static std::wstring ConvertString(const std::string& str);

    // std::wstring (UTF-8) から std::string (UTF-8) へ変換
    static std::string ConvertString(const std::wstring& str);
};

