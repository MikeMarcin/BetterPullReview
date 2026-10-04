#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_PROMOTION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_PROMOTION_HPP

#include "bpr/core/review_cycle_association.hpp"
#include "bpr/core/review_cycle_feedback.hpp"
#include "bpr/core/review_cycle_publication.hpp"
#include "bpr/core/review_tour.hpp"
#include "bpr/core/review_tour_destination.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

/// \file
/// \brief [[BPR-1273]]. A local note, deliberately promoted into the Review
/// Tour of the pull request its cycle became (RFC-BPR-0059, RC59-09).
/// \details A promoted note becomes one tour stop with the note's own words,
/// unchanged. It keeps exact lines only when they are proved: the note was
/// written on the revision a native publication published, and that
/// publication's commit holds the note's file with exactly the bytes the note
/// was written against (the plan names the file's object, and the bytes read
/// back hash to it). Every other note carries no source at all: it was
/// written on an earlier capture, the commit holds other bytes at its path, it
/// names a whole file, a folder or the whole change, or its pull request was
/// linked by hand and nothing here knows which bytes it holds. Nothing
/// chooses a convenient current line; the reviewer chooses lines when they
/// complete the stop, and the tour's own placement policy
/// (`plan_tour_destination`) decides how it appears on GitHub when the tour
/// is published through its own preview. Promotion itself sends nothing.
///
/// Each promotion carries a request identity of the cycle, the note and the
/// pull request, so a promotion whose answer was lost is asked again under
/// the same identity and the tour's request receipts answer it without a
/// second stop.
namespace bpr::core {
	/// \brief Where a promoted note's stop points, and why.
	enum class cycle_promotion_source : std::uint8_t {
		/// The note's lines, at the published commit, which holds its file's exact bytes.
		exact_lines,
		/// Written on an earlier capture than the one published.
		earlier_revision,
		/// The published commit holds other bytes at the note's path: a
		/// choice left the change out, line endings were normalised, or it
		/// deletes the file.
		other_bytes,
		/// The note's file is not among the commit's changes: nothing here
		/// proves which bytes the commit holds there.
		outside_changes,
		/// The note's bytes could not be read back, or did not hash to the
		/// published object.
		unread,
		/// A note about a whole file: it names no lines.
		whole_file,
		/// A note about a folder.
		folder,
		/// A note about the whole change.
		whole_change,
		/// The pull request was linked by hand: no plan says what it holds.
		linked_by_hand,
	};
	[[nodiscard]] auto name(cycle_promotion_source aSource) noexcept -> std::string_view;

	enum class cycle_promotion_refusal : std::uint8_t {
		/// The note has no words to promote.
		empty,
		/// The note is longer than a tour stop holds.
		bounds,
		/// The association, plan or note do not belong together.
		mismatch,
	};
	[[nodiscard]] auto name(cycle_promotion_refusal aRefusal) noexcept -> std::string_view;

	/// \brief One note as the cycle lists it: its primary location and its
	/// current wording (the latest revision of its first turn). Replies,
	/// decisions, suggestions and earlier wordings are never promoted.
	struct cycle_promotion_note
	{
		cycle_feedback_item_ref Item;
		cycle_feedback_target   Primary;
		std::string             Body;
	};

	/// \brief One promotion: the stop to add, why it points where it does,
	/// and the identity that makes adding it again change nothing.
	struct cycle_promotion
	{
		cycle_feedback_item_ref Item;
		cycle_promotion_source  Source = cycle_promotion_source::whole_change;
		/// Title, the note's words as its body, and the exact lines when proved.
		tour_stop Stop;
		/// At most 64 bytes, as a tour request receipt holds.
		std::string                  RequestId;
		std::array<std::uint8_t, 32> Digest{ };
	};

	/// \brief The object a native publication's plan writes at `aPath`, if any.
	[[nodiscard]] auto published_blob(const publication_plan& aPlan, std::string_view aPath) noexcept -> const candidate_blob*;

	/// \brief Whether promotion would read the note's file to prove its
	/// lines: an anchor on the published revision whose path the plan
	/// writes with exactly the note's bytes.
	[[nodiscard]] auto promotion_needs_file(const cycle_pull_association& aLink, const publication_plan* aPlan, const cycle_promotion_note& aNote) noexcept -> bool;

	/// \brief Promotes one note into a stop for the Review Tour of `aLink`'s
	/// pull request.
	/// \param aSubject The cycle the note belongs to; its identity names the request.
	/// \param aPlan The native publication's plan that `aLink` came from, or
	/// nothing for a pull request linked by hand.
	/// \param aFile The bytes of the note's file at the published revision, as
	/// read back for `promotion_needs_file`; nothing otherwise.
	[[nodiscard]] auto promote_note(const local_cycle_subject& aSubject, const cycle_pull_association& aLink, const publication_plan* aPlan, const cycle_promotion_note& aNote,
									std::optional<std::span<const std::uint8_t>> aFile) -> std::expected<cycle_promotion, cycle_promotion_refusal>;

	/// \brief How the tour's own placement policy would publish `aStop`, as
	/// tour publication observes it on a host that qualifies its operations,
	/// for a tour whose pull request's head is `aHead`. A stop without a
	/// source is unresolved until the reviewer chooses its lines.
	[[nodiscard]] auto promoted_destination(const tour_stop& aStop, const git_commit_oid& aHead, const git_commit_oid& aBase) -> tour_destination_plan;
}

#endif
