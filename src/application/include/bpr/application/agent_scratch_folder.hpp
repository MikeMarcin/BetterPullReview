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
	/// \brief Whether a turn starting now gets no write allowance there.
	[[nodiscard]] auto agent_scratch_full(const agent_scratch_listing& aListing) noexcept -> bool;
	/// \brief The files new or changed between two listings, in path order,
	/// at most `aLimit`.
	[[nodiscard]] auto agent_scratch_written(const agent_scratch_listing& aBefore, const agent_scratch_listing& aAfter, std::size_t aLimit) -> std::vector<std::string>;

	/// \brief Removes the folder for `aKey` under `aRoot` and everything in
	/// it, without following a link out. True when it is gone.
	[[nodiscard]] auto remove_agent_scratch_folder(const std::filesystem::path& aRoot, std::string_view aKey) -> bool;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SCRATCH_FOLDER_HPP
