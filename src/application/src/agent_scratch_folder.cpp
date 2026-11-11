#include "bpr/application/agent_scratch_folder.hpp"

#include "bpr/core/agent_grant.hpp"

#include <algorithm>
#include <chrono>
#include <system_error>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto plain_folder(const std::filesystem::path& aPath) -> bool
		{
			auto       error  = std::error_code{ };
			const auto status = std::filesystem::symlink_status(aPath, error);
			return !error && std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status);
		}
	} // namespace

	auto valid_agent_scratch_key(const std::string_view aKey) noexcept -> bool
	{
		if (aKey.empty() || aKey.size() > 96U || aKey.front() == '-') {
			return false;
		}
		return std::ranges::all_of(aKey, [](const char aCharacter) {
			return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= 'A' && aCharacter <= 'Z') || (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '-';
		});
	}

	auto prepare_agent_scratch_folder(const std::filesystem::path& aRoot, const std::string_view aKey) -> std::optional<std::filesystem::path>
	{
		if (aRoot.empty() || !aRoot.is_absolute() || !valid_agent_scratch_key(aKey)) {
			return std::nullopt;
		}
		auto error = std::error_code{ };
		std::filesystem::create_directories(aRoot, error);
		if (error || !plain_folder(aRoot)) {
			return std::nullopt;
		}
		std::filesystem::permissions(aRoot, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, error);
		auto folder = aRoot / std::string{ aKey };
		if (!std::filesystem::exists(std::filesystem::symlink_status(folder, error))) {
			std::filesystem::create_directory(folder, error);
			if (error) {
				return std::nullopt;
			}
			std::filesystem::permissions(folder, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, error);
		}
		if (!plain_folder(folder)) {
			return std::nullopt;
		}
		return folder;
	}

	auto list_agent_scratch_folder(const std::filesystem::path& aFolder) -> agent_scratch_listing
	{
		auto listing = agent_scratch_listing{ };
		if (!plain_folder(aFolder)) {
			return listing;
		}
		auto error   = std::error_code{ };
		auto walk    = std::filesystem::recursive_directory_iterator{ aFolder, std::filesystem::directory_options::skip_permission_denied, error };
		auto visited = std::size_t{ 0U };
		for (; !error && walk != std::filesystem::recursive_directory_iterator{ }; walk.increment(error)) {
			if (++visited > maximum_agent_scratch_entries) {
				listing.Truncated = true;
				break;
			}
			const auto status = walk->symlink_status(error);
			if (error || !std::filesystem::is_regular_file(status)) {
				error.clear();
				continue;
			}
			const auto size    = walk->file_size(error);
			const auto changed = walk->last_write_time(error);
			if (error) {
				error.clear();
				continue;
			}
			listing.Bytes += size;
			listing.Files[walk->path().lexically_relative(aFolder).generic_string()] = { size, static_cast<std::int64_t>(changed.time_since_epoch().count()) };
		}
		return listing;
	}

	auto agent_scratch_full(const agent_scratch_listing& aListing) noexcept -> bool
	{
		return aListing.Truncated || aListing.Bytes >= core::maximum_agent_scratch_bytes;
	}

	auto agent_scratch_written(const agent_scratch_listing& aBefore, const agent_scratch_listing& aAfter, const std::size_t aLimit) -> std::vector<std::string>
	{
		auto written = std::vector<std::string>{ };
		for (const auto& [path, facts] : aAfter.Files) {
			if (written.size() >= aLimit) {
				break;
			}
			const auto before = aBefore.Files.find(path);
			if (before == aBefore.Files.end() || before->second != facts) {
				written.push_back(path);
			}
		}
		return written;
	}

	auto remove_agent_scratch_folder(const std::filesystem::path& aRoot, const std::string_view aKey) -> bool
	{
		if (aRoot.empty() || !valid_agent_scratch_key(aKey) || !plain_folder(aRoot)) {
			return false;
		}
		const auto folder = aRoot / std::string{ aKey };
		auto       error  = std::error_code{ };
		const auto status = std::filesystem::symlink_status(folder, error);
		if (!std::filesystem::exists(status)) {
			return true;
		}
		if (std::filesystem::is_symlink(status)) {
			// Never followed: only the link goes.
			return std::filesystem::remove(folder, error) && !error;
		}
		// `remove_all` removes a link inside as a link, never what it names.
		std::filesystem::remove_all(folder, error);
		return !error;
	}
} // namespace bpr::application
