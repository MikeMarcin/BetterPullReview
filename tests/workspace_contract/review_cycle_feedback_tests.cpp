#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_cycle_runtime.hpp"
#include "bpr/core/sha256.hpp"
#include "cycle_course.hpp"
#include "cycle_session.hpp"
#include "workspace_fixture.hpp"

#include <array>
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
/// \brief Private cycle feedback on real captured material (RFC-BPR-0055,
/// BPR-1153): range, file, module and cycle notes with related locations,
/// verified against retained bytes, versioned, paged and reopened through the
/// production runtime and store.

namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using application::cycle_store_failure;
	using application::review_material_status;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::profile;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::session;
	using tests::workspace_contract::session_operation;
	using tests::workspace_contract::write_file;

	constexpr std::string_view parser_r1 = "int parse(const char* aText)\n{\n\treturn check(aText);\n}\n";
	constexpr std::string_view lexer_r1  = "void lex()\n{\n\tadvance();\n}\n";

	/// Every file under a root and its bytes, to prove nothing was written.
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

	auto item(const core::local_cycle_subject& aSubject, const std::uint64_t aId) -> core::cycle_feedback_item_ref
	{
		return { .Cycle = aSubject.Cycle, .Id = core::cycle_feedback_item_id{ aId } };
	}

	auto refusal(const application::cycle_store_result<application::cycle_write_receipt>& aResult) -> core::cycle_refusal
	{
		expect(!aResult && aResult.error().Failure == cycle_store_failure::refused, "refused by the core or the material check");
		return aResult.error().Refusal;
	}

	void authors_notes_on_retained_material(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto root   = aScratch / "notes";
		write_file(root / "src" / "parser.cpp", parser_r1);
		write_file(root / "src" / "lexer.cpp", lexer_r1);
		write_file(root / "docs" / "guide.md", "# Guide\n");
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");
		const auto before = snapshot(root);

		{
			auto notes = session{ cycle.StoreRoot.path(), cycle.Subject };

			// One discussion across two files, from full source, with only a body.
			const auto discussion = notes.begin({
				.Primary = notes.anchor(1, "src/parser.cpp", "check(aText)"),
				.Related = { { .Target = notes.anchor(1, "src/lexer.cpp", "advance()") } },
				.Body    = "These both validate the same input.",
			});
			expect(discussion && discussion->Allocated == 1 && discussion->Changed, "multi-file discussion");
			const auto file   = notes.begin({ .Primary = core::cycle_file_target{ core::cycle_source_revision_id{ 1 }, "docs/guide.md", tests::workspace_contract::digest_of("# Guide\n") }, .Body = "Needs an example." });
			const auto module = notes.begin({ .Primary = core::cycle_module_target{ .Path = "src" }, .Body = "Parser and lexer share too much." });
			const auto whole  = notes.begin({ .Primary = core::cycle_whole_target{ }, .Body = "Good direction overall." });
			expect(file && module && whole && whole->Allocated == 4, "file, module and cycle notes");

			// Targets are checked against the revision's retained bytes, not their claims.
			auto forged     = notes.anchor(1, "src/parser.cpp", "check(aText)");
			forged.Original = "CHECK(aText)";
			expect(refusal(notes.begin({ .Primary = forged, .Body = "x" })) == core::cycle_refusal::invalid_target, "bytes the revision does not hold");
			auto elsewhere = notes.anchor(1, "src/parser.cpp", "check(aText)");
			elsewhere.Path = "src/lexer.cpp";
			expect(refusal(notes.begin({ .Primary = elsewhere, .Body = "x" })) == core::cycle_refusal::invalid_target, "another file's digest");
			expect(refusal(notes.begin({ .Primary = core::cycle_file_target{ core::cycle_source_revision_id{ 1 }, "src/missing.cpp", tests::workspace_contract::digest_of("") }, .Body = "x" }))
					   == core::cycle_refusal::invalid_target,
				   "a path the revision does not hold");
			// Bytes only the live workspace holds were never captured.
			write_file(root / "src" / "parser.cpp", "int parse(const char* aText)\n{\n\treturn verify(aText);\n}\n");
			const auto live = core::make_anchor(core::cycle_source_revision_id{ 1 }, "src/parser.cpp", tests::workspace_contract::bytes_of(read_file(root / "src" / "parser.cpp")), 39, 52);
			expect(live && refusal(notes.begin({ .Primary = *live, .Body = "x" })) == core::cycle_refusal::invalid_target, "uncaptured live bytes");
			write_file(root / "src" / "parser.cpp", parser_r1);

			// A retried operation replays; reusing its identity for other content is refused.
			const auto expected = notes.state().Revision;
			const auto first    = notes.begin_as(session_operation(90), expected, { .Primary = core::cycle_whole_target{ }, .Body = "Once." });
			const auto replay   = notes.begin_as(session_operation(90), expected, { .Primary = core::cycle_whole_target{ }, .Body = "Once." });
			expect(first && replay && replay->Replayed && replay->Allocated == first->Allocated, "retry replays");
			const auto reused = notes.begin_as(session_operation(90), expected, { .Primary = core::cycle_whole_target{ }, .Body = "Different." });
			expect(!reused && reused.error().Failure == cycle_store_failure::operation_reused, "operation identity reused");
			const auto stale = notes.begin_as(session_operation(91), expected, { .Primary = core::cycle_whole_target{ }, .Body = "Stale." });
			expect(!stale && stale.error().Failure == cycle_store_failure::conflict, "stale cycle revision");

			// The conversation: an observed adapter reply, the reviewer's adoption, a pasted claim.
			const auto thread = item(cycle.Subject, 1);
			auto       agent  = core::cycle_feedback_author{ .Provenance = core::authored_provenance::adapter_observed, .Adapter = "codex-cli", .Model = "gpt-6" };
			const auto reply  = notes.change(thread, 1, core::append_feedback_turn{ .Author = agent, .Body = "Because the", .Completion = core::private_revision_completion::incomplete, .ReplyTo = core::private_review_turn_revision_id{ 1 } });
			expect(reply && reply->Revision == 2 && reply->Allocated == 2, "observed reply");
			expect(refusal(notes.change(thread, 2, core::edit_feedback_turn{ .Latest = core::private_review_turn_revision_id{ 2 }, .Body = "Because input is untrusted." })) == core::cycle_refusal::incomplete_source, "adoption is explicit");
			const auto adopted = notes.change(thread, 2, core::edit_feedback_turn{ .Latest = core::private_review_turn_revision_id{ 2 }, .Body = "Because input is untrusted.", .AdoptIncomplete = true });
			expect(adopted && adopted->Allocated == 3, "adopted");
			const auto oldEdit = notes.change(thread, 3, core::edit_feedback_turn{ .Latest = core::private_review_turn_revision_id{ 2 }, .Body = "stale" });
			expect(!oldEdit && oldEdit.error().Failure == cycle_store_failure::conflict, "editing a superseded revision is stale");
			const auto pasted = notes.change(thread, 3, core::append_feedback_turn{ .Author = { .Provenance = core::authored_provenance::self_reported, .Model = "says gpt-6" }, .Body = "Pasted." });
			expect(pasted && pasted->Revision == 4, "self-reported");

			const auto history = notes.revisions(thread);
			expect(history.Revisions.size() == 4 && !history.Next, "four revisions");
			const auto& edited = history.Revisions[2];
			expect(edited.Author.Provenance == core::authored_provenance::human && edited.Predecessor == history.Revisions[1].Identity, "adoption lineage");
			expect(edited.EditDerivation == core::private_derivation_kind::adopted_from_incomplete_run && edited.ReplyTo == history.Revisions[0].Identity, "derivation and reply kept");
			expect(history.Revisions[1].Author == agent && history.Revisions[1].Completion == core::private_revision_completion::incomplete, "adapter observation kept");
			expect(history.Revisions[3].Author.Provenance == core::authored_provenance::self_reported && !history.Revisions[3].Author.Adapter, "claim kept as a claim");
		}
		expect(snapshot(root) == before, "authoring never writes the workspace");
		std::cout << "NATIVE: range, file, module and cycle notes verified against retained bytes; lineage kept\n";
	}

	void observes_correspondence_and_rebinds(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto root   = aScratch / "correspond";
		write_file(root / "src" / "parser.cpp", parser_r1);
		write_file(root / "src" / "lexer.cpp", lexer_r1);
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");
		// R2: the snippet moves down; the lexer is unchanged.
		write_file(root / "src" / "parser.cpp", std::string{ "// header\n" } + std::string{ parser_r1 });
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R2");
		// R3: the snippet is copied into new context.
		write_file(root / "src" / "parser.cpp", "a(){ return check(aText); }\nb(){ return check(aText); }\n");
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R3");
		// R4: the file is deleted.
		std::filesystem::remove(root / "src" / "parser.cpp");
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R4");

		auto       notes    = session{ cycle.StoreRoot.path(), cycle.Subject };
		const auto original = notes.anchor(1, "src/parser.cpp", "check(aText)");
		// A note on historical source, made after newer revisions exist.
		const auto begun = notes.begin({ .Primary = original, .Body = "Historical note." });
		expect(begun && begun->Changed, "comment on an older revision");
		// And on a file unchanged between R1 and R4: a note needs no diff hunk.
		const auto unchanged = notes.begin({ .Primary = notes.anchor(4, "src/lexer.cpp", "advance()"), .Body = "Unchanged code." });
		expect(unchanged && unchanged->Changed, "comment on unchanged code");

		const auto kind = [&](const std::uint64_t aAgainst) {
			const auto observed = notes.observe(original, aAgainst);
			expect(observed.Correspondence.has_value(), "anchor correspondence");
			return *observed.Correspondence;
		};
		expect(kind(1).Kind == core::cycle_correspondence::exact_material, "same revision");
		const auto moved = kind(2);
		expect(moved.Kind == core::cycle_correspondence::exact_mapped && moved.Range == std::pair<std::uint64_t, std::uint64_t>{ 49, 61 }, "moved down");
		const auto repeated = kind(3);
		expect(repeated.Kind == core::cycle_correspondence::ambiguous && !repeated.Range && repeated.Candidates == 2, "repeated text is not chosen");
		expect(kind(4).Kind == core::cycle_correspondence::deleted, "deleted");
		const auto lexer = notes.observe(core::cycle_file_target{ core::cycle_source_revision_id{ 1 }, "src/lexer.cpp", tests::workspace_contract::digest_of(lexer_r1) }, 4);
		expect(lexer.Correspondence && lexer.Correspondence->Kind == core::cycle_correspondence::exact_material, "unchanged file");

		// Rebinding adds a reference to the R2 bytes; the R1 original is untouched.
		const auto thread = item(cycle.Subject, 1);
		const auto rebind = notes.change(thread, 1, core::add_feedback_reference{ { .Target = notes.anchor(2, "src/parser.cpp", "check(aText)"), .Rebinds = 0 } });
		expect(rebind && rebind->Revision == 2 && rebind->Allocated == 0, "rebound");
		auto forged  = notes.anchor(2, "src/parser.cpp", "check(aText)");
		forged.Start = 48;
		forged.End   = 60;
		expect(refusal(notes.change(thread, 2, core::add_feedback_reference{ { .Target = forged } })) == core::cycle_refusal::invalid_target, "a reference is verified too");
		const auto page = notes.threads();
		expect(page.Threads.size() == 2 && page.Threads[0].Primary == core::cycle_feedback_target{ original } && page.Threads[0].Related.size() == 1, "original kept beside the rebinding");
		expect(page.Threads[0].Related[0].Rebinds == 0U, "rebinding recorded");
		std::cout << "NATIVE: exact, mapped, ambiguous and deleted correspondence observed from retained material\n";
	}

	void pages_and_reopens(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto root   = aScratch / "pages";
		write_file(root / "a.txt", "a\n");
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");
		{
			auto notes = session{ cycle.StoreRoot.path(), cycle.Subject };
			for (auto index = 0; index < 70; ++index) {
				expect(notes.begin({ .Primary = core::cycle_whole_target{ }, .Body = "note " + std::to_string(index) }).has_value(), "note");
			}
			// A long thread: 70 short turns, then large observed answers.
			const auto thread  = item(cycle.Subject, 1);
			auto       version = std::uint64_t{ 1 };
			for (auto index = 0; index < 69; ++index) {
				const auto appended = notes.change(thread, version, core::append_feedback_turn{ .Author = { }, .Body = "turn " + std::to_string(index) });
				expect(appended.has_value(), "turn");
				version = appended->Revision;
			}
			const auto large = core::cycle_feedback_author{ .Provenance = core::authored_provenance::adapter_observed, .Adapter = "fixture" };
			for (auto index = 0; index < 4; ++index) {
				const auto appended = notes.change(thread, version, core::append_feedback_turn{ .Author = large, .Body = std::string(200U * 1024U, static_cast<char>('a' + index)) });
				expect(appended.has_value(), "large answer");
				version = appended->Revision;
			}
		}
		// Reopen through a new store object and runtime.
		auto       notes = session{ cycle.StoreRoot.path(), cycle.Subject };
		const auto first = notes.threads();
		expect(first.Threads.size() == application::maximum_cycle_page_entries && first.Next == core::cycle_feedback_item_id{ 64 }, "first thread page");
		const auto second = notes.threads(first.Next);
		expect(second.Threads.size() == 6 && !second.Next && second.Threads.back().Id.Id == core::cycle_feedback_item_id{ 70 }, "second thread page");

		const auto thread = item(cycle.Subject, 1);
		auto       seen   = std::vector<core::cycle_feedback_revision>{ };
		auto       after  = std::optional<core::private_review_turn_revision_id>{ };
		auto       pages  = 0;
		do {
			auto page = notes.revisions(thread, after);
			expect(!page.Revisions.empty(), "a page holds at least one revision");
			auto bytes = std::size_t{ 0 };
			for (std::size_t index = 0; index + 1U < page.Revisions.size(); ++index) {
				bytes += page.Revisions[index].Body.size();
			}
			expect(bytes < application::maximum_cycle_feedback_page_bytes && page.Revisions.size() <= application::maximum_cycle_page_entries, "page bounds");
			seen.insert(seen.end(), page.Revisions.begin(), page.Revisions.end());
			after = page.Next;
			++pages;
		} while (after);
		expect(seen.size() == 74 && pages >= 3, "every revision, across bounded pages");
		for (std::size_t index = 0; index < seen.size(); ++index) {
			expect(seen[index].Identity.Position.Revision.value() == index + 1U && core::valid(seen[index]), "ordered, valid revisions");
		}
		expect(seen.back().Body == std::string(200U * 1024U, 'd'), "exact bytes after reopen");

		// Removal counts and deletes the cycle's feedback.
		auto       store   = adapters::sqlite::make_review_cycle_store(cycle.StoreRoot.path(), profile);
		const auto preview = store->removal_preview(cycle.Subject, { });
		expect(preview && preview->Feedback == 70, "removal preview counts feedback");
		expect(store->remove(session_operation(90), *preview, { }).has_value(), "removed");
		std::cout << "NATIVE: 70 threads and 74 revisions paged within bounds after reopen\n";
	}

	void refuses_a_pull_request_selection(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto root   = aScratch / "refuse";
		write_file(root / "a.txt", "a\n");
		auto cycle = course{ *source, root };
		auto notes = session{ cycle.StoreRoot.path(), cycle.Subject };
		notes.Runtime.select(core::pull_request_authority{ });
		const auto begun = notes.run<application::cycle_write_receipt>([&](auto aDone) {
			return notes.Runtime.begin_feedback_selected(session_operation(1), 1, { .Primary = core::cycle_whole_target{ }, .Body = "x" }, std::move(aDone));
		});
		expect(!begun && begun.error().Refusal == core::cycle_refusal::invalid_identity, "a pull request is not a cycle");
	}
}

auto main() -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "cycle-feedback" };
	authors_notes_on_retained_material(scratch.path());
	observes_correspondence_and_rebinds(scratch.path());
	pages_and_reopens(scratch.path());
	refuses_a_pull_request_selection(scratch.path());
	std::cout << "review-cycle-feedback-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle feedback failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
