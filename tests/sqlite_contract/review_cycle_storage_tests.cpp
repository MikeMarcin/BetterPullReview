#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/core/sha256.hpp"
#include "scratch_root.hpp"

#include <sqlite3.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <variant>
#include <vector>

#if !defined(_WIN32)
	#include <csignal>
	#include <sys/wait.h>
	#include <unistd.h>
#endif

namespace {
	using namespace bpr;
	using application::cycle_store_failure;
	using tests::expect;

	constexpr auto profile = core::bpr_profile_id{ 7 };

	auto operation(const std::uint8_t aSeed) -> application::cycle_operation_id
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[31]  = aSeed;
		bytes[0]   = 0xC7;
		return application::cycle_operation_id{ bytes };
	}

	auto bytes_of(const std::string_view aText) -> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	auto database_path(const std::filesystem::path& aRoot) -> std::filesystem::path
	{
		return aRoot / "state" / "v1" / "review-cycles.sqlite3";
	}

	void sql(const std::filesystem::path& aPath, const char* aSql)
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open(aPath.string().c_str(), &database) == SQLITE_OK, "open owned fixture");
		expect(sqlite3_exec(database, aSql, nullptr, nullptr, nullptr) == SQLITE_OK, "owned fixture SQL");
		sqlite3_close(database);
	}

	auto file_bytes(const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream input{ aPath, std::ios::binary };
		return { std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
	}

	struct fixture
	{
		std::unique_ptr<application::review_cycle_store> Store;
		core::review_cycle_catalog                       Catalog;

		auto create(const std::uint8_t aOperation, const std::string& aTitle = "Parser") -> core::local_cycle_subject
		{
			const auto receipt = Store->create(operation(aOperation), { .Authority = Catalog.Authority, .Title = aTitle, .Scope = { } }, { });
			expect(receipt && receipt->Changed && !receipt->Replayed, "create cycle");
			return { .Authority = Catalog.Authority, .Cycle = receipt->Cycle, .Generation = 1 };
		}
		auto state(const core::local_cycle_subject& aSubject) -> core::review_cycle_state
		{
			auto loaded = Store->load(aSubject, { });
			expect(loaded && *loaded, "load cycle");
			return **loaded;
		}
		auto capture(const std::uint8_t aOperation, const core::local_cycle_subject& aSubject, const std::string_view aManifest, const std::uint64_t aEntries)
			-> application::cycle_store_result<application::cycle_write_receipt>
		{
			const auto object = Store->install(bytes_of(aManifest), { });
			expect(object.has_value(), "install manifest");
			const auto current = state(aSubject);
			return Store->record_capture(operation(aOperation), {
																	.Subject          = aSubject,
																	.ExpectedRevision = current.Revision,
																	.Capture          = { .Capture = { .ScopeVersion = current.Scope.Version, .Material = object->Digest, .Entries = aEntries, .Coverage = core::cycle_capture_coverage::complete } },
																	.Manifest         = *object,
																},
										 { });
		}
	};

	auto open(const std::filesystem::path& aRoot, const std::uint64_t aBudget = application::default_cycle_retained_budget_bytes) -> fixture
	{
		auto store   = adapters::sqlite::make_review_cycle_store(aRoot, profile, { .RetainedBudgetBytes = aBudget });
		auto catalog = store->catalog({ });
		expect(catalog.has_value(), "open cycle catalog");
		return { std::move(store), *catalog };
	}

	void creates_and_reopens_a_provider_free_cycle()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-reopen" };
		auto                                 first = open(scratch.path());
		expect(first.Catalog.Authority.Profile == profile && first.Catalog.Authority.Incarnation.valid() && first.Catalog.NextCycle == 1, "fresh catalog");
		const auto subject = first.create(1);
		expect(subject.Cycle == core::review_cycle_id{ 1 }, "first cycle id");
		expect(!core::project(first.state(subject)).Captured, "uncaptured before R0");
		const auto empty = first.capture(2, subject, "", 0);
		expect(empty && empty->Allocated == 1 && empty->Changed, "empty R0 capture");
		first.Store.reset();

		auto       reopened = open(scratch.path());
		const auto view     = core::project(reopened.state(subject));
		expect(reopened.Catalog.Authority == first.Catalog.Authority && reopened.Catalog.NextCycle == 2, "same incarnation after reopen");
		expect(view.Captured && view.InitialEmpty && view.Revisions == 1 && view.Title == "Parser", "reopened empty R0");
		const auto revisions = reopened.Store->revisions(subject, std::nullopt, { });
		expect(revisions && revisions->Revisions.size() == 1 && revisions->Revisions.front().Manifest.Size == 0, "revision history page");
		const auto manifest = reopened.Store->read_object(revisions->Revisions.front().Manifest, { });
		expect(manifest && manifest->empty(), "empty manifest object reads back");
		const auto page = reopened.Store->list(std::nullopt, { });
		expect(page && page->Cycles.size() == 1 && !page->Next, "list page");

		auto other = adapters::sqlite::make_review_cycle_store(scratch.path(), core::bpr_profile_id{ 8 });
		expect(other->catalog({ }).error().Failure == cycle_store_failure::wrong_profile, "another profile cannot open this catalog");
		auto foreign                  = subject;
		foreign.Authority.Incarnation = core::cycle_catalog_incarnation{ std::array<std::uint8_t, 32>{ 1 } };
		const auto refused            = reopened.Store->apply(operation(3), { .Subject = foreign, .ExpectedRevision = 2, .Change = core::archive_review_cycle{ } }, { });
		expect(!refused && refused.error().Refusal == core::cycle_refusal::invalid_identity, "foreign incarnation refused");
	}

	void deduplicates_operations_and_rejects_stale_writes()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-receipts" };
		auto                                 store = open(scratch.path());
		const auto                           first = store.Store->create(operation(1), { .Authority = store.Catalog.Authority, .Title = "One", .Scope = { } }, { });
		const auto                           again = store.Store->create(operation(1), { .Authority = store.Catalog.Authority, .Title = "One", .Scope = { } }, { });
		expect(first && again && again->Replayed && again->Allocated == first->Allocated, "identical retry returns the first result");
		const auto reused = store.Store->create(operation(1), { .Authority = store.Catalog.Authority, .Title = "Two", .Scope = { } }, { });
		expect(!reused && reused.error().Failure == cycle_store_failure::operation_reused, "same operation, different request");
		const auto second = store.Store->create(operation(2), { .Authority = store.Catalog.Authority, .Title = "Two", .Scope = { } }, { });
		expect(second && second->Allocated == 2, "new operation allocates the next identity");

		const auto subject = core::local_cycle_subject{ .Authority = store.Catalog.Authority, .Cycle = first->Cycle, .Generation = 1 };
		const auto renamed = store.Store->apply(operation(3), { .Subject = subject, .ExpectedRevision = 1, .Change = core::rename_review_cycle{ "Renamed" } }, { });
		expect(renamed && renamed->Revision == 2, "rename");
		const auto stale = store.Store->apply(operation(4), { .Subject = subject, .ExpectedRevision = 1, .Change = core::rename_review_cycle{ "Late" } }, { });
		expect(!stale && stale.error().Failure == cycle_store_failure::conflict && stale.error().Current && stale.error().Current->Title == "Renamed", "stale write refused with current state");
		const auto retry = store.Store->apply(operation(4), { .Subject = subject, .ExpectedRevision = 2, .Change = core::rename_review_cycle{ "Late" } }, { });
		expect(retry && !retry->Replayed, "a refused request left no receipt");
		expect(store.state(subject).Title == "Late", "retry applied once");
		const auto bounds = store.Store->create(operation(5), { .Authority = store.Catalog.Authority, .Title = "", .Scope = { } }, { });
		expect(!bounds && bounds.error().Refusal == core::cycle_refusal::bounds, "core refusal surfaces");
		expect(store.Store->create(application::cycle_operation_id{ }, { .Authority = store.Catalog.Authority, .Title = "x", .Scope = { } }, { }).error().Failure == cycle_store_failure::bounds, "zero operation id");
		auto stop = std::stop_source{ };
		stop.request_stop();
		expect(store.Store->catalog(stop.get_token()).error().Failure == cycle_store_failure::cancelled, "cancelled before opening");
	}

	void refuses_captures_without_their_objects()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-objects" };
		auto                                 store   = open(scratch.path());
		const auto                           subject = store.create(1);
		const auto                           missing = application::cycle_object_ref{ .Digest = core::cycle_material_digest{ core::sha256(bytes_of("never installed")) }, .Size = 15 };
		auto                                 commit  = application::cycle_capture_commit{
			.Subject          = subject,
			.ExpectedRevision = 1,
			.Capture          = { .Capture = { .ScopeVersion = 1, .Material = missing.Digest, .Entries = 1, .Coverage = core::cycle_capture_coverage::complete } },
			.Manifest         = missing,
		};
		expect(store.Store->record_capture(operation(2), commit, { }).error().Failure == cycle_store_failure::object_unavailable, "capture refuses a missing manifest");
		commit.Manifest.Digest = core::cycle_material_digest{ core::sha256(bytes_of("other")) };
		expect(store.Store->record_capture(operation(3), commit, { }).error().Refusal == core::cycle_refusal::invalid_capture, "manifest must be the capture's material");
		expect(!core::project(store.state(subject)).Captured, "nothing recorded");

		const auto object = store.Store->install(bytes_of("manifest bytes"), { });
		expect(object && store.Store->install(bytes_of("manifest bytes"), { }).value_or(application::cycle_object_ref{ }) == *object, "install is idempotent by content");
		sql(database_path(scratch.path()), "UPDATE object_chunks SET bytes=x'00'");
		expect(store.Store->read_object(*object, { }).error().Failure == cycle_store_failure::corrupt, "corrupt bytes are reported, not substituted");
		sql(database_path(scratch.path()), "DELETE FROM object_chunks; DELETE FROM objects");
		expect(store.Store->read_object(*object, { }).error().Failure == cycle_store_failure::object_unavailable, "missing bytes are unavailable");
	}

	void freezes_rounds_immutably()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-rounds" };
		auto                                 store   = open(scratch.path());
		const auto                           subject = store.create(1);
		const auto                           begun   = store.Store->begin_draft(operation(2), subject, 1, { });
		expect(begun && begun->Allocated == 1 && begun->Revision == 2, "draft allocated");
		const auto draft = core::cycle_round_draft_ref{ .Cycle = subject.Cycle, .Id = core::cycle_round_draft_id{ begun->Allocated } };
		const auto saved = store.Store->save_draft(operation(3), subject, draft, 1, bytes_of("Fix the lexer."), { });
		expect(saved && saved->Revision == 2, "draft saved");
		expect(store.Store->save_draft(operation(4), subject, draft, 1, bytes_of("late"), { }).error().Failure == cycle_store_failure::conflict, "stale draft save");

		const auto payload = store.Store->install(bytes_of("{\"round\":1}"), { });
		expect(payload.has_value(), "install payload");
		const auto commit = application::cycle_freeze_commit{ .Subject = subject, .Draft = draft, .ExpectedDraftRevision = 2, .Payload = *payload };
		const auto frozen = store.Store->freeze(operation(5), commit, { });
		expect(frozen && frozen->Allocated == 1 && frozen->Changed, "freeze");
		const auto replay = store.Store->freeze(operation(5), commit, { });
		expect(replay && replay->Replayed && replay->Allocated == 1, "freeze retry replays");
		const auto reused = store.Store->freeze(operation(6), commit, { });
		expect(reused && !reused->Changed && reused->Allocated == 1, "same draft revision and payload reuse the round");
		const auto otherPayload = store.Store->install(bytes_of("{\"round\":\"other\"}"), { });
		auto       changed      = commit;
		changed.Payload         = *otherPayload;
		expect(store.Store->freeze(operation(7), changed, { }).error().Failure == cycle_store_failure::conflict, "different payload for a frozen revision");

		expect(store.Store->save_draft(operation(8), subject, draft, 2, bytes_of("Fix the lexer and parser."), { }).has_value(), "edit after freeze");
		changed.ExpectedDraftRevision = 3;
		const auto successor          = store.Store->freeze(operation(9), changed, { });
		expect(successor && successor->Allocated == 2, "successor round");
		const auto rounds = store.Store->rounds(subject, std::nullopt, { });
		expect(rounds && rounds->Rounds.size() == 2 && rounds->Rounds[0].Round.DraftRevision == 2 && rounds->Rounds[0].Payload == *payload, "first round unchanged");
		const auto body = store.Store->read_object(rounds->Rounds[0].Payload, { });
		expect(body && *body == bytes_of("{\"round\":1}"), "frozen payload bytes");
		const auto loaded = store.Store->load_draft(subject, draft, { });
		expect(loaded && *loaded && (*loaded)->Body == bytes_of("Fix the lexer and parser.") && (*loaded)->Draft.Revision == 3, "draft reloads");
	}

	void keeps_pinned_material_under_quota()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-quota" };
		auto                                 store   = open(scratch.path(), 64);
		const auto                           subject = store.create(1);
		const auto                           pinned  = std::string(40, 'p');
		expect(store.capture(2, subject, pinned, 1).has_value(), "pinned capture");
		const auto loose = store.Store->install(bytes_of(std::string(20, 'u')), { });
		expect(loose.has_value(), "unreferenced object");
		const auto full = store.Store->install(bytes_of(std::string(10, 'n')), { });
		expect(!full && full.error().Failure == cycle_store_failure::quota && full.error().Inventory, "quota refuses a new object");
		const auto& inventory = *full.error().Inventory;
		expect(inventory.PinnedBytes == 40 && inventory.PinnedObjects == 1 && inventory.UnreferencedBytes == 20 && inventory.Budget == 64, "refusal carries the inventory");
		const auto reclaimed = store.Store->reclaim({ });
		expect(reclaimed && reclaimed->PinnedBytes == 40 && reclaimed->UnreferencedBytes == 0, "reclaim removes only unreferenced objects");
		expect(store.Store->read_object(*loose, { }).error().Failure == cycle_store_failure::object_unavailable, "reclaimed object is gone");
		const auto revision = store.Store->revisions(subject, std::nullopt, { });
		expect(revision && store.Store->read_object(revision->Revisions.front().Manifest, { }).has_value(), "pinned evidence survives");
		expect(store.Store->install(bytes_of(std::string(10, 'n')), { }).has_value(), "room after reclaim");
		const auto pinnedFull = store.Store->install(bytes_of(std::string(30, 'x')), { });
		expect(!pinnedFull && pinnedFull.error().Inventory->PinnedBytes == 40, "pinned material is not evicted to make room");
	}

	void previews_and_removes_a_cycle()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-remove" };
		auto                                 store  = open(scratch.path());
		const auto                           keep   = store.create(1, "Keep");
		const auto                           remove = store.create(2, "Remove");
		expect(store.capture(3, keep, "shared manifest", 1).has_value() && store.capture(4, remove, "shared manifest", 1).has_value(), "shared material");
		expect(store.capture(5, remove, "only removed", 1).has_value(), "own material");
		const auto preview = store.Store->removal_preview(remove, { });
		expect(preview && preview->Revisions == 2 && preview->ReleasedBytes == 12 && preview->Rounds == 0, "preview counts what would be lost");
		expect(store.Store->apply(operation(6), { .Subject = remove, .ExpectedRevision = 3, .Change = core::rename_review_cycle{ "Changed" } }, { }).has_value(), "cycle changes after preview");
		expect(store.Store->remove(operation(7), *preview, { }).error().Failure == cycle_store_failure::preview_stale, "stale preview refused");
		const auto fresh = store.Store->removal_preview(remove, { });
		const auto done  = store.Store->remove(operation(8), *fresh, { });
		expect(done && done->Changed, "removed");
		expect(store.Store->load(remove, { }) && !*store.Store->load(remove, { }), "removed cycle is absent");
		const auto inventory = store.Store->reclaim({ });
		expect(inventory && inventory->PinnedObjects == 1 && inventory->PinnedBytes == 15, "shared material stays pinned");
		expect(core::project(store.state(keep)).Revisions == 1, "other cycle intact");
		const auto next = store.Store->create(operation(9), { .Authority = store.Catalog.Authority, .Title = "Next", .Scope = { } }, { });
		expect(next && next->Allocated == 3, "removed identity is not reissued");
	}

	void leaves_other_stores_and_refuses_foreign_schemas()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-schema" };
		{
			auto store = open(scratch.path());
		}
		const auto neighbour = scratch.path() / "state" / "v1" / "neighbour.sqlite3";
		sql(neighbour, "CREATE TABLE pr_record(id INTEGER PRIMARY KEY, body TEXT); INSERT INTO pr_record VALUES(1,'unchanged')");
		const auto before = file_bytes(neighbour);
		{
			auto store = open(scratch.path());
			expect(store.create(1).Cycle.value() == 1, "cycle written beside another store");
		}
		expect(file_bytes(neighbour) == before, "other store bytes untouched");
		sql(database_path(scratch.path()), "CREATE TABLE extra(x INTEGER)");
		auto store = adapters::sqlite::make_review_cycle_store(scratch.path(), profile);
		expect(store->catalog({ }).error().Failure == cycle_store_failure::schema_mismatch, "unexpected schema refused");
	}

	void persists_workspace_bindings()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-binding" };
		auto                                 store   = open(scratch.path());
		const auto                           subject = store.create(1);
		const auto                           root    = core::workspace_root_observation{
			.Kind       = core::workspace_root_kind::git_linked_worktree,
			.Location   = std::string{ "/work/project-\xFF" },
			.Root       = { .Volume = 3, .ObjectLow = 4 },
			.Repository = core::workspace_object_identity{ .Volume = 3, .ObjectHigh = 9, .ObjectLow = 5 },
			.Worktree   = core::workspace_object_identity{ .Volume = 3, .ObjectLow = 6 },
		};
		const auto bind  = core::review_cycle_command{ .Subject = subject, .ExpectedRevision = 1, .Change = core::bind_cycle_workspace{ .Root = root } };
		const auto bound = store.Store->apply(operation(2), bind, { });
		expect(bound && bound->Changed && bound->Revision == 2, "bind through the store");
		auto changed                                                       = bind;
		std::get<core::bind_cycle_workspace>(changed.Change).Root.Location = "/work/other";
		expect(store.Store->apply(operation(2), changed, { }).error().Failure == cycle_store_failure::operation_reused, "a different root under one operation is refused");

		// A capture under a stale binding version never lands.
		const auto manifest = store.Store->install(bytes_of("bound manifest"), { });
		auto       commit   = application::cycle_capture_commit{
			.Subject          = subject,
			.ExpectedRevision = 2,
			.Capture          = { .Capture = { .ScopeVersion = 1, .Material = manifest->Digest, .Entries = 1, .Coverage = core::cycle_capture_coverage::complete } },
			.Manifest         = *manifest,
		};
		const auto stale = store.Store->record_capture(operation(3), commit, { });
		expect(!stale && stale.error().Failure == cycle_store_failure::refused && stale.error().Refusal == core::cycle_refusal::stale_workspace, "stale workspace version refused");
		commit.Capture.Capture.WorkspaceVersion = 1;
		expect(store.Store->record_capture(operation(4), commit, { }).has_value(), "current workspace version accepted");

		const auto check  = core::workspace_material_check{ .Compared = 1, .Matching = 1 };
		auto       moved  = root;
		moved.Location    = "/moved/project";
		const auto rebind = core::review_cycle_command{ .Subject = subject, .ExpectedRevision = 3, .Change = core::bind_cycle_workspace{ .Root = moved, .Check = check, .Confirmed = true } };
		expect(store.Store->apply(operation(5), rebind, { }).has_value(), "confirmed rebind");

		store.Store.reset();
		store         = open(scratch.path());
		const auto re = store.state(subject);
		expect(re.Workspace && re.Workspace->Version == 2 && re.Workspace->Root == moved && re.Workspace->Admission == check, "binding reloads exactly");
		expect(re.Workspace->Project == core::derive_workspace_project(subject), "project kept");
		expect(re.Latest && re.Latest->Capture.WorkspaceVersion == 1, "revision keeps its workspace version");
		const auto listed = store.Store->list(std::nullopt, { });
		expect(listed && listed->Cycles.front().WorkspaceVersion == 2, "projection names the binding");

		// Removing the cycle removes its binding.
		const auto preview = store.Store->removal_preview(subject, { });
		expect(preview && store.Store->remove(operation(6), *preview, { }).has_value(), "remove bound cycle");
		sqlite3* database = nullptr;
		expect(sqlite3_open(database_path(scratch.path()).string().c_str(), &database) == SQLITE_OK, "open for inspection");
		sqlite3_stmt* statement = nullptr;
		expect(sqlite3_prepare_v2(database, "SELECT count(*) FROM bindings", -1, &statement, nullptr) == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW
				   && sqlite3_column_int64(statement, 0) == 0,
			   "no binding left");
		sqlite3_finalize(statement);
		sqlite3_close(database);
	}

	void migrates_the_first_schema()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-migrate" };
		auto                                 subject = core::local_cycle_subject{ };
		{
			auto store = open(scratch.path());
			subject    = store.create(1);
			expect(store.capture(2, subject, "v1 manifest", 1).has_value(), "capture before downgrade");
		}
		// Rewrite the file into exactly the first published shape.
		sql(database_path(scratch.path()),
			"DROP TABLE checkpoints; DROP TABLE responses; DROP TABLE imports; DROP TABLE import_objects; DROP TABLE import_forks;"
			"DROP TABLE feedback;"
			"DROP TABLE feedback_revisions;"
			"DROP TABLE suggestions;"
			"DROP TABLE views;"
			"DROP TABLE bindings;"
			"DROP TABLE revision_objects;"
			"ALTER TABLE revisions RENAME TO revisions_v2;"
			"CREATE TABLE revisions(cycle INTEGER NOT NULL,revision INTEGER NOT NULL,parent INTEGER,scope_version INTEGER NOT NULL,material BLOB NOT NULL CHECK(length(material)=32),entries INTEGER NOT NULL,deletions INTEGER NOT NULL,unavailable INTEGER NOT NULL,coverage INTEGER NOT NULL,quality INTEGER NOT NULL,manifest_size INTEGER NOT NULL,PRIMARY KEY(cycle,revision)) STRICT;"
			"INSERT INTO revisions SELECT cycle,revision,parent,scope_version,material,entries,deletions,unavailable,coverage,quality,manifest_size FROM revisions_v2;"
			"DROP TABLE revisions_v2;"
			"PRAGMA user_version=1;");
		auto       store  = open(scratch.path());
		const auto loaded = store.state(subject);
		expect(loaded.Latest && loaded.Latest->Capture.WorkspaceVersion == 0 && !loaded.Workspace, "v1 revision reads as unbound");
		expect(store.capture(3, subject, "v2 manifest", 1).has_value(), "writes continue after migration");
		const auto note = store.Store->begin_feedback(operation(4), subject, store.state(subject).Revision, { .Primary = core::cycle_whole_target{ }, .Body = "after v1" }, { });
		expect(note && note->Allocated == 1, "feedback after migrating the first schema");
	}

	void migrates_the_second_schema()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-migrate-v2" };
		auto                                 subject = core::local_cycle_subject{ };
		{
			auto store = open(scratch.path());
			subject    = store.create(1);
			expect(store.capture(2, subject, "v2 manifest", 1).has_value(), "capture before downgrade");
		}
		// The second published shape had everything but feedback and suggestions.
		sql(database_path(scratch.path()), "DROP TABLE checkpoints; DROP TABLE responses; DROP TABLE imports; DROP TABLE import_objects; DROP TABLE import_forks;"
										   "DROP TABLE feedback; DROP TABLE feedback_revisions; DROP TABLE suggestions; DROP TABLE views; PRAGMA user_version=2;");
		auto       store  = open(scratch.path());
		const auto loaded = store.state(subject);
		expect(loaded.Latest && loaded.Revision == 2, "v2 cycle reads unchanged");
		const auto note = store.Store->begin_feedback(operation(3), subject, loaded.Revision, { .Primary = core::cycle_module_target{ .Path = "src" }, .Body = "after v2" }, { });
		expect(note && note->Allocated == 1, "feedback after migrating the second schema");
		const auto page = store.Store->feedback(subject, std::nullopt, { });
		expect(page && page->Threads.size() == 1 && std::holds_alternative<core::cycle_module_target>(page->Threads.front().Primary), "feedback reads back");
		// A third, unknown shape is still refused.
		store.Store.reset();
		sql(database_path(scratch.path()), "DROP TABLE feedback_revisions;");
		auto damaged = adapters::sqlite::make_review_cycle_store(scratch.path(), profile);
		expect(damaged->catalog({ }).error().Failure == cycle_store_failure::schema_mismatch, "missing feedback table refused");
	}

	void migrates_the_third_schema()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-migrate-v3" };
		auto                                 subject = core::local_cycle_subject{ };
		{
			auto store = open(scratch.path());
			subject    = store.create(1);
			expect(store.capture(2, subject, "v3 manifest", 1).has_value(), "capture before downgrade");
			const auto note = store.Store->begin_feedback(operation(3), subject, store.state(subject).Revision, { .Primary = core::cycle_whole_target{ }, .Body = "kept" }, { });
			expect(note.has_value(), "feedback before downgrade");
		}
		// The third published shape had everything but the tab views.
		sql(database_path(scratch.path()), "DROP TABLE checkpoints; DROP TABLE responses; DROP TABLE imports; DROP TABLE import_objects; DROP TABLE import_forks;"
										   "DROP TABLE views; PRAGMA user_version=3;");
		auto       store = open(scratch.path());
		const auto page  = store.Store->feedback(subject, std::nullopt, { });
		expect(page && page->Threads.size() == 1, "v3 feedback reads unchanged");
		const auto record = core::cycle_tabs_record{ .Revision = core::cycle_source_revision_id{ 1 }, .Views = { { .Revision = core::cycle_source_revision_id{ 1 }, .Path = "src/a.cpp", .Line = 12 } }, .Active = 0U };
		expect(store.Store->save_tabs(subject, record, { }).has_value(), "tabs saved after migration");
		const auto loaded = store.Store->load_tabs(subject, { });
		expect(loaded && *loaded && **loaded == record, "tabs read back exactly");
		auto future              = record;
		future.Views[0].Revision = core::cycle_source_revision_id{ 7 };
		const auto refused       = store.Store->save_tabs(subject, future, { });
		expect(!refused && refused.error().Refusal == core::cycle_refusal::invalid_target, "a revision the cycle never had is refused");
	}

