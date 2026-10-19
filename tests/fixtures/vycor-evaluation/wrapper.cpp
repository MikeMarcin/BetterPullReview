#include "capabilities.hpp"

namespace probe {
	bool wrapper(const std::filesystem::path& aPath)
	{
		return std::filesystem::is_directory(aPath);
	}

	bool value_wrapper(std::filesystem::file_status aStatus)
	{
		return std::filesystem::is_directory(aStatus);
	}
}
