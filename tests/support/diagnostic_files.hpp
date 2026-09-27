#ifndef BPR_TESTS_DIAGNOSTIC_FILES_HPP
#define BPR_TESTS_DIAGNOSTIC_FILES_HPP
#include "bpr/application/diagnostic_log_files.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
namespace bpr::tests {
	inline auto diagnostic_files(const std::filesystem::path& aRoot) -> std::vector<std::filesystem::path>
	{
		std::vector<std::filesystem::path> files;
		if (!std::filesystem::is_directory(aRoot / "logs")) {
			return files;
		}
		for (const auto& item : std::filesystem::directory_iterator{ aRoot / "logs" }) {
			if (application::diagnostic_log_file(item.path().filename().string())) {
				files.push_back(item.path());
			}
		}
		std::ranges::sort(files, [](const auto& aLeft, const auto& aRight) {
			const auto leftTime  = std::filesystem::last_write_time(aLeft);
			const auto rightTime = std::filesystem::last_write_time(aRight);
			return leftTime != rightTime ? leftTime < rightTime : aLeft < aRight;
		});
		return files;
	}
	inline auto read_diagnostic_logs(const std::filesystem::path& aRoot) -> std::string
	{
		std::string result;
		for (const auto& file : diagnostic_files(aRoot)) {
			std::ifstream stream{ file, std::ios::binary };
			result.append(std::istreambuf_iterator<char>{ stream }, { });
		}
		return result;
	}
}
#endif
