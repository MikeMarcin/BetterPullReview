#include "bpr/adapters/workspace/clone_folder_scan.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

namespace bpr::adapters::workspace {
	namespace {
		namespace fs = std::filesystem;

		[[nodiscard]] auto utf8_path(const std::string& aText) -> fs::path
		{
			return fs::path{ std::u8string{ reinterpret_cast<const char8_t*>(aText.data()), aText.size() } };
		}

		[[nodiscard]] auto utf8_text(const fs::path& aPath) -> std::string
		{
			const auto text = aPath.u8string();
			return std::string{ reinterpret_cast<const char*>(text.data()), text.size() };
		}

		/// A directory by itself, never through a link or reparse point.
		[[nodiscard]] auto plain_directory(const fs::path& aPath) -> bool
		{
			auto       error  = std::error_code{ };
			const auto status = fs::symlink_status(aPath, error);
			return !error && status.type() == fs::file_type::directory;
		}

		/// A regular file by itself, read up to `aLimit` bytes.
		[[nodiscard]] auto plain_file(const fs::path& aPath, const std::size_t aLimit) -> std::optional<std::string>
		{
			auto       error  = std::error_code{ };
			const auto status = fs::symlink_status(aPath, error);
			if (error || status.type() != fs::file_type::regular) {
				return std::nullopt;
			}
			auto input = std::ifstream{ aPath, std::ios::binary };
			if (!input) {
				return std::nullopt;
			}
			auto text = std::string(aLimit, '\0');
			input.read(text.data(), static_cast<std::streamsize>(aLimit));
			text.resize(static_cast<std::size_t>(input.gcount()));
			return text;
		}

		[[nodiscard]] auto clone_at(const fs::path& aDirectory, const std::string& aFolder) -> std::optional<core::clone_candidate>
		{
			const auto git = aDirectory / ".git";
			if (!plain_directory(git)) {
				return std::nullopt;
			}
			const auto config = plain_file(git / "config", core::maximum_clone_config_bytes);
			if (!config) {
				return std::nullopt;
			}
			auto candidate    = core::clone_candidate{ };
			candidate.Path    = utf8_text(aDirectory);
			candidate.Folder  = aFolder;
			candidate.Remotes = core::parse_git_config_remotes(*config);
			if (const auto head = plain_file(git / "HEAD", 1024U)) {
				candidate.Branch = core::head_branch(*head);
			}
			return candidate;
		}

		[[nodiscard]] auto hidden(const fs::path& aPath) -> bool
		{
			const auto name = aPath.filename().u8string();
			return !name.empty() && name.front() == u8'.';
		}

		void scan(const fs::path& aDirectory, const std::string& aFolder, const std::size_t aDepth, std::size_t& aVisited, std::vector<core::clone_candidate>& aFound,
				  const std::stop_token& aStop)
		{
			if (aStop.stop_requested() || aVisited >= core::maximum_clone_search_directories) {
				return;
			}
			++aVisited;
			if (auto clone = clone_at(aDirectory, aFolder)) {
				aFound.push_back(std::move(*clone));
				return;
			}
			if (aDepth >= core::clone_search_depth) {
				return;
			}
			auto error    = std::error_code{ };
			auto children = std::vector<fs::path>{ };
			for (auto entry = fs::directory_iterator{ aDirectory, fs::directory_options::skip_permission_denied, error };
				 !error && entry != fs::directory_iterator{ }; entry.increment(error)) {
				if (aStop.stop_requested() || children.size() >= core::maximum_clone_search_directories) {
					break;
				}
				const auto& path = entry->path();
				if (!hidden(path) && plain_directory(path)) {
					children.push_back(path);
				}
			}
			std::ranges::sort(children);
			for (const auto& path : children) {
				scan(path, aFolder, aDepth + 1U, aVisited, aFound, aStop);
			}
		}
	} // namespace

	auto scan_clone_folders(const std::span<const std::string> aFolders, const std::stop_token aStop) -> std::vector<core::clone_candidate>
	{
		auto found = std::vector<core::clone_candidate>{ };
		for (const auto& folder : aFolders) {
			try {
				const auto root = utf8_path(folder);
				if (!root.is_absolute() || !plain_directory(root)) {
					continue;
				}
				auto visited = std::size_t{ 0U };
				scan(root, folder, 0U, visited, found, aStop);
			} catch (...) { }
		}
		return found;
	}

	auto read_clone(const std::string& aPath) -> std::optional<core::clone_candidate>
	{
		try {
			const auto root = utf8_path(aPath);
			if (!root.is_absolute() || !plain_directory(root)) {
				return std::nullopt;
			}
			return clone_at(root, { });
		} catch (...) {
			return std::nullopt;
		}
	}

	auto existing_folders(const std::span<const std::string> aFolders) -> std::vector<std::string>
	{
		auto existing = std::vector<std::string>{ };
		for (const auto& folder : aFolders) {
			try {
				if (const auto path = utf8_path(folder); path.is_absolute() && plain_directory(path)) {
					existing.push_back(folder);
				}
			} catch (...) { }
		}
		return existing;
	}
} // namespace bpr::adapters::workspace
