#include "encoder.h"
#include "logger.h"
#include "script.h"
#include "stdafx.h"
#include "polyhook2/ErrorLog.hpp"

#include <filesystem>

class PolyHookLogger : public PLH::Logger {
    void log(const std::string& msg, PLH::ErrorLevel level) override {
        switch (level) {
        case PLH::ErrorLevel::INFO:
            LOG(LL_TRC, msg);
            break;
        case PLH::ErrorLevel::WARN:
            LOG(LL_WRN, msg);
            break;
        case PLH::ErrorLevel::SEV:
            LOG(LL_ERR, msg);
            break;
        case PLH::ErrorLevel::NONE:
        default:
            LOG(LL_DBG, msg);
            break;
        }
    }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        LOG(LL_NON, std::filesystem::current_path());
        SetDllDirectoryW(utf8_decode(AsiPath() + R"(\EVE\dlls\)").c_str());
        LOG(LL_DBG, "Initializing PolyHook logger");
        PLH::Log::registerLogger(std::make_unique<PolyHookLogger>());
        config::reload();
        if (!config::is_mod_enabled) {
            LOG(LL_NON, "Extended Video Export mod is disabled in the config file. Exiting...");
            return TRUE;
        } else {
            LOG(LL_NON, "Extended Video Export mod is enabled. Initializing...");
        }
        Logger::instance().level = config::log_level;
        LOG_CALL(LL_DBG, eve::initialize());
        LOG(LL_NFO, "Registering script...");
        LOG_CALL(LL_DBG, scriptRegister(hModule, eve::ScriptMain));
        break;
    case DLL_PROCESS_DETACH:
        LOG(LL_NFO, "Unregistering DXGI callback");
        LOG_CALL(LL_DBG, scriptUnregister(hModule));
        LOG_CALL(LL_DBG, eve::finalize());
        break;
    }
    return TRUE;
}
