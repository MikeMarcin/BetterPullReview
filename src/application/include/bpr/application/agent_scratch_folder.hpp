#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SCRATCH_FOLDER_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SCRATCH_FOLDER_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::application {
	/// \brief [[BPR-2007]], RFC-BPR-0104. The most entries a scratch folder
	/// walk visits; a folder with more is measured as full.
	inline constexpr std::size_t maximum_agent_scratch_entries = 16384U;

	/// \brief Whether `aKey` may name a scratch folder: 1 to 96 characters of
	/// ASCII letters, digits and `-`, never starting with `-`.
	[[nodiscard]] auto valid_agent_scratch_key(std::string_view aKey) noexcept -> bool;

	/// \brief The folder for `aKey` under `aRoot`, made private (owner only)
	/// when new. Nothing when the key is not valid, when either is a link
	/// or not a folder, or when it cannot be made.
	[[nodiscard]] auto prepare_agent_scratch_folder(const std::filesystem::path& aRoot, std::string_view aKey) -> std::optional<std::filesystem::path>;

	/// \brief What one scratch folder holds: each regular file's path,
	/// relative to it, with its size and change time. Links are listed and
	/// never followed.
	struct agent_scratch_listing
	{
		std::map<std::string, std::pair<std::uintmax_t, std::int64_t>> Files;
		std::uint64_t                                                  Bytes = 0U;
		/// The walk stopped at `maximum_agent_scratch_entries`.
		bool Truncated = false;
	};
	[[nodiscard]] auto list_agent_scratch_folder(const std::filesystem::path& aFolder) -> agent_scratch_listing;
	/// \brief Whether the folder is at or past `aLimitBytes`: a turn starting
	/// now gets no write allowance there, and a turn running now is stopped.
	[[nodiscard]] auto agent_scratch_full(const agent_scratch_listing& aListing, std::uint64_t aLimitBytes) noexcept -> bool;

	/// \brief Owner look e97c6a3d (2026-11-11): "we need to make sure that
	/// our app doesn't just consume disk space without bounds". One folder
	/// under the root, as Settings lists it.
	struct agent_scratch_entry
	{
		std::string   Key;
		std::uint64_t Bytes = 0U;
		/// The newest change in it (or the folder's own), seconds since the epoch.
		std::int64_t LastChanged = 0;
	};
	/// \brief Every folder under `aRoot`, at most `maximum_agent_scratch_entries`
	/// of them, each measured as `list_agent_scratch_folder` measures.
	[[nodiscard]] auto survey_agent_scratch_root(const std::filesystem::path& aRoot) -> std::vector<agent_scratch_entry>;
	/// \brief Removes each folder under `aRoot` whose newest change is older
	/// than `aBeforeSeconds` (all of them for none), except `aKeep`. Answers
	/// the folders removed and the bytes they held.
	struct agent_scratch_cleanup
	{
		std::size_t   Removed = 0U;
		std::uint64_t Bytes   = 0U;
		std::size_t   Failed  = 0U;
	};
	[[nodiscard]] auto remove_agent_scratch_folders(const std::filesystem::path& aRoot, std::optional<std::int64_t> aBeforeSeconds, const std::vector<std::string>& aKeep) -> agent_scratch_cleanup;
	/// \brief The files new or changed between two listings, in path order,
	/// at most `aLimit`.
	[[nodiscard]] auto agent_scratch_written(const agent_scratch_listing& aBefore, const agent_scratch_listing& aAfter, std::size_t aLimit) -> std::vector<std::string>;

	/// \brief Removes the folder for `aKey` under `aRoot` and everything in
	/// it, without following a link out. True when it is gone.
	[[nodiscard]] auto remove_agent_scratch_folder(const std::filesystem::path& aRoot, std::string_view aKey) -> bool;

	/// \brief RFC-BPR-0104, fourth review (2026-11-11). Every spelling an
	/// agent may key the checkout at `aPath` by: the path as given, its
	/// canonical path, and the path the system reports for the opened folder
	/// (Codex keys a project by its working folder as the kernel names it,
	/// which differs from the canonical path under a macOS firmlink such as
	/// `/System/Volumes/Data`). The given path first, each once. Nothing
	/// when the folder cannot be resolved or opened: a checkout that cannot
	/// be named is never launched in.
	[[nodiscard]] auto agent_checkout_spellings(const std::string& aPath) -> std::optional<std::vector<std::string>>;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SCRATCH_FOLDER_HPP
