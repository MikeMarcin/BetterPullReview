#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_PULL_REQUEST_LINK_ROUTE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_PULL_REQUEST_LINK_ROUTE_HPP

#include "bpr/core/github_model.hpp"
#include "bpr/core/pull_request_location.hpp"
#include "bpr/core/review_tour.hpp"
#include "bpr/core/setup_destination.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace bpr::core {
	/// \brief The pull request a window shows, as it accepted it: the profile
	/// authority's hostname and the read model's owner, name and number.
	struct pull_request_link_scope
	{
		std::string_view    Hostname;
		std::string_view    Owner;
		std::string_view    Repository;
		pull_request_number Number;
		/// The full object names of the pull request's commits, as read.
		std::span<const std::string> Commits;
	};

	/// \brief Where a clicked body link goes.
	enum class pull_request_link_route : std::uint8_t {
		/// An external link a person wrote: the link question, then the browser.
		external,
		/// One of the offered published tours, opened in Review's Tour tab.
		tour,
		/// The pull request's own page, its conversation: Overview's Comments.
		conversation_page,
		/// The pull request's Files changed page: Review.
		files_page,
		/// The pull request's Commits page: Overview's Commits tab.
		commits_page,
		/// One of the pull request's commits (`/commits/<sha>`): the Commits
		/// tab showing only that commit's changes. `Commit` is its full name.
		commit,
		/// A place in the pull request the window shows itself (a file, a
		/// line, a review or a review comment), followed as a pasted link's
		/// location is ([[BPR-0987]]).
		location,
	};

	/// \brief The core's decision for one clicked link. `Tour` indexes the
	/// roots the caller offered; `Target` is the resolved location.
	struct pull_request_link_decision
	{
		pull_request_link_route      Route = pull_request_link_route::external;
		std::size_t                  Tour  = 0;
		pull_request_location_target Target;
		std::string                  Commit;

		auto operator==(const pull_request_link_decision&) const -> bool = default;
	};

	/// \brief Decides whether a clicked, admitted link into GitHub is one
	/// into this window's own pull request that the application shows
	/// itself, with no question and no browser.
	///
	/// \details `aLink` is the link read as a pull request address
	/// (`application::parse_setup_destination`). It names this pull request
	/// only on the window's own host, owner and repository (compared
	/// without regard to ASCII case) and number; any other pull request,
	/// repository or host is external, because the window cannot know what
	/// it is without reading it.
	///
	/// A tour's description link `[Review tour](…)` and a stop's Tour
	/// overview link name the tour's index comment: a review
	/// (`#pullrequestreview-<id>`) or, in compatibility mode, a conversation
	/// comment (`#issuecomment-<id>`). When that identity and kind is one of
	/// `aRoots` (bindings of tour index comments already validated against
	/// this pull request), the link opens that tour.
	///
	/// Otherwise the bare page is its conversation, `/files` is Review and
	/// `/commits` the Commits tab. A commit page names one of
	/// `aScope.Commits` when its name (seven or more digits) is the start of
	/// exactly one of them. A fragment is resolved against `aFacts` by
	/// `resolve_pull_request_location`: a file, a line, a review or a
	/// review comment the read identifies is shown in place. What it would
	/// only explain (a commit the read does not hold, a single conversation
	/// comment, a review or comment the read does not hold, an unknown
	/// fragment) stays external, so the browser can still show it. Pure.
	[[nodiscard]] auto route_pull_request_link(const setup_destination&             aLink,
											   const pull_request_link_scope&       aScope,
											   std::span<const tour_remote_binding> aRoots,
											   const pull_request_location_facts&   aFacts) -> pull_request_link_decision;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_PULL_REQUEST_LINK_ROUTE_HPP
