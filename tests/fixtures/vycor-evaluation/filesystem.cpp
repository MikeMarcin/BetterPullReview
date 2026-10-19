#include "capabilities.hpp"

namespace probe {
	bool path_effect(const std::filesystem::path& aPath)
	{
		return std::filesystem::is_directory(aPath);
	}

	bool status_value(std::filesystem::file_status aStatus)
	{
		return std::filesystem::is_directory(aStatus);
	}
}
