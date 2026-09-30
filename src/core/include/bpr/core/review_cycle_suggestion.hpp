#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_SUGGESTION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_SUGGESTION_HPP

#include "bpr/core/review_cycle_feedback.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Exact suggested edits in a local Review Cycle (RFC-BPR-0055,
/// BPR-1155).
/// \details A suggestion is local review data attached to a note: the exact
/// original bytes of a half-open range of one captured file, the exact
/// replacement bytes, and whether it is meant to be applied as written or only
/// illustrates an idea. Empty original bytes mark an insertion and an empty
/// replacement a deletion. Nothing here reads or writes a workspace, compiles
/// or runs anything. Previews and applications are pure byte functions: an
/// exact application requires the whole original file to match and checks the
/// full set of edits before any is applied; there is no nearest-text patching.
/// Exactness is a byte property, not a claim that an edit is correct.
namespace bpr::core {
	using cycle_suggestion_id = strong_id<struct cycle_suggestion_id_tag>;

	/// Each side of a suggestion is small, focused text.
	inline constexpr std::size_t maximum_cycle_suggestion_bytes = 64U * 1024U;
	inline constexpr std::size_t maximum_cycle_suggestions      = 64U;

	enum class cycle_suggestion_intent : std::uint8_t {
		/// Meant to be applied exactly as written.
		exact_replacement,
		/// Shows an idea; never offered as an automatic patch.
		illustrative,
	};
	enum class cycle_suggestion_state : std::uint8_t {
		proposed,
		/// The recipient's own edit already produced exactly this result; it
		/// is labelled, never dispatched for a second application.
		observed_already_applied,
	};
	[[nodiscard]] auto stable_name(cycle_suggestion_intent aValue) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(cycle_suggestion_state aValue) noexcept -> std::string_view;

	struct cycle_suggestion_ref
	{
		cycle_feedback_item_ref Item;
		cycle_suggestion_id     Id;
		auto                    operator<=>(const cycle_suggestion_ref&) const = default;
	};

	/// \brief One immutable suggestion revision.
	/// \details `Start` and `End` are byte offsets in the exact file named by
	/// `Source`, `Path` and `OriginalBlob`; `Original` holds bytes
	/// [Start, End). Line endings, tabs and every other byte are kept as
	/// given. `AppliedIn` names the revision whose file showed the result.
	struct cycle_suggestion
	{
		cycle_suggestion_ref                    Id;
		std::uint64_t                           Revision = 1;
		cycle_source_revision_id                Source;
		std::string                             Path;
		cycle_material_digest                   OriginalBlob;
		std::uint64_t                           Start = 0;
		std::uint64_t                           End   = 0;
		std::string                             Original;
		std::string                             Replacement;
		cycle_suggestion_intent                 Intent = cycle_suggestion_intent::exact_replacement;
		cycle_suggestion_state                  State  = cycle_suggestion_state::proposed;
		std::optional<cycle_source_revision_id> AppliedIn;
		auto                                    operator<=>(const cycle_suggestion&) const = default;

		[[nodiscard]] auto insertion() const noexcept -> bool
		{
			return Start == End;
		}
		[[nodiscard]] auto deletion() const noexcept -> bool
		{
			return Replacement.empty() && Start != End;
		}
	};
	[[nodiscard]] auto valid(const cycle_suggestion& aSuggestion) -> bool;

	/// \brief A suggestion as first written from a selection.
	struct suggest_change
	{
		cycle_source_anchor     Selection;
		std::string             Replacement;
		cycle_suggestion_intent Intent = cycle_suggestion_intent::exact_replacement;
	};
	/// \brief Makes revision 1 of a suggestion from a verified selection. The
	/// selection's range and bytes become the suggestion's original.
	[[nodiscard]] auto make_suggestion(const cycle_suggestion_ref& aId, const suggest_change& aChange) -> std::expected<cycle_suggestion, cycle_refusal>;
	struct added_cycle_suggestion
	{
		cycle_feedback_thread Thread;
		cycle_suggestion      Suggestion;
	};
	/// \brief Adds a suggestion to a note, allocating its identity from the
	/// thread. The selection must be a valid anchor of `aCycle`; `aExisting`
	/// is how many suggestions the thread already holds.
	[[nodiscard]] auto add_suggestion(const review_cycle_state& aCycle, const cycle_feedback_thread& aThread, std::uint64_t aExpectedVersion, const suggest_change& aChange,
									  std::size_t aExisting) -> std::expected<added_cycle_suggestion, cycle_refusal>;
	/// \brief True when the suggestion names exactly these file bytes.
	[[nodiscard]] auto verify_suggestion(const cycle_suggestion& aSuggestion, std::span<const std::uint8_t> aFile) -> bool;