#if !defined(_WIN32)
	std::string KillPrefix;
	bool        KillArmed = false;
	bool        KillAfter = false;
	auto        kill_trace(const unsigned aEvent, void*, void* aStatement, void*) -> int
	{
		const auto text = std::string_view{ sqlite3_sql(static_cast<sqlite3_stmt*>(aStatement)) };
		if (aEvent == SQLITE_TRACE_STMT && text.starts_with(KillPrefix)) {
			KillArmed = true;
		}
		if (KillArmed && text == "COMMIT" && aEvent == (KillAfter ? SQLITE_TRACE_PROFILE : SQLITE_TRACE_STMT)) {
			kill(getpid(), SIGKILL);
			_exit(99);
		}
		return 0;
	}
	auto install_kill(sqlite3* aDatabase, char**, const sqlite3_api_routines*) -> int
	{
		return sqlite3_trace_v2(aDatabase, SQLITE_TRACE_STMT | SQLITE_TRACE_PROFILE, kill_trace, nullptr);
	}
	template <typename TWork>
	void killed(std::string aPrefix, const bool aAfter, TWork aWork)
	{
		const auto child = fork();
		expect(child >= 0, "fork crash writer");
		if (child == 0) {
			KillPrefix = std::move(aPrefix);
			KillArmed  = false;
			KillAfter  = aAfter;
			if (sqlite3_auto_extension(reinterpret_cast<void (*)()>(install_kill)) != SQLITE_OK) {
				_exit(98);
			}
			aWork();
			_exit(90);
		}
		int status = 0;
		expect(waitpid(child, &status, 0) == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL, "SIGKILL at the selected commit");
	}

	void survives_kills_at_every_commit()
	{
		for (const bool after : { false, true }) {
			tests::sqlite_contract::scratch_root scratch{ "cycle-kill" };
			auto                                 store   = open(scratch.path());
			const auto                           subject = store.create(1);
			const auto&                          root    = scratch.path();
			const auto                           object  = bytes_of("killed manifest");
			const auto                           ref     = application::cycle_object_ref{ .Digest = core::cycle_material_digest{ core::sha256(object) }, .Size = object.size() };
			store.Store.reset();

			// Object install: none or one verified object; never a catalog row.
			killed("INSERT INTO objects", after, [&] { static_cast<void>(open(root).Store->install(object, { })); });
			store = open(root);
			expect(store.Store->read_object(ref, { }).has_value() == after, "install is all or nothing");
			expect(!core::project(store.state(subject)).Captured, "an installed object alone records nothing");
			if (!after) {
				expect(store.Store->install(object, { }).has_value(), "install again after the crash");
			}
			const auto capture = application::cycle_capture_commit{
				.Subject          = subject,
				.ExpectedRevision = 1,
				.Capture          = { .Capture = { .ScopeVersion = 1, .Material = ref.Digest, .Entries = 1, .Coverage = core::cycle_capture_coverage::complete } },
				.Manifest         = ref,
			};
			store.Store.reset();

			// Capture commit: the revision and its receipt land together.
			killed("INSERT INTO revisions", after, [&] { static_cast<void>(open(root).Store->record_capture(operation(2), capture, { })); });
			store              = open(root);
			const auto retried = store.Store->record_capture(operation(2), capture, { });
			expect(retried && retried->Allocated == 1 && retried->Replayed == after, "capture retry after the crash");
			expect(core::project(store.state(subject)).Revisions == 1, "exactly one revision");

			const auto draft   = core::cycle_round_draft_ref{ .Cycle = subject.Cycle, .Id = core::cycle_round_draft_id{ store.Store->begin_draft(operation(3), subject, 2, { })->Allocated } };
			const auto payload = store.Store->install(bytes_of("killed round"), { });
			const auto freeze  = application::cycle_freeze_commit{ .Subject = subject, .Draft = draft, .ExpectedDraftRevision = 1, .Payload = *payload };
			store.Store.reset();

			// Freeze commit: a ready round exists completely or not at all.
			killed("INSERT INTO rounds", after, [&] { static_cast<void>(open(root).Store->freeze(operation(4), freeze, { })); });
			store             = open(root);
			const auto rounds = store.Store->rounds(subject, std::nullopt, { });
			expect(rounds && rounds->Rounds.size() == (after ? 1U : 0U), "no partial round");
			const auto frozen = store.Store->freeze(operation(4), freeze, { });
			expect(frozen && frozen->Allocated == 1 && frozen->Replayed == after, "freeze retry after the crash");
			expect(store.state(subject).Next.Round == 2, "round counter advanced once");
		}
	}
