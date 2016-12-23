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

#define CFG_EXPORT_SECTION "EXPORT"
#define CFG_EXPORT_MB_SAMPLES "motion_blur_samples"
#define CFG_EXPORT_FPS "fps"
#define CFG_EXPORT_FORMAT "format"

#define CFG_LOG_LEVEL "log_level"
#define CFG_VIDEO_SECTION "VIDEO"
#define CFG_VIDEO_ENC "encoder"
#define CFG_VIDEO_FMT "pixel_format"
#define CFG_VIDEO_CFG "options"

#define CFG_AUDIO_SECTION "AUDIO"
#define CFG_AUDIO_ENC "encoder"
#define CFG_AUDIO_FMT "sample_format"
#define CFG_AUDIO_RATE "sample_rate"
#define CFG_AUDIO_CFG "options"

//#define CFG_AUDIO_CODEC "audio_codec"
#define INI_FILE_NAME TARGET_NAME ".ini"

class Config {
public:

	bool isModEnabled() {
		return this->is_mod_enabled;
	}

	bool isAutoReloadEnabled() {
		return this->auto_reload_config;
	}

	std::pair<int32_t, int32_t> getFPS() {
		return this->fps;
	}

	uint8_t getMotionBlurSamples() {
		return this->motion_blur_samples;
	}

	std::string outputDir() {
		return this->output_dir;
	}

	std::string videoEnc() {
		return this->video_enc;
	}

	std::string videoFmt() {
		return this->video_fmt;
	}

	std::string videoCfg() {
		return this->video_cfg;
	}

	std::string audioEnc() {
		return this->audio_enc;
	}

	std::string audioCfg() {
		return this->audio_cfg;
	}

	std::string audioFmt() {
		return this->audio_fmt;
	}

	uint32_t audioRate() {
		return this->audio_rate;
	}

	LogLevel logLevel() {
		return this->log_level;
	}

	std::string getContainerFormat() {
		return this->container_format;
	}