	/// \brief A successor with new replacement bytes or intent.
	struct revise_suggestion
	{
		std::string             Replacement;
		cycle_suggestion_intent Intent = cycle_suggestion_intent::exact_replacement;
	};
	/// \brief A successor bound to a newer file where the same original bytes
	/// were found. The candidate must hold exactly the original bytes; the
	/// replacement and intent carry over.
	struct revalidate_suggestion
	{
		cycle_source_anchor Candidate;
	};
	/// \brief Records that `Result` holds exactly the suggested result.
	struct observe_suggestion_applied
	{
		cycle_source_revision_id Result;
	};
	using cycle_suggestion_change = std::variant<revise_suggestion, revalidate_suggestion, observe_suggestion_applied>;
	/// \brief Applies one change against the expected revision. A suggestion
	/// observed as already applied accepts no further change.
	[[nodiscard]] auto change_suggestion(const cycle_suggestion& aSuggestion, std::uint64_t aExpectedRevision, const cycle_suggestion_change& aChange)
		-> std::expected<cycle_suggestion, cycle_refusal>;

	/// \brief Before and after bytes of one suggestion, in memory only.
	struct cycle_suggestion_preview
	{
		/// The whole file with the edit applied.
		std::vector<std::uint8_t> After;
		cycle_material_digest     AfterBlob;
		/// Display lines of the original range and of the replacement.
		cycle_line_span Before;
		cycle_line_span Replaced;
	};
	/// \brief Previews one suggestion against its original file. Nothing when
	/// the file is not the suggestion's.
	[[nodiscard]] auto preview_suggestion(const cycle_suggestion& aSuggestion, std::span<const std::uint8_t> aFile) -> std::optional<cycle_suggestion_preview>;

	enum class cycle_suggestion_issue : std::uint8_t {
		/// The file is not the exact blob the suggestion names.
		stale_blob,
		/// The range does not hold the suggestion's original bytes.
		stale_range,
		/// Illustrative text is never applied.
		illustrative,
		/// Already present in the recipient's own edit; never applied again.
		already_applied,
		/// Overlapping or contained ranges, two insertions at one boundary, or
		/// an insertion on a replaced range's boundary.
		conflict,
		/// A suggestion for another file.
		other_file,
		/// Malformed or over the set bound.
		invalid,
	};
	[[nodiscard]] auto stable_name(cycle_suggestion_issue aValue) noexcept -> std::string_view;

	struct cycle_suggestion_finding
	{
		std::size_t            Index = 0;
		cycle_suggestion_issue Issue = cycle_suggestion_issue::invalid;
		/// The other suggestion in a conflict.
		std::optional<std::size_t> With;
		auto                       operator<=>(const cycle_suggestion_finding&) const = default;
	};
	/// \brief The whole-set decision: every finding, and the result only when
	/// there is none.
	struct cycle_suggestion_preflight
	{
		std::vector<cycle_suggestion_finding> Findings;
		/// Present only when every suggestion applies: the whole new file.
		std::optional<std::vector<std::uint8_t>> Result;
		std::optional<cycle_material_digest>     ResultBlob;

		[[nodiscard]] auto applicable() const noexcept -> bool
		{
			return Findings.empty() && Result.has_value();
		}
	};
	/// \brief Checks a set of suggestions to one file against that file's
	/// exact bytes and, when all pass, applies them in memory in descending
	/// offset order. Coordinates are all in the original file. No edit is
	/// applied when any finding exists, and conflicting edits are never
	/// ordered by position or by the last writer.
	[[nodiscard]] auto preflight_suggestions(std::span<const cycle_suggestion> aSuggestions, std::span<const std::uint8_t> aFile) -> cycle_suggestion_preflight;

	/// \brief What a later file says about a suggestion.
	enum class cycle_application_observation : std::uint8_t {
		/// The later file is exactly the original: nothing was applied yet.
		not_applied,
		/// The later file is exactly the suggested result.
		already_applied,
		/// Anything else. Similar text proves nothing.
		unknown,
	};
	/// \brief Compares a later file digest with the original file and with the
	/// result of applying this one suggestion to it.
	[[nodiscard]] auto observe_application(const cycle_suggestion& aSuggestion, std::span<const std::uint8_t> aOriginalFile, const cycle_material_digest& aLater)
		-> cycle_application_observation;

	/// \brief A self-describing text form of one suggestion revision: both
	/// sides as base64 with their byte lengths and SHA-256, so arbitrary bytes
	/// survive any transport that keeps ASCII lines.
	[[nodiscard]] auto encode_suggestion_wire(const cycle_suggestion& aSuggestion) -> std::string;
	/// \brief Reads `encode_suggestion_wire` output back, refusing any length,
	/// hash, field or encoding that does not match exactly.
	[[nodiscard]] auto decode_suggestion_wire(std::string_view aText) -> std::optional<cycle_suggestion>;
}

#endif
