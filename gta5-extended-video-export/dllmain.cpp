// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "script.h"
#include "logger.h"
#include "config.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	if (!Config::instance().isModEnabled()) {
		LOG(LL_NON, "Extended Video Export mod is disabled in the config file. Exiting...");
		return TRUE;
	}

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		initialize();
		Logger::instance().level = Config::instance().logLevel();
		LOG(LL_NFO, "Registering script...");
		//if (Config::instance().isUseD3DCaptureEnabled()) {
		LOG_CALL(LL_DBG, presentCallbackRegister((void(*)(void*))onPresent));
		//}
		LOG_CALL(LL_DBG, scriptRegister(hModule, ScriptMain));
		break;
	case DLL_PROCESS_DETACH:
		LOG(LL_NFO, "Unregistering DXGI callback");
		//if (Config::instance().isUseD3DCaptureEnabled()) {
		LOG_CALL(LL_DBG, presentCallbackUnregister((void(*)(void*))onPresent));
		//}
		LOG_CALL(LL_DBG, finalize());
		LOG_CALL(LL_DBG, scriptUnregister(hModule));
		break;
	}
	return TRUE;
}

