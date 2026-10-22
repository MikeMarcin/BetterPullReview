#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_BRIEF_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_BRIEF_HPP

#include "bpr/application/github_read_model.hpp"
#include "bpr/core/agent_context_supplement.hpp"

#include <optional>

namespace bpr::application {
	/// \brief [[BPR-PENDING-AQ5]], RFC-BPR-0092. What a review request's brief
	/// reads from the accepted pull-request snapshot, before its bounds.
	///
	/// \details The title, description and base branch; every changed file
	/// with its status; and the review threads: the GraphQL threads with
	/// their resolution when the snapshot has them, else the REST comment
	/// groups, which carry no resolution and read as unresolved. Deleted
	/// comments are left out, and so is a thread left with none. A comment's
	/// author is its login, or empty when GitHub named none. Nothing here is
	/// read that the signed-in reviewer cannot already read on GitHub.
	[[nodiscard]] auto read_review_brief(const github_pull_request_snapshot& aSnapshot, std::optional<core::brief_review_skill> aSkill) -> core::review_brief_input;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_BRIEF_HPP
