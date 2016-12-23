#pragma once

#include <string>

const std::string yara_get_render_time_base_function = R"__(
	rule yara_get_render_time_base_function
	{
		strings:
			$pattern = { 48 83 EC 28 48 8B 01 FF 50 ?? 8B C8 48 83 C4 28 E9 ?? ?? ?? ?? }

			condition:
			$pattern
	}
)__";