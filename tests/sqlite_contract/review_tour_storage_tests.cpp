#include "../../src/adapters/sqlite/src/review_tour_codec.hpp"
#include "../support/review_tour_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_tour_store.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_tour_host.hpp"
#include "bpr/core/sha256.hpp"
#include "scratch_root.hpp"
#include <sqlite3.h>
#if !defined(_WIN32)
	#include <sys/wait.h>
	#include <unistd.h>
#endif
namespace {
	using namespace bpr;
	using tests::expect;
	void sql(const std::filesystem::path& aPath, const char* aSql)
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open(aPath.string().c_str(), &database) == SQLITE_OK, "open owned fixture");
		expect(sqlite3_exec(database, aSql, nullptr, nullptr, nullptr) == SQLITE_OK, "owned fixture SQL");
		sqlite3_close(database);
	}
#if !defined(_WIN32)
	auto kill_commit(unsigned, void*, void* aStatement, void*) -> int
	{
		if (std::string_view{ sqlite3_sql(static_cast<sqlite3_stmt*>(aStatement)) } == "COMMIT") {
			_exit(73);
		}
		return 0;
	}
	auto install_kill(sqlite3* aDatabase, char**, const sqlite3_api_routines*) -> int { return sqlite3_trace_v2(aDatabase, SQLITE_TRACE_STMT, kill_commit, nullptr); }
