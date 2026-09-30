#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_ROUND_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_ROUND_HPP

#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_feedback.hpp"
#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/review_cycle_suggestion.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Frozen feedback rounds (RFC-BPR-0056, BPR-1158): what a reviewer
/// selects for one assignment, and the one canonical payload that freezes it.
/// \details A round draft names exact revisions: each note's wording, each
/// suggestion's revision, each piece of source context and the revision under
/// review, in the reviewer's order. Assembly checks every name against the
/// cycle's current records and refuses a stale, archived, unknown or
/// oversized selection rather than substituting or dropping anything. The
/// payload holds only what was selected: a note's current wording, its
/// primary and included related locations, the selected suggestions' exact
/// bytes and the selected source bytes. Replies, earlier wordings, private
/// references, authorship details, workspace locations and other notes never
/// enter it. Its digest is the SHA-256 of its canonical JSON
/// (`review_cycle_json.hpp`); the wire shape is
/// `docs/rfcs/assets/bpr-review-cycles-v1/schemas/round.schema.json`.
namespace bpr::core {
	inline constexpr std::size_t   maximum_round_items             = 4096U;
	inline constexpr std::uint64_t maximum_round_instruction_bytes = 16U * 1024U * 1024U;
	inline constexpr std::uint64_t default_round_context_bytes     = 8U * 1024U * 1024U;
	/// Reached only when the reviewer explicitly admits large context.
	inline constexpr std::uint64_t maximum_round_context_bytes    = 64U * 1024U * 1024U;
	inline constexpr std::size_t   maximum_round_body_bytes       = 64U * 1024U;
	inline constexpr std::size_t   maximum_round_text_bytes       = 64U * 1024U;
	inline constexpr std::size_t   maximum_round_constraints      = 256U;
	inline constexpr std::size_t   maximum_round_write_scope      = 1024U;
	inline constexpr std::size_t   maximum_round_omissions        = 4096U;
	inline constexpr std::size_t   maximum_round_sources          = 10'000U;
	inline constexpr std::size_t   maximum_round_anchors          = 256U;
	inline constexpr std::size_t   maximum_round_related_items    = 256U;
	inline constexpr std::size_t   maximum_round_suggestions      = 1024U;
	inline constexpr std::size_t   maximum_round_suggestion_bytes = 256U * 1024U;
	/// A stored draft or an imported round document is read within this bound.
	inline constexpr std::size_t maximum_round_document_bytes = 256U * 1024U * 1024U;

	/// \brief What the recipient is asked to do with the round, or one item.
	enum class cycle_round_action : std::uint8_t {
		discuss,
		plan,
		implement,
	};
	[[nodiscard]] auto stable_name(cycle_round_action aValue) noexcept -> std::string_view;

	struct cycle_round_suggestion_pin
	{
		cycle_suggestion_id Id;
		std::uint64_t       Revision                                             = 0;
		auto                operator<=>(const cycle_round_suggestion_pin&) const = default;
	};
	/// \brief One selected note, as the reviewer chose it.
	struct cycle_round_item_selection
	{
		cycle_feedback_item_id Item;
		/// The exact wording selected: the latest revision of the note's
		/// first turn when it was chosen.
		private_review_turn_revision_id Wording;
		/// An explicit exception to the round's action.
		std::optional<cycle_round_action>       Action;
		std::vector<cycle_round_suggestion_pin> Suggestions;
		/// Other selected notes this one should be read with.
		std::vector<cycle_feedback_item_id> Related;
		auto                                operator<=>(const cycle_round_item_selection&) const = default;
	};
	enum class cycle_context_kind : std::uint8_t {
		full_file,
		excerpt,
	};
	[[nodiscard]] auto stable_name(cycle_context_kind aValue) noexcept -> std::string_view;
	/// \brief Source explicitly included for the recipient to read.
	struct cycle_context_selection
	{
		cycle_source_revision_id Revision;
		std::string              Path;
		cycle_context_kind       Kind = cycle_context_kind::full_file;
		/// Half-open byte range of an excerpt; ignored for a full file.
		std::uint64_t Start                                             = 0;
		std::uint64_t End                                               = 0;
		auto          operator<=>(const cycle_context_selection&) const = default;
	};
	/// \brief A round draft's content: one assignment over ordered notes.
	struct cycle_round_selection
	{
		/// The revision under review.
		cycle_source_revision_id Source;
		cycle_round_action       Action = cycle_round_action::implement;
		std::string              Goal;
		std::vector<std::string> Constraints;
		/// Paths the recipient is asked to limit edits to; not an access grant.
		std::vector<std::string>                WriteScope;
		std::vector<cycle_round_item_selection> Items;
		std::vector<cycle_context_selection>    Context;
		/// The reviewer's own notes on what was deliberately left out.
		std::vector<std::string> Omissions;
		/// Admits up to `maximum_round_context_bytes` of context.
		bool LargeContext                                    = false;
		auto operator<=>(const cycle_round_selection&) const = default;
	};
	/// \brief The draft body stored for a selection
	/// (`bpr.cycle.round-draft.v1`, canonical JSON). Nothing when a text is not
	/// UTF-8 or an identity exceeds the exact JSON integer range.
	[[nodiscard]] auto encode_round_selection(const cycle_round_selection& aSelection) -> std::optional<std::string>;
	/// \brief Reads a stored draft body. An empty body is an empty selection.
	[[nodiscard]] auto decode_round_selection(std::string_view aBody) -> std::optional<cycle_round_selection>;

