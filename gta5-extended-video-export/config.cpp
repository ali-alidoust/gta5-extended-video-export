#include "config.h"

#include <codecvt>

std::shared_ptr<INIReader> config::config_parser;
std::shared_ptr<INIReader> config::preset_parser;

bool config::is_mod_enabled;
bool config::auto_reload_config;
std::string config::output_dir;
LogLevel config::log_level;
std::pair<uint32_t, uint32_t> config::fps;
uint8_t config::motion_blur_samples;
float config::motion_blur_strength;
bool config::export_openexr;
VKENCODERCONFIG config::encoder_config;