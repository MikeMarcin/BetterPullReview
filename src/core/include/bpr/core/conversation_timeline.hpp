#ifndef BPR_CORE_CONVERSATION_TIMELINE_HPP
#define BPR_CORE_CONVERSATION_TIMELINE_HPP

/// \file
/// \brief [[BPR-1779]]. The pull request's conversation, read as comments.
///
/// \details The owner, 2026-10-27: "to me the review is just a bundle of
/// comments posted together and what really matters is the comments
/// themselves on the files and lines". So the conversation views list
/// comments: each pull request comment, each review's overall body, and each
/// inline thread a review carried, in its own right. A review is attribution
/// only: one quiet line over the comments it bundled, naming who and when,
/// and its verdict only when it is Approved or Changes requested. A review
/// with no body that carried nothing shows only when its verdict matters for
/// merging, as a compact verdict line; a mere "Commented" one shows nothing.
///
/// Pure: equal items and filters give equal rows, in equal order.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace bpr::core {
	/// \brief What a conversation item is on GitHub.
	enum class timeline_item_kind : std::uint8_t {
		/// A comment on the pull request itself.
		pull_request_comment,
		/// A submitted review: its verdict and its overall body, if any.
		review,
		/// An inline comment thread, on a file and line.
		thread,
	};

	/// \brief A review's verdict as GitHub states it.
	enum class timeline_verdict : std::uint8_t {
		none,
		commented,
		approved,
		changes_requested,
		dismissed,
	};

	/// \brief One root of the conversation, as the timeline needs it.
	struct timeline_item
	{
		/// The comment's, review's or thread's identity.
		std::string        Id;
		timeline_item_kind Kind = timeline_item_kind::pull_request_comment;
		/// A review's own identity; a thread's, the review its first comment
		/// was posted in; empty when not known or not posted in a review.
		std::string Review;
		std::string Author;
		/// GitHub's creation or submission time, ISO 8601; a thread's is its
		/// first comment's. Empty for legacy observations: they come first.
		std::string CreatedAt;
		/// A review's verdict; `none` otherwise.
		timeline_verdict Verdict = timeline_verdict::none;
		/// A review has an overall body; a comment or thread is always one.
		bool HasBody = true;
		/// A thread's file and one-based line, which order a bundle's threads.
		std::string                  Path;
		std::optional<std::uint32_t> Line;
		/// A post in it is newer than what I have read.
		bool Unread = false;
		/// A resolved thread.
		bool Resolved = false;
		/// Never filtered out: the open thread, one holding my unsent reply.
		bool Kept = false;

		auto operator==(const timeline_item&) const -> bool = default;
	};

	/// \brief The conversation views' filters, which act on comments.
	struct timeline_filters
	{
		bool UnreadOnly     = false;
		bool UnresolvedOnly = false;
		/// [[BPR-1089]]. Exactly these comments, by identity.
		std::optional<std::set<std::string>> Listed;

		auto operator==(const timeline_filters&) const -> bool = default;
	};

	/// \brief What a row of the timeline is.
	enum class timeline_row_kind : std::uint8_t {
		/// The quiet line over a review's comments: who, when, and the
		/// verdict when it matters.
		bundle,
		/// A review that carried no comment, kept for its verdict: Approved
		/// or Changes requested, with whoever gave it.
		verdict,
		/// A comment in its own right: a pull request comment, a review's
		/// overall body, or an inline thread.
		comment,
	};

	/// \brief One row of the timeline.
	struct timeline_row
	{
		timeline_row_kind Kind = timeline_row_kind::comment;
		/// A comment row's item; a bundle or verdict row's review.
		std::string Id;
		/// The review whose bundle the row belongs to; empty for a comment
		/// posted outside any review shown.
		std::string Review;
		/// A bundle or verdict row's reviewer and time.
		std::string Author;
		std::string CreatedAt;
		/// A bundle or verdict row's verdict, only when it is Approved or
		/// Changes requested; `none` otherwise.
		timeline_verdict Verdict = timeline_verdict::none;
		/// A comment row's item kind.
		timeline_item_kind Item = timeline_item_kind::pull_request_comment;

		auto operator==(const timeline_row&) const -> bool = default;
	};

	/// \brief The rows in reading order, and what they count.
	struct conversation_timeline
	{
		std::vector<timeline_row> Rows;
		/// Comment rows: comments, review bodies and threads. Reviews are not
		/// counted.
		std::size_t Comments = 0U;
		/// Of those, inline threads.
		std::size_t Threads = 0U;
		/// Of the comments, those with a post I have not read.
		std::size_t Unread = 0U;
		/// Verdict rows.
		std::size_t Verdicts = 0U;

		auto operator==(const conversation_timeline&) const -> bool = default;
	};

	/// \brief Whether a review's verdict is named on its line: Approved and
	/// Changes requested matter for merging; Commented and the rest do not.
	[[nodiscard]] constexpr auto timeline_verdict_named(const timeline_verdict aVerdict) noexcept -> bool
	{
		return aVerdict == timeline_verdict::approved || aVerdict == timeline_verdict::changes_requested;
	}

	/// \brief Arranges the conversation's roots as the views list them.
	///
	/// \details Units in time order: each review's bundle at its submission
	/// time, each comment outside a bundle at its own. A bundle is its line,
	/// then the review's body, then its threads in file and line order. A
	/// thread whose review is not among the items stands alone. A review with
	/// no body that carried no comment adds a verdict row when its verdict is
	/// named and nothing otherwise. Equal times keep the items' order.
	///
	/// Filters act on comment rows only; a kept item is never filtered out.
	/// While any filter is on, verdict rows are left out (they are not
	/// comments), and a bundle shows its line only over comments still shown.
	[[nodiscard]] auto arrange_conversation_timeline(std::span<const timeline_item> aItems, const timeline_filters& aFilters = { })
		-> conversation_timeline;
}

#endif
