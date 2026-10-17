#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_CACHED_REVIEW_OVERVIEW_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_CACHED_REVIEW_OVERVIEW_HPP

#include "bpr/core/github_model.hpp"
#include <array>
#include <optional>
#include <vector>

namespace bpr::core {
	/// \brief Exact authenticated reader and destination for one retained overview.
	/// Authority fingerprints the complete host profile, including its revision.
	struct cached_review_key
	{
		std::array<std::uint8_t, 32> Authority{ };
		host_profile_id              Profile;
		git_host_id                  Host;
		github_account_id            Account;
		repository_locator           Locator;
		pull_request_number          Number;
		auto                         operator<=>(const cached_review_key&) const = default;
	};

	enum class cached_file_change : std::uint8_t {
		added,
		modified,
		removed,
		renamed,
		copied,
		type_changed,
		unchanged,
	};

	/// \brief Historical provider file metadata; no source, reading or navigation authority.
	struct cached_review_file
	{
		std::string                Path;
		std::optional<std::string> PreviousPath;
		cached_file_change         Change    = cached_file_change::modified;
		std::uint64_t              Additions = 0, Deletions = 0;
		auto                       operator<=>(const cached_review_file&) const = default;
	};
	inline constexpr std::size_t maximum_cached_files           = 1024;
	inline constexpr std::size_t maximum_cached_catalogue_bytes = 128 * 1024;

	enum class cached_thread_side : std::uint8_t {
		unknown,
		left,
		right,
	};

	/// \brief Historical comment text and reply identity, without actionable authority.
	struct cached_thread_comment
	{
		std::string                  Id;
		std::optional<std::string>   ReplyTo;
		std::string                  AuthorLogin, Body;
		std::optional<std::uint32_t> Line, OriginalLine;
		bool                         Outdated = false, Deleted = false;
		auto                         operator<=>(const cached_thread_comment&) const = default;
	};
	/// \brief Thread observations at the overview's exact base/head; anchors are inert.
	struct cached_review_thread
	{
		std::string                        Id, Path;
		std::optional<std::uint32_t>       Line, OriginalLine;
		cached_thread_side                 Side      = cached_thread_side::unknown;
		bool                               FileLevel = false, Resolved = false, Outdated = false;
		std::vector<cached_thread_comment> Comments;
		auto                               operator<=>(const cached_review_thread&) const = default;
	};
	inline constexpr std::size_t maximum_cached_threads         = 128;
	inline constexpr std::size_t maximum_cached_thread_comments = 512;
	inline constexpr std::size_t maximum_cached_threads_bytes   = 128 * 1024;

	enum class cached_discussion_kind : std::uint8_t {
		general_comment,
		pending_review,
		commented_review,
		approved_review,
		changes_requested_review,
		dismissed_review,
	};
	/// \brief Historical general comment or review summary; no review authority.
	struct cached_discussion_message
	{
		std::string                Id, AuthorLogin, Body;
		cached_discussion_kind     Kind = cached_discussion_kind::general_comment;
		std::optional<std::string> CommitOid;
		bool                       Deleted                                             = false;
		auto                       operator<=>(const cached_discussion_message&) const = default;
	};
	inline constexpr std::size_t maximum_cached_discussion_messages = 256;
	inline constexpr std::size_t maximum_cached_discussion_bytes    = 64 * 1024;

	/// \brief Bounded historical metadata, without source, live conversation or permission authority.
	/// Base/head are the exact accepted revisions, never a claim about the current server head.
	/// [[BPR-1560]]. Base is the accepted comparison base: the merge base GitHub
	/// named when it named one, else the recorded base.
	struct cached_review_overview
	{
		cached_review_key Key;
		repository_id     Repository;
		pull_request_id   PullRequest;
		std::string       BaseOid, HeadOid;
		std::string       Title, Body, AuthorLogin, BaseRef, HeadRef;
		std::uint32_t     Files = 0, Commits = 0;
		std::uint8_t      State = 0;
		bool              Draft = false;
		/// Absent means unavailable; present empty means the accepted read observed no files.
		/// The catalogue and base/head are one atomic historical observation.
		std::optional<std::vector<cached_review_file>> FileCatalogue;
		/// Absent means unavailable; present empty means no threads were observed.
		std::optional<std::vector<cached_review_thread>>      ReviewThreads;
		std::optional<std::vector<cached_discussion_message>> GeneralComments, ReviewSummaries;
		auto                                                  operator<=>(const cached_review_overview&) const = default;
	};
	inline constexpr std::size_t maximum_cached_overviews      = 10;
	inline constexpr std::size_t maximum_cached_overview_bytes = 704 * 1024;

	/// \brief Bounds complete catalogue content and rejects duplicate paths and malformed records.
	[[nodiscard]] auto valid_cached_catalogue(const std::vector<cached_review_file>& aFiles) noexcept -> bool;
	/// \brief Bounds complete thread content and validates identities and ordered reply links.
	[[nodiscard]] auto valid_cached_threads(const std::vector<cached_review_thread>& aThreads) noexcept -> bool;
	/// \brief Validates a complete bounded general-comment or review-summary collection.
	[[nodiscard]] auto valid_cached_discussion(const std::vector<cached_discussion_message>& aMessages, bool aReviews) noexcept -> bool;
	/// \brief Validates storage bounds and exact durable identities.
	[[nodiscard]] auto valid(const cached_review_key& aKey) noexcept -> bool;
	[[nodiscard]] auto valid(const cached_review_overview& aOverview) noexcept -> bool;
	/// \brief Revalidates a retained historical read after a failed open; no live authority.
	[[nodiscard]] auto admit_cached_source(const github_state& aState, const pull_request_request_context& aRequest,
										   const cached_review_key& aExpected, const cached_review_overview& aOverview, std::string_view aPath) noexcept -> bool;
	/// \brief Admits historical metadata only for the current authenticated transport-failed open.
	/// An accepted live snapshot, cancellation, identity retirement or a foreign key refuses it.
	[[nodiscard]] auto admit_cached_overview(const github_state&                 aState,
											 const pull_request_request_context& aRequest, github_failure aFailure,
											 const cached_review_key& aExpected, const cached_review_overview& aOverview) noexcept -> bool;
}
#endif
