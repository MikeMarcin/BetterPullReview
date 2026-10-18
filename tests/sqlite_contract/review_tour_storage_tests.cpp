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
		const application::tour_stored_state state{ after, { { before, before.Stops[0].Id, "Edit Problem", application::tour_presentation_origin::terminal } }, { }, { application::tour_section::stops, before.Stops[0].Id } };
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
		auto                         apply = [&](bool aRedo, std::uint64_t aExpected) {
			application::review_tour_host host{ before, adapters::sqlite::make_review_tour_store(scratch.path(), before.Owner.Profile), effects, decisions, { } };
			host.start();
			effects.drain();
			decisions.drain();
			expect(host.presentation().Resume == state.Resume, "optional section and stable stop survive a presentation switch");
			auto outcome = host.execute({ .Command = { .Draft = before.Id, .Owner = before.Owner, .ExpectedRevision = aExpected }, .Operation = aRedo ? application::tour_service_operation::redo : application::tour_service_operation::undo });
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

	void bounded_history()
	{
		tests::sqlite_contract::scratch_root scratch{ "tour-bounds" };
		auto                                 seed = tests::review_tour_fixture();
		seed.Conflicts.push_back({ core::tour_conflict_field::solution, { }, "baseline", "private candidate", "website candidate", { } });
		application::manual_executor  effects, decisions;
		application::review_tour_host host{ seed, adapters::sqlite::make_review_tour_store(scratch.path(), seed.Owner.Profile), effects, decisions, { } };
		host.start();
		effects.drain();
		decisions.drain();
		for (unsigned i = 0; i < 24; ++i) {
			auto command = tests::tour_edit(*host.presentation().Draft, "revision " + std::to_string(i));
			expect(host.execute({ .Command = command, .Origin = application::tour_presentation_origin::command_client }).Status == application::tour_service_status::accepted, "each command client edit advances shared history");
			effects.drain();
			decisions.drain();
		}
		auto       store  = adapters::sqlite::make_review_tour_store(scratch.path(), seed.Owner.Profile);
		const auto before = store->load_state(seed.Owner, seed.Id, { }).value().value();
		expect(before.Undo.size() == application::maximum_tour_history_entries && before.Draft.Conflicts == seed.Conflicts, "bounded history never evicts unresolved conflict candidates");
		expect(before.Undo.back().Origin == application::tour_presentation_origin::command_client, "private origin survives real store reload");
		expect(host.execute({ .Command = tests::tour_edit(*host.presentation().Draft, before.Draft.Problem) }).Status == application::tour_service_status::unchanged, "same text adds no journal transaction");
		effects.drain();
		decisions.drain();
		expect(store->load_state(seed.Owner, seed.Id, { }).value().value() == before, "no-op leaves text, revision and history unchanged");
		auto invalid  = before;
		invalid.Draft = core::apply(before.Draft, tests::tour_edit(before.Draft, "oversized history")).Accepted;
		invalid.Undo.push_back(before.Undo.back());
		expect(!store->save_state(invalid, before.Draft.Revision, { }), "over-budget history is refused atomically");
		host.shutdown();
	}

	/// [[BPR-1061]]. A request's outcome is committed with its edit and outlives the host that made it.
	void durable_request_receipts()
	{
		tests::sqlite_contract::scratch_root scratch{ "tour-receipts" };
		const auto                           seed = tests::review_tour_fixture();
		const auto                           run  = [&](auto&& aStep) {
			application::manual_executor  effects, decisions;
			application::review_tour_host host{ seed, adapters::sqlite::make_review_tour_store(scratch.path(), seed.Owner.Profile), effects, decisions, [](const auto&) { } };
			host.start();
			effects.drain();
			decisions.drain();
			auto result = aStep(host);
			host.shutdown();
			while (!host.is_quiescent()) {
				effects.drain();
				decisions.drain();
			}
			return result;
		};
		const auto request = [](const core::review_tour& aDraft, std::string aText, std::string aId, std::uint8_t aDigest) {
			application::tour_service_request result{ .Command = tests::tour_edit(aDraft, std::move(aText)), .RequestId = std::move(aId) };
			result.Digest.fill(aDigest);
			return result;
		};
		const auto first = run([&](application::review_tour_host& aHost) {
			const auto outcome = aHost.execute(request(*aHost.presentation().Draft, "receipt text", "req-a", 1));
			expect(outcome.Status == application::tour_service_status::accepted && !outcome.Replayed, "the first request executes");
			return std::pair{ outcome, aHost.request_status("req-a") };
		});
		expect(first.second && first.second->Revision == first.first.Revision, "the host reports the outcome of the exact request");
		// Restart: the record was committed with the edit, so the new host replays it and does not edit again.
		run([&](application::review_tour_host& aHost) {
			expect(aHost.request_status("req-a").has_value(), "the record survives the host");
			const auto stale                = request(*aHost.presentation().Draft, "receipt text", "req-a", 1);
			auto       replay               = stale;
			replay.Command.ExpectedRevision = 1;
			const auto again                = aHost.execute(replay);
			expect(again.Replayed && again.Status == application::tour_service_status::accepted && again.Revision == first.first.Revision, "a duplicate after restart returns the stored outcome");
			expect(aHost.presentation().Draft->Problem == "receipt text" && aHost.presentation().Draft->Revision == first.first.Revision, "the duplicate changed nothing");
			const auto clash = aHost.execute(request(*aHost.presentation().Draft, "another payload", "req-a", 2));
			expect(clash.Status == application::tour_service_status::refused && clash.Refusal == core::tour_refusal::identity && !clash.Replayed, "the same ID with another payload refuses");
			expect(aHost.presentation().Draft->Problem == "receipt text", "the clash changed nothing");
			return 0;
		});
		// An unchanged or refused request also leaves a durable record, though the draft revision does not move.
		const auto unchanged = run([&](application::review_tour_host& aHost) {
			const auto outcome = aHost.execute(request(*aHost.presentation().Draft, "receipt text", "req-b", 3));
			expect(outcome.Status == application::tour_service_status::unchanged, "an identical edit is unchanged");
			return outcome;
		});
		run([&](application::review_tour_host& aHost) {
			const auto record = aHost.request_status("req-b");
			expect(record && record->Revision == unchanged.Revision && record->Status == static_cast<std::uint8_t>(application::tour_service_status::unchanged), "an outcome that did not move the revision is still durable");
			const auto replay = aHost.execute(request(*aHost.presentation().Draft, "receipt text", "req-b", 3));
			expect(replay.Replayed && replay.Status == application::tour_service_status::unchanged, "and replays");
			return 0;
		});
		// The stored journal is validated: duplicate IDs, a revision from the future and overflow are refused.
		auto store = adapters::sqlite::make_review_tour_store(scratch.path(), seed.Owner.Profile);
		auto state = store->load_state(seed.Owner, seed.Id, { });
		expect(state && state->has_value() && state->value().Receipts.size() == 2, "the store returns both records");
		auto invalid = state->value();
		invalid.Receipts.push_back(invalid.Receipts.front());
		expect(!application::valid(invalid), "a duplicate request ID is not a valid journal");
		invalid                           = state->value();
		invalid.Receipts.front().Revision = invalid.Draft.Revision + 1U;
		expect(!application::valid(invalid), "a record from a revision the draft never reached is not valid");
		invalid = state->value();
		invalid.Receipts.front().Id.assign(65U, 'x');
		expect(!application::valid(invalid), "an over-long request ID is not valid");
		invalid = state->value();
		invalid.Receipts.resize(application::maximum_tour_request_receipts + 1U, invalid.Receipts.front());
		expect(!application::valid(invalid), "the journal is bounded");
		// A journal-only save at the same revision is compare-and-set against the committed draft.
		auto same = state->value();
		same.Receipts.push_back({ "req-c", { }, 1, core::tour_refusal::none, same.Draft.Revision });
		expect(store->save_state(same, same.Draft.Revision, { }).has_value(), "a receipt-only save at the current revision commits");
		auto forged          = same;
		forged.Draft.Problem = "changed without a revision";
		forged.Receipts.push_back({ "req-d", { }, 1, core::tour_refusal::none, forged.Draft.Revision });
		const auto refused = store->save_state(forged, forged.Draft.Revision, { });
		expect(!refused && refused.error().Failure == application::tour_store_failure::conflict, "same-revision content cannot be smuggled in beside a receipt");
	}
	/// [[BPR-1064]]. A pending request for the active agent belongs to the draft's journal, so it pairs
	/// across short-lived processes and an owner change, and every bound of it is validated on read.
	void durable_assist_tickets()
	{
		tests::sqlite_contract::scratch_root scratch{ "tour-assist" };
		const auto                           seed = tests::review_tour_fixture();
		const auto                           run  = [&](auto&& aStep) {
			application::manual_executor  effects, decisions;
			application::review_tour_host host{ seed, adapters::sqlite::make_review_tour_store(scratch.path(), seed.Owner.Profile), effects, decisions, [](const auto&) { } };
			host.start();
			effects.drain();
			decisions.drain();
			auto result = aStep(host);
			host.shutdown();
			while (!host.is_quiescent()) {
				effects.drain();
				decisions.drain();
			}
			return result;
		};
		const std::int64_t now    = 5000;
		const auto         issued = run([&](application::review_tour_host& aHost) {
			const auto                      draft = aHost.presentation().Draft;
			application::tour_assist_ticket ticket{ .Hook = core::writing_hook::problem, .Scope = core::writing_scope::selection, .Begin = 6, .End = 11, .DraftRevision = draft->Revision, .HookRevision = 3, .Sources = { { "field-before", "Crash ", std::nullopt } }, .Session = "cli" };
			ticket.RawDigest  = core::writing_digest(draft->Problem);
			const auto result = aHost.assist_issue(ticket, now);
			expect(result.has_value(), "the first host issues a ticket");
			return *result;
		});
		// The next process finds the same ticket and can complete it: a journal-only save was durable.
		run([&](application::review_tour_host& aHost) {
			const auto found = aHost.assist_ticket(issued.Id);
			expect(found && *found == issued, "the ticket survives the process that issued it, with its bound sources");
			const auto                        draft = aHost.presentation().Draft;
			application::tour_service_request request{ .Command = { .Draft = draft->Id, .Owner = draft->Owner }, .Operation = application::tour_service_operation::assist_complete, .Origin = application::tour_presentation_origin::command_client, .RequestId = "complete-1", .Assist = { .Id = issued.Id, .ResultJson = "{\"schema\":\"bpr.review-preparation.result.v1\",\"kind\":\"rewrite\",\"status\":\"ok\",\"replacement_text\":\"When\",\"notes\":[]}", .Now = now + 5, .CurrentHookRevision = 3 } };
			request.Digest.fill(9);
			const auto result = aHost.execute(request);
			expect(result.Status == application::tour_service_status::accepted && aHost.presentation().Draft->Problem.starts_with("Crash When"), "a later process completes exactly the issued range");
			return 0;
		});
		// Spending it was durable too, and a lost answer replays from the record.
		run([&](application::review_tour_host& aHost) {
			expect(!aHost.assist_ticket(issued.Id).has_value(), "the spent ticket did not come back");
			const auto                        draft = aHost.presentation().Draft;
			application::tour_service_request request{ .Command = { .Draft = draft->Id, .Owner = draft->Owner }, .Operation = application::tour_service_operation::assist_complete, .Origin = application::tour_presentation_origin::command_client, .RequestId = "complete-1", .Assist = { .Id = issued.Id, .ResultJson = "{\"schema\":\"bpr.review-preparation.result.v1\",\"kind\":\"rewrite\",\"status\":\"ok\",\"replacement_text\":\"When\",\"notes\":[]}", .Now = now + 6, .CurrentHookRevision = 3 } };
			request.Digest.fill(9);
			expect(aHost.execute(request).Replayed, "a duplicate delivery after restart replays the stored outcome");
			return 0;
		});
		// Bounds are validated on the stored record.
		auto store = adapters::sqlite::make_review_tour_store(scratch.path(), seed.Owner.Profile);
		auto state = store->load_state(seed.Owner, seed.Id, { });
		expect(state && state->has_value(), "the stored record");
		auto invalid           = state->value();
		invalid.AssistSequence = 5;
		application::tour_assist_ticket good{ .Id = 2, .Hook = core::writing_hook::problem, .Scope = core::writing_scope::field, .Begin = 0, .End = 1, .DraftRevision = 1, .HookRevision = 1, .Session = "x" };
		invalid.Tickets.push_back(good);
		expect(application::valid(invalid), "a bounded ticket is valid");
		auto future                          = invalid;
		future.Tickets.front().DraftRevision = invalid.Draft.Revision + 1U;
		expect(!application::valid(future), "a ticket for a revision the draft never reached is not valid");
		auto beyond               = invalid;
		beyond.Tickets.front().Id = 9;
		expect(!application::valid(beyond), "a ticket beyond the sequence is not valid");
		auto crowded = invalid;
		crowded.Tickets.assign(application::maximum_tour_assist_tickets + 1U, good);
		expect(!application::valid(crowded), "the tickets are bounded");
		auto hostile = invalid;
		hostile.Tickets.front().Sources.push_back({ "s", std::string(4097, 'x'), std::nullopt });
		expect(!application::valid(hostile), "an oversized source is not valid");
		auto stopless                 = invalid;
		stopless.Tickets.front().Hook = core::writing_hook::comment;
		expect(!application::valid(stopless), "a comment ticket must name its stop and no other hook may");
		expect(store->save_state(invalid, invalid.Draft.Revision, { }).has_value(), "a ticket-only save at the current revision commits");
		auto reread = store->load_state(seed.Owner, seed.Id, { });
		expect(reread && reread->has_value() && reread->value().Tickets.size() == 1 && reread->value().AssistSequence == 5, "and reads back");
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
	bounded_history();
	durable_history();
	target_catalog_migration();
	payload_versions();
	storage();
	composed_storage_host();
	durable_request_receipts();
	durable_assist_tickets();
}
