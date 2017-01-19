#pragma once

#include <string>

const std::string yara_get_render_time_base_function = "48 83 EC 28 48 8B 01 FF 50 ?? 8B C8 48 83 C4 28 E9 ?? ?? ?? ??";
//const std::string yara_create_texture_function = "48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 55 41 56 41 57 48 8D 68 C9 48 81 EC A0 00 00 00";
const std::string yara_create_texture_function = "48 8B C4 48 89 58 08 48 89 68 10 48 89 70 18 48 89 78 20 41 56 48 83 EC 30 41 8B F9 41 8B F0 48 8B DA 4C 8B F1";
const std::string yara_linearize_texture_function = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 54 41 56 48 83 EC 20 8B 2D ?? ?? ?? ?? 65 48 8B 04 25 ?? ?? ?? ?? BF B8 01 00 00";
const std::string yara_audio_unk01_function = "8A 81 78 4D 00 00 C0 E8 02 24 01 C3";
//const std::string yara_global_unk01_command = "48 8D ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 40 84 F6 0F 84 ?? ?? ?? ?? 4D 85 F6 0F 84 ?? ?? ?? ??";
//const std::string yara_get_var_ptr_by_hash_function = "45 33 C0 85 D2 74 ?? 0F B7 41 ?? 45 8B C8";
//const std::string yara_get_var_ptr_by_hash_2 = "53 48 83 EC 20 33 D2 E8 ?? ?? ?? ?? 45 33 D2 85 C0";