#ifndef BPR_TESTS_END_TO_END_COMPOSITION_SOURCE_HPP
#define BPR_TESTS_END_TO_END_COMPOSITION_SOURCE_HPP

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

namespace bpr::tests {
	/// [[BPR-1908]]. The composition root's definitions:
	/// `github_application.cpp` and the sources split out of it under
	/// `github_application/`, in a fixed order. Call-site counts read
	/// definitions, so the split headers' declarations are not part of it.
	/// Empty when the root cannot be read.
	[[nodiscard]] inline auto read_composition_source(const std::filesystem::path& aRoot) -> std::string
	{
		const auto read = [](const std::filesystem::path& aPath) {
			auto stream = std::ifstream{ aPath, std::ios::binary };
			return std::string{ std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
		};
		auto text = read(aRoot);
		if (text.empty()) {
			return text;
		}
		auto parts = std::vector<std::filesystem::path>{ };
		auto error = std::error_code{ };
		for (const auto& entry : std::filesystem::directory_iterator{ aRoot.parent_path() / "github_application", error }) {
			if (entry.path().extension() == ".cpp") {
				parts.push_back(entry.path());
			}
		}
		std::ranges::sort(parts);
		for (const auto& part : parts) {
			text += '\n';
			text += read(part);
		}
		return text;
	}
}

#endif
