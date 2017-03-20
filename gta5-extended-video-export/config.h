#pragma once

#ifndef _MY_CONFIG_H_
#define _MY_CONFIG_H_

#include "ini.hpp"
#include <string>
#include <algorithm>
#include <sstream>
#include <ShlObj.h>
#include <regex>
#include "logger.h"

#define CFG_XVX_SECTION "XVX"
#define CFG_AUTO_RELOAD_CONFIG "auto_reload_config"
#define CFG_ENABLE_XVX "enable_mod"
#define CFG_OUTPUT_DIR "output_folder"

#define CFG_EXPORT_SECTION "EXPORT"
#define CFG_EXPORT_MB_SAMPLES "motion_blur_samples"
#define CFG_EXPORT_MB_STRENGTH "motion_blur_strength"
#define CFG_EXPORT_FPS "fps"
#define CFG_EXPORT_OPENEXR "export_openexr"

#define CFG_FORMAT_SECTION "FORMAT"
#define CFG_EXPORT_FORMAT "format"
#define CFG_FORMAT_EXT "extension"
#define CFG_FORMAT_CFG "options"

#define CFG_LOG_LEVEL "log_level"
#define CFG_VIDEO_SECTION "VIDEO"
#define CFG_VIDEO_ENC "encoder"
#define CFG_VIDEO_FMT "pixel_format"
#define CFG_VIDEO_CFG "options"

#define CFG_AUDIO_SECTION "AUDIO"
#define CFG_AUDIO_ENC "encoder"
#define CFG_AUDIO_FMT "sample_format"
#define CFG_AUDIO_CFG "options"

//#define CFG_AUDIO_CODEC "audio_codec"
#define INI_FILE_NAME "EVE\\" TARGET_NAME ".ini"
#define PRESET_FILE_NAME "EVE\\preset.ini"

class config {
public:
	static bool                            is_mod_enabled;
	static bool							   auto_reload_config;
	static bool                            export_openexr;
	static std::pair<uint32_t, uint32_t>   resolution;
	static std::string                     output_dir;
	static std::string                     format_cfg;
	static std::string                     format_ext;
	static std::string                     video_enc;
	static std::string                     video_fmt;
	static std::string                     video_cfg;
	static std::string                     audio_enc;
	static std::string                     audio_cfg;
	static std::string                     audio_fmt;
	static LogLevel                        log_level;
	static std::pair<uint32_t, uint32_t>   fps;
	static uint8_t                         motion_blur_samples;
	static float                           motion_blur_strength;
	static std::string                     container_format;

	static void reload() {
		config_parser.reset(new INI::Parser(INI_FILE_NAME));
		preset_parser.reset(new INI::Parser(PRESET_FILE_NAME));

		is_mod_enabled = parse_lossless_export();
		auto_reload_config = parse_auto_reload_config();
		output_dir = parse_output_dir();
		format_cfg = parse_format_cfg();
		format_ext = parse_format_ext();
		video_enc = parse_video_enc();
		video_fmt = parse_video_fmt();
		video_cfg = parse_video_cfg();
		audio_enc = parse_audio_enc();
		audio_cfg = parse_audio_cfg();
		audio_fmt = parse_audio_fmt();
		container_format = parse_container_format();
		log_level = parse_log_level();
		fps = parse_fps();
		motion_blur_samples = parse_motion_blur_samples();
		motion_blur_strength = parse_motion_blur_strength();
		export_openexr = parse_export_openexr();
	}

private:
	static std::shared_ptr<INI::Parser> config_parser;
	static std::shared_ptr<INI::Parser> preset_parser;

	static std::string getTrimmed(std::shared_ptr<INI::Parser> parser, std::string config_name) {
		std::string orig_str = parser->top()[config_name];
		return std::regex_replace(orig_str, std::regex("(^\\s*)|(\\s*$)"), "");
	}

