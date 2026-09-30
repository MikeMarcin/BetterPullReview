#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_response.hpp"
#include "bpr/core/review_cycle_response.hpp"
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
#include <utility>
#include <vector>

/// \file
/// \brief Recipient statements appended to a frozen round (RFC-BPR-0057,
/// BPR-1164) through the production runtime and store: a structured response
/// and hand-entered prose from several recipients, idempotent replay,
/// conflicting identifier reuse, wrong cycle, round, digest and item, forged
/// authority, inert evidence, late output after the selection changed, and
/// proof that appending touches no source, revision, note or round.

namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using application::cycle_response_result;
	using application::cycle_store_failure;
	using core::cycle_response_problem;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::session;
	using tests::workspace_contract::session_operation;
	using tests::workspace_contract::write_file;

	constexpr std::string_view parser = "int parse(const char* aText)\n{\n\treturn check(aText);\n}\n";

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

	auto bytes(const std::string_view aText) -> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	auto import(session& aNotes, const std::uint64_t aRound, const std::string& aDocument, const std::optional<application::cycle_operation_id> aOperation = std::nullopt)
		-> application::cycle_store_result<cycle_response_result>
	{
		const auto operation = aOperation.value_or(session_operation(aNotes.NextOperation++));
		return aNotes.run<cycle_response_result>([&](auto aDone) {
			return aNotes.Runtime.import_response_selected(operation, core::cycle_round_id{ aRound }, [document = aDocument](std::stop_token) -> std::expected<std::vector<std::uint8_t>, application::cycle_export_failure> { return bytes(document); }, std::move(aDone));
		});
	}

	auto reply(session& aNotes, const std::uint64_t aRound, core::cycle_manual_reply aReply) -> application::cycle_store_result<cycle_response_result>
	{
		return aNotes.run<cycle_response_result>([&](auto aDone) { return aNotes.Runtime.attach_reply_selected(session_operation(aNotes.NextOperation++), core::cycle_round_id{ aRound }, std::move(aReply), std::move(aDone)); });
	}

	auto responses(session& aNotes) -> application::cycle_response_entries
	{
		const auto read = aNotes.run<application::cycle_response_entries>([&](auto aDone) { return aNotes.Runtime.responses_selected(std::move(aDone)); });
		expect(read.has_value(), "responses read");
		return *read;
	}

	auto refusal(const application::cycle_store_result<cycle_response_result>& aResult) -> std::optional<cycle_response_problem>
	{
		expect(aResult.has_value(), "decided");
		return aResult->Refusal ? std::optional{ aResult->Refusal->Problem } : std::nullopt;
	}

	void appends_statements(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto root   = aScratch / "responses" / "parser";
		write_file(root / "src" / "parser.cpp", parser);
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");
		auto       notes  = session{ cycle.StoreRoot.path(), cycle.Subject };
		const auto first  = notes.begin({ .Primary = core::cycle_whole_target{ }, .Body = "Handle a null text." });
		const auto second = notes.begin({ .Primary = core::cycle_module_target{ .Path = "src" }, .Body = "Add a test." });
		const auto third  = notes.begin({ .Primary = core::cycle_whole_target{ }, .Body = "Why check here?" });
		expect(first && second && third, "three notes");
		auto draft = tests::workspace_contract::begin_round(notes);
		draft.save({ .Source = core::cycle_source_revision_id{ 1 }, .Goal = "Harden parse.", .Items = { { .Item = core::cycle_feedback_item_id{ 1 }, .Wording = core::private_review_turn_revision_id{ 1 } }, { .Item = core::cycle_feedback_item_id{ 2 }, .Wording = core::private_review_turn_revision_id{ 1 } }, { .Item = core::cycle_feedback_item_id{ 3 }, .Wording = core::private_review_turn_revision_id{ 1 } } } });
		const auto frozen = draft.freeze(draft.flushed());
		expect(frozen.Frozen.has_value(), "F1");
		const auto& round    = frozen.Frozen->Content;
		const auto  before   = snapshot(root);
		const auto  captures = cycle.revisions();
		const auto  threads  = notes.threads().Threads;

		// A structured response from an agent that names its own model and
		// claims tests passed, with evidence that looks like a command.
		const auto canary = aScratch / "responses" / "canary";
		auto       answer = core::cycle_response_document{
			.ResponseId     = "RESP-A",
			.CycleId        = round.Payload.CycleId,
			.RoundId        = round.Payload.RoundId,
			.RoundDigest    = round.Digest,
			.ClaimedAuthor  = { .DisplayName = "Harness", .Kind = core::cycle_claimed_author_kind::agent, .Provider = "Example", .Model = "Claude Opus (verified)", .Effort = "high" },
			.ResultRevision = "R2",
			.Entries        = {
				{ .Item = "I1", .ItemRevision = 1, .Disposition = core::cycle_response_disposition::addressed, .Body = "Null now returns 0.", .Evidence = { { .Kind = core::cycle_evidence_kind::test_report, .Value = "ctest: 100% tests passed" }, { .Kind = core::cycle_evidence_kind::text, .Value = "$(touch " + canary.string() + ")" }, { .Kind = core::cycle_evidence_kind::path, .Value = "src/parser.cpp" } } },
				{ .Item = "I2", .ItemRevision = 1, .Disposition = core::cycle_response_disposition::partially_addressed, .Body = "Started a test." },
			},
		};
		const auto document = *core::encode_response_document(answer);
		const auto once     = session_operation(9000);
		const auto appended = import(notes, 1, document, once);
		expect(appended && appended->Receipt && appended->Receipt->Changed && appended->Receipt->Allocated == 1U, "a structured response appended as statement 1");
		const auto retried = import(notes, 1, document, once);
		expect(retried && retried->Receipt && retried->Receipt->Replayed, "a retried operation replays");
		const auto replay = import(notes, 1, " " + document + "\n");
		expect(replay && replay->Receipt && !replay->Receipt->Changed && replay->Receipt->Allocated == 1U, "the same response again changes nothing");
		auto changed            = answer;
		changed.Entries[0].Body = "Null now returns -1.";
		const auto reused       = import(notes, 1, *core::encode_response_document(changed));
		expect(refusal(reused) == cycle_response_problem::reused_identifier, "the same identifier with other content conflicts");

		// Wrong cycle, round, digest, item and revision, and forged authority, append nothing.
		auto wrong    = answer;
		wrong.CycleId = "RC-9";
		expect(refusal(import(notes, 1, *core::encode_response_document(wrong))) == cycle_response_problem::wrong_cycle, "another cycle");
		wrong         = answer;
		wrong.RoundId = "F2";
		expect(refusal(import(notes, 1, *core::encode_response_document(wrong))) == cycle_response_problem::wrong_round, "another round's name");
		expect(refusal(import(notes, 2, document)) == cycle_response_problem::wrong_round, "a round that was never frozen");
		wrong             = answer;
		wrong.RoundDigest = core::cycle_round_digest{ };
		expect(refusal(import(notes, 1, *core::encode_response_document(wrong))) == cycle_response_problem::wrong_digest, "the wrong round digest");
		wrong                 = answer;
		wrong.Entries[1].Item = "I9";
		expect(refusal(import(notes, 1, *core::encode_response_document(wrong))) == cycle_response_problem::unknown_item, "an unknown item");
		wrong                         = answer;
		wrong.Entries[1].ItemRevision = 2;
		expect(refusal(import(notes, 1, *core::encode_response_document(wrong))) == cycle_response_problem::stale_item, "another item revision");
		auto forged = document;
		forged.insert(1, "\"verified\":true,");
		const auto flag = import(notes, 1, forged);
		expect(flag && flag->Decode && flag->Decode->Problem == core::cycle_response_decode_problem::shape, "a forged verification flag is refused");
		expect(responses(notes).size() == 1U, "none of those appended anything");

		// Prose by hand from two more recipients: one about an item, one about the round.
		expect(reply(notes, 1, { .From = "Dana (chat)", .Items = { "I2" }, .Body = "I think the test belongs elsewhere." })->Receipt->Allocated == 2U, "a colleague's prose about one item");
		expect(reply(notes, 1, { .From = "Lee (email)", .Body = "Looks fine overall; did not run anything." })->Receipt->Allocated == 3U, "prose about the round as a whole");
		expect(refusal(reply(notes, 1, { .From = "Lee", .Body = "\n" })) == cycle_response_problem::empty, "a reply needs text");
		expect(refusal(reply(notes, 1, { .From = "Lee", .Items = { "I7" }, .Body = "x" })) == cycle_response_problem::unknown_item, "a reply names only round items");

		const auto all = responses(notes);
		expect(all.size() == 3U && all[0].first.Channel == core::cycle_response_channel::response_file && all[1].first.Channel == core::cycle_response_channel::manual_entry, "three statements, channels observed");
		const auto& kept = std::get<core::cycle_response_document>(all[0].second);
		expect(kept == answer && kept.ClaimedAuthor.Model == "Claude Opus (verified)", "the response is kept exactly, its claims as claims");
		const auto i2 = core::item_statements(all, "I2");
		expect(i2.size() == 2U && i2[0].Disposition == core::cycle_response_disposition::partially_addressed && i2[1].From == "Dana (chat)" && !i2[1].Disposition, "mixed recipients: both statements, neither overwriting");
		expect(core::item_statements(all, "I3").empty(), "an unanswered item has no invented response");

		// Nothing was run, written, captured, verified or rewritten.
		expect(!std::filesystem::exists(canary), "evidence text never ran");
		expect(snapshot(root) == before, "no source was written");
		expect(cycle.revisions() == captures, "no revision was captured");
		expect(notes.threads().Threads == threads, "no note, lifecycle or version changed");
		expect(draft.read(1).Content.Canonical == round.Canonical && draft.count() == 1U, "F1 unchanged, no round added");
		expect(!core::resolve_claimed_result(notes.state(), kept.ResultRevision), "a claimed result that was never captured stays a reference");
		write_file(root / "src" / "parser.cpp", "int parse(const char* aText)\n{\n\treturn aText ? check(aText) : 0;\n}\n");
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R2 captured by the reviewer, not by the response");
		expect(core::resolve_claimed_result(notes.state(), kept.ResultRevision) == core::cycle_source_revision_id{ 2 }, "once captured, the claimed result resolves");

		// Pinned content survives reclaim and a restart.
		expect(cycle.Store->reclaim({ }).has_value(), "reclaim");
		auto reopened = session{ cycle.StoreRoot.path(), cycle.Subject };
		expect(responses(reopened) == all, "statements read back exactly after a restart");

		// Output that completes after the reviewer moved to another cycle is dropped.
		const auto other = cycle.Store->create(tests::workspace_contract::operation(200), { .Authority = cycle.Catalog.Authority, .Title = "Other" }, { });
		expect(other.has_value(), "another cycle");
		auto late            = std::optional<application::cycle_store_result<cycle_response_result>>{ };
		auto lateDoc         = answer;
		lateDoc.ResponseId   = "RESP-LATE";
		const auto lateBytes = *core::encode_response_document(lateDoc);
		expect(reopened.Runtime.import_response_selected(session_operation(9100), core::cycle_round_id{ 1 }, [lateBytes](std::stop_token) -> std::expected<std::vector<std::uint8_t>, application::cycle_export_failure> { return bytes(lateBytes); }, [&](auto aValue) { late = std::move(aValue); }), "queued");
		reopened.Runtime.select(core::local_cycle_subject{ .Authority = cycle.Catalog.Authority, .Cycle = other->Cycle, .Generation = 1 });
		reopened.Lanes.drain();
		expect(late && !*late && late->error().Failure == cycle_store_failure::cancelled, "late output completes cancelled");
		reopened.Runtime.select(cycle.Subject);
		expect(responses(reopened).size() == 3U, "and appended nothing");
		std::cout << "NATIVE: structured and prose statements append to exact rounds as untrusted claims; replay, reuse, wrong targets, forged flags and late output refused\n";
	}
}

auto main() -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "cycle-responses" };
	appends_statements(scratch.path());
	std::cout << "review-cycle-response-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle response failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
