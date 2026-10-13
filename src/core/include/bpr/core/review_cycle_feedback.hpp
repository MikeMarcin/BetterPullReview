#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_FEEDBACK_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_FEEDBACK_HPP

#include "bpr/core/private_thread_lineage.hpp"
#include "bpr/core/review_cycle.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

/// \file
/// \brief Private feedback in a local Review Cycle (RFC-BPR-0055, BPR-1153):
/// notes on a range, file, module or the whole cycle, with related locations,
/// kept as versioned private threads.
/// \details A note's original attachment is immutable: the captured revision,
/// the exact file bytes (by digest) and a half-open byte range with the bytes
/// it held. Where that source sits in newer material is a separate, derived
/// observation that may be ambiguous; nothing here rewrites the original.
/// Threads share the provider-independent lineage of pull-request private
/// threads (`private_thread_lineage.hpp`) and hold no account, repository or
/// agent identity. Bodies are the author's raw Markdown and stay
/// authoritative; nothing interprets a label or rewrites text.
namespace bpr::core {
	/// Largest original range a note may hold, and the context kept around it.
	inline constexpr std::size_t maximum_cycle_anchor_bytes         = 64U * 1024U;
	inline constexpr std::size_t maximum_cycle_anchor_context_bytes = 512U;
	inline constexpr std::size_t maximum_cycle_symbol_bytes         = 256U;
	inline constexpr std::size_t maximum_cycle_related_references   = 32U;
	/// An adapter, provider, model or effort name is short text.
	inline constexpr std::size_t maximum_cycle_provenance_field_bytes = 128U;
	/// Context lines kept on each side of a new anchor, before the byte bound.
	inline constexpr std::size_t cycle_anchor_context_lines = 2U;

	/// \brief The whole cycle: a note about the change as a whole.
	struct cycle_whole_target
	{
		auto operator<=>(const cycle_whole_target&) const = default;
	};
	/// \brief A module: a root-relative directory or scope path, never a line.
	struct cycle_module_target
	{
		std::string Path;
		/// The revision the reviewer was reading, when there was one.
		std::optional<cycle_source_revision_id> Revision;
		auto                                    operator<=>(const cycle_module_target&) const = default;
	};
	/// \brief One file of one captured revision, by its exact bytes.
	struct cycle_file_target
	{
		cycle_source_revision_id Revision;
		std::string              Path;
		cycle_material_digest    Blob;
		auto                     operator<=>(const cycle_file_target&) const = default;
	};
	/// \brief A half-open byte range of one file of one captured revision.
	/// \details `Original` holds exactly bytes [Start, End); an empty range
	/// marks an insertion point. `Before` and `After` hold the bytes
	/// immediately around it, as found, so a later search can tell repeated
	/// snippets apart. Lines are a presentation of these offsets, not the
	/// durable coordinates. `Symbol` is a navigation hint, never proof.
	struct cycle_source_anchor
	{
		cycle_source_revision_id   Revision;
		std::string                Path;
		cycle_material_digest      Blob;
		std::uint64_t              Start = 0;
		std::uint64_t              End   = 0;
		std::string                Original;
		std::string                Before;
		std::string                After;
		std::optional<std::string> Symbol;
		auto                       operator<=>(const cycle_source_anchor&) const = default;
	};
	using cycle_feedback_target = std::variant<cycle_whole_target, cycle_module_target, cycle_file_target, cycle_source_anchor>;

	/// \brief The captured revision a target names, if any.
	[[nodiscard]] auto source_revision_of(const cycle_feedback_target& aTarget) noexcept -> std::optional<cycle_source_revision_id>;
	/// \brief True when the target is well formed and every revision it names
	/// was allocated by `aCycle`. Whether the bytes match is `verify_target`'s.
	[[nodiscard]] auto valid_target(const review_cycle_state& aCycle, const cycle_feedback_target& aTarget) -> bool;
	/// \brief True when a file or anchor target matches the exact bytes of
	/// its file: the digest, the range, its bytes and its context, with text
	/// ranges on UTF-8 scalar boundaries. Whole and module targets name no
	/// bytes and always verify.
	[[nodiscard]] auto verify_target(const cycle_feedback_target& aTarget, std::span<const std::uint8_t> aFile) -> bool;
	/// \brief Builds an anchor for [aStart, aEnd) of `aFile`, keeping up to
	/// two lines of context on each side within the byte bound. Nothing when
	/// the range is out of bounds, too large or splits a UTF-8 scalar.
	[[nodiscard]] auto make_anchor(cycle_source_revision_id aRevision, std::string aPath, std::span<const std::uint8_t> aFile, std::uint64_t aStart, std::uint64_t aEnd)
		-> std::optional<cycle_source_anchor>;

