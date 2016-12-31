#pragma once

#include <Windows.h>
#include <Psapi.h>
#include "yara-patterns.h"
#include <mutex>
#include <map>

extern "C" {
#include <yara\libyara.h>
#include <yara\rules.h>
#include <yara\compiler.h>
}

class YaraHelper {
public:

	~YaraHelper();

	void initialize();
	void performScan();

	void addEntry(std::string name, std::string pattern, void** dest);

private:

	struct entry {
		entry(std::string name, std::string pattern, void** dest):
			name(name),
			pattern(pattern),
			dest(dest)
		{ }

		entry() { }

		std::string name;
		std::string pattern;
		void** dest;
	};

	std::map<std::string, entry> entries;

	YR_COMPILER* pYrCompiler;
	YR_RULES* rules;

	HMODULE moduleHandle;
	MODULEINFO moduleInfo;
	std::mutex mxScan;
	std::condition_variable cvScan;
	bool isScanFinished = false;
	static int callback_function(int message, void* message_data, void* user_data);
	static std::string build_yara_rule(std::string name, std::string pattern);
};