// dllmain.cpp : Defines the entry point for the DLL application.
#include "encoder.h"
#include "logger.h"
#include "script.h"
#include "stdafx.h"

std::wstring ExePath() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
    return std::wstring(buffer).substr(0, pos);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        LOG(LL_ERR, "HI!!");
        SetDllDirectoryW((ExePath() + L"\\EVE\\dlls\\").c_str());
        config::reload();
        if (!config::is_mod_enabled) {
            LOG(LL_NON, "Extended Video Export mod is disabled in the config file. Exiting...");
            return TRUE;
        } else {
            LOG(LL_NON, "Extended Video Export mod is enabled. Initializing...");
        }
        Logger::instance().level = config::log_level;
        LOG_CALL(LL_DBG, initialize());
        LOG(LL_NFO, "Registering script...");
        LOG_CALL(LL_DBG, presentCallbackRegister((void (*)(void*))onPresent));
        LOG_CALL(LL_DBG, scriptRegister(hModule, ScriptMain));
        LOG_CALL(LL_DBG, keyboardHandlerRegister(onKeyboardMessage));
        break;
    case DLL_PROCESS_DETACH:
        LOG(LL_NFO, "Unregistering DXGI callback");
        LOG_CALL(LL_DBG, scriptUnregister(hModule));
        LOG_CALL(LL_DBG, presentCallbackUnregister((void (*)(void*))onPresent));
        LOG_CALL(LL_DBG, keyboardHandlerUnregister(onKeyboardMessage));
        LOG_CALL(LL_DBG, finalize());
        break;
    }
    return TRUE;
}
