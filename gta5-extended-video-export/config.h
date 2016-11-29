#pragma once

#include <ini.h>
#include <cpp\INIReader.h>

#define CFG_XVX_SECTION "XVX"
#define CFG_LOSSLESS_RENDER "lossless_render"
#define CFG_OUTPUT_DIR "output_folder"
#define INI_FILE_NAME TARGET_NAME ".ini"

class Config {
public:

	static INIReader& config() {
		return instance().ini;
	}

	Config(Config const&) = delete;
	void operator=(Config const&) = delete;

private:

	Config() {};
	~Config() {};

	INIReader ini = INIReader(INI_FILE_NAME);

	static Config& instance()
	{
		static Config cfg;
		return cfg;
	}
};