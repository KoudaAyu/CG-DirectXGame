#include "EngineAssert.h"
#include <windows.h>
#include <string>
#include <sstream>

#ifdef ASSERT_ENABLED

bool ReportAssertionFailure(const char* expression, const char* message, const char* file, int line)
{
    auto toWideString = [](const std::string& str) -> std::wstring {
        if (str.empty()) return L"";
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    };

    std::wstringstream wss;
    wss << L"Assertion Failed!\n\n"
        << L"Expression: " << toWideString(expression) << L"\n"
        << L"Message: " << toWideString(message) << L"\n\n"
        << L"File: " << toWideString(file) << L"\n"
        << L"Line: " << line << L"\n\n"
        << L"Press 'Abort' to terminate the application.\n"
        << L"Press 'Retry' to trigger a breakpoint in your debugger.\n"
        << L"Press 'Ignore' to skip this assertion and continue running.";

    int result = MessageBoxW(
        NULL,
        wss.str().c_str(),
        L"Assertion Failure",
        MB_ABORTRETRYIGNORE | MB_ICONERROR | MB_DEFBUTTON2 | MB_SETFOREGROUND
    );

    if (result == IDABORT)
    {
        ExitProcess(3);
    }
    else if (result == IDRETRY)
    {
        return true;
    }

    return false;
}

#endif
