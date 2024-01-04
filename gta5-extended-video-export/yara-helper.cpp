#include "yara-helper.h"

#include "logger.h"
#include "polyhook2/ErrorLog.hpp"

#include <polyhook2/Misc.hpp>
#include <utility>

void YaraHelper::Initialize() {
    PRE();
    module_handle_ = ::GetModuleHandle(nullptr);
    const HANDLE processHandle = ::GetCurrentProcess();
    module_info_ = {
        .lpBaseOfDll = nullptr,
        .SizeOfImage = 0,
        .EntryPoint = nullptr,
    };

    if (GetModuleInformation(processHandle, module_handle_, &module_info_, sizeof(module_info_)) !=
        TRUE) {
        const DWORD err = ::GetLastError();
        LOG(LL_ERR, "Failed to get module information:", err, " ", Logger::hex(err, 8));
    }
    LOG(LL_NFO, "Process: base:", Logger::hex(reinterpret_cast<uint64_t>(module_info_.lpBaseOfDll), 16),
        " size:", Logger::hex(module_info_.SizeOfImage, 16));

    POST();
}

void YaraHelper::PerformScan() {
    PRE();

    for (std::pair<std::string, Entry> item : entries_) {
        const auto address = PLH::findPattern(reinterpret_cast<uint64_t>(this->module_info_.lpBaseOfDll),
                                              this->module_info_.SizeOfImage, item.second.pattern.c_str());
        *item.second.dest = address;
        if (address) {
            LOG(LL_NFO, "Found matching address: ", item.first);
        } else {
            LOG(LL_NFO, "Matching address not found: ", item.first);
        }
    }
    POST();
}

void YaraHelper::AddEntry(std::string const& name, std::string const& pattern, uint64_t* dest) {
    entries_[name] = Entry{.pattern = pattern, .dest = dest};
}