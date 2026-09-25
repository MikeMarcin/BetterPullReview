#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_CACHED_REVIEW_OVERVIEW_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_CACHED_REVIEW_OVERVIEW_HPP

#include "bpr/core/github_model.hpp"
#include <array>

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

	/// \brief Bounded historical metadata, without source, conversation or permission authority.
	/// Base/head are the exact accepted revisions, never a claim about the current server head.
	struct cached_review_overview
	{
		cached_review_key Key;
		repository_id     Repository;
		pull_request_id   PullRequest;
		std::string       BaseOid, HeadOid;
		std::string       Title, Body, AuthorLogin, BaseRef, HeadRef;
		std::uint32_t     Files = 0, Commits = 0;
		std::uint8_t      State                                            = 0;
		bool              Draft                                            = false;
		auto              operator<=>(const cached_review_overview&) const = default;
	};
	inline constexpr std::size_t maximum_cached_overviews      = 10;
	inline constexpr std::size_t maximum_cached_overview_bytes = 272 * 1024;

	/// \brief Validates storage bounds and exact durable identities.
	[[nodiscard]] auto valid(const cached_review_key& aKey) noexcept -> bool;
	[[nodiscard]] auto valid(const cached_review_overview& aOverview) noexcept -> bool;
	/// \brief Admits historical metadata only for the current authenticated transport-failed open.
	/// An accepted live snapshot, cancellation, identity retirement or a foreign key refuses it.
	[[nodiscard]] auto admit_cached_overview(const github_state&                 aState,
											 const pull_request_request_context& aRequest, github_failure aFailure,
											 const cached_review_key& aExpected, const cached_review_overview& aOverview) noexcept -> bool;
}
#endif
