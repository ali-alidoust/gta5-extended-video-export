#pragma once

#include <string>

const std::string yara_get_render_time_base_function = "48 83 EC 28 48 8B 01 FF 50 ?? 8B C8 48 83 C4 28 E9 ?? ?? ?? ??";
const std::string yara_get_game_speed_multiplier_function = "8B 91 90 00 00 00 85 D2 74 65 FF CA 74 58 FF CA 74 4B FF CA 74 3E 83 EA 02 74 30 FF CA 74 23 FF CA 74 16 FF CA 74 09";
const std::string yara_step_audio_function = "48 8B C4 48 89 58 10 48 89 70 18 48 89 78 20 55 41 54 41 55 41 56 41 57 48 8D 68 98 48 81 EC 40 01 00 00 48 8B D9";
const std::string yara_create_thread_function = "48 83 EC 48 48 8B 84 24 80 00 00 00 48 89 44 24 30 48 8B 44 24 70 83 4C 24 28 FF 48 89 44 24 20";
const std::string yara_wait_for_single_object = "48 89 5C 24 08 57 48 83 EC 20 8B DA 48 8B F9 83 CA FF 48 8B CF";
const std::string yara_create_texture_function = "48 8B C4 48 89 58 08 48 89 68 10 48 89 70 18 48 89 78 20 41 56 48 83 EC 30 41 8B F9 41 8B F0 48 8B DA 4C 8B F1";
const std::string yara_linearize_texture_function = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 54 41 56 48 83 EC 20 8B 2D ?? ?? ?? ?? 65 48 8B 04 25 ?? ?? ?? ?? BF B8 01 00 00";
const std::string yara_audio_unk01_function = "8A 81 78 4D 00 00 C0 E8 02 24 01 C3";
const std::string yara_create_export_context_function = "40 53 48 81 EC A0 00 00 00 89 11 48 8B D9 44 89 41 04 44 89 49 08 48 8D 4C 24 40 BA FF FF 00 00";