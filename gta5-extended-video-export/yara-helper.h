#pragma once

#define NOMINMAX
#include <Windows.h>
#include <map>
#include <string>

// Must be included after <Windows.h>
#include <Psapi.h>

class YaraHelper {
  public:
    void Initialize();
    void PerformScan();
    void AddEntry(std::string const& name, std::string const& pattern, uint64_t* dest);

  private:
    struct Entry {
        std::string pattern;
        uint64_t* dest;
    };

    std::map<std::string, Entry> entries_{};
    HMODULE module_handle_{nullptr};
    MODULEINFO module_info_{};
};