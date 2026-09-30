#include "../support/test_expectation.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "cycle_course.hpp"
#include "cycle_session.hpp"
#include "workspace_fixture.hpp"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Exact suggested edits on real captured material (RFC-BPR-0055,
/// BPR-1155), through the production runtime and store: byte-exact records,
/// in-memory previews, whole-set preflight with whole-blob preconditions, and
/// already-applied observation. Nothing is ever written to the workspace.

namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using application::cycle_store_failure;
	using application::review_material_status;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::session;
	using tests::workspace_contract::write_file;

	constexpr std::string_view original = "int f()\r\n{\r\n\treturn caf\xC3\xA9(1);\r\n}\r\n";

	auto snapshot(const std::filesystem::path& aRoot) -> std::map<std::string, std::string>
	{
		auto files = std::map<std::string, std::string>{ };
		for (const auto& entry : std::filesystem::recursive_directory_iterator{ aRoot }) {
			if (entry.is_regular_file()) {
				files.emplace(entry.path().lexically_relative(aRoot).generic_string(), read_file(entry.path()));
			}
		}
		return files;
	}

	auto text(const std::vector<std::uint8_t>& aBytes) -> std::string
	{
		return { aBytes.begin(), aBytes.end() };
	}

	struct suggestions
	{
		session&                      Notes;
		core::cycle_feedback_item_ref Item;
		std::uint64_t                 Version = 1;

		auto add(const std::uint64_t aRevision, const std::string_view aSelected, std::string aReplacement,
				 const core::cycle_suggestion_intent aIntent = core::cycle_suggestion_intent::exact_replacement) -> core::cycle_suggestion_ref
		{
			const auto receipt = add_raw(Notes.anchor(aRevision, "src/f.cpp", aSelected), std::move(aReplacement), aIntent);
			expect(receipt.has_value(), "suggestion added");
			return { .Item = Item, .Id = core::cycle_suggestion_id{ receipt->Allocated } };
		}
		auto add_raw(core::cycle_source_anchor aSelection, std::string aReplacement, const core::cycle_suggestion_intent aIntent = core::cycle_suggestion_intent::exact_replacement)
			-> application::cycle_store_result<application::cycle_write_receipt>
		{
			auto receipt = Notes.run<application::cycle_write_receipt>([&](auto aDone) {
				return Notes.Runtime.add_suggestion_selected(tests::workspace_contract::session_operation(Notes.NextOperation++), Item, Version,
															 { .Selection = std::move(aSelection), .Replacement = std::move(aReplacement), .Intent = aIntent }, std::move(aDone));
			});
			if (receipt) {
				Version = receipt->Revision;
			}
			return receipt;
		}
		auto latest() -> std::vector<core::cycle_suggestion>
		{
			const auto found = Notes.run<std::vector<core::cycle_suggestion>>([&](auto aDone) { return Notes.Runtime.suggestions_selected(Item, std::move(aDone)); });
			expect(found.has_value(), "suggestions listed");
			return *found;
		}
		auto get(const core::cycle_suggestion_ref& aRef) -> core::cycle_suggestion
		{
			for (auto& value : latest()) {
				if (value.Id == aRef) {
					return value;
				}
			}
			expect(false, "suggestion present");
			return { };
		}
		auto change(const core::cycle_suggestion_ref& aRef, const std::uint64_t aExpected, core::cycle_suggestion_change aChange)
			-> application::cycle_store_result<application::cycle_write_receipt>
		{
			return Notes.run<application::cycle_write_receipt>([&](auto aDone) {
				return Notes.Runtime.change_suggestion_selected(tests::workspace_contract::session_operation(Notes.NextOperation++), aRef, aExpected, std::move(aChange), std::move(aDone));
			});
		}
		auto preflight(std::vector<core::cycle_suggestion> aSet, const std::optional<std::uint64_t> aAgainst = std::nullopt) -> application::cycle_suggestion_preflight_observation
		{
			const auto against = aAgainst ? std::optional{ core::cycle_source_revision_id{ *aAgainst } } : std::nullopt;
			const auto result  = Notes.run<application::cycle_suggestion_preflight_observation>([&](auto aDone) {
				return Notes.Runtime.preflight_suggestions_selected(std::move(aSet), against, std::move(aDone));
			});
			expect(result && result->Status == review_material_status::available && result->Preflight, "preflight observed");
			return *result;
		}
	};

	auto issues(const application::cycle_suggestion_preflight_observation& aObservation) -> std::vector<core::cycle_suggestion_issue>
	{
		auto found = std::vector<core::cycle_suggestion_issue>{ };
		for (const auto& finding : aObservation.Preflight->Findings) {
			found.push_back(finding.Issue);
		}
		return found;
	}

	void suggests_previews_and_preflights_without_writing(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto root   = aScratch / "suggest";
		write_file(root / "src" / "f.cpp", original);
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");

		auto       notes  = session{ cycle.StoreRoot.path(), cycle.Subject };
		const auto begun  = notes.begin({ .Primary = notes.anchor(1, "src/f.cpp", "caf\xC3\xA9(1)"), .Body = "Name this clearly." });
		auto       edits  = suggestions{ notes, { .Cycle = cycle.Subject.Cycle, .Id = core::cycle_feedback_item_id{ begun->Allocated } } };
		const auto before = snapshot(root);
		const auto held   = cycle.Store->inventory({ });

		// Two exact edits with CRLF, tab and a multibyte scalar in their bytes, and an illustration.
		const auto name   = edits.add(1, "caf\xC3\xA9(1)", "coffee(1)\t// renamed\r\n\t");
		const auto head   = edits.add(1, "int f()", "long f()");
		const auto sketch = edits.add(1, "{", "{ /* maybe */", core::cycle_suggestion_intent::illustrative);
		expect(name.Id == core::cycle_suggestion_id{ 1 } && sketch.Id == core::cycle_suggestion_id{ 3 } && edits.Version == 4, "numbered per note");
		const auto stored = edits.get(name);
		expect(stored.Original == "caf\xC3\xA9(1)" && stored.Replacement == "coffee(1)\t// renamed\r\n\t" && stored.Start == 20 && stored.End == 28, "exact bytes kept");

		const auto preview = notes.run<application::cycle_suggestion_preview_observation>([&](auto aDone) { return notes.Runtime.preview_suggestion_selected(stored, std::move(aDone)); });
		expect(preview && preview->Preview && text(preview->Before) == original, "preview from the retained original");
		expect(text(preview->Preview->After) == "int f()\r\n{\r\n\treturn coffee(1)\t// renamed\r\n\t;\r\n}\r\n" && preview->Preview->Replaced == core::cycle_line_span{ 3, 4 }, "after bytes and lines");

		const auto both = edits.preflight({ stored, edits.get(head) });
		expect(both.Preflight->applicable() && text(*both.Preflight->Result) == "long f()\r\n{\r\n\treturn coffee(1)\t// renamed\r\n\t;\r\n}\r\n", "original-coordinate set");
		expect(issues(edits.preflight({ stored, edits.get(sketch) })) == std::vector{ core::cycle_suggestion_issue::illustrative }, "an illustration is never applied");

		// Overlaps and two insertions at one boundary are conflicts; nothing is applied.
		const auto overlap = edits.add(1, "(1)", "(2)");
		expect(issues(edits.preflight({ stored, edits.get(overlap) })) == std::vector{ core::cycle_suggestion_issue::conflict }, "contained edit conflicts");
		const auto point    = *core::make_anchor(core::cycle_source_revision_id{ 1 }, "src/f.cpp", tests::workspace_contract::bytes_of(original), 9, 9);
		const auto first    = edits.add_raw(point, "// one\r\n");
		const auto second   = edits.add_raw(point, "// two\r\n");
		const auto insertAt = [&](const std::uint64_t aId) { return edits.get({ .Item = edits.Item, .Id = core::cycle_suggestion_id{ aId } }); };
		expect(first && second, "insertions recorded");
		const auto twice = edits.preflight({ insertAt(first->Allocated), insertAt(second->Allocated) });
		expect(issues(twice) == std::vector{ core::cycle_suggestion_issue::conflict } && !twice.Preflight->Result, "two insertions at one boundary");

		// A selection naming bytes the revision does not hold is refused before the store.
		auto forged     = notes.anchor(1, "src/f.cpp", "int f()");
		forged.Original = "INT F()";
		const auto bad  = edits.add_raw(forged, "x");
		expect(!bad && bad.error().Refusal == core::cycle_refusal::invalid_target, "forged selection");

		expect(snapshot(root) == before, "suggesting, previewing and preflighting write nothing to the workspace");
		const auto after = cycle.Store->inventory({ });
		expect(held && after && after->PinnedBytes == held->PinnedBytes && after->UnreferencedBytes == held->UnreferencedBytes, "and install nothing in the store");
		std::cout << "NATIVE: exact suggestions previewed and preflighted in memory; workspace and retained material unchanged\n";
	}

	void refuses_stale_blobs_and_never_applies_twice(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto root   = aScratch / "stale";
		write_file(root / "src" / "f.cpp", original);
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");
		// R2: the range still reads the same, but the file changed elsewhere.
		const auto shifted = std::string{ "// header\r\n" } + std::string{ original };
		write_file(root / "src" / "f.cpp", shifted);
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R2");

		auto       notes = session{ cycle.StoreRoot.path(), cycle.Subject };
		const auto begun = notes.begin({ .Primary = core::cycle_file_target{ core::cycle_source_revision_id{ 1 }, "src/f.cpp", tests::workspace_contract::digest_of(original) }, .Body = "Rename." });
		auto       edits = suggestions{ notes, { .Cycle = cycle.Subject.Cycle, .Id = core::cycle_feedback_item_id{ begun->Allocated } } };
		const auto ref   = edits.add(1, "int f()", "long f()");
		auto       edit  = edits.get(ref);

		expect(issues(edits.preflight({ edit }, 2)) == std::vector{ core::cycle_suggestion_issue::stale_blob }, "locally matching text in a changed file is stale");
		expect(edits.preflight({ edit }, 1).Preflight->applicable(), "applicable against its own file");

		// Revalidating binds a successor to the R2 bytes without fuzzy matching.
		const auto moved = edits.change(ref, 1, core::revalidate_suggestion{ notes.anchor(2, "src/f.cpp", "int f()") });
		expect(moved && moved->Revision == 2, "revalidated");
		edit = edits.get(ref);
		expect(edit.Source == core::cycle_source_revision_id{ 2 } && edit.Start == 11 && edits.preflight({ edit }, 2).Preflight->applicable(), "applies to R2");
		auto candidate     = notes.anchor(2, "src/f.cpp", "int f()");
		candidate.Original = "INT F()";
		expect(!edits.change(ref, 2, core::revalidate_suggestion{ candidate }), "a candidate must hold the original bytes");

		// The developer makes a similar edit: unknown, not applied.
		const auto result = text(*edits.preflight({ edit }, 2).Preflight->Result);
		write_file(root / "src" / "f.cpp", result + " ");
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R3");
		const auto similar = edits.change(ref, 2, core::observe_suggestion_applied{ core::cycle_source_revision_id{ 3 } });
		expect(!similar && similar.error().Refusal == core::cycle_refusal::invalid_target, "similar text is not an application");
		// Then exactly this edit: observed as already applied, never offered again.
		write_file(root / "src" / "f.cpp", result);
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R4");
		const auto observed = edits.change(ref, 2, core::observe_suggestion_applied{ core::cycle_source_revision_id{ 4 } });
		expect(observed && observed->Revision == 3, "already applied");
		edit = edits.get(ref);
		expect(edit.State == core::cycle_suggestion_state::observed_already_applied && edit.AppliedIn == core::cycle_source_revision_id{ 4 }, "labelled");
		expect(issues(edits.preflight({ edit }, 2)) == std::vector{ core::cycle_suggestion_issue::already_applied }, "not applied twice");
		const auto closed = edits.change(ref, 3, core::revise_suggestion{ .Replacement = "x" });
		expect(!closed && closed.error().Refusal == core::cycle_refusal::closed, "no revision after application");
		const auto stale = edits.change(ref, 1, core::revise_suggestion{ .Replacement = "x" });
		expect(!stale && stale.error().Failure == cycle_store_failure::conflict, "stale revision");

		// Every revision survives a reopen, and the wire form keeps its bytes.
		notes.Runtime.shutdown();
		auto       store   = adapters::sqlite::make_review_cycle_store(cycle.StoreRoot.path(), tests::workspace_contract::profile);
		const auto history = store->suggestion_history(cycle.Subject, ref, std::nullopt, { });
		expect(history && history->Revisions.size() == 3 && !history->Next, "three revisions after reopen");
		expect(history->Revisions[0].Source == core::cycle_source_revision_id{ 1 } && history->Revisions[1].Source == core::cycle_source_revision_id{ 2 }, "earlier revisions kept");
		expect(core::decode_suggestion_wire(core::encode_suggestion_wire(history->Revisions[2])) == history->Revisions[2], "wire round trip");
		std::cout << "NATIVE: stale blob refused, revalidated exactly, similar edit unknown, exact edit observed as applied\n";
	}
}

auto main() -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "cycle-suggestion" };
	suggests_previews_and_preflights_without_writing(scratch.path());
	refuses_stale_blobs_and_never_applies_twice(scratch.path());
	std::cout << "review-cycle-suggestion-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle suggestion failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
