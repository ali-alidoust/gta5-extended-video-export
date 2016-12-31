#include "config.h"

std::unique_ptr<INI::Parser>    config::parser;

bool                            config::is_mod_enabled;
bool                            config::auto_reload_config;
std::pair<uint32_t, uint32_t>   config::resolution;
std::string                     config::output_dir;
std::string                     config::video_enc;
std::string                     config::video_fmt;
std::string                     config::video_cfg;
std::string                     config::audio_enc;
std::string                     config::audio_cfg;
std::string                     config::audio_fmt;
uint32_t                        config::audio_rate;
LogLevel                        config::log_level;
std::pair<uint32_t, uint32_t>   config::fps;
uint8_t                         config::motion_blur_samples;
std::string                     config::container_format;
bool                            config::export_openexr;