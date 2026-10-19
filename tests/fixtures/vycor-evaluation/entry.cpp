#include "capabilities.hpp"

namespace probe {
	bool cross_file_effect(const std::filesystem::path& aPath)
	{
		return wrapper(aPath);
	}

	bool cross_file_value(std::filesystem::file_status aStatus)
	{
		return value_wrapper(aStatus);
	}
}