	static std::string getTrimmed(std::shared_ptr<INI::Parser> parser, std::string config_name, std::string section) {
		std::string orig_str = parser->top()(section)[config_name];
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

	static std::string toLower(std::string input) {
		std::string result = input;
		std::transform(result.begin(), result.end(), result.begin(), ::tolower);
		return result;
	}

	static bool stringToBoolean(std::string booleanString) {
		std::transform(booleanString.begin(), booleanString.end(), booleanString.begin(), ::tolower);

		bool value;
		std::istringstream(booleanString) >> std::boolalpha >> value;
		return value;
	}

	template <typename T>
	static T failed(std::string key, std::string value, T def) {
		LOG(LL_NON, "Failed to parse value for \"", key, "\": ", value);
		LOG(LL_NON, "Using default value of \"", def, "\" for \"", key, "\"");
		return def;
	}

	template <typename T1, typename T2>
	static std::pair<T1, T2> failed(std::string key, std::string value, std::pair<T1, T2> def) {
		LOG(LL_NON, "Failed to parse value for \"", key, "\": ", value);
		LOG(LL_NON, "Using default value of <", def.first, ", ", def.second, "> for \"", key, "\"");
		return def;
	}

	template <typename T>
	static T succeeded(std::string key, T value) {
		LOG(LL_NON, "Loaded value for \"", key, "\": ", value);
		return value;
	}

	template <typename T1, typename T2>
	static std::pair<T1, T2> succeeded(std::string key, std::pair<T1, T2> value) {
		LOG(LL_NON, "Loaded value for \"", key, "\": <", value.first, ", ", value.second, ">");
		return value;
	}

	static bool parse_lossless_export() {
		std::string string = config_parser->top()[CFG_ENABLE_XVX];

		try {
			return succeeded(CFG_ENABLE_XVX, stringToBoolean(string));
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		return failed(CFG_ENABLE_XVX, string, true);
	}

	static bool parse_auto_reload_config() {
		std::string string = config_parser->top()[CFG_AUTO_RELOAD_CONFIG];

		try {
			return succeeded(CFG_AUTO_RELOAD_CONFIG, stringToBoolean(config_parser->top()[CFG_AUTO_RELOAD_CONFIG]));
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		return failed(CFG_AUTO_RELOAD_CONFIG, string, true);
	}

	static bool parse_export_openexr() {
		std::string string = config_parser->top()(CFG_EXPORT_SECTION)[CFG_EXPORT_OPENEXR];

		try {
			return succeeded(CFG_EXPORT_OPENEXR, stringToBoolean(string));
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		return failed(CFG_EXPORT_OPENEXR, string, false);
	}

	static std::string parse_output_dir() {
		try {
			std::string string = config_parser->top()[CFG_OUTPUT_DIR];
			string = std::regex_replace(string, std::regex("(^\\s*)|(\\s*$)"), "");

			if ((!string.empty()) && (string.find_first_not_of(' ') != std::string::npos))
			{
				return succeeded(CFG_OUTPUT_DIR, string);
			} else {
				char buffer[MAX_PATH] = { 0 };
				REQUIRE(GetVideosDirectory(buffer), "Failed to get Videos directory for the current user.");
				return failed(CFG_OUTPUT_DIR, string, buffer);
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}
		throw std::logic_error("Could not parse output directory");
	}

	static std::string parse_format_cfg() {
		std::string string = getTrimmed(preset_parser, CFG_FORMAT_CFG, CFG_FORMAT_SECTION);
		try {
			return succeeded(CFG_FORMAT_CFG, string);
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		return failed(CFG_FORMAT_CFG, string, "");
	}

	static std::string parse_format_ext() {
		std::string string = getTrimmed(preset_parser, CFG_FORMAT_EXT, CFG_FORMAT_SECTION);
		try {
			return succeeded(CFG_FORMAT_EXT, string);
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		return failed(CFG_FORMAT_EXT, string, "");
	}

	static std::string parse_video_enc() {
		std::string string;
		string = getTrimmed(preset_parser, CFG_VIDEO_ENC, CFG_VIDEO_SECTION);
		try {
			if (!string.empty()) {
				return succeeded(CFG_VIDEO_ENC, string);
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		LOG(LL_NFO, "No video encoder specified. Video encoding will be disabled.");
		return "";
		//return failed(CFG_VIDEO_ENC, string, "");
	}

	static std::string parse_video_fmt() {
		std::string string = getTrimmed(preset_parser, CFG_VIDEO_FMT, CFG_VIDEO_SECTION);
		try {
			if (!string.empty()) {
				return succeeded(CFG_VIDEO_FMT, string);
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		return failed(CFG_VIDEO_FMT, string, "yuv420p");
	}

	static std::string parse_video_cfg() {
		std::string string = getTrimmed(preset_parser, CFG_VIDEO_CFG, CFG_VIDEO_SECTION);
		try {
			if (!string.empty()) {
				return succeeded(CFG_VIDEO_CFG, string);
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		return failed(CFG_VIDEO_CFG, string, "");
	}

	static std::string parse_audio_enc() {
		std::string string = getTrimmed(preset_parser, CFG_AUDIO_ENC, CFG_AUDIO_SECTION);
		try {
			if (!string.empty()) {
				return succeeded(CFG_AUDIO_ENC, string);
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		LOG(LL_NFO, "No audio encoder specified. Audio encoding will be disabled.");
		return "";
		//return failed(CFG_AUDIO_ENC, string, "ac3");
	}

	static std::string parse_audio_cfg() {
		std::string string = getTrimmed(preset_parser, CFG_AUDIO_CFG, CFG_AUDIO_SECTION);
		try {
			if (!string.empty()) {
				return succeeded(CFG_AUDIO_CFG, string);
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		return failed(CFG_AUDIO_CFG, string, "");
	}

	static std::string parse_audio_fmt() {
		std::string string = getTrimmed(preset_parser, CFG_AUDIO_FMT, CFG_AUDIO_SECTION);
		try {
			if (!string.empty()) {
				return succeeded(CFG_AUDIO_FMT, string);
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		return failed(CFG_AUDIO_FMT, string, "fltp");
	}

	static LogLevel parse_log_level() {
		std::string string = toLower(getTrimmed(config_parser, CFG_LOG_LEVEL));
		try {
			if (string == "error") {
				return succeeded(CFG_LOG_LEVEL, LL_ERR);
			} else if (string == "warn") {
				return succeeded(CFG_LOG_LEVEL, LL_WRN);
			} else if (string == "info") {
				return succeeded(CFG_LOG_LEVEL, LL_NFO);
			} else if (string == "debug") {
				return succeeded(CFG_LOG_LEVEL, LL_DBG);
			} else if (string == "trace") {
				return succeeded(CFG_LOG_LEVEL, LL_TRC);
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		return failed(CFG_LOG_LEVEL, string, LL_ERR);
	}

	static uint8_t parse_motion_blur_samples() {
		std::string string = config_parser->top()(CFG_EXPORT_SECTION)[CFG_EXPORT_MB_SAMPLES];;
		string = std::regex_replace(string, std::regex("\\s+"), "");
		try {
			uint64_t value = std::stoul(string);
			if (value > 255) {
				LOG(LL_NON, "Specified motion blur samples exceed 255");
				LOG(LL_NON, "Using maximum value of 255");
				return 255;
			} else {
				return (uint8_t)succeeded(CFG_EXPORT_MB_SAMPLES, value);
			}
		} catch (std::exception& ex) {
			LOG(LL_NON, ex.what());
		}

		return failed(CFG_EXPORT_MB_SAMPLES, string, 0);

	}

	static std::string parse_container_format() {
		std::string string = preset_parser->top()(CFG_FORMAT_SECTION)[CFG_EXPORT_FORMAT];
		string = std::regex_replace(string, std::regex("\\s+"), "");
		string = toLower(string);
		try {
			//if (string == "mkv" || string == "avi" || string == "mp4") {
				return succeeded(CFG_EXPORT_FORMAT, string);
			//}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		return failed(CFG_EXPORT_FORMAT, string, "mp4");
	}

	static std::pair<int32_t, int32_t> parse_fps() {
		std::string string = config_parser->top()(CFG_EXPORT_SECTION)[CFG_EXPORT_FPS];
		string = std::regex_replace(string, std::regex("\\s+"), "");
		try {
			std::smatch match;
			if (std::regex_match(string, match, std::regex("^(\\d+)/(\\d+)$"))) {
				return succeeded(CFG_EXPORT_FPS, std::make_pair(std::stoi(match[1]), std::stoi(match[2])));
			}

			match = std::smatch();
			if (std::regex_match(string, match, std::regex("^\\d+(\\.\\d+)?$"))) {
				float value = std::stof(string);
				int32_t num = (int32_t)(value * 1000);
				int32_t den = 1000;
				return succeeded(CFG_EXPORT_FPS, std::make_pair(num, den));
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		return failed(CFG_EXPORT_FPS, string, std::make_pair(30000, 1001));
	}

	static float parse_motion_blur_strength() {
		std::string string = config_parser->top()(CFG_EXPORT_SECTION)[CFG_EXPORT_MB_STRENGTH];
		try {
			float value = std::stof(string);
			if (value < 0) {
				value = 0;
			} else if (value > 1) {
				value = 1;
			}
			return succeeded(CFG_EXPORT_MB_STRENGTH, value);
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}
		return failed(CFG_EXPORT_MB_STRENGTH, string, 0.5);
	}
};

#endif _MY_CONFIG_H_