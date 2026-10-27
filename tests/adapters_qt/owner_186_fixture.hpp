#ifndef BPR_TESTS_ADAPTERS_QT_OWNER_186_FIXTURE_HPP
#define BPR_TESTS_ADAPTERS_QT_OWNER_186_FIXTURE_HPP

// [[BPR-1779]]. A sanitized forest shaped like the owner's #186, as the
// drawer's Pull request view showed it on 2026-10-26: a review with no
// overall body that carried two file comments, two body-less "Commented"
// reviews (each a reply posted later in one of those threads), and two
// approvals with nothing to say.

#include "bpr/application/github_projection.hpp"

#include <string>
#include <vector>

namespace bpr::test {
	[[nodiscard]] inline auto owner_186_forest() -> std::vector<application::github_conversation_projection>
	{
		using conversation = application::github_conversation_projection;
		using kind         = application::github_conversation_kind;
		using state        = application::github_review_state_projection;
		const auto review  = [](std::string aId, std::string aAuthor, std::string aAt, const state aState, std::string aBody = { }) {
			return conversation{ .Id = std::move(aId), .Kind = kind::review, .AuthorLogin = std::move(aAuthor), .Body = std::move(aBody), .ReviewState = aState, .CreatedAt = std::move(aAt) };
		};
		const auto thread = [](std::string aId, std::string aPath, const std::uint32_t aLine) {
			return conversation{ .Id = std::move(aId), .Kind = kind::review_thread, .Path = std::move(aPath), .Line = aLine, .DiffSide = application::github_conversation_diff_side::right, .AnchorAvailability = application::github_conversation_anchor_availability::current };
		};
		const auto post = [](std::string aId, std::string aParent, std::string aAuthor, std::string aBody, std::string aPath, const std::uint32_t aLine, std::string aReview, std::string aAt) {
			return conversation{ .Id = std::move(aId), .ParentId = std::move(aParent), .Kind = kind::review_comment, .AuthorLogin = std::move(aAuthor), .Body = std::move(aBody), .Path = std::move(aPath), .Line = aLine, .DiffSide = application::github_conversation_diff_side::right, .AnchorAvailability = application::github_conversation_anchor_availability::current, .CreatedAt = std::move(aAt), .ReviewId = std::move(aReview) };
		};
		const auto cpp = std::string{ "Client/AppCore/src/Instance.cpp" };
		const auto h   = std::string{ "Client/AppCore/include/AppCore/Instance.h" };
		return {
			review("review:dos", "dosullivan", "2026-10-20T10:00:00Z", state::commented),
			thread("review-thread:cpp", cpp, 42U),
			post("review-comment:cpp", "review-thread:cpp", "dosullivan", "Shutdown can run before `Init` has finished on the loader thread. Should this wait on the init future first?", cpp, 42U, "review:dos", "2026-10-20T10:00:00Z"),
			post("review-comment:cpp-reply", "review-comment:cpp", "fzhu", "Good catch, it should. I'll join the future in `Shutdown`.", cpp, 42U, "review:fzhu", "2026-10-21T09:00:00Z"),
			thread("review-thread:h", h, 18U),
			post("review-comment:h", "review-thread:h", "dosullivan", "nit: this can be `const`; nothing mutates it after construction.", h, 18U, "review:dos", "2026-10-20T10:00:00Z"),
			post("review-comment:h-reply", "review-comment:h", "dosullivan", "Never mind, `Reload` writes it. Ignore.", h, 18U, "review:dos2", "2026-10-21T11:00:00Z"),
			review("review:fzhu", "fzhu", "2026-10-21T09:00:00Z", state::commented),
			review("review:dos2", "dosullivan", "2026-10-21T11:00:00Z", state::commented),
			review("review:csw", "cswiedler", "2026-10-22T08:00:00Z", state::approved),
			review("review:sau", "saustin", "2026-10-23T08:00:00Z", state::approved),
		};
	}
}

#endif
