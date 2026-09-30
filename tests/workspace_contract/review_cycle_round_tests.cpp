#include "../support/test_expectation.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_round.hpp"
#include "cycle_course.hpp"
#include "cycle_session.hpp"
#include "workspace_fixture.hpp"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Frozen rounds through the production runtime and store
/// (RFC-BPR-0056, BPR-1158): a draft selection stored and frozen into one
/// immutable canonical payload, reused when repeated, refused when stale or
/// unflushed, and recovered after an interrupted freeze with one round.

namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::session;
	using tests::workspace_contract::session_operation;
	using rounds = tests::workspace_contract::round_drafts;
	using tests::workspace_contract::write_file;

	constexpr std::string_view header = "#pragma once\nstruct Cache {\n    void insert(int key);\n};\n";
	constexpr std::string_view body   = "#include \"cache.hpp\"\r\n\r\nvoid Cache::insert(int key) {\r\n\tentries.push_back(key);\r\n}\r\n";

	auto contains(const std::string_view aHaystack, const std::string_view aNeedle) -> bool
	{
		return aHaystack.find(aNeedle) != std::string_view::npos;
	}

	auto problem_of(const application::cycle_round_freeze_result& aResult) -> std::optional<core::cycle_round_problem>
	{
		return aResult.Refusal ? std::optional{ aResult.Refusal->Problem } : std::nullopt;
	}

	void freezes_one_complete_round(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto root   = aScratch / "freeze";
		write_file(root / "include" / "cache.hpp", header);
		write_file(root / "src" / "cache.cpp", body);
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");

		auto       notes     = session{ cycle.StoreRoot.path(), cycle.Subject };
		const auto first     = notes.begin({ .Primary = notes.anchor(1, "include/cache.hpp", "    void insert(int key);\n"),
											 .Related = { { .Target = core::cycle_whole_target{ }, .Disclosure = core::cycle_reference_disclosure::private_only } },
											 .Body    = "issue: Return whether an entry was added." });
		const auto firstItem = core::cycle_feedback_item_ref{ .Cycle = cycle.Subject.Cycle, .Id = core::cycle_feedback_item_id{ first->Allocated } };
		const auto second    = notes.begin({ .Primary = core::cycle_file_target{ core::cycle_source_revision_id{ 1 }, "src/cache.cpp", tests::workspace_contract::digest_of(body) }, .Body = "todo: Keep CRLF." });
		expect(first && second, "two notes");
		expect(notes.begin({ .Primary = core::cycle_whole_target{ }, .Body = "UNSELECTED-SCRATCH" }).has_value(), "unselected note");
		expect(notes.change(firstItem, 1, core::append_feedback_turn{ .Author = { .Provenance = core::authored_provenance::adapter_observed, .Adapter = "codex-cli", .Model = "gpt-6" }, .Body = "HIDDEN-REPLY" }).has_value(),
			   "hidden reply");
		const auto suggested = notes.run<application::cycle_write_receipt>([&](auto aDone) {
			return notes.Runtime.add_suggestion_selected(session_operation(notes.NextOperation++), firstItem, 2,
														 { .Selection = notes.anchor(1, "src/cache.cpp", "\tentries.push_back(key);\r\n"), .Replacement = "\tif (!contains(key)) {\r\n\t\tentries.push_back(key);\r\n\t}\r\n" },
														 std::move(aDone));
		});
		expect(suggested.has_value(), "exact CRLF suggestion");

		const auto begun = notes.run<application::cycle_write_receipt>([&](auto aDone) { return notes.Runtime.begin_round_selected(session_operation(notes.NextOperation++), notes.state().Revision, std::move(aDone)); });
		expect(begun.has_value(), "draft begun");
		auto draft     = rounds{ notes, { .Cycle = cycle.Subject.Cycle, .Id = core::cycle_round_draft_id{ begun->Allocated } } };
		auto selection = core::cycle_round_selection{
			.Source     = core::cycle_source_revision_id{ 1 },
			.Goal       = "Make insertion idempotent.",
			.WriteScope = { "include/cache.hpp", "src/cache.cpp" },
			.Items      = {
				{ .Item = firstItem.Id, .Wording = core::private_review_turn_revision_id{ 1 }, .Suggestions = { { .Id = core::cycle_suggestion_id{ 1 }, .Revision = 1 } } },
				{ .Item = core::cycle_feedback_item_id{ second->Allocated }, .Wording = core::private_review_turn_revision_id{ 1 } },
			},
			.Context = { { .Revision = core::cycle_source_revision_id{ 1 }, .Path = "src/cache.cpp" } },
		};
		draft.save(selection);
		const auto loaded = notes.run<application::cycle_round_draft_content>([&](auto aDone) { return notes.Runtime.load_round_selected(draft.Draft, std::move(aDone)); });
		expect(loaded && loaded->Selection == selection && loaded->Draft.Revision == draft.Revision, "draft reads back exactly");

		// An open IME composition, unsaved typing or an older editor view never freezes.
		expect(problem_of(draft.freeze({ .Composing = true, .ShownRevision = draft.Revision })) == core::cycle_round_problem::unflushed, "IME composition");
		expect(problem_of(draft.freeze({ .SavePending = true, .ShownRevision = draft.Revision })) == core::cycle_round_problem::unflushed, "busy editor");
		expect(problem_of(draft.freeze({ .ShownRevision = draft.Revision - 1U })) == core::cycle_round_problem::stale_draft, "stale editor view");
		expect(draft.count() == 0U, "no round from a refused freeze");

		const auto frozen = draft.freeze(draft.flushed());
		expect(frozen.Frozen && !frozen.Frozen->Reused && frozen.Frozen->Round.Round.Id.Id == core::cycle_round_id{ 1 }, "round F1 frozen");
		const auto& payload = frozen.Frozen->Content.Payload;
		expect(payload.RoundId == "F1" && payload.Items.size() == 2U && payload.Sources.size() == 1U && payload.Sources[0].Bytes == body, "complete selected content");
		expect(payload.Items[0].Suggestions.size() == 1U && payload.Items[0].Suggestions[0].Replacement == "\tif (!contains(key)) {\r\n\t\tentries.push_back(key);\r\n\t}\r\n", "exact suggestion bytes");
		const auto& canonical = frozen.Frozen->Content.Canonical;
		for (const auto hidden : { std::string_view{ "HIDDEN-REPLY" }, std::string_view{ "UNSELECTED-SCRATCH" }, std::string_view{ "codex-cli" }, std::string_view{ "gpt-6" } }) {
			expect(!contains(canonical, hidden), "nothing unselected or hidden");
		}
		expect(!contains(canonical, root.string()) && !contains(canonical, root.generic_string()) && !contains(canonical, cycle.StoreRoot.path().string()), "no absolute local path");

		// The stored round reads back byte for byte after a restart.
		{
			auto       reopened = session{ cycle.StoreRoot.path(), cycle.Subject };
			auto       again    = rounds{ reopened, draft.Draft, draft.Revision };
			const auto read     = again.read(1);
			expect(read.Content.Canonical == canonical && read.Round.Round.Digest == frozen.Frozen->Round.Round.Digest, "frozen payload unchanged after reopen");
		}

		// Identical freezes reuse the round; the same operation replays.
		const auto repeat = draft.freeze(draft.flushed());
		expect(repeat.Frozen && repeat.Frozen->Reused && repeat.Frozen->Round == frozen.Frozen->Round, "identical freeze reuses F1");
		const auto operation = session_operation(9000);
		const auto once      = draft.freeze(draft.flushed(), operation);
		const auto replay    = draft.freeze(draft.flushed(), operation);
		expect(once.Frozen && replay.Frozen && replay.Frozen->Replayed && replay.Frozen->Round == frozen.Frozen->Round, "retry replays");
		expect(draft.count() == 1U, "one logical round");

		// An edited note makes the old selection stale; F1 is untouched.
		const auto edited = notes.change(firstItem, 3, core::edit_feedback_turn{ .Latest = core::private_review_turn_revision_id{ 1 }, .Body = "issue: Return whether an entry was added; update callers." });
		expect(edited.has_value(), "wording edited");
		const auto stale = draft.freeze(draft.flushed());
		expect(problem_of(stale) == core::cycle_round_problem::stale_wording && stale.Refusal->Item == 0U, "stale wording named, not substituted");
		// Selecting the new wording is a new draft revision and a new round.
		selection.Items[0].Wording = core::private_review_turn_revision_id{ edited->Allocated };
		draft.save(selection);
		const auto amended = draft.freeze(draft.flushed());
		expect(amended.Frozen && amended.Frozen->Round.Round.Id.Id == core::cycle_round_id{ 2 } && amended.Frozen->Round.Round.Digest != frozen.Frozen->Round.Round.Digest, "amendment is F2");
		expect(draft.read(1).Content.Canonical == canonical, "F1 bytes never change");
		expect(draft.count() == 2U, "two rounds");
		std::cout << "NATIVE: one complete round frozen through the runtime and store; repeats reuse, edits amend, refusals commit nothing\n";
	}

	void recovers_an_interrupted_freeze(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto root   = aScratch / "interrupt";
		write_file(root / "src" / "cache.cpp", body);
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");
		auto&      store = *cycle.Store;
		auto       host  = application::review_cycle_comparison_host{ store, cycle.Subject.Authority };
		const auto note  = store.begin_feedback(tests::workspace_contract::operation(200), cycle.Subject, cycle.state().Revision, { .Primary = core::cycle_whole_target{ }, .Body = "note: One." }, { });
		expect(note.has_value(), "note");
		const auto begun = store.begin_draft(tests::workspace_contract::operation(201), cycle.Subject, cycle.state().Revision, { });
		expect(begun.has_value(), "draft");
		const auto draft     = core::cycle_round_draft_ref{ .Cycle = cycle.Subject.Cycle, .Id = core::cycle_round_draft_id{ begun->Allocated } };
		const auto selection = core::cycle_round_selection{ .Source = core::cycle_source_revision_id{ 1 }, .Items = { { .Item = core::cycle_feedback_item_id{ note->Allocated }, .Wording = core::private_review_turn_revision_id{ 1 } } } };
		const auto saved     = application::save_round_selection(store, tests::workspace_contract::operation(202), cycle.Subject, draft, 1, selection, { });
		expect(saved.has_value(), "draft saved");
		const auto request = application::cycle_round_freeze_request{ .Operation = tests::workspace_contract::operation(203), .Draft = draft, .ExpectedDraftRevision = saved->Revision, .Editor = { .ShownRevision = saved->Revision } };

		// Cancelled before its transaction: no round, and a retry freezes once.
		auto stop = std::stop_source{ };
		stop.request_stop();
		const auto cancelled = application::freeze_cycle_round(store, host, cycle.Subject, request, stop.get_token());
		expect(!cancelled && cancelled.error().Failure == application::cycle_store_failure::cancelled, "cancelled freeze");
		const auto none = store.rounds(cycle.Subject, std::nullopt, { });
		expect(none && none->Rounds.empty(), "no partial round");

		// A process that installed the payload and died before the round
		// commit: the same assembly the freeze performs, installed alone.
		const auto state   = cycle.state();
		const auto thread  = store.load_feedback(cycle.Subject, { .Cycle = cycle.Subject.Cycle, .Id = core::cycle_feedback_item_id{ note->Allocated } }, { });
		const auto history = store.feedback_revisions(cycle.Subject, (*thread)->Id, std::nullopt, { });
		expect(thread && *thread && history, "records read");
		const auto items = std::vector{ core::cycle_round_item_material{ .Thread = **thread, .History = history->Revisions } };
		const auto built = core::assemble_round({ .Cycle = &state, .Round = core::cycle_round_id{ 1 }, .SourceCapture = state.Latest->Capture, .Selection = &selection, .Items = items });
		expect(built.has_value(), "payload assembled");
		const auto bytes     = *core::encode_round_payload(*built);
		const auto installed = store.install(tests::workspace_contract::bytes_of(bytes), { });
		expect(installed.has_value(), "payload installed before the crash");
		const auto orphan = store.inventory({ });
		expect(orphan && orphan->UnreferencedObjects == 1U && orphan->UnreferencedBytes == bytes.size(), "only an unreferenced payload remains");
		expect(store.rounds(cycle.Subject, std::nullopt, { })->Rounds.empty(), "and no round");

		// Recovery: the freeze completes once, adopting the same bytes.
		const auto recovered = application::freeze_cycle_round(store, host, cycle.Subject, request, { });
		expect(recovered && recovered->Frozen && !recovered->Frozen->Reused && recovered->Frozen->Content.Canonical == bytes, "freeze completes with the same payload");
		expect(recovered->Frozen->Round.Payload == *installed, "the installed object is the round's");
		const auto settled = store.inventory({ });
		expect(settled && settled->UnreferencedObjects == 0U, "nothing left unreferenced");
		const auto again = application::freeze_cycle_round(store, host, cycle.Subject, request, { });
		expect(again && again->Frozen && again->Frozen->Replayed && again->Frozen->Round == recovered->Frozen->Round, "repeating the operation replays");
		const auto repeated = application::freeze_cycle_round(store, host, cycle.Subject, { .Operation = tests::workspace_contract::operation(204), .Draft = draft, .ExpectedDraftRevision = saved->Revision, .Editor = request.Editor }, { });
		expect(repeated && repeated->Frozen && repeated->Frozen->Reused && repeated->Frozen->Round == recovered->Frozen->Round, "a new request reuses the round");
		const auto all = store.rounds(cycle.Subject, std::nullopt, { });
		expect(all && all->Rounds.size() == 1U, "exactly one logical round");
		std::cout << "NATIVE: an interrupted freeze leaves no round and recovers to exactly one\n";
	}
}

auto main() -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "cycle-round" };
	freezes_one_complete_round(scratch.path());
	recovers_an_interrupted_freeze(scratch.path());
	std::cout << "review-cycle-round-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle round failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
