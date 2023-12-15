#include "yara-helper.h"
#include "logger.h"
#include <polyhook2/Misc.hpp>
#include <utility>

void YaraHelper::initialize() {
    PRE();
    this->moduleHandle = ::GetModuleHandle(nullptr);
    const HANDLE processHandle = ::GetCurrentProcess();
    this->moduleInfo = {0};
    LOG(LL_DBG, sizeof(this->moduleInfo.lpBaseOfDll));
    if (GetModuleInformation(processHandle, this->moduleHandle, &this->moduleInfo, sizeof(this->moduleInfo)) != TRUE) {
        const DWORD err = ::GetLastError();
        LOG(LL_ERR, "Failed to get module information:", err, " ", Logger::hex(err, 8));
    }

    LOG(LL_NFO, "Process: base:", Logger::hex(reinterpret_cast<uint64_t>(this->moduleInfo.lpBaseOfDll), 16),
        " size:", Logger::hex(this->moduleInfo.SizeOfImage, 16));

    POST();
}

void YaraHelper::performScan() {
    PRE();

    for (std::pair<std::string, entry> item : entries) {
        const auto address = PLH::findPattern(reinterpret_cast<uint64_t>(this->moduleInfo.lpBaseOfDll), this->moduleInfo.SizeOfImage,
                                              item.second.pattern.c_str());
        *item.second.dest = address;
        if (address) {
            LOG(LL_NFO, "Found matching address: ", item.first);
        } else {
            LOG(LL_NFO, "Matching address not found: ", item.first);
        }
    }
    POST();
}

void YaraHelper::addEntry(const std::string& name, std::string pattern, uint64_t *dest) {
    entries[name] = entry(name, std::move(pattern), dest);
}