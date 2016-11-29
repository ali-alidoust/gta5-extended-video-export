// dllmain.cpp : Defines the entry point for the DLL application.

#pragma comment(lib, "lib\\ScriptHookV.lib")
#include <sstream>
#include "inc\main.h"
#include "script.h"
#include "logger.h"
#include "config.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	std::stringstream stream;
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		LOG("Starting up...");
		LOG("Loading config...");
		stream << TARGET_NAME;
		stream << ".ini";
		scriptRegister(hModule, ScriptMain);
		break;
	case DLL_PROCESS_DETACH:
		unhookAll();
		scriptUnregister(hModule);
		break;
	}
	return TRUE;
}

