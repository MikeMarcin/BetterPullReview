#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_COMMENT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_COMMENT_HPP
#include "bpr/core/diff_model.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/note_placement.hpp"
#include "bpr/core/review_round_trip.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace bpr::core {
	/// \brief RFC-BPR-0127. The three states of one comment.
	///
	/// \details Local is only on this computer, and no action that sends
	/// anything ever sends it. Draft is mine, not sent yet, and meant to be.
	/// Published is on GitHub. A Local comment is its own record
	/// (`local_comment_anchor` with its text), never a `review_comment_draft`,
	/// so no path that publishes a draft can be handed one.
	enum class comment_state : std::uint8_t {
		local,
		draft,
		published,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(comment_state aState) noexcept -> std::string_view;

	/// \brief The state a draft record is in: Published once published,
	/// otherwise Draft. A draft record is never Local.
	[[nodiscard]] auto state_of(const review_comment_draft& aDraft) noexcept -> comment_state;

	/// \brief RFC-BPR-0127, "Migration". Local comments a review may keep:
	/// the note bound, outside the 64 active drafts.
	inline constexpr std::size_t maximum_local_comments = 4'096U;
	/// \brief A comment's body bound in every state: a draft's, so Keep
	/// local never cuts a draft's text.
	inline constexpr std::size_t maximum_comment_body_bytes = 64U * 1'024U;
	/// \brief The longest provider thread identity a Local reply keeps.
	inline constexpr std::size_t maximum_local_reply_thread_bytes = 1'024U;

	/// \brief RFC-BPR-0127. Where a Local comment is.
	///
	/// \details `Place` is a note's anchor: none is the pull request, a file
	/// without a line its whole file, a line the line and the blob its side
	/// showed. A range keeps its first line in `StartLine`, on the line's
	/// side, above it. A Local reply names its thread; its line is the
	/// thread's line when it was written, or none.
	struct local_comment_anchor
	{
		note_anchor                  Place;
		std::optional<std::uint32_t> StartLine;
		std::optional<std::string>   ReplyToThread;

		auto operator<=>(const local_comment_anchor&) const = default;
	};

	/// \brief A file and comparison together, a line only with a file, a
	/// range only on a line and above it and never on a reply, a reply's
	/// thread non-empty and bounded, and a kept blob a real one.
	[[nodiscard]] auto valid(const local_comment_anchor& aAnchor) noexcept -> bool;

	/// \brief Why Keep local is refused. Nothing is changed by a refusal.
	enum class keep_local_refusal : std::uint8_t {
		/// A submitted review holds the draft until its answer is known.
		held_by_review,
		/// The draft is being published on its own.
		publishing,
		/// The comment is on GitHub; Published is GitHub's.
		published,
		/// The draft was discarded.
		discarded,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(keep_local_refusal aRefusal) noexcept -> std::string_view;

	/// \brief What Keep local is decided from.
	struct keep_local_input
	{
		review_comment_draft Draft;
		/// The draft's file, which a draft's comparison digest does not name.
		file_identity File;
		/// The blob the draft's side shows, when its comparison is the one
		/// shown; none keeps the comparison alone, and another comparison
		/// shows it outdated (`note_outdated_reason::unmatched`).
		std::optional<git_blob_oid> ShownBlob;

		auto operator<=>(const keep_local_input&) const = default;
	};

	/// \brief RFC-BPR-0127, "Transitions". Draft to Local: the same text,
	/// line or range and reply target, out of the review.
	/// \details A thread-bound reply keeps its thread and no line.
	[[nodiscard]] auto decide_keep_local(const keep_local_input& aInput) -> std::expected<local_comment_anchor, keep_local_refusal>;

	/// \brief Why Make draft is refused. Nothing is changed by a refusal.
	enum class make_draft_refusal : std::uint8_t {
		/// The shown comparison does not place the comment at a line of its
		/// own (outdated, another file, the whole file or the pull request)
		/// and no line is selected.
		select_a_line,
		/// My draft with words already stands at that line or in that thread.
		draft_stands,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(make_draft_refusal aRefusal) noexcept -> std::string_view;

	/// \brief What Make draft is decided from.
	struct make_draft_input
	{
		local_comment_anchor Anchor;
		/// `place_note(Anchor.Place, shown)` against the comparison shown.
		note_placement Placement;
		/// The comparison shown, when one is.
		std::optional<file_comparison_digest> Shown;
		/// The line selected in the code, when one is.
		std::optional<source_line_ref> Selected;
		/// My drafts of this review.
		std::span<const review_comment_draft> Drafts;
	};

	/// \brief Where Make draft puts the draft.
	struct make_draft_target
	{
		source_line_ref              Anchor;
		std::optional<std::uint32_t> StartLine;
		std::optional<std::string>   ReplyToThread;
		/// It was put at the selected line, not its own ("Make draft on line N").
		bool Reanchored = false;

		auto operator<=>(const make_draft_target&) const = default;
	};

	/// \brief RFC-BPR-0127, "Transitions". Local to Draft: a draft of the
	/// same kind at the same place, not in the review.
	/// \details A Local reply becomes a reply to its thread. A Local comment
	/// the shown comparison places at its line becomes a draft there, its
	/// range kept. Otherwise it is offered only at the selected line, as
	/// one line. Refused where my draft with words stands.
	[[nodiscard]] auto decide_make_draft(const make_draft_input& aInput) -> std::expected<make_draft_target, make_draft_refusal>;

	/// \brief RFC-BPR-0127, "One action where there were two". What the one
	/// keep action of an agent surface makes.
	enum class keep_kind : std::uint8_t {
		/// Draft comment: a draft reply, or a draft at its line.
		draft,
		/// Keep local: a Local comment on the file, or on the pull request.
		local,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(keep_kind aKind) noexcept -> std::string_view;

	/// \brief Where a Local comment the keep action makes is anchored.
	enum class keep_local_place : std::uint8_t {
		file,
		pull_request,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(keep_local_place aPlace) noexcept -> std::string_view;

	/// \brief What one agent surface knows about what it would keep.
	struct keep_facts
	{
		/// It is about a review thread the reviewer may reply in.
		bool ThreadReply = false;
		/// A line of the pull request at its current head takes a draft: the
		/// lines asked about, or a finding's first line within its file.
		bool LineTakesDraft = false;
		/// It names a file the pull request changes.
		bool FileInPullRequest = false;

		auto operator<=>(const keep_facts&) const = default;
	};

	/// \brief The keep action's choice.
	struct keep_choice
	{
		keep_kind Kind = keep_kind::local;
		/// Where a Local comment goes; the pull request for a draft.
		keep_local_place Local = keep_local_place::pull_request;

		auto operator<=>(const keep_choice&) const = default;
	};

	/// \brief RFC-BPR-0127, "Architectural allocation": the keep action's
	/// choice of state from whether a draft has a place.
	/// \details A thread reply or a line that takes a draft makes a draft
	/// (Draft comment). Otherwise it is kept local (Keep local): on the file
	/// when it names one the pull request changes, else on the pull request.
	/// The surface's follow-up goes to what it made (Go to draft, Go to
	/// comment).
	[[nodiscard]] auto decide_keep(const keep_facts& aFacts) noexcept -> keep_choice;
} // namespace bpr::core
#endif