	enum class cycle_anchor_kind : std::uint8_t {
		range,
		file,
		module,
		cycle,
	};
	[[nodiscard]] auto stable_name(cycle_anchor_kind aValue) noexcept -> std::string_view;
	enum class cycle_source_consistency : std::uint8_t {
		observed_stable,
		immutable_source,
		/// The revision under review covered only part of its scope.
		incomplete,
	};
	[[nodiscard]] auto stable_name(cycle_source_consistency aValue) noexcept -> std::string_view;

	/// \brief A location as it travels: identities are wire strings, so an
	/// imported round keeps its sender's names.
	struct cycle_wire_anchor
	{
		cycle_anchor_kind                    Kind = cycle_anchor_kind::cycle;
		std::optional<std::string>           Path;
		std::string                          Revision;
		std::optional<cycle_material_digest> Blob;
		std::optional<std::uint64_t>         Start;
		std::optional<std::uint64_t>         End;
		auto                                 operator<=>(const cycle_wire_anchor&) const = default;
	};
	struct cycle_wire_suggestion
	{
		std::string                Id;
		std::uint64_t              Revision = 1;
		cycle_suggestion_intent    Intent   = cycle_suggestion_intent::exact_replacement;
		cycle_suggestion_state     State    = cycle_suggestion_state::proposed;
		cycle_wire_anchor          Anchor;
		std::string                Original;
		std::string                Replacement;
		std::optional<std::string> AppliedIn;
		auto                       operator<=>(const cycle_wire_suggestion&) const = default;
	};
	struct cycle_wire_item
	{
		std::string                        Id;
		std::uint64_t                      Revision = 1;
		std::string                        Body;
		std::optional<cycle_round_action>  ActionOverride;
		std::vector<cycle_wire_anchor>     Anchors;
		std::vector<std::string>           Related;
		std::vector<cycle_wire_suggestion> Suggestions;
		auto                               operator<=>(const cycle_wire_item&) const = default;
	};
	struct cycle_wire_source
	{
		std::string           Id;
		std::string           Path;
		std::string           Revision;
		cycle_context_kind    Kind  = cycle_context_kind::full_file;
		std::uint64_t         Start = 0;
		std::uint64_t         End   = 0;
		cycle_material_digest Blob;
		std::string           Bytes;
		auto                  operator<=>(const cycle_wire_source&) const = default;
	};
	struct cycle_round_project
	{
		std::string              Id;
		std::string              Label;
		std::vector<std::string> ScopePaths;
		auto                     operator<=>(const cycle_round_project&) const = default;
	};
	struct cycle_round_assignment
	{
		cycle_round_action       Action = cycle_round_action::implement;
		std::string              Goal;
		std::vector<std::string> Constraints;
		std::vector<std::string> WriteScope;
		auto                     operator<=>(const cycle_round_assignment&) const = default;
	};
	struct cycle_round_completeness
	{
		std::uint64_t            ItemCount   = 0;
		std::uint64_t            SourceCount = 0;
		std::vector<std::string> Omissions;
		bool                     Complete                                           = true;
		auto                     operator<=>(const cycle_round_completeness&) const = default;
	};
	/// \brief The immutable content of one round (`bpr.cycle.round.v1`).
	struct cycle_round_payload
	{
		std::string                    CycleId;
		std::string                    RoundId;
		std::string                    SourceRevision;
		cycle_source_consistency       Consistency = cycle_source_consistency::observed_stable;
		cycle_round_project            Project;
		cycle_round_assignment         Assignment;
		std::vector<cycle_wire_item>   Items;
		std::vector<cycle_wire_source> Sources;
		cycle_round_completeness       Selection;
		auto                           operator<=>(const cycle_round_payload&) const = default;
	};