	/// \brief One-based, inclusive display lines. An insertion point shows as
	/// the line it sits on.
	struct cycle_line_span
	{
		std::uint64_t First                                     = 0;
		std::uint64_t Last                                      = 0;
		auto          operator<=>(const cycle_line_span&) const = default;
	};
	/// \brief Projects a byte range onto display lines. LF, CRLF and a lone
	/// CR each end a line. Nothing when the range lies outside the file.
	[[nodiscard]] auto project_lines(std::span<const std::uint8_t> aFile, std::uint64_t aStart, std::uint64_t aEnd) -> std::optional<cycle_line_span>;

	/// \brief A half-open byte range of one file.
	struct cycle_byte_range
	{
		std::uint64_t Start                                      = 0;
		std::uint64_t End                                        = 0;
		auto          operator<=>(const cycle_byte_range&) const = default;
	};
	/// \brief The text of the display line that holds byte `aOffset`, without
	/// its ending: what a note made at a caret is about ([[BPR-1157]]).
	/// \details LF and CRLF each end a line, as `project_lines` and Git count
	/// them, and a lone CR is content; an offset between a CR and its LF is the end of the line
	/// before. An empty line is an insertion point at its start. Nothing when
	/// the offset lies outside the file.
	[[nodiscard]] auto line_text_at(std::span<const std::uint8_t> aFile, std::uint64_t aOffset) -> std::optional<cycle_byte_range>;
	/// \brief The bytes of display lines `aLines` (one-based, inclusive):
	/// from the first line's start to the last line's end, without its
	/// ending, counted as `project_lines` counts them (BPR-1167). Nothing
	/// when a line is zero, the span runs backwards or the file has fewer lines.
	[[nodiscard]] auto line_span_bytes(std::span<const std::uint8_t> aFile, cycle_line_span aLines) -> std::optional<cycle_byte_range>;

	/// \brief Whether a related location travels with the note when a round is
	/// frozen. A private reference stays in the cycle.
	enum class cycle_reference_disclosure : std::uint8_t {
		included,
		private_only,
	};
	/// \brief A further location a note is about, with its own revision.
	/// \details `Rebinds` names an earlier reference this one re-attaches to
	/// newer source (0 for the primary, n for the nth related reference); the
	/// earlier one is kept as it was.
	struct cycle_related_reference
	{
		cycle_feedback_target        Target;
		cycle_reference_disclosure   Disclosure = cycle_reference_disclosure::included;
		std::optional<std::uint32_t> Rebinds;
		auto                         operator<=>(const cycle_related_reference&) const = default;
	};

	/// \brief Who wrote a revision, as recorded.
	/// \details Adapter, provider, model and effort are optional observations
	/// or claims, never inferred. Human and unknown revisions carry none; an
	/// adapter-observed revision names the adapter that observed it.
	struct cycle_feedback_author
	{
		authored_provenance        Provenance = authored_provenance::human;
		std::optional<std::string> Adapter;
		std::optional<std::string> Provider;
		std::optional<std::string> Model;
		std::optional<std::string> Effort;
		auto                       operator<=>(const cycle_feedback_author&) const = default;
	};
	[[nodiscard]] auto valid(const cycle_feedback_author& aAuthor) noexcept -> bool;

	/// \brief One feedback item: a private thread about one primary location.
	/// \details Revisions are numbered per thread; each new revision's order is
	/// the next one after `Lineage.LastOrder`, so its identity is that order.
	struct cycle_feedback_thread
	{
		cycle_feedback_item_ref              Id;
		cycle_feedback_target                Primary;
		std::vector<cycle_related_reference> Related;
		private_thread_lineage               Lineage;
		std::uint64_t                        NextTurn = 1;
		/// Suggestions are numbered per thread (`review_cycle_suggestion.hpp`).
		std::uint64_t NextSuggestion                                  = 1;
		auto          operator<=>(const cycle_feedback_thread&) const = default;
	};
	/// \brief A revision's identity within its thread.
	struct cycle_feedback_revision_ref
	{
		cycle_feedback_item_ref  Item;
		private_lineage_position Position;
		private_body_digest      Digest;
		auto                     operator<=>(const cycle_feedback_revision_ref&) const = default;
	};
	/// \brief One immutable authored revision; an edit is a successor.
	struct cycle_feedback_revision
	{
		cycle_feedback_revision_ref                Identity;
		cycle_feedback_author                      Author;
		std::string                                Body;
		private_revision_completion                Completion = private_revision_completion::complete;
		std::optional<cycle_feedback_revision_ref> Predecessor;
		std::optional<cycle_feedback_revision_ref> ReplyTo;
		std::optional<private_derivation_kind>     EditDerivation;
		auto                                       operator<=>(const cycle_feedback_revision&) const = default;
	};
	[[nodiscard]] auto valid(const cycle_feedback_thread& aThread) -> bool;
	[[nodiscard]] auto valid(const cycle_feedback_revision& aRevision) -> bool;

