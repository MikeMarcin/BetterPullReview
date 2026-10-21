#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_HOME_DISCOVERY_PLAN_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_HOME_DISCOVERY_PLAN_HPP

#include "bpr/core/github_model.hpp"

#include <compare>
#include <cstdint>
#include <span>
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