#endif
}

namespace {
	/// [[BPR-1160]]. The fourth published shape had everything but imports;
	/// after migration an imported round is admitted once, by digest, pinned,
	/// replayed on retry, and refused while its payload is not installed.
	void migrates_the_fourth_schema_and_quarantines_imports()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-migrate-v4" };
		auto                                 subject = core::local_cycle_subject{ };
		{
			auto store = open(scratch.path());
			subject    = store.create(1);
			expect(store.capture(2, subject, "v4 manifest", 1).has_value(), "capture before downgrade");
		}
		sql(database_path(scratch.path()), "DROP TABLE checkpoints; DROP TABLE responses; DROP TABLE imports; DROP TABLE import_objects; DROP TABLE import_forks; PRAGMA user_version=4;");
		auto store = open(scratch.path());
		expect(store.state(subject).Latest.has_value(), "v4 cycle reads unchanged");

		const auto spanOf   = [](const std::string& aBytes) { return std::span{ reinterpret_cast<const std::uint8_t*>(aBytes.data()), aBytes.size() }; };
		const auto objectOf = [&](const std::string& aBytes) {
			return application::cycle_object_ref{ .Digest = core::cycle_material_digest{ core::sha256(spanOf(aBytes)) }, .Size = aBytes.size() };
		};
		const auto payload  = std::string{ "{\"schema\":\"bpr.cycle.round.v1\"}" };
		const auto attached = std::string{ "attached source" };
		const auto summary  = core::cycle_import_summary{ .Digest = core::cycle_round_digest{ core::sha256(spanOf(payload)) }, .ForeignCycle = "RC-17", .ForeignRound = "F1", .ForeignProject = "example-cache" };
		const auto commit   = application::cycle_import_commit{ .Summary = summary, .Payload = objectOf(payload), .Objects = { objectOf(attached) } };
		const auto absent   = store.Store->admit_import(operation(20), commit, { });
		expect(!absent && absent.error().Failure == cycle_store_failure::object_unavailable, "an import whose bytes are not installed is refused");
		expect(store.Store->imports(std::nullopt, { })->Imports.empty(), "and admits nothing");
		const auto pieces = std::vector<std::span<const std::uint8_t>>{ spanOf(payload), spanOf(attached) };
		expect(store.Store->install_all(pieces, { }).has_value(), "bytes installed");
		const auto admitted = store.Store->admit_import(operation(21), commit, { });
		expect(admitted && admitted->Changed && admitted->Allocated == 1U && admitted->Cycle == core::review_cycle_id{ 0 }, "admitted as import 1; no cycle changes");
		const auto replayed = store.Store->admit_import(operation(21), commit, { });
		expect(replayed && replayed->Replayed && replayed->Allocated == 1U, "a retried operation replays");
		const auto again = store.Store->admit_import(operation(22), commit, { });
		expect(again && !again->Changed && again->Allocated == 1U, "the same round again finds its record");
		const auto loaded = store.Store->load_import(core::cycle_import_id{ 1 }, { });
		expect(loaded && *loaded && (*loaded)->Summary.ForeignCycle == "RC-17" && (*loaded)->Summary.ForeignProject == "example-cache" && (*loaded)->Objects == std::vector{ objectOf(attached) },
			   "sender identities and pins read back exactly");
		const auto reclaimed = store.Store->reclaim({ });
		expect(reclaimed && reclaimed->UnreferencedObjects == 0U && store.Store->read_object(objectOf(attached), { }).has_value(), "quarantined bytes are pinned, never reclaimed");
		expect(store.Store->imports(std::nullopt, { })->Imports.size() == 1U, "one quarantined round");
	}
}

