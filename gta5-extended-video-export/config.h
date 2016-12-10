#pragma once

#include <ini.hpp>
#include <string>
#include <algorithm>
#include <sstream>
#include <ShlObj.h>
#include <regex>

#define CFG_XVX_SECTION "XVX"
#define CFG_AUTO_RELOAD_CONFIG "auto_reload_config"
#define CFG_ENABLE_XVX "enable_mod"
#define CFG_OUTPUT_DIR "output_folder"
#define CFG_EXPORT_RESOLUTION "resolution"
#define CFG_VIDEO_ENC "video_enc"
#define CFG_VIDEO_FMT "video_fmt"
#define CFG_VIDEO_CFG "video_cfg"

//#define CFG_AUDIO_CODEC "audio_codec"
#define INI_FILE_NAME TARGET_NAME ".ini"

class Config {
public:

	bool isModEnabled() {
		return this->is_mod_enabled;
	}

	std::pair<uint32_t, uint32_t> exportResolution() {
		return this->resolution;
	}

	std::string outputDir() {
		return this->output_dir;
	}

	std::string videoEnc() {
		return video_enc;
	}

	std::string videoFmt() {
		return video_fmt;
	}

	std::string videoCfg() {
		return video_cfg;
	}

	void reload() {
		parser.reset(new INI::Parser(INI_FILE_NAME));

		this->parse_lossless_export();
		this->parse_auto_reload_config();
		this->parse_output_dir();
		this->parse_resolution();
		this->parse_video_enc();
		this->parse_video_fmt();
		this->parse_video_cfg();
	}

	static Config& instance()
	{
		static Config cfg;
		return cfg;
	}

	Config(Config const&) = delete;
	void operator=(Config const&) = delete;

private:
	std::unique_ptr<INI::Parser> parser;

	Config() {
		this->reload();
	};
	~Config() {};

	bool                            is_mod_enabled             = true;
	bool							auto_reload_config         = true;
	std::pair<uint32_t, uint32_t>   resolution                 = std::make_pair(0, 0);
	std::string                     output_dir                 = "";
	std::string                     video_enc                  = "";
	std::string                     video_fmt                  = "";
	std::string                     video_cfg                  = "";


	void parse_lossless_export() {
		is_mod_enabled = stringToBoolean(parser->top()[CFG_ENABLE_XVX], true);
	}

	void parse_auto_reload_config() {
		auto_reload_config = stringToBoolean(parser->top()[CFG_AUTO_RELOAD_CONFIG], true);
	}

	void parse_resolution() {
		std::string string = parser->top()[CFG_EXPORT_RESOLUTION];
		string = std::regex_replace(string, std::regex("\\s+"), "");

		if (string.empty()) {
			resolution = std::make_pair(0, 0);
			return;
		}

		std::smatch match;
		if (!std::regex_match(string, match, std::regex("^(\\d+)x(\\d+)$"))) {
			LOG("Could not parse resolution: ", string);
			resolution = std::make_pair(0, 0);
			return;
		}

		uint32_t width = std::stoul(match[1]);
		uint32_t height = std::stoul(match[2]);

		resolution = std::make_pair(width, height);
		return;
	}

	void parse_output_dir() {
		this->output_dir = std::string();

		std::string stringValue = parser->top()[CFG_OUTPUT_DIR];
		stringValue = std::regex_replace(stringValue, std::regex("(^\\s*)|(\\s*$)"), "");

		if ((!stringValue.empty()) && (stringValue.find_first_not_of(' ') != std::string::npos))
		{
			this->output_dir += stringValue;
		} else {
			char buffer[MAX_PATH];
			LOG_IF_FAILED(GetVideosDirectory(buffer), "Failed to get Videos directory for the current user.");
			this->output_dir += buffer;
		}
	}

	void parse_video_enc() {
		if (!getTrimmed(CFG_VIDEO_ENC).empty()) {
			this->video_enc = parser->top()[CFG_VIDEO_ENC];
			return;
		}

		this->video_enc = "ffv1";
	}

	void parse_video_fmt() {
		if (!getTrimmed(CFG_VIDEO_FMT).empty()) {
			this->video_fmt = parser->top()[CFG_VIDEO_FMT];
			return;
		}
		
		this->video_fmt = "bgr";
	}

	void parse_video_cfg() {
		if (!getTrimmed(CFG_VIDEO_CFG).empty()) {
			this->video_cfg = parser->top()[CFG_VIDEO_CFG];
			return;
		}

		this->video_cfg = "";
	}

	std::string getTrimmed(std::string config_name) {
		std::string orig_str = parser->top()[config_name];
		return std::regex_replace(orig_str, std::regex("(^\\s*)|(\\s*$)"), "");
	}


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