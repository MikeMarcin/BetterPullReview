#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_verification.hpp"
#include "bpr/core/review_cycle_response.hpp"
#include "cycle_course.hpp"
#include "cycle_session.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
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
/// \brief Reconciliation and explicit reviewer verification through the
/// production runtime and store (RFC-BPR-0057, BPR-1165): repeated, moved,
/// deleted and missing source, an already-present exact replacement, an
/// illustrative suggestion, a recipient's "addressed", verification bound to
/// checked material that becomes history when that material or the note
/// changes, and the next round's draft carried forward without sending.

namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using core::cycle_item_status;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::session;
	using tests::workspace_contract::session_operation;
	using tests::workspace_contract::write_file;

	constexpr std::string_view parser = "int parse(const char* aText)\n{\n\treturn check(aText);\n}\n";
	constexpr std::string_view errors = "int fail()\n{\n\treturn -1;\n}\n";
	constexpr std::string_view lexer  = "void tokenize()\n{\n}\n";
	constexpr std::string_view util   = "int twice(int aValue)\n{\n\treturn aValue * 2;\n}\n";

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

	auto reconcile(session& aNotes, const std::uint64_t aAgainst) -> std::vector<core::cycle_item_reconciliation>
	{
		const auto result = aNotes.run<std::vector<core::cycle_item_reconciliation>>([&](auto aDone) { return aNotes.Runtime.reconcile_selected(core::cycle_source_revision_id{ aAgainst }, std::move(aDone)); });
		expect(result.has_value(), "reconciled");
		return *result;
	}

	auto decide(session& aNotes, const core::local_cycle_subject& aSubject, const std::uint64_t aItem, const std::optional<core::cycle_reviewer_decision> aDecision, const std::uint64_t aChecked)
		-> application::cycle_decision_result
	{
		const auto request = application::cycle_decision_request{ .Item = { .Cycle = aSubject.Cycle, .Id = core::cycle_feedback_item_id{ aItem } }, .Decision = aDecision, .Checked = core::cycle_source_revision_id{ aChecked } };
		const auto result  = aNotes.run<application::cycle_decision_result>([&](auto aDone) { return aNotes.Runtime.decide_selected(session_operation(aNotes.NextOperation++), request, std::move(aDone)); });
		expect(result.has_value(), "decided");
		return *result;
	}

	auto status_of(const std::vector<core::cycle_item_reconciliation>& aItems, const std::uint64_t aItem) -> const core::cycle_item_reconciliation&
	{
		const auto found = std::ranges::find(aItems, core::cycle_feedback_item_id{ aItem }, &core::cycle_item_reconciliation::Item);
		expect(found != aItems.end(), "note reconciled");
		return *found;
	}

	void verifies_explicitly(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto root   = aScratch / "verify" / "parser";
		write_file(root / "src" / "parser.cpp", parser);
		write_file(root / "src" / "errors.cpp", errors);
		write_file(root / "src" / "lexer.cpp", lexer);
		write_file(root / "src" / "util.cpp", util);
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");
		auto       notes = session{ cycle.StoreRoot.path(), cycle.Subject };
		const auto ref   = [&](const std::uint64_t aItem) { return core::cycle_feedback_item_ref{ .Cycle = cycle.Subject.Cycle, .Id = core::cycle_feedback_item_id{ aItem } }; };

		// Five notes: an exact suggestion, a file that will be deleted, a
		// snippet that will be copied, an illustrative suggestion, a module.
		expect(notes.begin({ .Primary = notes.anchor(1, "src/parser.cpp", "check(aText)"), .Body = "Name what it does." }).has_value(), "I1");
		expect(notes.run<application::cycle_write_receipt>([&](auto aDone) { return notes.Runtime.add_suggestion_selected(session_operation(notes.NextOperation++), ref(1), 1, core::suggest_change{ .Selection = notes.anchor(1, "src/parser.cpp", "check(aText)"), .Replacement = "validate(aText)" }, std::move(aDone)); }).has_value(), "exact suggestion");
		expect(notes.begin({ .Primary = notes.anchor(1, "src/lexer.cpp", "tokenize()"), .Body = "Does this belong here?" }).has_value(), "I2");
		expect(notes.begin({ .Primary = notes.anchor(1, "src/errors.cpp", "return -1;"), .Body = "Use an error type." }).has_value(), "I3");
		expect(notes.begin({ .Primary = notes.anchor(1, "src/util.cpp", "aValue * 2"), .Body = "Something like this." }).has_value(), "I4");
		expect(notes.run<application::cycle_write_receipt>([&](auto aDone) { return notes.Runtime.add_suggestion_selected(session_operation(notes.NextOperation++), ref(4), 1, core::suggest_change{ .Selection = notes.anchor(1, "src/util.cpp", "aValue * 2"), .Replacement = "aValue << 1", .Intent = core::cycle_suggestion_intent::illustrative }, std::move(aDone)); }).has_value(), "illustrative suggestion");
		expect(notes.begin({ .Primary = core::cycle_module_target{ .Path = "src" }, .Body = "Add tests." }).has_value(), "I5");
		auto draft = tests::workspace_contract::begin_round(notes);
		auto items = std::vector<core::cycle_round_item_selection>{ };
		for (std::uint64_t item = 1; item <= 5; ++item) {
			items.push_back({ .Item = core::cycle_feedback_item_id{ item }, .Wording = core::private_review_turn_revision_id{ 1 } });
		}
		items[0].Suggestions = { { .Id = core::cycle_suggestion_id{ 1 }, .Revision = 1 } };
		draft.save({ .Source = core::cycle_source_revision_id{ 1 }, .Action = core::cycle_round_action::implement, .Goal = "Harden parsing.", .Constraints = { "Keep the API." }, .Items = items });
		const auto frozen = draft.freeze(draft.flushed());
		expect(frozen.Frozen.has_value(), "F1");

		// The recipient edits: the exact suggestion applied, the lexer deleted,
		// the error path copied, the illustrative idea done differently.
		write_file(root / "src" / "parser.cpp", "int parse(const char* aText)\n{\n\treturn validate(aText);\n}\n");
		write_file(root / "src" / "errors.cpp", "int bad()\n{\n\t// bad\n\treturn -1;\n}\nint worse()\n{\n\t// worse\n\treturn -1;\n}\n");
		std::filesystem::remove(root / "src" / "lexer.cpp");
		write_file(root / "src" / "util.cpp", "int twice(int aValue)\n{\n\treturn aValue + aValue;\n}\n");
		const auto& round = frozen.Frozen->Content;
		// The claimed author names its type for GCC 15's nested-aggregate
		// miscompile ([[BPR-0816]], [[BPR-1262]]).
		const auto response = core::cycle_response_document{
			.ResponseId    = "RESP-1",
			.CycleId       = round.Payload.CycleId,
			.RoundId       = round.Payload.RoundId,
			.RoundDigest   = round.Digest,
			.ClaimedAuthor = core::cycle_claimed_author{ .DisplayName = "Harness", .Kind = core::cycle_claimed_author_kind::agent },
			.Entries       = { { .Item = "I1", .ItemRevision = 1, .Disposition = core::cycle_response_disposition::addressed, .Body = "Renamed; tests pass." },
							   { .Item = "I2", .ItemRevision = 1, .Disposition = core::cycle_response_disposition::addressed, .Body = "Removed the lexer." },
							   { .Item = "I3", .ItemRevision = 1, .Disposition = core::cycle_response_disposition::disputed, .Body = "The -1 is the contract." } },
		};
		const auto document = *core::encode_response_document(response);
		const auto imported = notes.run<application::cycle_response_result>([&](auto aDone) {
			return notes.Runtime.import_response_selected(session_operation(notes.NextOperation++), core::cycle_round_id{ 1 }, [document](std::stop_token) -> std::expected<std::vector<std::uint8_t>, application::cycle_export_failure> { return std::vector<std::uint8_t>(document.begin(), document.end()); }, std::move(aDone));
		});
		expect(imported && imported->Receipt, "the recipient's claims appended");
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R2");

		// Reconciled against R2: each fact separate, nothing resolved by itself.
		const auto r2 = reconcile(notes, 2);
		expect(r2.size() == 5U, "five notes");
		const auto& first = status_of(r2, 1);
		expect(first.Primary && first.Primary->Kind == core::cycle_correspondence::unmapped && first.Suggestions.size() == 1U && first.Suggestions[0].Observation == core::cycle_application_observation::already_applied,
			   "the original code is gone and the exact replacement is present");
		expect(first.Status == cycle_item_status::needs_verification && !core::resolved(first), "still only asks for verification");
		expect(status_of(r2, 2).Primary->Kind == core::cycle_correspondence::deleted && status_of(r2, 2).Status == cycle_item_status::needs_verification, "a deleted file is not proof either");
		expect(status_of(r2, 3).Primary->Kind == core::cycle_correspondence::ambiguous && status_of(r2, 3).Primary->Candidates == 2U && status_of(r2, 3).Status == cycle_item_status::reported,
			   "repeated source is ambiguous; a dispute is reported");
		expect(status_of(r2, 4).Suggestions[0].Observation == core::cycle_application_observation::unknown && status_of(r2, 4).Status == cycle_item_status::open, "an illustrative fix is never already present");
		expect(!status_of(r2, 5).Primary && status_of(r2, 5).Status == cycle_item_status::open, "a module note has no correspondence and nobody answered it");

		// Only the reviewer's explicit decisions change a status.
		const auto verified = decide(notes, cycle.Subject, 1, core::cycle_reviewer_decision::verified, 2);
		expect(verified.Receipt && verified.Record && verified.Record->Checked == core::cycle_source_revision_id{ 2 } && verified.Record->Wording == core::private_review_turn_revision_id{ 1 }, "verified against R2's material");
		expect(decide(notes, cycle.Subject, 2, core::cycle_reviewer_decision::needs_work, 2).Receipt.has_value(), "needs work");
		expect(decide(notes, cycle.Subject, 5, core::cycle_reviewer_decision::withdrawn, 2).Receipt.has_value(), "withdrawn");
		expect(decide(notes, cycle.Subject, 3, core::cycle_reviewer_decision::verified, 7).Refusal == core::cycle_refusal::invalid_target, "an uncaptured revision cannot be checked");
		expect(decide(notes, cycle.Subject, 9, core::cycle_reviewer_decision::verified, 2).Refusal == core::cycle_refusal::invalid_identity, "an unknown note");
		const auto decided = reconcile(notes, 2);
		expect(status_of(decided, 1).Status == cycle_item_status::verified && core::resolved(status_of(decided, 1)), "verified");
		expect(status_of(decided, 2).Status == cycle_item_status::needs_work && status_of(decided, 5).Status == cycle_item_status::withdrawn, "needs work and withdrawn");

		// An unrelated change keeps it current; a relevant one makes it history.
		write_file(root / "src" / "util.cpp", "int twice(int aValue)\n{\n\treturn 2 * aValue;\n}\n");
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R3");
		expect(status_of(reconcile(notes, 3), 1).Status == cycle_item_status::verified, "another file changing leaves the verification current");
		write_file(root / "src" / "parser.cpp", "int parse(const char* aText)\n{\n\treturn validate(aText) ? 1 : 0;\n}\n");
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R4");
		const auto r4 = reconcile(notes, 4);
		expect(status_of(r4, 1).Status == cycle_item_status::recheck_needed && status_of(r4, 1).Currency == core::cycle_verification_currency::material_changed
				   && status_of(r4, 1).Decision->Checked == core::cycle_source_revision_id{ 2 },
			   "the checked material changed: recheck needed, the R2 verification kept as history");
		expect(status_of(reconcile(notes, 2), 1).Status == cycle_item_status::verified, "against R2 it is still what was verified");
		// Amending the note makes the verification history too.
		const auto edited = notes.change(ref(1), notes.threads().Threads[0].Lineage.Version, core::edit_feedback_turn{ .Latest = core::private_review_turn_revision_id{ 1 }, .Body = "Name what it does, and document it." });
		expect(edited.has_value(), "amended");
		expect(status_of(reconcile(notes, 2), 1).Currency == core::cycle_verification_currency::item_amended, "an amended note needs checking again");

		// A new note after F1, then the next round, planned and saved, not sent.
		expect(notes.begin({ .Primary = core::cycle_whole_target{ }, .Body = "Update the changelog." }).has_value(), "I6");
		const auto before  = snapshot(root);
		const auto planned = notes.run<application::cycle_next_round_result>([&](auto aDone) {
			const auto beginOperation = session_operation(notes.NextOperation++);
			const auto saveOperation  = session_operation(notes.NextOperation++);
			return notes.Runtime.prepare_next_round_selected(beginOperation, saveOperation, std::move(aDone));
		});
		expect(planned && planned->Draft && planned->From == core::cycle_round_id{ 1 }, "a draft planned from F1");
		const auto& plan = planned->Plan;
		expect(plan.Carried == std::vector{ core::cycle_feedback_item_id{ 1 }, core::cycle_feedback_item_id{ 2 }, core::cycle_feedback_item_id{ 3 }, core::cycle_feedback_item_id{ 4 } }
				   && plan.Resolved == std::vector{ core::cycle_feedback_item_id{ 5 } } && plan.Added == std::vector{ core::cycle_feedback_item_id{ 6 } },
			   "open items carried, the withdrawn one left out, the new note added");
		expect(plan.Selection.Source == core::cycle_source_revision_id{ 4 } && plan.Selection.Goal == "Harden parsing." && plan.Selection.Constraints == std::vector<std::string>{ "Keep the API." },
			   "the previous assignment carried against the latest capture");
		expect(plan.Selection.Items[0].Wording != core::private_review_turn_revision_id{ 1 } && plan.Selection.Items[0].Suggestions.size() == 1U, "the amended wording and the carried suggestion");
		const auto stored = notes.run<application::cycle_round_draft_content>([&](auto aDone) { return notes.Runtime.load_round_selected(*planned->Draft, std::move(aDone)); });
		expect(stored && stored->Selection == plan.Selection, "the plan is the saved draft");
		expect(draft.count() == 1U && snapshot(root) == before, "nothing frozen, sent or written by preparing");
		auto next = tests::workspace_contract::round_drafts{ .Notes = notes, .Draft = *planned->Draft, .Revision = stored->Draft.Revision };
		expect(next.freeze(next.flushed()).Frozen.has_value() && draft.count() == 2U, "the reviewer can freeze it as F2");
		std::cout << "NATIVE: reconciliation keeps correspondence, suggestion results, claims and decisions apart; verification is explicit and becomes history; the next round is carried forward\n";
	}
}

auto main() -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "cycle-verification" };
	verifies_explicitly(scratch.path());
	std::cout << "review-cycle-verification-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle verification failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
