#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_HOME_DISCOVERY_PLAN_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_HOME_DISCOVERY_PLAN_HPP

#include "bpr/core/github_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief [[BPR-1680]]. Why a discovery search looks for the viewer's
	/// pull requests.
	enum class discovery_search_reason : std::uint8_t {
		authored,
		requested,
	};

	/// \brief [[BPR-1680]]. What one refresh's retained read established about
	/// a pull request, enough to stand for a search row's own detail reads.
	struct discovery_retained_read
	{
		repository_locator  Repository;
		pull_request_number Number;
		/// The pull request was open when it was read.
		bool Open = false;
		/// Its requested reviewers and Home summary were read in full.
		bool Complete = false;
		/// Its requested reviewers name the viewer as a user, not only a team.
		bool RequestsViewer = false;

		auto operator<=>(const discovery_retained_read&) const = default;
	};

	/// \brief [[BPR-1680]]. The pull requests a refresh's searches need not
	/// read again: each one this refresh's retained read found open and read
	/// in full. In repository and number order, without repeats.
	///
	/// \details A refresh reads its retained pull requests before it searches,
	/// so a pull request both retained and found was read twice. A search row
	/// for one of these takes the retained read's detail instead of its own
	/// requested-reviewer, pull request and Home summary reads.
	[[nodiscard]] auto discovery_known_pull_requests(std::span<const discovery_retained_read> aReads) -> std::vector<discovery_retained_read>;

	/// \brief [[BPR-1680]]. The longest personal search query, in bytes, and
	/// the most saved repositories one search names.
	///
	/// \details GitHub refuses a search query longer than 256 characters; its
	/// documentation says operators and qualifiers do not count, but this
	/// bound counts the whole query so that no server's reading of the rule
	/// refuses one. The repository count keeps one page of results likely to
	/// hold every match.
	inline constexpr std::size_t maximum_discovery_search_query_bytes  = 256U;
	inline constexpr std::size_t maximum_discovery_search_repositories = 10U;

	/// \brief [[BPR-1680]]. The query a personal search sends, exactly:
	/// `is:pr is:open author:LOGIN repo:OWNER/NAME ...` or with
	/// `review-requested:LOGIN`, in the repositories' order.
	[[nodiscard]] auto discovery_search_query(discovery_search_reason aReason, std::string_view aLogin, std::span<const repository_locator> aRepositories) -> std::string;

	/// \brief [[BPR-1680]]. The saved repositories' personal searches for one
	/// reason: each search names consecutive repositories, in the order given,
	/// as many as fit `maximum_discovery_search_query_bytes` and
	/// \p aMaximumRepositories (at least one, at most
	/// `maximum_discovery_search_repositories`). A repository in \p aAlone, or
	/// one whose query alone is already too long to share, is searched on its
	/// own, after the shared searches. Every repository is in exactly one
	/// search.
	[[nodiscard]] auto plan_discovery_searches(discovery_search_reason aReason, std::string_view aLogin, std::span<const repository_locator> aRepositories,
											   std::span<const repository_locator> aAlone = { }, std::size_t aMaximumRepositories = maximum_discovery_search_repositories)
		-> std::vector<std::vector<repository_locator>>;

	/// \brief [[BPR-1680]]. How one search ended, as far as splitting it goes.
	enum class discovery_search_outcome : std::uint8_t {
		/// Every match was read.
		complete,
		/// GitHub said its own search was incomplete.
		incomplete_results,
		/// More matched than one page holds.
		result_limit,
		/// More rows than the detail reads could confirm.
		detail_limit,
		/// GitHub refused a repository qualifier (removed, renamed, or not
		/// visible to this account).
		refused,
		/// Any other failure.
		failed,
	};

	/// \brief [[BPR-1680]]. Whether a search of several repositories is asked
	/// again one repository at a time instead of being applied.
	///
	/// \details A search of one repository used to say on its own whether that
	/// repository's answer was complete and whether that repository could be
	/// read. A refusal names no repository, and a page or detail bound cannot
	/// say whose rows it left out, so either would make every repository of a
	/// shared search partial or unreadable; asked alone, each gets its own
	/// answer as before. GitHub's own incomplete search and other failures are
	/// what they were, for each repository the search names.
	[[nodiscard]] auto splits_discovery_search(std::size_t aRepositories, discovery_search_outcome aOutcome) noexcept -> bool;

	/// \brief [[BPR-1680]]. Whether a search row that reuses a retained read
	/// stays in the search's answer.
	///
	/// \details An authored search matched the author itself, so its row
	/// stays. A requested search keeps a row only when the pull request asks
	/// the viewer by name, exactly as its own requested-reviewer read would
	/// have decided: a team-only request is not the viewer's.
	[[nodiscard]] auto keeps_reused_row(discovery_search_reason aReason, const discovery_retained_read& aRead) noexcept -> bool;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_HOME_DISCOVERY_PLAN_HPP
