#include "util.h"
#include "logger.h"
#include <filesystem>

namespace {
void DummyFunction() {}
} // namespace

std::string AsiPath() {
    wchar_t buffer[MAX_PATH];
    HMODULE hModule = nullptr;

    if (FAILED(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  reinterpret_cast<LPWSTR>(&DummyFunction), &hModule))) {
        const DWORD err = ::GetLastError();
        LOG(LL_ERR, "Failed to get module handle:", err, " ", Logger::hex(err, 8));
    }

    if (FAILED(GetModuleFileNameW(hModule, buffer, MAX_PATH))) {
        const DWORD err = ::GetLastError();
        LOG(LL_ERR, "Failed to get module file name:", err, " ", Logger::hex(err, 8));
    }

    const std::filesystem::path path(buffer);
    return path.parent_path().string();
}