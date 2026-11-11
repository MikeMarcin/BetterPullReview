#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DRAFT_ROSTER_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DRAFT_ROSTER_HPP

#include "bpr/core/revisioned_review_draft.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief [[BPR-2092]] [[BPR-2093]] [[BPR-2094]]. Every unsent draft of mine,
/// where it is and where it stands with the review.
/// \details The owner, 2026-11-11: "I don't seem to have an easy way to find
/// all my draft comments", and "finish review should note draft comments that
/// haven't been added to the review". Threads lists the roster, Overview's
/// Comments column counts it per file, and Finish your review lists the drafts
/// it does not carry, with Add. Which drafts are shown, in which file, in
/// which order, and what Add marks are decided here from the saved drafts and
/// what the window knows of their files. Pure: nothing here reads a store,
/// opens a file or reaches GitHub.
namespace bpr::core {
	/// \brief Where one draft stands with the review.
	enum class draft_standing : std::uint8_t {
		/// Pending in the review: Submit sends it.
		in_review,
		/// A new comment the review does not carry: published only by its own
		/// Publish, or added to the review.
		not_in_review,
		/// A submitted review holds it until GitHub answers.
		held,
		/// A reply: sent on its own, never in a review.
		reply,
	};

	[[nodiscard]] auto stable_name(draft_standing aStanding) noexcept -> std::string_view;

	/// \brief Why Add cannot put a draft not in the review into it.
	enum class draft_add_refusal : std::uint8_t {
		none,
		/// A reply goes to its thread on its own.
		reply,
		/// Where a review would take it is not known: it was kept before
		/// drafts recorded their place. Opening it records one.
		place_unknown,
		/// It was written where a review cannot take it at its own line: a
		/// commit-filtered or earlier pair, a base line, or a line GitHub has no
		/// change near. Open it to publish or rewrite it.
		not_at_head,
	};

	[[nodiscard]] auto stable_name(draft_add_refusal aRefusal) noexcept -> std::string_view;

	/// \brief What the window knows besides the drafts.
	struct draft_roster_facts
	{
		/// The head the review is written on.
		git_commit_oid Head;
		/// Comparisons shown in this window, by digest, and the file's path at
		/// their head: a draft that recorded no place is placed by these.
		std::span<const std::pair<file_comparison_digest, std::string>> Shown;
		/// Each review thread on a file, by the provider's thread identity, and
		/// its path: a reply is in its thread's file.
		std::span<const std::pair<std::string, std::string>> Threads;
	};

	/// \brief One unsent draft, as every surface lists it.
	struct draft_roster_entry
	{
		review_draft_id       Id;
		std::uint64_t         Revision = 0U;
		review_draft_scope_id Scope;
		draft_standing        Standing = draft_standing::not_in_review;
		/// The file, or empty when it is not known.
		std::string Path;
		/// The anchor: zero-based line and side; the first line of a range.
		diff_side                    Side = diff_side::right;
		std::uint32_t                Line = 0U;
		std::optional<std::uint32_t> StartLine;
		/// The anchor's comparison, to open the draft at its line.
		file_comparison_digest     Comparison;
		std::optional<std::string> ReplyToThread;
		std::string                Body;
		/// For a draft not in the review: what Add marks, or why it cannot.
		std::optional<review_pending_mark> Mark;
		draft_add_refusal                  Refusal = draft_add_refusal::none;

		auto operator==(const draft_roster_entry&) const -> bool = default;
	};

	/// \brief The roster and its counts.
	struct draft_roster
	{
		/// In file order (unknown files last), then side, line and identity.
		std::vector<draft_roster_entry> Entries;
		std::size_t                     InReview    = 0U;
		std::size_t                     NotInReview = 0U;
		std::size_t                     Held        = 0U;
		std::size_t                     Replies     = 0U;

		auto operator==(const draft_roster&) const -> bool = default;
	};

	/// \brief The drafts still being written or held by a review, with words.
	/// \details An empty or blank draft is a line's placeholder, not a draft
	/// anyone wrote, and is never listed. Published and discarded drafts are
	/// gone. A thread-bound reply is listed in its thread's file at no line.
	[[nodiscard]] auto project_draft_roster(std::span<const revisioned_review_draft> aDrafts, const draft_roster_facts& aFacts) -> draft_roster;

	/// \brief One file's drafts, for Overview's Comments column.
	struct draft_file_count
	{
		std::string Path;
		/// In the review, or held by it.
		std::size_t InReview = 0U;
		/// Not in the review, replies included.
		std::size_t NotInReview = 0U;

		auto operator==(const draft_file_count&) const -> bool = default;
	};

	/// \brief Per file with drafts, in path order; drafts of unknown files are
	/// not counted on any file.
	[[nodiscard]] auto draft_counts_by_file(const draft_roster& aRoster) -> std::vector<draft_file_count>;

	/// \brief The drafts Finish your review notes: every one the review does
	/// not carry, in roster order.
	[[nodiscard]] auto drafts_outside_review(const draft_roster& aRoster) -> std::vector<draft_roster_entry>;

	/// \brief Add, for the chosen drafts (all of them for Add all): one edit
	/// per draft a review can take, at the revision listed, setting its mark.
	/// Drafts it cannot take, or not in the roster, are left out.
	[[nodiscard]] auto plan_add_to_review(const draft_roster& aRoster, std::span<const review_draft_id> aChosen) -> std::vector<revisioned_review_draft_command>;
} // namespace bpr::core

#endif
