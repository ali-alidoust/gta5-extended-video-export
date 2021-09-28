#pragma once

#include "yara-patterns.h"
#include <Windows.h>
#include <Psapi.h>
#include <map>
#include <mutex>

// extern "C" {
//#include <yara\libyara.h>
//#include <yara\rules.h>
//#include <yara\compiler.h>
//}

class YaraHelper {
  public:
    void initialize();
    void performScan();

    void addEntry(std::string name, std::string pattern, uint64_t *dest);

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
    // std::mutex mxScan;
    // std::condition_variable cvScan;
    // bool isScanFinished = false;
};