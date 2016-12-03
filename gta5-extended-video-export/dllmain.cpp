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
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		LOG("Registering script...");
		if (Config::instance().isUseD3DCaptureEnabled()) {
			LOG_CALL(presentCallbackRegister((void(*)(void*))onPresent));
		}
		LOG_CALL(scriptRegister(hModule, ScriptMain));
		break;
	case DLL_PROCESS_DETACH:
		LOG("Unregistering DXGI callback");
		if (Config::instance().isUseD3DCaptureEnabled()) {
			LOG_CALL(presentCallbackUnregister((void(*)(void*))onPresent));
		}
		LOG_CALL(unhookAll());
		LOG_CALL(scriptUnregister(hModule));
		break;
	}
	return TRUE;
}

