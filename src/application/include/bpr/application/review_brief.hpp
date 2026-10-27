#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_BRIEF_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_BRIEF_HPP

#include "bpr/application/github_read_model.hpp"
#include "bpr/core/agent_context_supplement.hpp"

#include <optional>

namespace bpr::application {
	/// \brief [[BPR-1752]], RFC-BPR-0092. What a review request's brief
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

	/// \brief [[BPR-1773]]. What an `ask_about_conversation` turn reads of
	/// one review thread, before its bounds: its path, its lines (the
	/// current ones, or the original ones of an outdated thread), its state
	/// and its comments oldest first, deleted ones left out. The excerpt is
	/// the composition's to add, from the file it shows.
	[[nodiscard]] auto read_conversation(const github_review_thread_record& aThread) -> core::conversation_input;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_BRIEF_HPP