	/// \brief Starts a note. The reviewer writes it; no title, label,
	/// severity, recipient or model is asked for.
	/// \details `Provenance` is `human` for a note the reviewer types, or
	/// `unknown` for text taken from a round someone else sent (BPR-1162),
	/// whose author this profile never observed.
	struct begin_cycle_feedback
	{
		cycle_feedback_target                Primary;
		std::vector<cycle_related_reference> Related;
		std::string                          Body;
		authored_provenance                  Provenance = authored_provenance::human;
	};
	struct begun_cycle_feedback
	{
		review_cycle_state      Cycle;
		cycle_feedback_thread   Thread;
		cycle_feedback_revision Revision;
	};
	/// \brief Allocates a feedback item and its first revision.
	[[nodiscard]] auto begin_feedback(const review_cycle_state& aCycle, std::uint64_t aExpectedRevision, const begin_cycle_feedback& aCommand)
		-> std::expected<begun_cycle_feedback, cycle_refusal>;

	/// \brief A new turn: a reply, a pasted answer or an adapter's output.
	struct append_feedback_turn
	{
		cycle_feedback_author                          Author;
		std::string                                    Body;
		private_revision_completion                    Completion = private_revision_completion::complete;
		std::optional<private_review_turn_revision_id> ReplyTo;
	};
	/// \brief The reviewer's successor to the latest revision of one turn.
	/// Adopting text a run left incomplete must be explicit.
	struct edit_feedback_turn
	{
		private_review_turn_revision_id Latest;
		std::string                     Body;
		bool                            AdoptIncomplete = false;
	};
	/// \brief Adds a related location; earlier ones never change.
	struct add_feedback_reference
	{
		cycle_related_reference Reference;
	};
	struct set_feedback_lifecycle
	{
		private_thread_lifecycle Lifecycle = private_thread_lifecycle::open;
	};
	using cycle_feedback_change = std::variant<append_feedback_turn, edit_feedback_turn, add_feedback_reference, set_feedback_lifecycle>;

	struct cycle_feedback_outcome
	{
		cycle_feedback_thread                  Thread;
		std::optional<cycle_feedback_revision> Revision;
		bool                                   Changed = false;
	};
	/// \brief Applies one change against the expected thread version.
	/// \param aNamed The stored revision the change names: the reply target of
	/// an appended turn, or the latest revision of the edited turn. The caller
	/// loads it; a missing or mismatched one is refused.
	[[nodiscard]] auto apply_feedback(const review_cycle_state& aCycle, const cycle_feedback_thread& aThread, std::uint64_t aExpectedVersion,
									  const cycle_feedback_change& aChange, const cycle_feedback_revision* aNamed) -> std::expected<cycle_feedback_outcome, cycle_refusal>;

	/// \brief Where an original attachment stands in other material.
	enum class cycle_correspondence : std::uint8_t {
		/// The file's bytes are identical; the range is unchanged.
		exact_material,
		/// The file changed, and the original bytes (with their context when
		/// repeated) occur at exactly one place, or a file note's path holds
		/// new bytes.
		exact_mapped,
		/// The original bytes occur at more than one place and context does
		/// not single one out. Nothing is chosen.
		ambiguous,
		/// The file is deleted or absent in a complete capture.
		deleted,
		/// The path is unknown, outside the scope, unreadable or not text.
		unavailable,
		/// The file is present and the original bytes occur nowhere.
		unmapped,
	};
	[[nodiscard]] auto stable_name(cycle_correspondence aValue) noexcept -> std::string_view;

	/// \brief What one revision holds at a target's path.
	struct cycle_current_source
	{
		cycle_source_revision_id             Revision;
		cycle_path_state                     State = cycle_path_state::unknown;
		std::optional<cycle_material_digest> Blob;
		/// The retained bytes, when they were read and are text.
		std::optional<std::span<const std::uint8_t>> Bytes;
	};
	/// \brief A derived observation of one target against one revision.
	struct cycle_target_correspondence
	{
		cycle_correspondence     Kind = cycle_correspondence::unavailable;
		cycle_source_revision_id Against;
		/// The corresponding range, for an exact correspondence of an anchor.
		std::optional<std::pair<std::uint64_t, std::uint64_t>> Range;
		/// Places the original bytes occur, bounded; at least two when ambiguous.
		std::uint32_t Candidates                                            = 0;
		auto          operator<=>(const cycle_target_correspondence&) const = default;
	};
	/// Candidate occurrences are counted up to this bound.
	inline constexpr std::uint32_t maximum_cycle_correspondence_candidates = 64U;
	/// \brief Observes a file or anchor target against `aCurrent`. Nothing
	/// for whole-cycle and module targets, which name no bytes.
	[[nodiscard]] auto observe_correspondence(const cycle_feedback_target& aTarget, const cycle_current_source& aCurrent)
		-> std::optional<cycle_target_correspondence>;
}

#endif
