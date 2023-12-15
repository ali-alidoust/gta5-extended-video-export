#pragma once

#include "yara-patterns.h"
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#include <map>
#include <mutex>

class YaraHelper {
  public:
    void initialize();
    void performScan();

    void addEntry(const std::string& name, std::string pattern, uint64_t *dest);

  private:
    struct entry {
        entry(std::string name, std::string pattern, uint64_t *dest) : name(name), pattern(pattern), dest(dest) {}

        entry() {}

        std::string name;
        std::string pattern;
        uint64_t *dest;
    };

    std::map<std::string, entry> entries;

    HMODULE moduleHandle;
    MODULEINFO moduleInfo;
};