	void reload() {
		parser.reset(new INI::Parser(INI_FILE_NAME));

		this->parse_lossless_export();
		this->parse_auto_reload_config();
		this->parse_output_dir();
		//this->parse_resolution();
		this->parse_video_enc();
		this->parse_video_fmt();
		this->parse_video_cfg();
		this->parse_audio_enc();
		this->parse_audio_cfg();
		this->parse_audio_fmt();
		this->parse_audio_rate();
		this->container_format = this->parse_container_format();
		this->log_level = this->parse_log_level();
		this->fps = this->parse_fps();
		this->motion_blur_samples = this->parse_motion_blur_samples();
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
	std::string                     audio_enc                  = "";
	std::string                     audio_cfg                  = "";
	std::string                     audio_fmt                  = "";
	uint32_t                        audio_rate                 = 48000;
	LogLevel						log_level                  = LL_ERR;
	std::pair<uint32_t, uint32_t>   fps                        = std::make_pair(30000, 1001);
	uint8_t                         motion_blur_samples        = 0;
	std::string                     container_format		   = "mkv";


	void parse_lossless_export() {
		try {
			is_mod_enabled = stringToBoolean(parser->top()[CFG_ENABLE_XVX], true);
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}
	}

	void parse_auto_reload_config() {
		try {
			auto_reload_config = stringToBoolean(parser->top()[CFG_AUTO_RELOAD_CONFIG], true);
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}
	}

	void parse_output_dir() {
		try {
			this->output_dir = std::string();

			std::string stringValue = parser->top()[CFG_OUTPUT_DIR];
			stringValue = std::regex_replace(stringValue, std::regex("(^\\s*)|(\\s*$)"), "");

			if ((!stringValue.empty()) && (stringValue.find_first_not_of(' ') != std::string::npos))
			{
				this->output_dir += stringValue;
			} else {
				char buffer[MAX_PATH];
				REQUIRE(GetVideosDirectory(buffer), "Failed to get Videos directory for the current user.");
				this->output_dir += buffer;
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}
	}

	void parse_video_enc() {
		try {
			std::string value = getTrimmed(CFG_VIDEO_ENC, CFG_VIDEO_SECTION);
			if (!value.empty()) {
				this->video_enc = parser->top()(CFG_VIDEO_SECTION)[CFG_VIDEO_ENC];
				return;
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		LOG(LL_NON, "Video encoder not supplied in .ini file, using default: \"libx264\"");
		this->video_enc = "libx264";
	}

	void parse_video_fmt() {
		try {
			std::string value = getTrimmed(CFG_VIDEO_FMT, CFG_VIDEO_SECTION);
			if (!value.empty()) {
				this->video_fmt = parser->top()(CFG_VIDEO_SECTION)[CFG_VIDEO_FMT];
				return;
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}
		
		LOG(LL_NON, "Video pixel format not supplied in .ini file, using default: \"yuv420p\"");
		this->video_fmt = "yuv420p";
	}

	void parse_video_cfg() {
		try {
			std::string value = getTrimmed(CFG_VIDEO_CFG, CFG_VIDEO_SECTION);
			if (!value.empty()) {
				this->video_cfg = parser->top()(CFG_VIDEO_SECTION)[CFG_VIDEO_CFG];
				return;
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		this->video_cfg = "";
	}

	void parse_audio_enc() {
		try {
			std::string value = getTrimmed(CFG_AUDIO_ENC, CFG_AUDIO_SECTION);
			if (!value.empty()) {
				this->audio_enc = parser->top()(CFG_AUDIO_SECTION)[CFG_AUDIO_ENC];
				return;
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		LOG(LL_NON, "Audio encoder not supplied in .ini file, using default: \"ac3\"");
		this->audio_enc = "ac3";
	}

	void parse_audio_cfg() {
		try {
			std::string value = getTrimmed(CFG_AUDIO_CFG, CFG_AUDIO_SECTION);
			if (!value.empty()) {
				this->audio_cfg = parser->top()(CFG_AUDIO_SECTION)[CFG_AUDIO_CFG];
				return;
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		this->audio_cfg = "";
	}

	void parse_audio_fmt() {
		std::string value;
		try {
			value = getTrimmed(CFG_AUDIO_FMT, CFG_AUDIO_SECTION);
			if (!value.empty()) {
				this->audio_fmt = parser->top()(CFG_AUDIO_SECTION)[CFG_AUDIO_FMT];
				return;
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		LOG(LL_NON, "Audio sample format not supplied in .ini file, using default: \"fltp\"");
		this->audio_fmt = "fltp";
	}

	void parse_audio_rate() {
		std::string value;
		try {
			value = getTrimmed(CFG_AUDIO_RATE, CFG_AUDIO_SECTION);
			if (!value.empty()) {
				try {
					this->audio_rate = std::stoul(parser->top()(CFG_AUDIO_SECTION)[CFG_AUDIO_RATE]);

				} catch (std::exception&) {
					LOG(LL_NON, "Failed to parse audio sample rate: ", value);
					LOG(LL_NON, "using default: 48000");
				}
			} else {
				LOG(LL_NON, "Audio sample format not supplied in .ini file, using default: 48000");
			}
		}
		catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}
		this->audio_rate = 48000;
	}

	LogLevel parse_log_level() {
		std::string value;
		try {
			value = getTrimmed(CFG_LOG_LEVEL);
			value = toLower(value);
			if (value == "error") {
				return LL_ERR;
			} else if (value == "warn") {
				return LL_WRN;
			} else if (value == "info") {
				return LL_NFO;
			} else if (value == "debug") {
				return LL_DBG;
			} else if (value == "trace") {
				return LL_TRC;
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		LOG(LL_NON, "Could not parse log level, using default: \"error\"");
		return LL_ERR;
	}

	uint8_t parse_motion_blur_samples() {
		std::string string;
		try {
			string = parser->top()(CFG_EXPORT_SECTION)[CFG_EXPORT_MB_SAMPLES];
			string = std::regex_replace(string, std::regex("\\s+"), "");
			uint64_t value = std::stoul(string);
			if (value > 255) {
				LOG(LL_NON, "Specified motion blur samples exceed 255");
				LOG(LL_NON, "Using maximum value of 255");
				return 255;
			} else {
				return (uint8_t)value;
			}
		} catch (std::exception& ex) {
			LOG(LL_NON, ex.what());
		}

		LOG(LL_NON, "Could not parse motion blur samples: ", string);
		LOG(LL_NON, "Using default value of 0 (disabled)");
		return 0;

	}

	std::string parse_container_format() {
		std::string string;
		try {
			string = parser->top()(CFG_EXPORT_SECTION)[CFG_EXPORT_FORMAT];
			string = std::regex_replace(string, std::regex("\\s+"), "");
			string = Config::toLower(string);

			if (string == "mkv" || string == "avi" || string == "mp4") {
				return string;
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		LOG(LL_NON, "Could not parse video file format: ", string);
		LOG(LL_NON, "Using default value: mkv");
		return "mkv";
	}

	std::pair<int32_t, int32_t> parse_fps() {
		std::string string;
		try {
			string = parser->top()(CFG_EXPORT_SECTION)[CFG_EXPORT_FPS];
			string = std::regex_replace(string, std::regex("\\s+"), "");

			auto result = std::make_pair<int32_t, int32_t>(0, 0);

			if (string.empty()) {
				return result;
			}

			std::smatch match;
			if (std::regex_match(string, match, std::regex("^(\\d+)/(\\d+)$"))) {
				return std::make_pair(std::stoi(match[1]), std::stoi(match[2]));
			}

			match = std::smatch();
			if (std::regex_match(string, match, std::regex("^\\d+(\\.\\d+)?$"))) {
				float value = std::stof(string);
				int32_t num = (int32_t)(value * 1000);
				int32_t den = 1000;
				return std::make_pair(num, den);
			}
		} catch (std::exception& ex) {
			LOG(LL_ERR, ex.what());
		}

		LOG(LL_NON, "Could not parse fps value: ", string);
		LOG(LL_NON, "Using default value: ~30");

		return std::make_pair(30000, 1001);
	}


	std::string getTrimmed(std::string config_name) {
		std::string orig_str = parser->top()[config_name];
		return std::regex_replace(orig_str, std::regex("(^\\s*)|(\\s*$)"), "");
	}

	std::string getTrimmed(std::string config_name, std::string section) {
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