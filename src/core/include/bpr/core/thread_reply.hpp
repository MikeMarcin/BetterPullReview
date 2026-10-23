#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_THREAD_REPLY_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_THREAD_REPLY_HPP
#include "bpr/core/review_round_trip.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-1745]]. Where a reply to a review thread is written.
	enum class thread_reply_route : std::uint8_t {
		/// On the thread's current line: the code selects that line and the
		/// reply is anchored there, as before.
		on_line,
		/// Bound to the thread alone (`thread_bound_reply`). The thread is
		/// outdated, so it has no line in the code; GitHub takes the reply by
		/// the thread, as its web interface does.
		on_thread,
		/// Not here; `thread_reply_refusal` says why.
		refused,
	};

	/// \brief [[BPR-1745]]. Why a thread takes no reply here.
	enum class thread_reply_refusal : std::uint8_t {
		none,
		/// Not a review thread on a file: a pull request comment is answered
		/// by its own conversation reply.
		not_a_review_thread,
		/// The session reads anonymously; a reply has no account to act as.
		anonymous_read,
		/// GitHub says this viewer cannot reply in the thread.
		viewer_cannot_reply,
		/// GitHub says this viewer cannot reply because the pull request's
		/// conversation is locked: only collaborators can reply.
		locked,
	};

	/// \brief [[BPR-1745]]. What GitHub's thread data says about one
	/// thread, as the accepted snapshot holds it.
	/// \details `ViewerCanReply` and `Locked` are GitHub's `viewerCanReply`
	/// and the pull request's lock (`locked`), read with the conversation
	/// ([[BPR-PENDING-AX]]). When the read does not carry one, as on a GitHub
	/// Enterprise Server whose schema lacks it, it is absent, and nothing is
	/// refused on it: GitHub decides at Publish and its answer is shown there.
	struct thread_reply_facts
	{
		/// A review thread on a file, not a pull request comment.
		bool ReviewThread = false;
		/// GitHub reports the thread outdated: its line has changed since it
		/// was written.
		bool Outdated = false;
		/// The session reads anonymously.
		bool AnonymousRead = false;
		/// GitHub's `viewerCanReply`, when the read carries it.
		std::optional<bool> ViewerCanReply;
		/// The pull request's conversation lock, when the read carries it.
		std::optional<bool> Locked;

		auto operator<=>(const thread_reply_facts&) const = default;
	};

	/// \brief [[BPR-1745]]. The route and, when refused, the reason.
	struct thread_reply_decision
	{
		thread_reply_route   Route   = thread_reply_route::refused;
		thread_reply_refusal Refusal = thread_reply_refusal::none;
		/// The reply field says, quietly, that the thread's line has changed
		/// since it was written.
		bool OutdatedNote = false;

		auto operator<=>(const thread_reply_decision&) const = default;
	};

	/// \brief [[BPR-1745]]. Decides where a reply to one thread is
	/// written, from GitHub's thread data alone.
	/// \details A current thread is answered on its line, and whether that
	/// line is in the code shown is the code's to say. An outdated thread is
	/// answered by the thread: GitHub accepts a reply to it because a reply
	/// names only the thread it answers. A thread GitHub says this viewer
	/// cannot reply in takes no reply, refused up front rather than after
	/// Publish; the reason is the lock when the pull request is locked. A
	/// lock alone refuses nothing: GitHub lets a collaborator reply on a
	/// locked pull request, and its `viewerCanReply` says so
	/// ([[BPR-PENDING-AX]]).
	[[nodiscard]] auto decide_thread_reply(const thread_reply_facts& aFacts) noexcept -> thread_reply_decision;

	/// \brief [[BPR-1745]]. The saved reply a thread's Reply opens
	/// again: the latest thread-bound reply to `aThread` still being written
	/// or being published, wherever it was written.
	/// \returns The draft to open, or none when a new one is to be made.
	[[nodiscard]] auto reopen_thread_reply(std::span<const review_comment_draft> aDrafts, std::string_view aThread) -> std::optional<review_draft_id>;

	[[nodiscard]] auto name(thread_reply_route aRoute) noexcept -> std::string_view;
	[[nodiscard]] auto name(thread_reply_refusal aRefusal) noexcept -> std::string_view;
}
#endif
