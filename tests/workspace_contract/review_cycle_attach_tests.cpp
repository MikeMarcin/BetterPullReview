#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_capture.hpp"
#include "bpr/application/review_cycle_comparison.hpp"
#include "bpr/application/review_cycle_import.hpp"
#include "bpr/core/review_cycle_bundle.hpp"
#include "cycle_course.hpp"
#include "cycle_session.hpp"
#include "product_diff.hpp"
#include "workspace_fixture.hpp"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Rounds from elsewhere attached through explicit mappings
/// (RFC-BPR-0056 and RFC-BPR-0057, BPR-1162), through the production store,
/// capture host and import services: a second profile opens a colleague's
/// bundle with no account or agent, associates it with a cycle only through
/// an explicit project and revision mapping over exactly the named bytes,
/// forks it into local notes of unknown provenance without changing the
/// sender's round, and keeps everything when a write fails. On the sender's
/// side, prose replies need no structured return before the next capture.

namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using application::cycle_store_failure;
	using core::cycle_import_problem;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::operation;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::session;
	using tests::workspace_contract::write_file;

	constexpr std::string_view header = "#pragma once\nstruct Cache {\n    void insert(int key);\n};\n";
	constexpr std::string_view body   = "#include \"cache.hpp\"\r\n\r\nvoid Cache::insert(int key) {\r\n\tentries.push_back(key);\r\n}\r\n";

	void write_project(const std::filesystem::path& aRoot)
	{
		write_file(aRoot / "include" / "cache.hpp", header);
		write_file(aRoot / "src" / "cache.cpp", body);
	}

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

	/// A profile's store holding several local cycles, each bound to a root.
	struct profile
	{
		tests::sqlite_contract::scratch_root             Root;
		std::unique_ptr<application::review_cycle_store> Store;
		core::review_cycle_catalog                       Catalog;
		std::uint8_t                                     Next = 100;

		profile(const char* aName, const std::uint64_t aProfile, const std::uint64_t aBudget = application::default_cycle_retained_budget_bytes)
			: Root{ aName }
			, Store{ adapters::sqlite::make_review_cycle_store(Root.path(), core::bpr_profile_id{ aProfile }, { .RetainedBudgetBytes = aBudget }) }
		{
			auto catalog = Store->catalog({ });
			expect(catalog.has_value(), "catalog");
			Catalog = *catalog;
		}

		auto add(application::workspace_source_port& aSource, const std::filesystem::path& aRoot, const std::string& aTitle) -> core::local_cycle_subject
		{
			const auto created = Store->create(operation(Next++), { .Authority = Catalog.Authority, .Title = aTitle, .Scope = { } }, { });
			expect(created.has_value(), "create cycle");
			const auto subject = core::local_cycle_subject{ .Authority = Catalog.Authority, .Cycle = created->Cycle, .Generation = 1 };
			auto       host    = application::review_cycle_capture_host{ aSource, *Store };
			const auto root    = host.observe(aRoot.string(), { });
			expect(root.has_value(), "observe root");
			expect(Store->apply(operation(Next++), { .Subject = subject, .ExpectedRevision = 1, .Change = core::bind_cycle_workspace{ .Root = *root } }, { }).has_value(), "bind root");
			return subject;
		}

		auto state(const core::local_cycle_subject& aSubject) -> core::review_cycle_state
		{
			const auto loaded = Store->load(aSubject, { });
			expect(loaded && *loaded, "load cycle");
			return **loaded;
		}

		void capture(application::workspace_source_port& aSource, const core::local_cycle_subject& aSubject)
		{
			auto host = application::review_cycle_capture_host{ aSource, *Store };
			expect(host.capture(state(aSubject), { .Operation = operation(Next++), .ExpectedRevision = state(aSubject).Revision }, { }).Status == cycle_capture_status::captured, "capture");
		}

		auto attach(const core::local_cycle_subject& aSubject, const core::cycle_import_id aImport, std::vector<core::workspace_import_mapping> aProjects,
					std::vector<core::cycle_revision_mapping> aRevisions) -> application::cycle_import_attach_result
		{
			auto       host   = application::review_cycle_comparison_host{ *Store, Catalog.Authority, bpr::tests::product_line_matcher() };
			const auto result = application::attach_imported_round(*Store, host, aSubject, { .Operation = operation(Next++), .Import = aImport, .Projects = std::move(aProjects), .Revisions = std::move(aRevisions) }, { });
			expect(result.has_value(), "attach decided");
			return *result;
		}

		auto fork(const core::local_cycle_subject& aSubject, const core::cycle_import_id aImport) -> application::cycle_import_fork_result
		{
			auto       host   = application::review_cycle_comparison_host{ *Store, Catalog.Authority, bpr::tests::product_line_matcher() };
			const auto result = application::fork_imported_round(*Store, host, aSubject, operation(Next++), aImport, { });
			expect(result.has_value(), "fork decided");
			return *result;
		}
	};

	auto problem_of(const application::cycle_import_attach_result& aResult) -> std::optional<cycle_import_problem>
	{
		return aResult.Refusal ? std::optional{ aResult.Refusal->Problem } : std::nullopt;
	}

	void attaches_through_explicit_mappings(const std::filesystem::path& aScratch)
	{
		auto source = adapters::workspace::make_local_workspace_source();

		// The sender freezes a round over a project and shares it as a bundle.
		const auto senderRoot = aScratch / "sender" / "cache";
		write_project(senderRoot);
		auto sender = course{ *source, senderRoot };
		expect(sender.capture(*source).Status == cycle_capture_status::captured, "sender R1");
		auto       notes  = session{ sender.StoreRoot.path(), sender.Subject };
		const auto range  = notes.begin({ .Primary = notes.anchor(1, "src/cache.cpp", "\tentries.push_back(key);\r\n"), .Body = "issue: Duplicates." });
		const auto file   = notes.begin({ .Primary = core::cycle_file_target{ core::cycle_source_revision_id{ 1 }, "include/cache.hpp", tests::workspace_contract::digest_of(header) }, .Body = "todo: Return bool." });
		const auto module = notes.begin({ .Primary = core::cycle_module_target{ .Path = "src" }, .Body = "note: Keep CRLF." });
		const auto whole  = notes.begin({ .Primary = core::cycle_whole_target{ }, .Body = "question: Thread safety?" });
		expect(range && file && module && whole, "four notes");
		auto draft = tests::workspace_contract::begin_round(notes);
		auto items = std::vector<core::cycle_round_item_selection>{ };
		for (const auto* note : { &range, &file, &module, &whole }) {
			items.push_back({ .Item = core::cycle_feedback_item_id{ (*note)->Allocated }, .Wording = core::private_review_turn_revision_id{ 1 } });
		}
		draft.save({ .Source = core::cycle_source_revision_id{ 1 }, .Goal = "Idempotent insert.", .Items = items, .Context = { { .Revision = core::cycle_source_revision_id{ 1 }, .Path = "src/cache.cpp" } } });
		const auto frozen = draft.freeze(draft.flushed());
		expect(frozen.Frozen.has_value(), "sender F1");
		const auto bundle = core::build_round_bundle(frozen.Frozen->Content);
		const auto bytes  = std::vector<std::uint8_t>(bundle.begin(), bundle.end());

		// The recipient has two projects titled "cache" and one whose root is gone.
		const auto first  = aScratch / "recipient" / "a" / "cache";
		const auto second = aScratch / "recipient" / "b" / "cache";
		const auto gone   = aScratch / "recipient" / "gone" / "cache";
		write_project(first);
		write_project(second);
		write_project(gone);
		auto       recipient = profile{ "cycle-attach-recipient", 9 };
		const auto a         = recipient.add(*source, first, "cache");
		const auto b         = recipient.add(*source, second, "cache");
		const auto c         = recipient.add(*source, gone, "cache");
		recipient.capture(*source, a);
		recipient.capture(*source, b);
		std::filesystem::remove_all(gone);
		// A second capture of the first project, with the file changed.
		write_file(first / "src" / "cache.cpp", std::string{ body } + "// local change\r\n");
		recipient.capture(*source, a);
		write_file(first / "src" / "cache.cpp", body);
		const auto folders = snapshot(aScratch / "recipient");

		const auto imported = application::import_round_bundle(*recipient.Store, operation(1), bytes, { });
		expect(imported && imported->Import && !imported->Import->Association, "opened into quarantine, unattached");
		const auto id      = imported->Import->Summary.Id;
		const auto foreign = imported->Import->Summary.ForeignProject;
		const auto toA     = core::workspace_import_mapping{ .Imported = core::imported_project_key(foreign), .Local = recipient.state(a).Workspace->Project };
		const auto toB     = core::workspace_import_mapping{ .Imported = core::imported_project_key(foreign), .Local = recipient.state(b).Workspace->Project };
		const auto toC     = core::workspace_import_mapping{ .Imported = core::imported_project_key(foreign), .Local = recipient.state(c).Workspace->Project };
		const auto r1      = std::vector{ core::cycle_revision_mapping{ .Foreign = "R1", .Local = core::cycle_source_revision_id{ 1 } } };

		expect(problem_of(recipient.attach(b, id, { }, r1)) == cycle_import_problem::unmapped_project, "no mapping, no association");
		expect(problem_of(recipient.attach(b, id, { toA }, r1)) == cycle_import_problem::unmapped_project, "same alias, another project");
		expect(problem_of(recipient.attach(c, id, { toC }, r1)) == cycle_import_problem::invalid_mapping, "a project whose root is gone and holds no capture");
		const auto mismatched = recipient.attach(a, id, { toA }, { { .Foreign = "R1", .Local = core::cycle_source_revision_id{ 2 } } });
		expect(problem_of(mismatched) == cycle_import_problem::mismatched_source && mismatched.Refusal->Detail == "src/cache.cpp", "mapping to mismatched source names the file");
		expect(!recipient.Store->load_import(id, { }).value()->Association, "a refused mapping records nothing");

		const auto attached = recipient.attach(a, id, { toA }, r1);
		expect(attached.Association && attached.Changed && attached.Association->Cycle == a, "associated with project a");
		expect(!recipient.attach(a, id, { toA }, r1).Changed, "the same association again changes nothing");
		const auto settled = recipient.Store->load_import(id, { });
		expect(settled && *settled, "record read");
		expect(problem_of(recipient.attach(b, id, { toB }, r1)) == cycle_import_problem::already_associated, "never re-associated silently");

		// A fork starts local notes; the sender's round and names never change.
		expect(recipient.fork(b, id).Refusal->Problem == cycle_import_problem::not_associated, "a fork needs the association");
		const auto forked = recipient.fork(a, id);
		expect(forked.Notes.size() == 4U && !forked.Existing && forked.Notes.front().ForeignItem == "I1", "one local note per item, by the sender's names");
		const auto threads = recipient.Store->feedback(a, std::nullopt, { });
		expect(threads && threads->Threads.size() == 4U, "four notes in project a");
		for (const auto& fork : forked.Notes) {
			const auto history = recipient.Store->feedback_revisions(a, fork.Item, std::nullopt, { });
			expect(history && history->Revisions.size() == 1U && history->Revisions.front().Author.Provenance == core::authored_provenance::unknown, "unknown provenance, never the reviewer's");
		}
		const auto primary = std::get_if<core::cycle_source_anchor>(&threads->Threads.front().Primary);
		expect(primary && primary->Revision == core::cycle_source_revision_id{ 1 } && primary->Original == "\tentries.push_back(key);\r\n", "range anchored on the mapped local bytes");
		expect(recipient.fork(a, id).Existing && recipient.Store->feedback(a, std::nullopt, { })->Threads.size() == 4U, "a repeated fork adds nothing");
		const auto edit = recipient.Store->change_feedback(operation(2), a, forked.Notes.front().Item, 1,
														   core::edit_feedback_turn{ .Latest = core::private_review_turn_revision_id{ 1 }, .Body = "issue: Duplicates; also update the caller." }, { });
		expect(edit && edit->Allocated == 2U, "the reviewer edits the fork");
		const auto kept = application::read_imported_round(*recipient.Store, id, { });
		expect(kept && kept->Content.Canonical == frozen.Frozen->Content.Canonical && kept->Record.Summary.ForeignCycle == "RC-1" && kept->Record.Summary.ForeignRound == "F1",
			   "the sender's round and identities are unchanged");
		expect(recipient.Store->import_forks(id, { })->size() == 4U, "fork provenance recorded");
		// The quarantined round is read-only: every refused change leaves it exactly as it was.
		const auto preserved = recipient.Store->load_import(id, { });
		expect(preserved && *preserved && **preserved == **settled, "refused re-association and forks preserve the record");
		expect(snapshot(aScratch / "recipient") == folders, "no bundle writes, applies or runs anything in a project");

		// Removing the cycle returns the round to quarantine, unattached and intact.
		{
			auto       closing = profile{ "cycle-attach-removal", 13 };
			const auto d       = closing.add(*source, first, "cache");
			closing.capture(*source, d);
			const auto again = application::import_round_bundle(*closing.Store, operation(1), bytes, { });
			expect(again && again->Import, "imported");
			const auto mapping = core::workspace_import_mapping{ .Imported = core::imported_project_key(foreign), .Local = closing.state(d).Workspace->Project };
			expect(closing.attach(d, again->Import->Summary.Id, { mapping }, r1).Association.has_value() && closing.fork(d, again->Import->Summary.Id).Notes.size() == 4U, "attached and forked");
			const auto preview = closing.Store->removal_preview(d, { });
			expect(preview && closing.Store->remove(operation(2), *preview, { }).has_value(), "cycle removed");
			const auto released = closing.Store->load_import(again->Import->Summary.Id, { });
			expect(released && *released && !(*released)->Association && closing.Store->import_forks(again->Import->Summary.Id, { })->empty(), "association and forks released");
			expect(application::read_imported_round(*closing.Store, again->Import->Summary.Id, { }).has_value(), "the quarantined round stays readable");
		}

		// Another profile opens the same bundle independently.
		auto       third       = profile{ "cycle-attach-third", 11 };
		const auto independent = application::import_round_bundle(*third.Store, operation(1), bytes, { });
		expect(independent && independent->Import && independent->Import->Summary.Id == core::cycle_import_id{ 1 } && !independent->Import->Association, "a second profile's own import");
		expect(recipient.Store->imports(std::nullopt, { })->Imports.size() == 1U, "the first profile is unaffected");

		// A failed write admits nothing.
		auto       cramped = profile{ "cycle-attach-quota", 12, 64U };
		const auto quota   = application::import_round_bundle(*cramped.Store, operation(1), bytes, { });
		expect(!quota && quota.error().Failure == cycle_store_failure::quota && cramped.Store->imports(std::nullopt, { })->Imports.empty(), "over quota admits nothing");
		std::cout << "NATIVE: an imported round attaches only through explicit mappings over exact bytes and forks without changing the sender's round\n";
	}

	void continues_from_prose_alone(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto root   = aScratch / "prose" / "cache";
		write_project(root);
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");
		auto       notes  = session{ cycle.StoreRoot.path(), cycle.Subject };
		const auto first  = notes.begin({ .Primary = core::cycle_whole_target{ }, .Body = "issue: Duplicates." });
		const auto second = notes.begin({ .Primary = core::cycle_module_target{ .Path = "src" }, .Body = "todo: Tests." });
		const auto third  = notes.begin({ .Primary = core::cycle_whole_target{ }, .Body = "question: Threads?" });
		expect(first && second && third, "notes");
		auto draft = tests::workspace_contract::begin_round(notes);
		draft.save({ .Source = core::cycle_source_revision_id{ 1 }, .Goal = "Fix it.", .Items = { { .Item = core::cycle_feedback_item_id{ first->Allocated }, .Wording = core::private_review_turn_revision_id{ 1 } }, { .Item = core::cycle_feedback_item_id{ second->Allocated }, .Wording = core::private_review_turn_revision_id{ 1 } }, { .Item = core::cycle_feedback_item_id{ third->Allocated }, .Wording = core::private_review_turn_revision_id{ 1 } } } });
		const auto frozen = draft.freeze(draft.flushed());
		expect(frozen.Frozen.has_value(), "F1");

		// Replies arrive as plain prose from two recipients, covering only some items.
		const auto ref       = [&](const std::uint64_t aItem) { return core::cycle_feedback_item_ref{ .Cycle = cycle.Subject.Cycle, .Id = core::cycle_feedback_item_id{ aItem } }; };
		const auto colleague = notes.change(ref(first->Allocated), 1, core::append_feedback_turn{ .Author = { .Provenance = core::authored_provenance::unknown }, .Body = "Fixed the duplicate insert; didn't touch tests." });
		const auto agent     = notes.change(ref(second->Allocated), 1, core::append_feedback_turn{ .Author = { .Provenance = core::authored_provenance::self_reported, .Model = "some-model" }, .Body = "I added tests and verified everything." });
		expect(colleague && agent, "prose attached by hand, no JSON");
		// The recipient changed files with ordinary tools; the next capture needs nothing more.
		write_file(root / "src" / "cache.cpp", std::string{ body } + "// fixed\r\n");
		const auto next = cycle.capture(*source);
		expect(next.Status == cycle_capture_status::captured && cycle.state().Latest->Id.Id == core::cycle_source_revision_id{ 2 }, "R2 captured with no structured response");
		const auto unanswered = notes.revisions(ref(third->Allocated));
		expect(unanswered.Revisions.size() == 1U, "an item nobody answered stays as written");
		const auto claimed = notes.revisions(ref(second->Allocated));
		expect(claimed.Revisions.size() == 2U && claimed.Revisions.back().Author.Provenance == core::authored_provenance::self_reported, "a claimed verification stays self-reported");
		expect(draft.read(1).Content.Canonical == frozen.Frozen->Content.Canonical, "F1 unchanged by replies or capture");
		std::cout << "NATIVE: prose-only, partial, mixed-recipient replies attach by hand and R2 is captured without any structured return\n";
	}
}

auto main() -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "cycle-attach" };
	attaches_through_explicit_mappings(scratch.path());
	continues_from_prose_alone(scratch.path());
	std::cout << "review-cycle-attach-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle attach failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
