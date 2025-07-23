#include "StringConverter.h"

std::wstring StringConverter::ConvertString(const std::string& str)
{
    if (str.empty())
    {
        return std::wstring();
    }

    // CP_UTF8 を使用しているので、UTF-8 から UTF-16 (std::wstring) への変換
    auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(str.data()), static_cast<int>(str.size()), NULL, 0);
    if (sizeNeeded == 0)
    {
        // エラーハンドリング: GetLastError() で詳細を確認可能
        return std::wstring();
    }
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(str.data()), static_cast<int>(str.size()), &result[0], sizeNeeded);
    return result;
}

std::string StringConverter::ConvertString(const std::wstring& str)
{
    if (str.empty())
    {
        return std::string();
    }

    // UTF-16 (std::wstring) から UTF-8 (std::string) へ変換
    auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
    if (sizeNeeded == 0)
    {
        // エラーハンドリング: GetLastError() で詳細を確認可能
        return std::string();
    }
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
    return result;
}