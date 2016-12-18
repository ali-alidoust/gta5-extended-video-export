#pragma once

#include <Windows.h>
#include <Psapi.h>
#include "yara-patterns.h"
#include <mutex>

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
	void* getPtr_getRenderTimeBase();

private:
	YR_COMPILER* pYrCompiler;
	YR_RULES* rules;

	void* _ptr_yara_get_render_time_base_function = NULL;

	HMODULE moduleHandle;
	MODULEINFO moduleInfo;
	std::mutex mxScan;
	std::condition_variable cvScan;
	bool isScanFinished = false;
	static int callback_function(int message, void* message_data, void* user_data);
};