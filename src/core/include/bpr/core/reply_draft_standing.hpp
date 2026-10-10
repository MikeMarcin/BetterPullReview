#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REPLY_DRAFT_STANDING_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REPLY_DRAFT_STANDING_HPP
#include "bpr/core/review_round_trip.hpp"
#include "bpr/core/revisioned_review_draft.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief [[BPR-1374]]. One review thread as the accepted server snapshot
	/// holds it, by the provider's opaque thread identity.
	struct reply_thread_fact
	{
		std::string Thread;
		bool        Resolved = false;

		auto operator<=>(const reply_thread_fact&) const = default;
	};

	/// \brief [[BPR-1374]]. The review threads the accepted server snapshot
	/// holds.
	/// \details `Complete` says the snapshot read the whole thread collection.
	/// When it did not (a signed-out read, a read that failed), a thread
	/// missing from `Threads` is not known to be gone, and nothing is decided
	/// from its absence.
	struct reply_thread_observation
	{
		bool                           Complete = false;
		std::vector<reply_thread_fact> Threads;

		auto operator<=>(const reply_thread_observation&) const = default;
	};

	/// \brief [[BPR-1374]]. Where an unsent reply stands against what the
	/// server holds now.
	enum class reply_draft_standing : std::uint8_t {
		/// Not an unsent reply: a new comment, one without words, or one
		/// published, being published or discarded.
		none,
		/// Its thread is on the server and open: shown on its thread, and
		/// sent there.
		answerable,
		/// Its thread is on the server and resolved: shown on its thread,
		/// flagged, and still sent there, as the server takes a reply on a
		/// resolved thread.
		thread_resolved,
		/// Its thread is no longer on the server: kept and flagged, never sent
		/// there. It can be copied, discarded, or kept as a new comment on its
		/// line.
		thread_deleted,
		/// The snapshot could not say whether its thread is there: kept as
		/// written, never called deleted, and not sent until the threads are
		/// read again.
		thread_unknown,
	};

	/// \brief [[BPR-1374]]. One unsent reply's standing and what may be done
	/// with it.
	struct reply_draft_decision
	{
		review_draft_id      Draft;
		std::string          Thread;
		reply_draft_standing Standing = reply_draft_standing::none;
		/// It may be published as a reply in its thread.
		bool Sendable = false;
		/// It may become a new comment on its own line, keeping its words.
		bool KeepableAsComment = false;

		auto operator<=>(const reply_draft_decision&) const = default;
	};

	/// \brief [[BPR-1374]]. Decides where one draft stands as an unsent reply.
	/// \details Only a reply still being written, with words in it, has a
	/// standing; every other draft is `none`. The server's truth is only read:
	/// a reply whose thread is gone is never sent, and nothing here recreates
	/// the thread or changes the server.
	[[nodiscard]] auto decide_reply_draft(const review_comment_draft& aDraft, const reply_thread_observation& aThreads) -> reply_draft_decision;

	/// \brief [[BPR-1374]]. The standing of every unsent reply among the
	/// saved drafts, in their order; drafts with no standing are left out.
	[[nodiscard]] auto decide_reply_drafts(std::span<const review_comment_draft> aDrafts, const reply_thread_observation& aThreads) -> std::vector<reply_draft_decision>;

	/// \brief [[BPR-1374]]. Which saved draft choosing a line opens again.
	/// \details The latest draft being written or published at exactly
	/// `aLine` that answers `aThread`, or that answers no thread when
	/// `aThread` is none. Before BPR-1374 a reply was never opened again, so
	/// after a Refresh or a restart the reviewer's unsent reply was not shown
	/// and Reply started an empty one.
	/// \returns The draft to open, or none when a new one is to be made.
	[[nodiscard]] auto reopen_draft_at(std::span<const review_comment_draft> aDrafts, const source_line_ref& aLine, const std::optional<std::string>& aThread) -> std::optional<review_draft_id>;

	/// \brief [[BPR-1374]]. The reply the editor held before a Refresh, to
	/// be held again after it.
	/// \details A Refresh retires the editor's draft with the composition it
	/// was written in. The same reply is held again when it is still unsent
	/// and its thread is still there; a reply whose thread is gone or unknown
	/// is shown as kept instead, and is never put back into a thread's
	/// in-place reply.
	[[nodiscard]] auto resume_reply_after_refresh(std::span<const review_comment_draft> aDrafts, review_draft_id aHeld, const reply_thread_observation& aThreads) -> std::optional<review_draft_id>;

	/// \brief [[BPR-1374]]. Keeps a reply whose thread is gone as a new
	/// comment on its line, with its words, at the revision given.
	/// \returns The command, or none when the decision does not allow it.
	[[nodiscard]] auto keep_reply_as_comment(const revisioned_review_draft& aDraft, const reply_thread_observation& aThreads) -> std::optional<revisioned_review_draft_command>;

	/// \brief [[BPR-1374]]. Discards an unsent reply, at the revision given.
	/// \returns The command, or none when it is not an unsent reply.
	[[nodiscard]] auto discard_unsent_reply(const revisioned_review_draft& aDraft) -> std::optional<revisioned_review_draft_command>;

	[[nodiscard]] auto name(reply_draft_standing aStanding) noexcept -> std::string_view;
}
#endif