namespace {
	/// [[BPR-1163]]. The fifth published shape had everything but review
	/// checkpoints; tab records written before named comparisons still read as
	/// full source, and comparisons round-trip exactly.
	void migrates_the_fifth_schema_and_keeps_checkpoints()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-migrate-v5" };
		auto                                 subject = core::local_cycle_subject{ };
		const auto                           r1      = core::cycle_source_revision_id{ 1 };
		const auto                           r2      = core::cycle_source_revision_id{ 2 };
		const auto                           plain   = core::cycle_tabs_record{ .Revision = r1, .Views = { { .Revision = r1, .Path = "src/a.cpp", .Line = 3 } }, .Active = 0U };
		{
			auto store = open(scratch.path());
			subject    = store.create(1);
			expect(store.capture(2, subject, "v5 manifest", 1).has_value(), "capture before downgrade");
			expect(store.Store->save_tabs(subject, plain, { }).has_value(), "a full-source record before downgrade");
		}
		sql(database_path(scratch.path()), "DROP TABLE checkpoints; DROP TABLE responses; PRAGMA user_version=5;");
		auto store = open(scratch.path());
		expect(store.state(subject).Latest.has_value(), "v5 cycle reads unchanged");
		const auto old = store.Store->load_tabs(subject, { });
		expect(old && *old && **old == plain, "a record without comparisons reads as full source");
		expect(store.Store->last_review(subject, { }) && !*store.Store->last_review(subject, { }), "no checkpoint until one is marked");
		expect(store.Store->responses(subject, std::nullopt, { }) && store.Store->responses(subject, std::nullopt, { })->Responses.empty(), "no statements after migration");
		const auto nowhere = store.Store->append_response(operation(29), { .Subject = subject, .Round = core::cycle_round_id{ 1 }, .Channel = core::cycle_response_channel::manual_entry, .Content = { .Digest = core::cycle_material_digest{ }, .Size = 0 } }, { });
		expect(!nowhere && nowhere.error().Failure == cycle_store_failure::refused, "a statement needs a frozen round of the cycle");
		expect(store.capture(3, subject, "v6 manifest", 1).has_value(), "R2");

