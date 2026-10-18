#include "bpr/core/note_placement.hpp"

namespace bpr::core {
	namespace {
		[[nodiscard]] auto side_blob(const file_alignment& aShown, const diff_side aSide) noexcept -> const std::optional<git_blob_oid>&
		{
			return aSide == diff_side::left ? aShown.Key.LeftBlob : aShown.Key.RightBlob;
		}

		[[nodiscard]] auto side_lines(const file_alignment& aShown, const diff_side aSide) noexcept -> std::uint32_t
		{
			return aSide == diff_side::left ? aShown.LeftLineCount : aShown.RightLineCount;
		}

		[[nodiscard]] auto outdated(const note_outdated_reason aReason) noexcept -> note_placement
		{
			return { .Kind = note_placement_kind::outdated, .Reason = aReason, .At = std::nullopt };
		}
	} // namespace

	auto name(const note_placement_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case note_placement_kind::pull_request: return "pull_request";
		case note_placement_kind::other_file:   return "other_file";
		case note_placement_kind::whole_file:   return "whole_file";
		case note_placement_kind::placed:       return "placed";
		case note_placement_kind::outdated:     return "outdated";
		}
		return "unknown";
	}

	auto name(const note_outdated_reason aReason) noexcept -> std::string_view
	{
		switch (aReason) {
		case note_outdated_reason::none:            return "none";
		case note_outdated_reason::content_changed: return "content_changed";
		case note_outdated_reason::side_absent:     return "side_absent";
		case note_outdated_reason::unmatched:       return "unmatched";
		case note_outdated_reason::line_absent:     return "line_absent";
		}
		return "unknown";
	}

	auto place_note(const note_anchor& aNote, const file_alignment* const aShown) noexcept -> note_placement
	{
		if (!aNote.File.has_value()) {
			return { .Kind = note_placement_kind::pull_request, .Reason = note_outdated_reason::none, .At = std::nullopt };
		}
		if (aShown == nullptr || aShown->Key.File != *aNote.File) {
			return { .Kind = note_placement_kind::other_file, .Reason = note_outdated_reason::none, .At = std::nullopt };
		}
		if (!aNote.Line.has_value()) {
			return { .Kind = note_placement_kind::whole_file, .Reason = note_outdated_reason::none, .At = std::nullopt };
		}
		const auto& line = *aNote.Line;
		const auto& blob = side_blob(*aShown, line.Side);
		// The comparison it was written on shows its line; another shows it
		// only when that side's text is the same blob.
		const auto same = aNote.Comparison.has_value() && *aNote.Comparison == aShown->Digest;
		if (!same) {
			if (!blob.has_value()) {
				return outdated(note_outdated_reason::side_absent);
			}
			if (!line.Blob.has_value()) {
				return outdated(note_outdated_reason::unmatched);
			}
			if (*line.Blob != *blob) {
				return outdated(note_outdated_reason::content_changed);
			}
		}
		if (line.Line >= side_lines(*aShown, line.Side)) {
			return outdated(blob.has_value() ? note_outdated_reason::line_absent : note_outdated_reason::side_absent);
		}
		return { .Kind = note_placement_kind::placed, .Reason = note_outdated_reason::none, .At = source_line_ref{ .Comparison = aShown->Digest, .Side = line.Side, .Line = line.Line } };
	}

	auto note_line_anchor_at(const file_alignment& aShown, const diff_side aSide, const std::uint32_t aLine) noexcept -> std::optional<note_line_anchor>
	{
		if (aLine >= side_lines(aShown, aSide)) {
			return std::nullopt;
		}
		return note_line_anchor{ .Side = aSide, .Line = aLine, .Blob = side_blob(aShown, aSide) };
	}
} // namespace bpr::core
