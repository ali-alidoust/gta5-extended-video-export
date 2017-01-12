// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "script.h"
#include "logger.h"

std::string ExePath() {
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	return std::string(buffer).substr(0, pos);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		LOG(LL_DBG, "HI!!");
		SetDllDirectoryA((ExePath() + "\\EVE\\dlls\\").c_str());
		config::reload();
		if (!config::is_mod_enabled) {
			LOG(LL_NON, "Extended Video Export mod is disabled in the config file. Exiting...");
			return TRUE;
		}
		Logger::instance().level = config::log_level;
		LOG_CALL(LL_DBG,initialize());
		LOG(LL_NFO, "Registering script...");
		LOG_CALL(LL_DBG, presentCallbackRegister((void(*)(void*))onPresent));
		LOG_CALL(LL_DBG, scriptRegister(hModule, ScriptMain));
		break;
	case DLL_PROCESS_DETACH:
		LOG(LL_NFO, "Unregistering DXGI callback");
		LOG_CALL(LL_DBG, scriptUnregister(hModule));
		LOG_CALL(LL_DBG, presentCallbackUnregister((void(*)(void*))onPresent));
		LOG_CALL(LL_DBG, finalize());
		break;
	}
	return TRUE;
}