	/// \brief Wire names of local identities: `RC-3`, `F2`, `R1`, `I7`,
	/// `SG7.2` (the second suggestion of note 7) and `P<64 hex>` for the
	/// cycle's logical project, which is derived from identities, never a path.
	[[nodiscard]] auto cycle_wire_name(review_cycle_id aCycle) -> std::string;
	[[nodiscard]] auto cycle_wire_name(cycle_round_id aRound) -> std::string;
	[[nodiscard]] auto cycle_wire_name(cycle_source_revision_id aRevision) -> std::string;
	[[nodiscard]] auto cycle_wire_name(cycle_feedback_item_id aItem) -> std::string;
	[[nodiscard]] auto cycle_wire_name(const cycle_suggestion_ref& aSuggestion) -> std::string;
	[[nodiscard]] auto cycle_wire_project(const local_cycle_subject& aSubject) -> std::string;
	/// \brief The wire identifier pattern `^[A-Za-z][A-Za-z0-9._:-]{0,127}$`.
	[[nodiscard]] auto valid_cycle_wire_identifier(std::string_view aText) noexcept -> bool;

	/// \brief Why a selection cannot be frozen as it stands.
	enum class cycle_round_problem : std::uint8_t {
		/// Malformed identities, paths, duplicates or counts.
		invalid_selection,
		/// A text is not UTF-8 or exceeds its bound.
		invalid_text,
		/// No note is selected.
		empty,
		unknown_item,
		archived_item,
		/// The note's wording changed after it was selected.
		stale_wording,
		unknown_suggestion,
		/// The suggestion has a later revision than the one selected.
		stale_suggestion,
		/// A related note that is not in the round, or the note itself.
		invalid_relation,
		/// The revision under review is not a captured revision of this cycle.
		invalid_source,
		/// Selected context names bytes its revision does not hold.
		unavailable_context,
		/// Over a round budget; `Measure` says by how much. Splitting the
		/// assignment is the reviewer's decision.
		oversized,
		/// An editor still holds text it has not saved (an IME composition
		/// or a pending save).
		unflushed,
		/// The editor shows an older draft revision than the stored one.
		stale_draft,
	};
	[[nodiscard]] auto stable_name(cycle_round_problem aValue) noexcept -> std::string_view;

	/// \brief What a selection holds against the round budgets.
	struct cycle_round_measure
	{
		std::uint64_t Items                                         = 0;
		std::uint64_t InstructionBytes                              = 0;
		std::uint64_t ContextBytes                                  = 0;
		std::uint64_t ContextBudget                                 = default_round_context_bytes;
		auto          operator<=>(const cycle_round_measure&) const = default;
	};
	struct cycle_round_refusal
	{
		cycle_round_problem Problem = cycle_round_problem::invalid_selection;
		/// Index into the selection's items or context, when one is at fault.
		std::optional<std::size_t> Item;
		std::optional<std::size_t> Context;
		cycle_round_measure        Measure;
		auto                       operator<=>(const cycle_round_refusal&) const = default;
	};

	/// \brief What the editor reports before a freeze.
	struct cycle_draft_editor_state
	{
		/// An input method composition is open.
		bool Composing = false;
		/// Text typed but not yet saved to the draft.
		bool SavePending = false;
		/// The draft revision the editor last saved or loaded.
		std::uint64_t ShownRevision = 0;
	};
	/// \brief A freeze needs every editor flushed and showing the stored
	/// draft revision. Nothing when the draft may be frozen.
	[[nodiscard]] auto freeze_readiness(const cycle_draft_editor_state& aEditor, std::uint64_t aStoredRevision) noexcept -> std::optional<cycle_round_problem>;

