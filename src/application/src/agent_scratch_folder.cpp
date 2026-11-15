#include "bpr/application/agent_scratch_folder.hpp"

#include "bpr/core/agent_grant.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <system_error>

#if defined(__APPLE__)
	#include <fcntl.h>
	#include <sys/param.h>
	#include <unistd.h>
#endif

namespace bpr::application {
	namespace {
		[[nodiscard]] auto plain_folder(const std::filesystem::path& aPath) -> bool
		{
			auto       error  = std::error_code{ };
			const auto status = std::filesystem::symlink_status(aPath, error);
			return !error && std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status);
		}
		/// A file time as seconds since the epoch, through the two clocks'
		/// present readings (the library offers no portable cast).
		[[nodiscard]] auto unix_seconds(const std::filesystem::file_time_type aTime) -> std::int64_t
		{
			const auto since = std::chrono::duration_cast<std::chrono::seconds>(aTime - std::filesystem::file_time_type::clock::now());
			return (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()) + since).count();
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
		// In use now: an age cleanup counts from here.
		std::filesystem::last_write_time(folder, std::filesystem::file_time_type::clock::now(), error);
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

	auto agent_scratch_full(const agent_scratch_listing& aListing, const std::uint64_t aLimitBytes) noexcept -> bool
	{
		return aListing.Truncated || aListing.Bytes >= aLimitBytes;
	}

	auto survey_agent_scratch_root(const std::filesystem::path& aRoot) -> std::vector<agent_scratch_entry>
	{
		auto entries = std::vector<agent_scratch_entry>{ };
		if (!plain_folder(aRoot)) {
			return entries;
		}
		auto error = std::error_code{ };
		for (auto walk = std::filesystem::directory_iterator{ aRoot, error }; !error && walk != std::filesystem::directory_iterator{ }; walk.increment(error)) {
			if (entries.size() >= maximum_agent_scratch_entries) {
				break;
			}
			const auto key = walk->path().filename().string();
			if (!valid_agent_scratch_key(key) || !plain_folder(walk->path())) {
				continue;
			}
			const auto listing = list_agent_scratch_folder(walk->path());
			auto       newest  = std::int64_t{ 0 };
			auto       ignored = std::error_code{ };
			if (const auto own = std::filesystem::last_write_time(walk->path(), ignored); !ignored) {
				newest = unix_seconds(own);
			}
			for (const auto& [path, facts] : listing.Files) {
				newest = std::max(newest, unix_seconds(std::filesystem::file_time_type{ std::filesystem::file_time_type::duration{ facts.second } }));
			}
			entries.push_back({ .Key = key, .Bytes = listing.Bytes, .LastChanged = newest });
		}
		std::ranges::sort(entries, { }, &agent_scratch_entry::Key);
		return entries;
	}

	auto remove_agent_scratch_folders(const std::filesystem::path& aRoot, const std::optional<std::int64_t> aBeforeSeconds, const std::vector<std::string>& aKeep) -> agent_scratch_cleanup
	{
		auto cleanup = agent_scratch_cleanup{ };
		for (const auto& entry : survey_agent_scratch_root(aRoot)) {
			if (std::ranges::contains(aKeep, entry.Key) || (aBeforeSeconds && entry.LastChanged >= *aBeforeSeconds)) {
				continue;
			}
			if (remove_agent_scratch_folder(aRoot, entry.Key)) {
				++cleanup.Removed;
				cleanup.Bytes += entry.Bytes;
			} else {
				++cleanup.Failed;
			}
		}
		return cleanup;
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

namespace bpr::application {
	namespace {
		/// The path the system reports for the folder at `aPath` once opened:
		/// the spelling a process running there sees as its working folder.
		[[nodiscard]] auto opened_folder_path(const std::string& aPath) -> std::optional<std::string>
		{
#if defined(__APPLE__)
			const int descriptor = ::open(aPath.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
			if (descriptor < 0) {
				return std::nullopt;
			}
			auto       buffer = std::array<char, MAXPATHLEN>{ };
			const bool read   = ::fcntl(descriptor, F_GETPATH, buffer.data()) != -1;
			::close(descriptor);
			if (!read || buffer.front() == '\0') {
				return std::nullopt;
			}
			return std::string{ buffer.data() };
#else
			// Elsewhere the kernel's name for a folder is its resolved path.
			auto       failed   = std::error_code{ };
			const auto resolved = std::filesystem::canonical(std::filesystem::path{ aPath }, failed);
			return failed ? std::nullopt : std::optional{ resolved.string() };
#endif
		}
	} // namespace

	auto agent_checkout_spellings(const std::string& aPath) -> std::optional<std::vector<std::string>>
	{
		auto       failed    = std::error_code{ };
		const auto canonical = aPath.empty() ? std::filesystem::path{ } : std::filesystem::canonical(std::filesystem::path{ aPath }, failed);
		if (aPath.empty() || failed || !std::filesystem::is_directory(canonical, failed) || failed) {
			return std::nullopt;
		}
		const auto opened = opened_folder_path(aPath);
		if (!opened) {
			return std::nullopt;
		}
		auto result = std::vector<std::string>{ aPath };
		for (const auto& spelling : { canonical.string(), *opened }) {
			if (std::ranges::find(result, spelling) == result.end()) {
				result.push_back(spelling);
			}
		}
		return result;
	}
} // namespace bpr::application
