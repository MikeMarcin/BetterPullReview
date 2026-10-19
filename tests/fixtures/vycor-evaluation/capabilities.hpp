#pragma once

#include <chrono>
#include <filesystem>

namespace probe_alias {
	using wall_clock = std::chrono::system_clock;
}

#define BPR_PROBE_WALL_NOW() probe_alias::wall_clock::now()

namespace probe {
	bool wrapper(const std::filesystem::path& aPath);
	bool value_wrapper(std::filesystem::file_status aStatus);
}
