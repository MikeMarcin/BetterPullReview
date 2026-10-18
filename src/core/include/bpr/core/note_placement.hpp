#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_NOTE_PLACEMENT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_NOTE_PLACEMENT_HPP
#include "bpr/core/diff_model.hpp"
#include "bpr/core/epoch_model.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-1591]]. The line a private note was written on.
	///
	/// \details RFC-BPR-0085 D3. A note on a selected line records its side,
	/// its zero-based line, and the blob that side showed when the note was
	/// written. The comparison it was written against is the note's own
	/// attachment; the blob is what lets the line outlive that comparison
	/// honestly: a later comparison whose side shows the same blob shows the
	/// same line, and any other is not guessed at.
	struct note_line_anchor
	{
		diff_side                   Side = diff_side::right;
		std::uint32_t               Line = 0U;
		std::optional<git_blob_oid> Blob;

		auto operator<=>(const note_line_anchor&) const = default;
	};

	/// \brief [[BPR-1591]]. What a note is anchored to.
	///
	/// \details No file is the pull request; a file without a line is the
	/// whole file, as notes were before; a line is RFC-BPR-0085's margin note.
	struct note_anchor
	{
		std::optional<file_identity>          File;
		std::optional<file_comparison_digest> Comparison;
		std::optional<note_line_anchor>       Line;

		auto operator<=>(const note_anchor&) const = default;
	};

	/// \brief [[BPR-1591]]. Where a note shows against the comparison shown.
	enum class note_placement_kind : std::uint8_t {
		/// On the pull request as a whole.
		pull_request,
		/// On another file than the one shown, line or not: not judged here.
		other_file,
		/// On the shown file as a whole.
		whole_file,
		/// At its line in the shown comparison.
		placed,
		/// On the shown file, but its line is not in the shown comparison.
		outdated,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(note_placement_kind aKind) noexcept -> std::string_view;

	/// \brief [[BPR-1591]]. Why a line note is outdated.
	enum class note_outdated_reason : std::uint8_t {
		none,
		/// The side it was written on shows different content now.
		content_changed,
		/// The side it was written on has no content now: the file was added
		/// or deleted on that side.
		side_absent,
		/// The note kept no blob and the comparison is another one, so its
		/// line cannot be matched.
		unmatched,
		/// The line is past the end of the side's text, which shows no lines
		/// it could be on (binary content, or a line number the text does not
		/// reach).
		line_absent,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(note_outdated_reason aReason) noexcept -> std::string_view;

	/// \brief [[BPR-1591]]. Where one note shows, and why it is outdated.
	struct note_placement
	{
		note_placement_kind  Kind   = note_placement_kind::pull_request;
		note_outdated_reason Reason = note_outdated_reason::none;
		/// The side and line it shows at, only when placed.
		std::optional<source_line_ref> At;

		auto operator<=>(const note_placement&) const = default;
	};

	/// \brief [[BPR-1591]]. Decides where a note shows against the comparison
	/// shown, or none.
	///
	/// \details The note is never moved to a nearby line: its line holds in
	/// the comparison it was written on, and in any other whose side shows
	/// the same blob, because that is the same text; otherwise it is
	/// outdated, with its reason, as an outdated thread is.
	[[nodiscard]] auto place_note(const note_anchor& aNote, const file_alignment* aShown) noexcept -> note_placement;

	/// \brief [[BPR-1591]]. The anchor a note written on one line of the shown
	/// comparison records, or none when that line is not in it.
	[[nodiscard]] auto note_line_anchor_at(const file_alignment& aShown, diff_side aSide, std::uint32_t aLine) noexcept -> std::optional<note_line_anchor>;
} // namespace bpr::core
#endif