#endif
	void payload_versions()
	{
		auto                                 original = tests::review_tour_fixture();
		adapters::sqlite::detail::tour_codec legacy;
		legacy.Version = 1;
		legacy.tour(original);
		auto              read = adapters::sqlite::detail::tour_codec{ .Input = legacy.Bytes, .Reading = true };
		core::review_tour restored;
		read.tour(restored);
		expect(legacy.Good && read.Good && read.Input.empty() && restored == original, "v1 draft payload remains exactly readable by v3");
		original.Remote          = core::tour_remote_binding{ .DatabaseId = 18446744073709551615ULL, .Entity = "node", .Actor = "author", .WebUrl = "https://github.com/o/r/pull/7#pullrequestreview-18446744073709551615", .Own = true };
		original.IndexBaseline   = core::tour_field_baseline{ .Raw = "index\r\n", .RemoteEntity = "node" };
		original.Stops[0].Remote = original.Remote;
		original.ImportNotice    = "third party";
		original.ReadOnly        = true;
		adapters::sqlite::detail::tour_codec version2;
		version2.Version = 2;
		version2.tour(original);
		auto              read2 = adapters::sqlite::detail::tour_codec{ .Input = version2.Bytes, .Reading = true };
		core::review_tour restored2;
		read2.tour(restored2);
		expect(read2.Good && read2.Input.empty() && restored2 == original, "v2 remote draft remains exactly readable by v3");
		original.Stops[0].Destination       = core::destination_of(*original.Stops[0].Original);
		original.RemoteStale                = true;
		original.ProblemBaseline->DraftText = "exact editor baseline";
		original.Conflicts.push_back({ core::tour_conflict_field::problem, { }, "old raw α\r\n", "local desired α\r\n", "website raw α\r\n", *original.ProblemBaseline });
		original.IncomingRoute           = original.Stops;
		original.ObservedHead            = original.Head;
		original.Stops[0].ReaffirmedHead = original.Head;
		original.HasPriorObservation     = true;
		original.ObservedChanges         = { { core::tour_change_kind::reordered, original.Stops[0].Id, "Observed website change" } };

		adapters::sqlite::detail::tour_codec version6;
		version6.Version = 6;
		version6.tour(original);
		auto              read6 = adapters::sqlite::detail::tour_codec{ .Input = version6.Bytes, .Reading = true };
		core::review_tour restored6;
		read6.tour(restored6);
		expect(read6.Good && read6.Input.empty() && restored6 == original, "v6 migration does not invent generated provenance");
		original.Stops[0].Suggestion       = core::tour_suggestion{ "selected-source", "Private reason", "Author question", true };
		original.Stops[0].CompletionOrigin = core::tour_completion_origin::relayed;
		tests::sqlite_contract::scratch_root scratch{ "tour-v2" };
		auto                                 store = adapters::sqlite::make_review_tour_store(scratch.path(), original.Owner.Profile);
		expect(store->save(original, 0, { }).has_value(), "v3 remote source/provenance saved");
		store.reset();
		store = adapters::sqlite::make_review_tour_store(scratch.path(), original.Owner.Profile);
		expect(store->load(original.Owner, original.Id, { }).value() == original, "v3 source provenance baselines and readonly state survive real reopen");
	}
	void storage()
	{
		tests::sqlite_contract::scratch_root scratch{ "tour-store" };
		const auto                           path  = scratch.path() / "state/v1/review-tours.sqlite3";
		auto                                 draft = tests::review_tour_fixture();
		auto                                 store = adapters::sqlite::make_review_tour_store(scratch.path(), draft.Owner.Profile);
		const auto                           empty = store->load(draft.Owner, draft.Id, { });
		expect(empty && !*empty, "new schema loads absent draft");
		expect(store->save(draft, 0, { }).has_value(), "first save");
		store.reset();
		store = adapters::sqlite::make_review_tour_store(scratch.path(), draft.Owner.Profile);
		expect(store->load(draft.Owner, draft.Id, { }).value() == draft, "raw Markdown, baseline span, source and order survive reopen");
		auto second = adapters::sqlite::make_review_tour_store(scratch.path(), draft.Owner.Profile);
		auto edited = core::apply(draft, tests::tour_edit(draft, "first writer")).Accepted;
		expect(store->save(edited, 1, { }).has_value(), "first writer CAS");
		auto conflict = second->save(core::apply(draft, tests::tour_edit(draft, "second writer")).Accepted, 1, { });
		expect(!conflict && conflict.error().Failure == application::tour_store_failure::conflict && conflict.error().Current == edited, "second writer retains exact current copy");
		auto wrong    = draft.Owner;
		wrong.Account = core::github_account_id{ 42 };
		expect(!store->load(wrong, draft.Id, { }).value(), "account is part of durable key");
		wrong         = draft.Owner;
		wrong.Profile = core::bpr_profile_id{ 42 };
		expect(!store->load(wrong, draft.Id, { }), "wrong profile refuses before access");
		tests::sqlite_contract::scratch_root other{ "tour-other-profile" };
		auto                                 isolated = adapters::sqlite::make_review_tour_store(other.path(), draft.Owner.Profile);
		expect(!isolated->load(draft.Owner, draft.Id, { }).value(), "separate admitted root has no borrowed draft");
		auto stop = std::stop_source{ };
		stop.request_stop();
		expect(!store->save(core::apply(edited, tests::tour_edit(edited, "cancelled")).Accepted, edited.Revision, stop.get_token()), "cancelled save refused");
		expect(store->load(draft.Owner, draft.Id, { }).value() == edited, "cancelled save leaves old bytes");
#if !defined(_WIN32)
		const auto child = fork();
		expect(child >= 0, "fork killed-save fixture");
		if (child == 0) {
			sqlite3_auto_extension(reinterpret_cast<void (*)()>(install_kill));
			auto candidate = core::apply(edited, tests::tour_edit(edited, "killed before commit")).Accepted;
			static_cast<void>(store->save(candidate, edited.Revision, { }));
			_exit(74);
		}
		int status = 0;
		expect(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 73, "child stopped at actual COMMIT");
		expect(store->load(draft.Owner, draft.Id, { }).value() == edited, "WAL rollback recovers pre-crash draft");
#endif
		sql(path, "PRAGMA user_version=99");
		expect(!store->load(draft.Owner, draft.Id, { }), "unknown schema refused");
		sql(path, "PRAGMA user_version=3");
		expect(store->load(draft.Owner, draft.Id, { }).value() == edited, "schema refusal changes no draft");
		sql(path, "UPDATE tours SET digest=zeroblob(32)");
		const auto corrupt = store->load(draft.Owner, draft.Id, { });
		expect(!corrupt && corrupt.error().Failure == application::tour_store_failure::corrupt, "corrupt payload never treated as empty");
	}
	void target_catalog_migration()
	{
		tests::sqlite_contract::scratch_root scratch{ "tour-targets" };
		const auto                           draft = tests::review_tour_fixture();
		auto                                 store = adapters::sqlite::make_review_tour_store(scratch.path(), draft.Owner.Profile);
		expect(store->save(draft, 0, { }).has_value(), "create draft for legacy migration");
		const auto path = scratch.path() / "state/v1/review-tours.sqlite3";
		// Install an actual legacy payload, not a current journal with an older schema number.
		auto                                 legacyDraft = draft;
		adapters::sqlite::detail::tour_codec legacy;
		legacy.Version = 7;
		legacy.tour(legacyDraft);
		expect(legacy.Good, "encode pre-journal version 7 fixture");
		const auto digest   = core::sha256(std::span<const std::uint8_t>{ legacy.Bytes });
		sqlite3*   database = nullptr;
		expect(sqlite3_open(path.string().c_str(), &database) == SQLITE_OK, "open legacy payload fixture");
		sqlite3_stmt* statement = nullptr;
		expect(sqlite3_prepare_v2(database, "UPDATE tours SET payload=?1,digest=?2", -1, &statement, nullptr) == SQLITE_OK, "prepare legacy payload replacement");
		expect(sqlite3_bind_blob(statement, 1, legacy.Bytes.data(), static_cast<int>(legacy.Bytes.size()), SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_bind_blob(statement, 2, digest.data(), static_cast<int>(digest.size()), SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(statement) == SQLITE_DONE, "install exact digest-protected legacy draft");
		sqlite3_finalize(statement);
		sqlite3_close(database);
		sql(path, "DROP TABLE targets; PRAGMA user_version=1");
		store.reset();
		store = adapters::sqlite::make_review_tour_store(scratch.path(), draft.Owner.Profile);
		expect(store->load(draft.Owner, draft.Id, { }).value() == draft, "exact legacy schema migrates without rewriting tour payload");
		application::tour_saved_target target{ .Owner = draft.Owner, .Draft = draft.Id, .HostProfile = core::host_profile_id{ 1 }, .Hostname = "github.com", .Repository = { "o", "r" }, .Number = core::pull_request_number{ 7 }, .Base = draft.Base, .Head = draft.Head };
		expect(store->remember_target(target, { }).has_value(), "remember exact accepted target");
		store.reset();
		store = adapters::sqlite::make_review_tour_store(scratch.path(), draft.Owner.Profile);
		expect(store->targets({ }).value() == std::vector{ target }, "target metadata survives reopen beside original draft");
		target.Owner.Account = core::github_account_id{ 42 };
		expect(store->remember_target(target, { }).has_value() && store->targets({ })->size() == 2, "accounts retain separate lookup records");
		sql(path, "UPDATE targets SET digest=zeroblob(32)");
		expect(!store->targets({ }), "corrupt target catalog is not an empty profile");
		expect(store->load(draft.Owner, draft.Id, { }).value() == draft, "lookup corruption does not rewrite retained draft");
	}

	void durable_history()
	{
		tests::sqlite_contract::scratch_root scratch{ "tour-history" };
		const auto                           before = tests::review_tour_fixture();
		auto                                 store  = adapters::sqlite::make_review_tour_store(scratch.path(), before.Owner.Profile);
		expect(store->save(before, 0, { }).has_value(), "save journal base");
		const auto                           after = core::apply(before, tests::tour_edit(before, "raw α\r\n")).Accepted;
		const application::tour_stored_state state{ after, { { before, { }, "Edit Problem" } }, { } };
		expect(store->save_state(state, before.Revision, { }).has_value(), "current text and undo saved in one CAS record");
		store.reset();
		store = adapters::sqlite::make_review_tour_store(scratch.path(), before.Owner.Profile);
		expect(store->load_state(before.Owner, before.Id, { }).value() == state, "history and raw text survive process-independent store reopen");
		auto wrong                             = state;
		wrong.Draft                            = core::apply(after, tests::tour_edit(after, "next")).Accepted;
		wrong.Undo.front().Draft.Owner.Account = core::github_account_id{ 999 };
		expect(!store->save_state(wrong, after.Revision, { }), "foreign-owner history is refused without changing current text");
		expect(store->load_state(before.Owner, before.Id, { }).value() == state, "refused journal cannot partially replace current state");
#if !defined(_WIN32)
		const auto child = fork();
		expect(child >= 0, "fork journal crash fixture");
		if (child == 0) {
			sqlite3_auto_extension(reinterpret_cast<void (*)()>(install_kill));
			auto next  = state;
			next.Draft = core::apply(after, tests::tour_edit(after, "uncommitted replacement")).Accepted;
			next.Undo.push_back({ after, { }, "Edit Problem" });
			static_cast<void>(store->save_state(next, after.Revision, { }));
			_exit(74);
		}
		int status = 0;
		expect(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 73, "journal writer killed at its actual COMMIT");
		expect(store->load_state(before.Owner, before.Id, { }).value() == state, "killed transaction rolls back text and both history directions together");
#endif
		application::manual_executor effects, decisions;
		auto                         apply = [&](bool redo, std::uint64_t expected) {
			application::review_tour_host host{ before, adapters::sqlite::make_review_tour_store(scratch.path(), before.Owner.Profile), effects, decisions, { } };
			host.start();
			effects.drain();
			decisions.drain();
			auto outcome = host.execute({ .Command = { .Draft = before.Id, .Owner = before.Owner, .ExpectedRevision = expected }, .Operation = redo ? application::tour_service_operation::redo : application::tour_service_operation::undo });
			host.shutdown();
			while (!host.is_quiescent()) {
				effects.drain();
				decisions.drain();
			}
			expect(outcome.Status == application::tour_service_status::accepted && !host.presentation().Failure, "shared host commits one durable history step");
			return *host.presentation().Draft;
		};
		const auto undone = apply(false, after.Revision);
		expect(undone.Problem == before.Problem && undone.Stops == before.Stops && undone.Revision > after.Revision, "undo restores exact content with a new invalidating revision");
		const auto redone = apply(true, undone.Revision);
		expect(redone.Problem == after.Problem && redone.Stops == after.Stops, "redo across host recreation restores exact stored bytes");
	}

	void composed_storage_host()
	{
		tests::sqlite_contract::scratch_root scratch{ "tour-host" };
		auto                                 seed = tests::review_tour_fixture();
		application::manual_executor         effects, decisions;
		application::review_tour_host        first{ seed, adapters::sqlite::make_review_tour_store(scratch.path(), seed.Owner.Profile), effects, decisions, [](const auto&) { } };
		application::review_tour_host        second{ seed, adapters::sqlite::make_review_tour_store(scratch.path(), seed.Owner.Profile), effects, decisions, [](const auto&) { } };
		first.start();
		second.start();
		effects.drain();
		decisions.drain();
		first.dispatch(tests::tour_edit(*first.presentation().Draft, "initial"));
		first.dispatch(tests::tour_edit(*first.presentation().Draft, "final coalesced"));
		first.shutdown();
		effects.drain();
		decisions.drain();
		effects.drain();
		decisions.drain();
		expect(first.is_quiescent() && first.presentation().SavedRevision == first.presentation().Draft->Revision, "orderly close persists last coalesced edit");
		second.dispatch(tests::tour_edit(*second.presentation().Draft, "other window text"));
		effects.drain();
		decisions.drain();
		expect(second.presentation().Conflict && second.presentation().Conflict->Problem == "final coalesced" && second.presentation().Draft->Problem == "other window text", "two composed windows retain both conflicting texts");
		second.use_saved_copy();
		expect(second.presentation().Draft->Problem == "final coalesced", "explicit conflict resolution");
		auto reopened = adapters::sqlite::make_review_tour_store(scratch.path(), seed.Owner.Profile);
		expect(reopened->load(seed.Owner, seed.Id, { }).value()->Problem == "final coalesced", "restarted store returns final edit");
		second.shutdown();
	}
}
int main()
{
	durable_history();
	target_catalog_migration();
	payload_versions();
	storage();
	composed_storage_host();
}
