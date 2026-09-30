#include "bpr/core/review_cycle_suggestion.hpp"
#include "bpr/core/sha256.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle suggestion failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto bytes(const std::string_view aText) -> std::span<const std::uint8_t>
	{
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}

	auto text(const std::vector<std::uint8_t>& aBytes) -> std::string
	{
		return { aBytes.begin(), aBytes.end() };
	}

	auto digest(const std::string_view aText) -> cycle_material_digest
	{
		return cycle_material_digest{ sha256(bytes(aText)) };
	}

	/// CRLF lines, a tab and a multibyte scalar, all kept byte for byte.
	constexpr std::string_view file = "int f()\r\n{\r\n\treturn caf\xC3\xA9(1);\r\n}\r\n";

	auto selection(const std::string_view aFile, const std::uint64_t aStart, const std::uint64_t aEnd) -> cycle_source_anchor
	{
		auto made = make_anchor(cycle_source_revision_id{ 1 }, "src/f.cpp", bytes(aFile), aStart, aEnd);
		expect(made.has_value(), "selection");
		return *made;
	}

	auto suggest(const std::uint64_t aId, const std::uint64_t aStart, const std::uint64_t aEnd, std::string aReplacement,
				 const cycle_suggestion_intent aIntent = cycle_suggestion_intent::exact_replacement) -> cycle_suggestion
	{
		const auto ref  = cycle_suggestion_ref{ .Item = { .Cycle = review_cycle_id{ 1 }, .Id = cycle_feedback_item_id{ 1 } }, .Id = cycle_suggestion_id{ aId } };
		auto       made = make_suggestion(ref, { .Selection = selection(file, aStart, aEnd), .Replacement = std::move(aReplacement), .Intent = aIntent });
		expect(made.has_value(), "suggestion");
		return *made;
	}

	auto offset(const std::string_view aText) -> std::uint64_t
	{
		const auto found = file.find(aText);
		expect(found != std::string_view::npos, "offset");
		return found;
	}

	void keeps_exact_bytes_and_previews_in_memory()
	{
		const auto start = offset("caf\xC3\xA9(1)");
		const auto edit  = suggest(1, start, start + 8, "caf\xC3\xA9(2)\t// tab\r\n\t");
		expect(edit.Original == "caf\xC3\xA9(1)" && edit.Replacement == "caf\xC3\xA9(2)\t// tab\r\n\t" && !edit.insertion() && !edit.deletion(), "exact bytes");
		expect(edit.Source == cycle_source_revision_id{ 1 } && edit.OriginalBlob == digest(file) && edit.Start == 20 && edit.End == 28, "coordinates");
		expect(verify_suggestion(edit, bytes(file)), "verifies against its file");

		const auto preview = preview_suggestion(edit, bytes(file));
		expect(preview.has_value(), "preview");
		expect(text(preview->After) == "int f()\r\n{\r\n\treturn caf\xC3\xA9(2)\t// tab\r\n\t;\r\n}\r\n", "whole after bytes");
		expect(preview->AfterBlob == digest(text(preview->After)) && preview->Before == cycle_line_span{ 3, 3 } && preview->Replaced == cycle_line_span{ 3, 4 }, "before and after lines");
		expect(!preview_suggestion(edit, bytes("int f()\n")), "no preview against another file");

		// Insertion and deletion are the empty sides.
		const auto insertion = suggest(2, 9, 9, "// added\r\n");
		expect(insertion.insertion() && insertion.Original.empty(), "insertion");
		expect(text(preview_suggestion(insertion, bytes(file))->After).starts_with("int f()\r\n// added\r\n{"), "inserted");
		const auto deletion = suggest(3, 0, 9, "");
		expect(deletion.deletion() && text(preview_suggestion(deletion, bytes(file))->After).starts_with("{\r\n"), "deleted");

		// An exact edit must change something; bounds hold.
		const auto ref = cycle_suggestion_ref{ .Item = { .Cycle = review_cycle_id{ 1 }, .Id = cycle_feedback_item_id{ 1 } }, .Id = cycle_suggestion_id{ 9 } };
		expect(make_suggestion(ref, { .Selection = selection(file, 0, 3), .Replacement = "int" }).error() == cycle_refusal::invalid_target, "no-op exact edit");
		expect(make_suggestion(ref, { .Selection = selection(file, 0, 3), .Replacement = "int", .Intent = cycle_suggestion_intent::illustrative }).has_value(), "illustration may repeat");
		expect(make_suggestion(ref, { .Selection = selection(file, 0, 3), .Replacement = std::string(maximum_cycle_suggestion_bytes + 1U, 'x') }).error() == cycle_refusal::bounds, "replacement bound");
	}

	auto issues(const cycle_suggestion_preflight& aPreflight) -> std::vector<cycle_suggestion_issue>
	{
		auto found = std::vector<cycle_suggestion_issue>{ };
		for (const auto& finding : aPreflight.Findings) {
			found.push_back(finding.Issue);
		}
		return found;
	}

	void applies_nonoverlapping_edits_in_original_coordinates()
	{
		// Three edits of different lengths, given out of order: each names the
		// original file, so the later ones must not shift the earlier ones.
		const auto name   = suggest(1, offset("caf"), offset("caf") + 5, "coffee");
		const auto head   = suggest(2, 0, 7, "long function_name()");
		const auto insert = suggest(3, offset("}"), offset("}"), "\t// end\r\n");
		const auto all    = std::vector{ name, insert, head };
		const auto result = preflight_suggestions(all, bytes(file));
		expect(result.applicable(), "applicable set");
		expect(text(*result.Result) == "long function_name()\r\n{\r\n\treturn coffee(1);\r\n\t// end\r\n}\r\n", "original-coordinate application");
		expect(result.ResultBlob == digest(text(*result.Result)), "result digest");

		// Adjacent replacements do not overlap.
		const auto left  = suggest(4, 0, 3, "long");
		const auto right = suggest(5, 3, 5, "__");
		expect(preflight_suggestions(std::vector{ left, right }, bytes(file)).applicable(), "adjacent edits");
		expect(!preflight_suggestions(std::vector<cycle_suggestion>{ }, bytes(file)).applicable(), "an empty set applies nothing");
	}

	void refuses_conflicts_and_stale_material_whole()
	{
		const auto check = [](const std::vector<cycle_suggestion>& aSet, const std::string_view aFile) {
			const auto result = preflight_suggestions(aSet, bytes(aFile));
			expect(!result.applicable() && !result.Result && !result.ResultBlob, "nothing applied");
			return result;
		};
		const auto overlap = check({ suggest(1, 0, 5, "a"), suggest(2, 3, 9, "b") }, file);
		expect(overlap.Findings.size() == 1 && overlap.Findings[0].Issue == cycle_suggestion_issue::conflict && overlap.Findings[0].With == 1U, "overlap");
		expect(issues(check({ suggest(1, 0, 9, "a"), suggest(2, 2, 4, "b") }, file)) == std::vector{ cycle_suggestion_issue::conflict }, "contained");
		expect(issues(check({ suggest(1, 9, 9, "a"), suggest(2, 9, 9, "b") }, file)) == std::vector{ cycle_suggestion_issue::conflict }, "two insertions at one boundary");
		expect(issues(check({ suggest(1, 0, 9, "a"), suggest(2, 9, 9, "b") }, file)) == std::vector{ cycle_suggestion_issue::conflict }, "insertion at a replacement's end");
		expect(issues(check({ suggest(1, 9, 9, "a"), suggest(2, 9, 12, "b") }, file)) == std::vector{ cycle_suggestion_issue::conflict }, "insertion at a replacement's start");
		expect(issues(check({ suggest(1, 0, 3, "a"), suggest(2, 0, 3, "a") }, file)) == std::vector{ cycle_suggestion_issue::conflict }, "identical edits still conflict");

		// The range still reads the same, but the file is not the blob: stale.
		const auto edited = std::string{ file } + "// appended elsewhere\r\n";
		const auto stale  = check({ suggest(1, 0, 3, "long") }, edited);
		expect(issues(stale) == std::vector{ cycle_suggestion_issue::stale_blob }, "locally matching text is not enough");

		auto torn     = suggest(1, 0, 3, "long");
		torn.Original = "INT";
		expect(issues(check({ torn }, file)) == std::vector{ cycle_suggestion_issue::stale_range }, "range bytes differ");
		expect(issues(check({ suggest(1, 0, 3, "x", cycle_suggestion_intent::illustrative) }, file)) == std::vector{ cycle_suggestion_issue::illustrative }, "illustrative is never applied");
		auto applied      = suggest(1, 0, 3, "long");
		applied.State     = cycle_suggestion_state::observed_already_applied;
		applied.AppliedIn = cycle_source_revision_id{ 2 };
		expect(issues(check({ applied, suggest(2, 9, 9, "x") }, file)) == std::vector{ cycle_suggestion_issue::already_applied }, "already applied is not applied twice");
		auto other = suggest(2, 9, 9, "x");
		other.Path = "src/g.cpp";
		expect(issues(check({ suggest(1, 0, 3, "long"), other }, file)) == std::vector{ cycle_suggestion_issue::other_file }, "one file per set");
		expect(issues(check(std::vector<cycle_suggestion>(maximum_cycle_suggestions + 1U, suggest(1, 0, 3, "long")), file)) == std::vector{ cycle_suggestion_issue::invalid }, "set bound");
	}

	void observes_application_without_similarity()
	{
		const auto edit = suggest(1, 0, 3, "long");
		expect(observe_application(edit, bytes(file), digest(file)) == cycle_application_observation::not_applied, "unchanged file");
		const auto result = text(preview_suggestion(edit, bytes(file))->After);
		expect(observe_application(edit, bytes(file), digest(result)) == cycle_application_observation::already_applied, "exact result");
		// The same line with one more space is not the result, however similar.
		expect(observe_application(edit, bytes(file), digest(result + " ")) == cycle_application_observation::unknown, "similar is unknown");
		expect(observe_application(edit, bytes("other"), digest(result)) == cycle_application_observation::unknown, "wrong original");
	}

	void revises_revalidates_and_records_application()
	{
		const auto first   = suggest(1, offset("caf"), offset("caf") + 5, "coffee");
		const auto revised = change_suggestion(first, 1, revise_suggestion{ .Replacement = "tea", .Intent = cycle_suggestion_intent::illustrative });
		expect(revised && revised->Revision == 2 && revised->Replacement == "tea" && revised->Intent == cycle_suggestion_intent::illustrative && revised->Original == first.Original, "revised");
		expect(change_suggestion(*revised, 1, revise_suggestion{ .Replacement = "x" }).error() == cycle_refusal::conflict, "stale revision");

		// The same bytes found in a newer file: a revalidated successor.
		constexpr auto newer     = std::string_view{ "// header\r\nint f()\r\n{\r\n\treturn caf\xC3\xA9(1);\r\n}\r\n" };
		auto           candidate = *make_anchor(cycle_source_revision_id{ 2 }, "src/f.cpp", bytes(newer), newer.find("caf"), newer.find("caf") + 5);
		const auto     moved     = change_suggestion(first, 1, revalidate_suggestion{ candidate });
		expect(moved && moved->Source == cycle_source_revision_id{ 2 } && moved->Start == first.Start + 11 && moved->OriginalBlob == digest(newer), "revalidated");
		expect(moved->Replacement == first.Replacement && verify_suggestion(*moved, bytes(newer)) && !verify_suggestion(*moved, bytes(file)), "bound to the newer file only");
		auto different = *make_anchor(cycle_source_revision_id{ 2 }, "src/f.cpp", bytes(newer), 0, 5);
		expect(change_suggestion(first, 1, revalidate_suggestion{ different }).error() == cycle_refusal::invalid_target, "different bytes are not a candidate");

		const auto recorded = change_suggestion(first, 1, observe_suggestion_applied{ cycle_source_revision_id{ 3 } });
		expect(recorded && recorded->State == cycle_suggestion_state::observed_already_applied && recorded->AppliedIn == cycle_source_revision_id{ 3 }, "recorded as applied");
		expect(change_suggestion(*recorded, 2, revise_suggestion{ .Replacement = "x" }).error() == cycle_refusal::closed, "no change after application");
	}

	void round_trips_the_wire_form()
	{
		auto awkward       = suggest(7, offset("\treturn"), offset("\treturn") + 7, std::string{ "\0\r\n\t\xC3\xA9 end", 10 });
		awkward.Path       = "src/with space/f.cpp";
		const auto encoded = encode_suggestion_wire(awkward);
		const auto decoded = decode_suggestion_wire(encoded);
		expect(decoded && *decoded == awkward, "round trip keeps every byte");
		expect(encoded.find('\0') == std::string::npos && encoded.find('\r') == std::string::npos && encoded.find('\t') == std::string::npos, "wire text is plain ASCII lines");
		const auto insertion = suggest(8, 9, 9, "x");
		expect(decode_suggestion_wire(encode_suggestion_wire(insertion)) == insertion, "insertion round trip");
		const auto deletion = suggest(9, 0, 9, "");
		expect(decode_suggestion_wire(encode_suggestion_wire(deletion)) == deletion, "deletion round trip");

		const auto tamper = [&](const std::string_view aFrom, const std::string_view aTo) {
			auto       changed = encoded;
			const auto at      = changed.find(aFrom);
			expect(at != std::string::npos, "tamper target");
			changed.replace(at, aFrom.size(), aTo);
			return decode_suggestion_wire(changed);
		};
		expect(!tamper("replacement 10 ", "replacement 11 "), "length checked");
		expect(!tamper("range 12 19", "range 12 20"), "range checked against the bytes");
		expect(!tamper("exact_replacement", "exact"), "intent checked");
		expect(!tamper("end\n", "end\nextra\n"), "trailing text refused");
		expect(!decode_suggestion_wire(encoded.substr(0, encoded.size() - 1U)), "final newline required");
		const auto line  = encoded.find("original ");
		const auto hash  = encoded.find(' ', encoded.find(' ', line) + 1U) + 1U;
		auto       wrong = encoded;
		wrong[hash]      = wrong[hash] == '0' ? '1' : '0';
		expect(!decode_suggestion_wire(wrong), "hash checked");
	}
}

auto main() -> int
{
	keeps_exact_bytes_and_previews_in_memory();
	applies_nonoverlapping_edits_in_original_coordinates();
	refuses_conflicts_and_stale_material_whole();
	observes_application_without_similarity();
	revises_revalidates_and_records_application();
	round_trips_the_wire_form();
	return EXIT_SUCCESS;
}