		const auto marked = store.Store->mark_reviewed(operation(30), subject, r1, { });
		expect(marked && marked->Changed && marked->Allocated == 1U, "first checkpoint");
		const auto replayed = store.Store->mark_reviewed(operation(30), subject, r1, { });
		expect(replayed && replayed->Replayed, "a retried mark replays");
		const auto same = store.Store->mark_reviewed(operation(31), subject, r1, { });
		expect(same && !same->Changed && same->Allocated == 1U, "the same revision again changes nothing");
		const auto unknown = store.Store->mark_reviewed(operation(32), subject, core::cycle_source_revision_id{ 9 }, { });
		expect(!unknown && unknown.error().Failure == cycle_store_failure::refused, "an uncaptured revision cannot be marked");
		expect(store.Store->mark_reviewed(operation(33), subject, r2, { })->Allocated == 2U, "second checkpoint");
		const auto last = store.Store->last_review(subject, { });
		expect(last && *last && (*last)->Ordinal == 2U && (*last)->Revision == r2, "the latest checkpoint is the reviewer's last review");

		auto compared                = plain;
		compared.Views[0].Revision   = r2;
		compared.Views[0].Comparison = core::cycle_tab_comparison{ .Request = core::chosen_pair_request{ .Left = r1, .Right = r2 }, .Left = r1 };
		compared.Views.push_back({ .Revision = r2, .Path = "b.txt", .Comparison = core::cycle_tab_comparison{ .Request = core::this_round_request{ .Round = core::cycle_round_id{ 1 }, .Result = r2 }, .Left = r1 } });
		expect(!store.Store->save_tabs(subject, compared, { }).has_value(), "a round never frozen is refused");
		compared.Views[1].Comparison = core::cycle_tab_comparison{ .Request = core::since_last_review_request{ }, .Left = r1 };
		compared.Views.push_back({ .Revision = r1, .Path = "c.txt" });
		expect(store.Store->save_tabs(subject, compared, { }).has_value(), "comparisons saved");
		store.Store.reset();
		auto       reopened = open(scratch.path());
		const auto read     = reopened.Store->load_tabs(subject, { });
		expect(read && *read && **read == compared, "comparisons read back exactly after a restart");
		const auto again = reopened.Store->last_review(subject, { });
		expect(again && *again && (*again)->Revision == r2, "checkpoints survive a restart");
	}
}

int main()
{
	creates_and_reopens_a_provider_free_cycle();
	deduplicates_operations_and_rejects_stale_writes();
	refuses_captures_without_their_objects();
	freezes_rounds_immutably();
	keeps_pinned_material_under_quota();
	previews_and_removes_a_cycle();
	leaves_other_stores_and_refuses_foreign_schemas();
	persists_workspace_bindings();
	migrates_the_first_schema();
	migrates_the_second_schema();
	migrates_the_third_schema();
	migrates_the_fourth_schema_and_quarantines_imports();
	migrates_the_fifth_schema_and_keeps_checkpoints();
#if !defined(_WIN32)
	survives_kills_at_every_commit();
#endif
	return EXIT_SUCCESS;
}
