#pragma once

#include <ini.hpp>
#include <string>
#include <algorithm>
#include <sstream>
#include <ShlObj.h>
#include <regex>

#define CFG_XVX_SECTION "XVX"
#define CFG_LOSSLESS_EXPORT "lossless_export"
#define CFG_OUTPUT_DIR "output_folder"
//#define CFG_USE_D3D_CAPTURE "use_d3d_capture"
#define CFG_EXPORT_RESOLUTION "resolution"
#define INI_FILE_NAME TARGET_NAME ".ini"

class Config {
public:

	bool isLosslessExportEnabled() {
		return stringToBoolean(parser.top()[CFG_LOSSLESS_EXPORT], true);
	}

	/*bool isUseD3DCaptureEnabled() {
		return stringToBoolean(parser.top()[CFG_USE_D3D_CAPTURE], false);
		
	}*/

	std::pair<uint32_t, uint32_t> exportResolution() {
		std::string string = parser.top()[CFG_EXPORT_RESOLUTION];
		string = std::regex_replace(string, std::regex("\\s+"), "");

		if (string.empty()) {
			return std::make_pair(0, 0);
		}

		std::smatch match;
		if (!std::regex_match(string, match, std::regex("^(\\d+)x(\\d+)$"))) {
			LOG("Could not parse resolution: ", string);
			return std::make_pair(0, 0);
		}

		uint32_t width  = std::stoul(match[1]);
		uint32_t height = std::stoul(match[2]);

		return std::make_pair(width, height);
	}

	std::stringstream outputDir() {
		std::stringstream stream;

		std::string stringValue = parser.top()[CFG_OUTPUT_DIR];
		stringValue = std::regex_replace(stringValue, std::regex("(^\\s*)|(\\s*$)"), "");

		if ((!stringValue.empty()) && (stringValue.find_first_not_of(' ') != std::string::npos))
		{
			stream << stringValue;
		} else {
			char buffer[MAX_PATH];
			LOG_IF_FAILED(GetVideosDirectory(buffer), "Failed to get Videos directory for the current user.");
			stream << buffer;
		}

		

		return stream;
	}

	static Config& instance()
	{
		static Config cfg;
		return cfg;
	}

	Config(Config const&) = delete;
	void operator=(Config const&) = delete;

private:
	INI::Parser parser = INI::Parser(INI_FILE_NAME);

	Config() {};
	~Config() {};

	static HRESULT GetVideosDirectory(LPSTR output)
	{
		PWSTR vidPath = NULL;

		RET_IF_FAILED((SHGetKnownFolderPath(FOLDERID_Videos, 0, NULL, &vidPath) != S_OK), "Failed to get Videos directory for the current user.", E_FAIL);

		int pathlen = lstrlenW(vidPath);

		int buflen = WideCharToMultiByte(CP_UTF8, 0, vidPath, pathlen, NULL, 0, NULL, NULL);
		if (buflen <= 0)
		{
			return E_FAIL;
		}

		buflen = WideCharToMultiByte(CP_UTF8, 0, vidPath, pathlen, output, buflen, NULL, NULL);

		output[buflen] = 0;

		CoTaskMemFree(vidPath);

		return S_OK;
	}

	static bool stringToBoolean(std::string booleanString, bool defaultValue) {
		if (booleanString.empty()) {
			return defaultValue;
		}
		std::transform(booleanString.begin(), booleanString.end(), booleanString.begin(), ::tolower);

		bool value;
		std::istringstream(booleanString) >> std::boolalpha >> value;
		return value;
	}
};