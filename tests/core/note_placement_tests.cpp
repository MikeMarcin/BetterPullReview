// [[BPR-1591]]. RFC-BPR-0085 AC05's data half: a private note written on a
// line keeps its line in the comparison it was written on and in any other
// whose side shows the same blob, and is outdated, with its reason, otherwise.
// Nothing is moved to a nearby line.
#include "bpr/core/note_placement.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto identity(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto blob(const std::uint8_t aSeed) -> core::git_blob_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	/// One comparison of `src/engine/renderer.cpp`: its digest, each side's
	/// blob (absent for an added or deleted side) and line count.
	[[nodiscard]] auto shown(const std::uint8_t aDigest, const std::optional<core::git_blob_oid>& aLeft, const std::uint32_t aLeftLines, const std::optional<core::git_blob_oid>& aRight,
							 const std::uint32_t aRightLines, const char* aPath = "src/engine/renderer.cpp") -> core::file_alignment
	{
		auto alignment           = core::file_alignment{ };
		alignment.Key.File       = core::file_identity_of(aPath);
		alignment.Key.LeftBlob   = aLeft;
		alignment.Key.RightBlob  = aRight;
		alignment.Digest         = identity<core::file_comparison_digest>(aDigest);
		alignment.LeftLineCount  = aLeftLines;
		alignment.RightLineCount = aRightLines;
		return alignment;
	}

	[[nodiscard]] auto written_on(const core::file_alignment& aShown, const core::diff_side aSide, const std::uint32_t aLine) -> core::note_anchor
	{
		auto note       = core::note_anchor{ };
		note.File       = aShown.Key.File;
		note.Comparison = aShown.Digest;
		note.Line       = core::note_line_anchor_at(aShown, aSide, aLine);
		expect(note.Line.has_value(), "the note's line is in the comparison it was written on");
		return note;
	}

	void a_note_hangs_at_its_line_where_it_was_written()
	{
		const auto first  = shown(1U, blob(10U), 120U, blob(20U), 130U);
		const auto note   = written_on(first, core::diff_side::right, 41U);
		const auto placed = core::place_note(note, &first);
		expect(note.Line->Blob == blob(20U), "the anchor records the blob its side showed");
		expect(placed.Kind == core::note_placement_kind::placed && placed.Reason == core::note_outdated_reason::none && placed.At.has_value()
				   && placed.At->Comparison == first.Digest && placed.At->Side == core::diff_side::right && placed.At->Line == 41U,
			   "a note shows at its own line in the comparison it was written on");
		// Restart: the same stored anchor against the same comparison, read again.
		const auto again = core::place_note(core::note_anchor{ note }, &first);
		expect(again == placed, "after a restart the stored anchor places the note at the same line");
	}

	void a_new_head_keeps_the_line_only_when_its_side_is_the_same_text()
	{
		const auto written = shown(1U, blob(10U), 120U, blob(20U), 130U);
		const auto onRight = written_on(written, core::diff_side::right, 41U);
		const auto onLeft  = written_on(written, core::diff_side::left, 7U);
		// A new head that did not touch the file's head side: same blob.
		const auto untouched = shown(2U, blob(10U), 120U, blob(20U), 130U);
		const auto kept      = core::place_note(onRight, &untouched);
		expect(kept.Kind == core::note_placement_kind::placed && kept.At->Comparison == untouched.Digest && kept.At->Line == 41U,
			   "a new comparison whose side shows the same blob keeps the line, in the new comparison");
		// A new head that changed the file: the right side is another blob.
		const auto changed = shown(3U, blob(10U), 120U, blob(21U), 131U);
		const auto moved   = core::place_note(onRight, &changed);
		expect(moved.Kind == core::note_placement_kind::outdated && moved.Reason == core::note_outdated_reason::content_changed && !moved.At.has_value(),
			   "a changed side outdates the note, with its reason, and never moves it to a nearby line");
		// Negative control: the base side did not change, so a base note holds.
		const auto base = core::place_note(onLeft, &changed);
		expect(base.Kind == core::note_placement_kind::placed && base.At->Side == core::diff_side::left && base.At->Line == 7U,
			   "a note on the unchanged base side keeps its line across the same new head");
		// The file deleted at the new head: the side is gone.
		const auto deleted = shown(4U, blob(10U), 120U, std::nullopt, 0U);
		expect(core::place_note(onRight, &deleted).Reason == core::note_outdated_reason::side_absent, "a side that no longer has the file outdates the note as absent");
	}

	void anchors_without_a_line_and_other_files_are_not_judged_by_line()
	{
		const auto current = shown(1U, blob(10U), 120U, blob(20U), 130U);
		auto       whole   = core::note_anchor{ };
		expect(core::place_note(whole, &current).Kind == core::note_placement_kind::pull_request, "a note on no file is on the pull request");
		expect(core::place_note(whole, nullptr).Kind == core::note_placement_kind::pull_request, "with no comparison shown it is still on the pull request");
		whole.File       = current.Key.File;
		whole.Comparison = current.Digest;
		expect(core::place_note(whole, &current).Kind == core::note_placement_kind::whole_file, "a file note without a line is on the whole file");
		const auto other = shown(9U, blob(30U), 10U, blob(31U), 10U, "src/engine/renderer.h");
		const auto line  = written_on(current, core::diff_side::right, 3U);
		expect(core::place_note(line, &other).Kind == core::note_placement_kind::other_file && core::place_note(whole, &other).Kind == core::note_placement_kind::other_file,
			   "a note on another file waits as that file's, line or not");
		expect(core::place_note(line, nullptr).Kind == core::note_placement_kind::other_file, "with no comparison shown a file note is another file's");
	}

	void a_line_is_only_where_the_text_reaches()
	{
		const auto current = shown(1U, blob(10U), 120U, blob(20U), 130U);
		expect(!core::note_line_anchor_at(current, core::diff_side::right, 130U).has_value() && !core::note_line_anchor_at(current, core::diff_side::left, 120U).has_value(),
			   "no anchor past a side's last line");
		const auto added = shown(2U, std::nullopt, 0U, blob(20U), 130U);
		expect(!core::note_line_anchor_at(added, core::diff_side::left, 0U).has_value(), "no anchor on the empty side of an added file");
		auto past              = written_on(current, core::diff_side::right, 129U);
		past.Line->Line        = 400U;
		const auto beyond      = core::place_note(past, &current);
		auto       unmatched   = written_on(current, core::diff_side::right, 12U);
		unmatched.Line->Blob   = std::nullopt;
		const auto another     = shown(5U, blob(10U), 120U, blob(20U), 130U);
		const auto withoutBlob = core::place_note(unmatched, &another);
		const auto stillThere  = core::place_note(unmatched, &current);
		expect(beyond.Kind == core::note_placement_kind::outdated && beyond.Reason == core::note_outdated_reason::line_absent, "a line past the text is outdated as absent");
		expect(withoutBlob.Reason == core::note_outdated_reason::unmatched && stillThere.Kind == core::note_placement_kind::placed,
			   "a line kept without its blob holds only in its own comparison");
		expect(core::name(core::note_placement_kind::outdated) == "outdated" && core::name(core::note_outdated_reason::content_changed) == "content_changed", "stable names");
	}
} // namespace

int main()
{
	a_note_hangs_at_its_line_where_it_was_written();
	a_new_head_keeps_the_line_only_when_its_side_is_the_same_text();
	anchors_without_a_line_and_other_files_are_not_judged_by_line();
	a_line_is_only_where_the_text_reaches();
	std::cout << "NOTE_PLACEMENT_OK\n";
	return EXIT_SUCCESS;
}