	/// \brief One selected note's records, as the store holds them now.
	struct cycle_round_item_material
	{
		cycle_feedback_thread Thread;
		/// The thread's complete revision history, in order.
		std::vector<cycle_feedback_revision> History;
		/// The latest revision of each of the thread's suggestions.
		std::vector<cycle_suggestion> Suggestions;
	};
	/// \brief One retained file a context selection names.
	struct cycle_round_file
	{
		cycle_source_revision_id Revision;
		std::string              Path;
		cycle_material_digest    Blob;
		std::string              Bytes;
	};
	struct cycle_round_inputs
	{
		const review_cycle_state* Cycle = nullptr;
		/// The identity the round will take.
		cycle_round_id Round;
		/// The capture of the selection's source revision, when it has one.
		std::optional<cycle_capture_observation>   SourceCapture;
		const cycle_round_selection*               Selection = nullptr;
		std::span<const cycle_round_item_material> Items;
		std::span<const cycle_round_file>          Files;
	};
	/// \brief Assembles the complete payload for a selection, in its order.
	/// \details Context naming the same file of the same revision is included
	/// once: as the whole file when any selection asks for it, otherwise as
	/// one excerpt spanning every selected range. Nothing is summarized,
	/// reworded, normalized, reordered or dropped.
	[[nodiscard]] auto assemble_round(const cycle_round_inputs& aInputs) -> std::expected<cycle_round_payload, cycle_round_refusal>;

	/// \brief The canonical payload bytes whose SHA-256 is the round digest.
	/// Nothing when the payload has no canonical form.
	[[nodiscard]] auto encode_round_payload(const cycle_round_payload& aPayload) -> std::optional<std::string>;
	[[nodiscard]] auto round_digest(std::string_view aCanonicalPayload) -> cycle_round_digest;
	/// \brief The envelope document (`round.json`): schema, canonicalization,
	/// digest and payload, as canonical JSON with a final newline.
	[[nodiscard]] auto encode_round_envelope(const cycle_round_payload& aPayload) -> std::optional<std::string>;

	/// \brief Why a round document was refused. Nothing is admitted in part.
	enum class cycle_round_decode_problem : std::uint8_t {
		/// Not strict JSON; see `Json`.
		json,
		/// An unsupported schema or canonicalization.
		version,
		/// A missing, extra or mistyped field, or a value outside its enum.
		shape,
		/// An identifier, revision or digest that breaks its pattern.
		identifier,
		/// A byte record whose base64, length or SHA-256 does not agree.
		bytes,
		/// The payload digest does not match its content.
		digest,
		/// Declared counts differ from the content, or the instruction set
		/// is not declared complete.
		completeness,
		/// A repeated item, suggestion or source identity.
		duplicate,
		/// A path that is absolute, traverses, or is not a portable relative path.
		path,
		/// A range that is inconsistent with its bytes or its file.
		range,
		/// Over a count or size bound.
		bounds,
		/// A related item that is not in the round.
		relation,
		/// A stored payload that is not in canonical form.
		noncanonical,
	};
	[[nodiscard]] auto stable_name(cycle_round_decode_problem aValue) noexcept -> std::string_view;
	struct cycle_round_decode_error
	{
		cycle_round_decode_problem      Problem = cycle_round_decode_problem::shape;
		std::optional<cycle_json_error> Json;
		auto                            operator<=>(const cycle_round_decode_error&) const = default;
	};
	struct cycle_decoded_round
	{
		cycle_round_payload Payload;
		cycle_round_digest  Digest;
		/// The canonical payload bytes the digest covers.
		std::string Canonical;
	};
	/// \brief Validates an envelope document as untrusted data: strict JSON,
	/// exact field sets, patterns, byte records, digest, counts, uniqueness,
	/// paths, ranges and bounds, before anything is returned.
	[[nodiscard]] auto decode_round_envelope(std::string_view aDocument) -> std::expected<cycle_decoded_round, cycle_round_decode_error>;
	/// \brief Reads a stored canonical payload, refusing anything that is not
	/// byte-for-byte canonical.
	[[nodiscard]] auto decode_round_payload(std::string_view aCanonical) -> std::expected<cycle_decoded_round, cycle_round_decode_error>;
}

#endif
