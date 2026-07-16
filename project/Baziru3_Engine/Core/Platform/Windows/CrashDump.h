#pragma once

#include <Windows.h>
#include <Dbghelp.h>
#include <Strsafe.h>

#pragma comment(lib, "Dbghelp.lib")

class CrashDump
{
public:
	
	static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception);

	
	static void Install();
};

