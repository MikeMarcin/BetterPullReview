#include "../../src/adapters/sqlite/src/private_review_codec.hpp"
#include "../../src/adapters/sqlite/src/private_review_database.hpp"
#include "../../src/adapters/sqlite/src/review_ledger_database.hpp"
#include "../core/private_review_fixture.hpp"
#include "bpr/adapters/sqlite/private_review_store.hpp"
#include "bpr/application/agent_launch.hpp"
#include "review_ledger_legacy_fixture.hpp"
#include "scratch_root.hpp"
#include <barrier>
#include <fstream>
#include <iostream>
#include <sqlite3.h>
#include <thread>
#if !defined(_WIN32)
	#include <signal.h>
	#include <sys/wait.h>
	#include <unistd.h>
#endif
namespace {
	using namespace bpr;
	using namespace core;
	using namespace application;
	using namespace adapters::sqlite;
	using private_review_fixture::expect;
	private_review_configuration configuration(const std::filesystem::path& aRoot) { return { { aRoot, "github.com", "octo", "example", 5 }, bpr_profile_id{ 1 }, git_host_id{ 2 }, github_account_id{ 3 }, repository_id{ 4 }, pull_request_id{ 5 } }; }
	auto                         open(const private_review_configuration& aConfig)
	{
		auto store = make_private_review_store(aConfig);
		expect(store.has_value(), "public factory opens migrated ledger");
		return std::move(*store);
	}
	auto create(private_review_store_port& aStore, std::string aBody = "reviewer exact\r\nα") -> private_revision_commit
	{
		auto f = aStore.frontier();
		expect(f.has_value(), "frontier observable");
		auto binding  = private_review_fixture::binding();
		binding.Scope = f->Scope;
		auto next     = f->NextIdentity;
		auto proposal = create_private_thread(private_review_thread_id{ next }, { binding, private_pr_subject{ } }, { private_review_turn_id{ next + 1 }, private_review_turn_revision_id{ next + 2 }, private_vertex_order{ next + 2 }, private_reviewer_actor{ }, std::move(aBody), private_revision_completion::complete });
		expect(proposal.has_value(), "core prepares public store transaction");
		private_store_write write;
		write.ExpectedNextIdentity = next;
		write.Threads.push_back({ proposal->Thread, 0 });
		write.Revisions.push_back(proposal->Revision);
		auto committed = aStore.commit(write);
		if (!committed) {
			std::cerr << "commit error " << static_cast<int>(committed.error()) << '\n';
		}
		expect(committed.has_value(), "atomic thread turn and body committed");
		return *proposal;
	}
	void run_codec_compatibility()
	{
		agent_run_provenance old;
		old.ProfileDisplayName = "retained v1 name";
		detail::private_codec codec;
		std::uint64_t         version = 1;
		codec.value(version);
		codec.value(old);
		auto decoded = detail::decode_private<agent_run_provenance>(codec.Bytes);
		expect(decoded && *decoded == old && decoded->OutputItemBytes.empty(), "v1 run payload remains exact and unknown item boundaries stay absent");
		auto current            = old;
		current.OutputItemBytes = { 3, 5 };
		auto encoded            = detail::encode_private(current);
		expect(encoded && detail::decode_private<agent_run_provenance>(*encoded) == current, "v2 run payload preserves boundaries without duplicate text");
	}
	void publication_codec_compatibility()
	{
		private_publication_record old;
		old.LogicalPublication = "retained-v1-logical";
		old.ServerComment      = "retained-server";
		old.ReconciledIdentity = old.ServerComment;
		detail::private_codec codec;
		std::uint64_t         version = 1;
		codec.value(version);
		codec.value(old);
		auto decoded = detail::decode_private<private_publication_record>(codec.Bytes);
		expect(decoded && decoded->LogicalPublication == old.LogicalPublication && decoded->ServerComment == old.ServerComment && decoded->IdempotencyKey.empty() && decoded->ReceiptReserve == 0 && decoded->ActorNode.empty() && decoded->Path.empty(), "v1 publication retains old evidence without inventing actor or recovery metadata");
		auto current            = old;
		current.IdempotencyKey  = "local-key";
		current.ActorNode       = "U_fixture";
		current.ActorDatabaseId = 73;
		current.ActorLogin      = "fixture";
		current.Path            = "src/example.cpp";
		current.AcceptedHead    = private_review_fixture::binding().Revision.HeadOid;
		auto encoded            = detail::encode_private(current);
		auto roundTrip          = encoded ? detail::decode_private<private_publication_record>(*encoded) : std::nullopt;
		expect(roundTrip && roundTrip->ServerComment == old.ServerComment && roundTrip->IdempotencyKey == current.IdempotencyKey && roundTrip->ActorNode == current.ActorNode && roundTrip->ActorDatabaseId == current.ActorDatabaseId && roundTrip->ActorLogin == current.ActorLogin && roundTrip->Path == current.Path && roundTrip->AcceptedHead == current.AcceptedHead, "v2 publication retains exact local recovery binding");
	}

	void smoke()
	{
		tests::sqlite_contract::scratch_root scratch{ "private-review" };
		auto                                 config   = configuration(scratch.path());
		auto                                 store    = open(config);
		auto                                 original = create(*store);
		auto                                 peer     = open(config);
		auto                                 header   = peer->load_thread(original.Thread.Id);
		expect(header && *header == original.Thread, "second connection reads exact header");
		auto page = peer->list_revisions(original.Thread.Id);
		expect(page && page->Revisions.size() == 1 && page->Revisions[0].Revision == original.Revision, "exact immutable revision round trip");
		auto ledger = make_review_ledger_store(config.Ledger);
		expect(ledger.has_value(), "legacy facade shares authority");
		auto note = (*ledger)->store_note({ .Id = local_note_id{ 1 }, .Note = { .Text = "facade" } });
		expect(note.has_value(), "facade adds one thread");
		auto all = store->list_threads();
		expect(all && all->Threads.size() == 2, "legacy and new threads share paged catalog");
		auto privateNotes = (*ledger)->load_private();
		expect(privateNotes && privateNotes->Notes.size() == 1, "old aggregate loads mapped notes only");
		store.reset();
		peer.reset();
		ledger->reset();
		store = open(config);
		page  = store->list_revisions(original.Thread.Id);
		expect(page && page->Revisions[0].Revision == original.Revision, "restart retains exact revision");
		auto purged = store->purge();
		if (!purged) {
			std::cerr << "purge error " << static_cast<int>(purged.error()) << "\n";
		}
		expect(purged.has_value(), "purge removes private scope");
		auto frontier = store->frontier();
		expect(frontier && frontier->NextIdentity > original.Revision.Identity.Order.value(), "purge keeps monotonic high water");
		expect(store->list_threads()->Threads.empty(), "purged catalog empty");
	}
	void fixture_sql(const std::filesystem::path& aPath, std::string_view aSql)
	{
		sqlite3* db = nullptr;
		expect(sqlite3_open(aPath.string().c_str(), &db) == SQLITE_OK, "open synthetic raw fixture");
		auto status = sqlite3_exec(db, std::string{ aSql }.c_str(), nullptr, nullptr, nullptr);
		if (status != SQLITE_OK) {
			std::cerr << sqlite3_errmsg(db) << '\n';
		}
		expect(status == SQLITE_OK, "apply designated fixture SQL");
		sqlite3_close(db);
	}
	auto ledger_path(const private_review_configuration& aConfig) { return aConfig.Ledger.StorageRoot / "state/v1/review-ledger.sqlite3"; }
	void indexed_revision_page(const std::filesystem::path& aPath)
	{
		sqlite3* db = nullptr;
		expect(sqlite3_open(aPath.string().c_str(), &db) == SQLITE_OK, "open page-plan fixture");
		sqlite3_stmt* statement = nullptr;
		expect(sqlite3_prepare_v2(db, "EXPLAIN QUERY PLAN SELECT id,length(body),record,digest FROM private_content WHERE scope_id=1 AND kind=0 AND owner_id=1 AND id>1 ORDER BY id LIMIT 65", -1, &statement, nullptr) == SQLITE_OK, "prepare actual keyset query plan");
		bool searched = false;
		while (sqlite3_step(statement) == SQLITE_ROW) {
			std::string plan{ reinterpret_cast<const char*>(sqlite3_column_text(statement, 3)) };
			expect(plan.find("TEMP B-TREE") == std::string::npos && plan.find("SCAN") == std::string::npos, "page seeks without scanning retained rows or sorting transcript");
			searched |= plan.find("scope_id=? AND kind=? AND owner_id=? AND id>?") != std::string::npos;
		}
		expect(searched, "page uses scope thread and keyset index");
		sqlite3_finalize(statement);
		sqlite3_close(db);
	}
	auto append(private_review_store_port& aStore, const private_review_thread& aHeader, std::string aBody) -> private_revision_commit
	{
		auto f        = *aStore.frontier();
		auto next     = f.NextIdentity;
		auto proposal = append_private_turn(aHeader, aHeader.Version, { private_review_turn_id{ next }, private_review_turn_revision_id{ next + 1 }, private_vertex_order{ next + 1 }, private_reviewer_actor{ }, std::move(aBody), private_revision_completion::complete });
		expect(proposal.has_value(), "core append accepted");
		private_store_write write;
		write.ExpectedNextIdentity = next;
		write.Threads              = { { proposal->Thread, aHeader.Version } };
		write.Revisions            = { proposal->Revision };
		expect(aStore.commit(write).has_value(), "append transaction accepted");
		return *proposal;
	}
	auto assistant_write(private_review_store_port& aStore, const private_review_thread& aThread, const std::string& aBody) -> private_store_write
	{
		auto                 next    = aStore.frontier()->NextIdentity;
		auto                 binding = std::get<exact_private_subject>(aThread.Binding).Binding;
		agent_run_provenance run{ .Run = agent_run_id{ next }, .Session = agent_session_id{ 77 }, .Order = private_vertex_order{ next }, .Profile = agent_profile_id{ 1 }, .ProfileDisplayName = "fixture", .Protocol = agent_protocol::codex_app_server, .EntryPoint = agent_entry_point::ask_about_selection, .Purpose = private_run_purpose::conversation, .Binding = binding, .Metadata = { .Requested = { "requested", "high" }, .Configured = { "default", { } }, .ProtocolReported = { { .Basis = "result.model", .Settings = { "reported", "low" } } } }, .SelectedPrivateSources = { }, .InputSelectionDigest = *private_selection_digest({ }), .OutputDigest = private_digest(aBody), .Outcome = private_run_outcome::succeeded, .Completion = private_revision_completion::complete };
		auto                 admission = run;
		admission.Outcome              = private_run_outcome::admitted;
		admission.Completion           = private_revision_completion::incomplete;
		admission.OutputDigest.reset();
		private_store_write admitted;
		admitted.ExpectedNextIdentity = next;
		admitted.Runs                 = { admission };
		expect(aStore.commit(admitted).has_value(), "run admission precedes terminal storage");
		auto auth = authorize_private_run(run);
		expect(auth.has_value(), "bounded run admission policy");
		auto proposal = append_private_turn(aThread, aThread.Version, { private_review_turn_id{ next + 1 }, private_review_turn_revision_id{ next + 2 }, private_vertex_order{ next + 2 }, private_agent_actor{ run.Run }, aBody, private_revision_completion::complete }, nullptr, &*auth);
		expect(proposal.has_value(), "core authorizes exact assistant body");
		private_store_write write;
		write.ExpectedNextIdentity = next + 1;
		write.Runs                 = { run };
		write.Threads              = { { proposal->Thread, aThread.Version } };
		write.Revisions            = { proposal->Revision };
		return write;
	}
	void facade_thread_coherence()
	{
		tests::sqlite_contract::scratch_root scratch{ "private-facade" };
		auto                                 config   = configuration(scratch.path());
		auto                                 store    = open(config);
		auto                                 ledger   = make_review_ledger_store(config.Ledger);
		auto                                 original = *(*ledger)->store_note({ .Id = local_note_id{ 1 }, .Note = { .Text = "first note" } });
		auto                                 header   = store->list_threads()->Threads.front();
		auto                                 expanded = append(*store, header, "second turn");
		auto                                 stale    = (*ledger)->store_note(original, original.Revision);
		expect(!stale && stale.error().Reason == ledger_store_failure::conflict, "canonical thread update invalidates old facade CAS");
		auto current = (*ledger)->load_private()->Notes.front();
		expect(current.Note.Text == "first note" && current.Revision != original.Revision, "facade projects only original note turn after expansion");
		current.Note.Text = "edited first note";
		expect((*ledger)->store_note(current, current.Revision).has_value(), "facade edits original turn after expansion");
		auto page = store->list_revisions(header.Id);
		expect(page && page->Revisions.size() == 3 && page->Revisions[1].Revision == expanded.Revision && page->Revisions.back().Revision.Body == "edited first note", "facade preserves all expanded transcript bodies");
	}
	void body_pages_and_cursors()
	{
		tests::sqlite_contract::scratch_root scratch{ "private-body" };
		auto                                 config = configuration(scratch.path());
		auto                                 store  = open(config);
		auto                                 first  = create(*store);
		indexed_revision_page(ledger_path(config));
		auto body            = std::string(maximum_private_assistant_body_bytes, 'x');
		body[524287]         = '\0';
		body[524288]         = '\r';
		auto write           = assistant_write(*store, first.Thread, body);
		auto assistantStored = store->commit(write);
		if (!assistantStored) {
			std::cerr << "assistant error " << static_cast<int>(assistantStored.error()) << "\n";
		}
		expect(assistantStored.has_value(), "exact 1 MiB assistant persists");
		auto revision = write.Revisions.front();
		auto page     = store->list_revisions(first.Thread.Id);
		expect(page && page->Revisions.size() == 2 && page->Revisions.back().Revision.Body.empty() && page->Revisions.back().BodyContinuation, "oversize page row carries descriptor and explicit continuation");
		auto cursor = *page->Revisions.back().BodyContinuation;
		auto a      = store->read_revision_body(cursor);
		expect(a && a->Bytes.size() == 524288 && a->Next && a->Next->Offset == 524288, "first body chunk makes exact progress");
		auto b = store->read_revision_body(*a->Next);
		expect(b && b->Bytes.size() == 524288 && !b->Next && a->Bytes + b->Bytes == body && private_digest(a->Bytes + b->Bytes) == revision.Identity.Digest, "second chunk completes exact persisted bytes and digest");
		auto bad          = cursor;
		bad.Source.Digest = private_digest("wrong");
		expect(!store->read_revision_body(bad), "digest mismatch refuses body continuation");
		bad        = cursor;
		bad.Offset = body.size();
		expect(!store->read_revision_body(bad), "terminal cursor cannot create empty-success loop");
		bad                          = cursor;
		bad.Source.Scope.Incarnation = private_review_fixture::opaque<private_ledger_incarnation>(99);
		expect(!store->read_revision_body(bad), "foreign incarnation refuses");
		auto header = write.Threads.front().Thread;
		auto latest = revision;
		auto next   = store->frontier()->NextIdentity;
		auto edit   = edit_private_revision(header, header.Version, latest, latest.Identity.Revision, { latest.Identity.Turn, private_review_turn_revision_id{ next }, private_vertex_order{ next }, private_reviewer_actor{ }, "human edit", private_revision_completion::complete });
		expect(edit.has_value(), "core edits agent into reviewer successor");
		private_store_write changed;
		changed.ExpectedNextIdentity = next;
		changed.Threads              = { { edit->Thread, header.Version } };
		changed.Revisions            = { edit->Revision };
		expect(store->commit(changed).has_value(), "immutable predecessor edit persists");
		expect(store->read_revision_body(cursor)->Bytes == a->Bytes, "old body continuation stays immutable across edit");
		auto selected = std::array{ source_of(latest) };
		auto observed = store->load_sources(selected);
		expect(observed && std::get<private_turn_source>(observed->front().Latest).Revision == edit->Revision.Identity.Revision, "exact source reads expose successor tip");
		for (int i = 0; i < 65; ++i) {
			auto now = store->load_thread(first.Thread.Id);
			(void)append(*store, *now, "bounded");
		}
		auto turns = store->list_revisions(first.Thread.Id);
		expect(turns && turns->Revisions.size() == 64 && turns->Next, "turn page obeys 64-row bound");
		auto more = store->list_revisions(first.Thread.Id, turns->Next);
		expect(more && more->Revisions.size() == 4 && !more->Next, "keyset cursor exhausts history without skipping");
		auto oldCursor = turns->Next;
		auto now       = store->load_thread(first.Thread.Id);
		(void)append(*store, *now, "invalidate cursor");
		expect(store->list_revisions(first.Thread.Id, oldCursor).error() == private_store_failure::stale_cursor, "header revision invalidates stale page cursor");
		for (int i = 0; i < 64; ++i) {
			(void)create(*store, "catalog");
		}
		auto catalog = store->list_threads();
		expect(catalog && catalog->Threads.size() == 64 && catalog->Next, "catalog fixed page bound");
		expect(store->list_threads(false, catalog->Next)->Threads.size() == 1, "catalog cursor exhausts");
		(void)create(*store);
		expect(store->list_threads(false, catalog->Next).error() == private_store_failure::stale_cursor, "catalog generation refuses stale cursor");
		auto runId = write.Runs.front().Run;
		store.reset();
		store        = open(config);
		auto details = store->load_run_details(std::array{ runId });
		expect(details && details->front().Metadata.Requested.Model == "requested" && details->front().Metadata.ProtocolReported.front().Settings.Model == "reported", "runtime fact bases survive restart");
		fixture_sql(ledger_path(config), "DELETE FROM agent_session");
		expect(store->load_run_details(std::array{ runId }).has_value(), "session pruning cannot delete provenance");
	}
	struct composition_fixture
	{
		private_store_write         Write;
		private_draft_record        Draft;
		review_composition_revision Candidate;
	};
	auto compose(private_review_store_port& aStore, const private_review_revision& aSource, review_composition_mode aMode = review_composition_mode::reviewer_authored) -> composition_fixture
	{
		auto next          = aStore.frontier()->NextIdentity;
		auto index         = private_review_fixture::index();
		auto binding       = private_review_fixture::binding();
		binding.Scope      = aStore.frontier()->Scope;
		binding.Comparison = index.diff().Alignment.Digest;
		auto target        = make_composition_target(binding, { *binding.Comparison, diff_side::right, 1 }, index);
		expect(target.has_value(), "semantic target admitted");
		auto                                  source = source_of(aSource);
		std::vector<private_source_selection> selected{ { source, source.Key, false } };
		auto                                  observed = aStore.load_sources(std::array{ source });
		expect(observed.has_value(), "selected source loaded");
		auto composition = create_review_composition(review_composition_id{ next }, binding, aMode, selected, *observed, *target);
		expect(composition.has_value(), "core composition with explicit target");
		auto candidate = propose_composition_revision(*composition, review_composition_revision_id{ next + 1 }, private_vertex_order{ next + 1 }, aMode == review_composition_mode::exact_copy ? aSource.Body : "approved exact prose\r\nα", selected, *observed);
		expect(candidate.has_value(), "core produces immutable candidate");
		auto acceptance = accept_composition_as_draft(candidate->State, candidate->Revision, selected, *observed, index);
		expect(acceptance.has_value(), "core local draft acceptance");
		auto accepted = acknowledge_composition_draft(candidate->State, candidate->Revision.Identity);
		expect(accepted.has_value(), "core acknowledged state");
		composition_fixture f;
		f.Candidate                  = candidate->Revision;
		f.Draft                      = { binding, { { review_draft_id{ next + 2 }, target->Line, acceptance->Body, review_draft_state::drafting, { } }, 1, { }, review_composition_link{ binding.Scope, composition->Id, review_composition_revision_id{ next + 1 }, candidate->Revision.Identity.Digest } } };
		f.Write.ExpectedNextIdentity = next;
		f.Write.SourceGuards         = selected;
		f.Write.Compositions         = { { { *accepted, 1, selected }, 0 } };
		f.Write.Candidates           = { candidate->Revision };
		f.Write.Drafts               = { { f.Draft, 0 } };
		return f;
	}
	void exact_copy_and_late_draft_terminal()
	{
		tests::sqlite_contract::scratch_root scratch{ "private-copy" };
		auto                                 config   = configuration(scratch.path());
		auto                                 store    = open(config);
		auto                                 first    = create(*store);
		auto                                 response = assistant_write(*store, first.Thread, "exact assistant prose");
		expect(store->commit(response).has_value(), "assistant source persisted");
		auto prepared = compose(*store, response.Revisions.front(), review_composition_mode::exact_copy);
		expect(store->commit(prepared.Write).has_value(), "exact-copy candidate preserves conversation actor without a synthesis run");
		expect(store->load_candidate(prepared.Candidate.Identity)->Actor == response.Revisions.front().Actor, "exact-copy actor survives storage");
		auto next   = store->frontier()->NextIdentity;
		auto source = response.Revisions.front();
		auto header = response.Threads.front().Thread;
		auto edited = edit_private_revision(header, header.Version, source, source.Identity.Revision, { source.Identity.Turn, private_review_turn_revision_id{ next }, private_vertex_order{ next }, private_reviewer_actor{ }, "later reviewer text", private_revision_completion::complete });
		expect(edited.has_value(), "source successor after draft acceptance");
		private_store_write edit;
		edit.ExpectedNextIdentity = next;
		edit.Threads              = { { edited->Thread, header.Version } };
		edit.Revisions            = { edited->Revision };
		expect(store->commit(edit).has_value(), "source changes independently of immutable accepted prose");
		auto draft              = prepared.Draft;
		draft.Draft.Revision    = 2;
		draft.Draft.Draft.State = review_draft_state::published;
		private_store_write terminal;
		terminal.ExpectedNextIdentity = store->frontier()->NextIdentity;
		terminal.Drafts               = { { draft, 1 } };
		expect(store->commit(terminal).has_value(), "durable draft terminal retains original link after source tip changes");
		expect(store->load_draft(draft.Draft.Draft.Id)->Draft.Draft.Body == "exact assistant prose", "terminal never replaces accepted body with new source text");
		fixture_sql(ledger_path(config), "UPDATE private_scope_state SET last_identity=9223372036854775807");
		terminal.ExpectedNextIdentity                 = 0;
		terminal.Drafts.front().ExpectedRevision      = 2;
		terminal.Drafts.front().Record.Draft.Revision = 3;
		expect(store->commit(terminal).has_value() && store->frontier()->NextIdentity == 0, "exhausted allocation still permits already allocated terminal updates without identity reuse");
	}
	void draft_lineage_and_conflicts()
	{
		tests::sqlite_contract::scratch_root scratch{ "private-draft" };
		auto                                 config   = configuration(scratch.path());
		auto                                 store    = open(config);
		auto                                 first    = create(*store);
		auto                                 prepared = compose(*store, first.Revision);
		auto                                 peer     = open(config);
		expect(store->commit(prepared.Write).has_value(), "composition candidate source set and existing draft type commit atomically");
		auto sourceBadge = store->source_lineage(core::source_of(first.Revision));
		expect(sourceBadge && sourceBadge->Composition && sourceBadge->Draft && !sourceBadge->Published, "exact source badges distinguish local accepted draft from publication");
		expect(peer->commit(prepared.Write).error() == private_store_failure::conflict, "second allocator loses without ID reuse");
		auto restored = peer->load_draft(prepared.Draft.Draft.Draft.Id);
		expect(restored && restored->Draft == prepared.Draft.Draft && restored->Binding == prepared.Draft.Binding, "linked draft body and exact target round trip without runtime scope");
		expect(peer->erase_thread(first.Thread.Id, first.Thread.Version).error() == private_store_failure::references_present, "referenced source deletion refuses whole");
		auto                       next = peer->frontier()->NextIdentity;
		private_publication_record publication{ .Id = review_publication_lineage_id{ next }, .Draft = restored->Draft.Draft.Id, .DraftRevision = restored->Draft.Revision, .Composition = restored->Draft.Composition, .Target = { restored->Binding, restored->Draft.Draft.Anchor }, .Actor = github_account_id{ 3 }, .ApprovedProseDigest = private_digest(restored->Draft.Draft.Body), .WireBodyDigest = private_digest("visible permalink\n\n" + restored->Draft.Draft.Body), .LogicalPublication = "logical-1", .Representation = "file-permalink", .ServerComment = { }, .ReconciledIdentity = { } };
		private_store_write        journal;
		journal.ExpectedNextIdentity = next;
		journal.Publications         = { publication };
		expect(peer->commit(journal).has_value(), "pending publication join is local only");
		publication.ServerComment      = "server-17";
		publication.ReconciledIdentity = "github.com/repo/5/comment/17";
		journal.ExpectedNextIdentity   = peer->frontier()->NextIdentity;
		journal.Publications           = { publication };
		expect(peer->commit(journal).has_value(), "receipt updates existing join without a second publication ID");
		store.reset();
		peer.reset();
		store             = open(config);
		auto publications = store->find_publication("server-17");
		expect(publications && publications->Records.size() == 1 && publications->Records.front().WireBodyDigest == publication.WireBodyDigest && publications->Records.front().ApprovedProseDigest != publication.WireBodyDigest, "publication wire and approved prose digests remain separate after restart");
		sourceBadge = store->source_lineage(core::source_of(first.Revision));
		expect(sourceBadge && sourceBadge->Published, "source publication badge is reconstructed from durable server receipt after restart");
		expect(store->load_candidate(prepared.Candidate.Identity)->Body == prepared.Candidate.Body, "composition body survives linkage and receipt");
		auto dependents = store->list_thread_dependents(first.Thread.Id);
		expect(dependents && dependents->Objects == std::vector{ private_object_reference{ private_object_kind::composition, prepared.Write.Compositions[0].Record.State.Id.value() } }, "bounded dependency page explains deletion refusal");
		expect(store->erase_local_object({ private_object_kind::composition, prepared.Write.Compositions[0].Record.State.Id.value() }, 1).error() == private_store_failure::references_present, "composition cleanup cannot erase accepted draft");
		expect(store->erase_local_object({ private_object_kind::publication, publication.Id.value() }).has_value() && store->erase_local_object({ private_object_kind::draft, publication.Draft.value() }, 1).has_value() && store->erase_local_object({ private_object_kind::composition, prepared.Write.Compositions[0].Record.State.Id.value() }, 1).has_value(), "explicit dependency cleanup is ordered and local");

		auto before                                 = *store->frontier();
		auto poisoned                               = prepared.Write;
		poisoned.ExpectedNextIdentity               = before.NextIdentity;
		poisoned.SourceGuards.front().Source.Digest = private_digest("edited");
		expect(!store->commit(poisoned) && store->frontier()->NextIdentity == before.NextIdentity, "invalid selected source refuses entire transaction with no ID consumption");
	}
	void migration_bytes()
	{
		for (int version : { 5, 6 }) {
			tests::sqlite_contract::scratch_root scratch{ "private-migration" };
			auto                                 config = configuration(scratch.path());
			auto                                 ledger = make_review_ledger_store(config.Ledger);
			expect(ledger.has_value(), "prepare legacy notes");
			// Keep both the leading and trailing NUL in the migration payload.
			constexpr char    textBytes[] = "\0line\r\nα\tend";
			std::string       text{ std::begin(textBytes), std::end(textBytes) };
			local_note_record note{ .Id = local_note_id{ 922 }, .Attachment = local_note_attachment{ file_identity_of("f.cpp"), private_review_fixture::opaque<file_comparison_digest>(6) }, .Note = { .Text = text } };
			auto              stored = (*ledger)->store_note(note);
			expect(stored.has_value(), "exact bytes seed");
			ledger->reset();
			{
				auto raw = detail::review_ledger_database::open(ledger_path(config), { });
				expect(raw.has_value(), "legacy fixture opens");
				tests::sqlite_contract::restore_v6_fixture(raw->handle());
				expect(detail::execute(raw->handle(), "UPDATE local_note_sequence SET last_revision=99").has_value(), "v6 high water exceeds retained maximum");
				if (version == 5) {
					expect(detail::execute(raw->handle(), "ALTER TABLE local_note DROP COLUMN revision").has_value() && detail::execute(raw->handle(), "DROP TABLE local_note_sequence").has_value() && detail::execute(raw->handle(), "UPDATE ledger_meta SET schema_format=5").has_value() && detail::execute(raw->handle(), "PRAGMA user_version=5").has_value(), "v5 exact source shape");
				}
			}
			auto store   = open(config);
			auto catalog = store->list_threads();
			expect(catalog && catalog->Threads.size() == 1, "legacy migrates to one thread");
			auto page = store->list_revisions(catalog->Threads.front().Id);
			expect(page && page->Revisions.size() == 1 && page->Revisions[0].Revision.Body == text && page->Revisions[0].Revision.Identity.Digest == private_digest(text) && std::holds_alternative<private_legacy_unknown_actor>(page->Revisions[0].Revision.Actor), "v5/v6 preserves bytes digest and unknown actor exactly");
			auto legacy = std::get<legacy_retained_subject>(catalog->Threads.front().Binding);
			expect(legacy.Attachment == private_file_subject{ note.Attachment->File, note.Attachment->Comparison }, "exact legacy attachment preserved without invented binding");
			ledger        = make_review_ledger_store(config.Ledger);
			auto restored = (*ledger)->load_private()->Notes.front();
			expect(restored.Id == note.Id && restored.Revision == stored->Revision, "legacy ID and CAS token mapped");
			restored.Note.Text = "reviewer edit";
			auto edited        = (*ledger)->store_note(restored, restored.Revision);
			expect(edited && edited->Revision.value() == (version == 6 ? 100U : 2U), "v6 high water preserved independently of retained maxima");
			page = store->list_revisions(catalog->Threads.front().Id);
			expect(page && page->Revisions.size() == 2 && page->Revisions[0].Revision.Body == text && std::holds_alternative<private_reviewer_actor>(page->Revisions[1].Revision.Actor), "facade edit appends reviewer successor and retains unknown original");
			auto raw   = detail::review_ledger_database::open(ledger_path(config), { });
			auto count = detail::scalar_integer(raw->handle(), "SELECT COUNT(*) FROM sqlite_schema WHERE name IN ('local_note','local_note_sequence')");
			expect(count && *count == 0, "canonical v7 has no competing legacy note authority");
		}
	}

	auto pending_run(private_review_store_port& aStore, const private_review_thread& aThread) -> private_store_write
	{
		auto                 next = aStore.frontier()->NextIdentity;
		agent_run_provenance run{ .Run = agent_run_id{ next }, .Session = agent_session_id{ 77 }, .Order = private_vertex_order{ next }, .Profile = agent_profile_id{ 1 }, .ProfileDisplayName = "fixture", .Protocol = agent_protocol::codex_app_server, .EntryPoint = agent_entry_point::ask_about_selection, .Purpose = private_run_purpose::conversation, .Binding = std::get<exact_private_subject>(aThread.Binding).Binding, .Metadata = { }, .SelectedPrivateSources = { }, .InputSelectionDigest = *private_selection_digest({ }), .OutputDigest = { }, .Outcome = private_run_outcome::admitted, .Completion = private_revision_completion::incomplete };
		private_store_write  write;
		write.ExpectedNextIdentity = next;
		write.Runs                 = { run };
		return write;
	}
	void protocol_metadata_storage_and_secret_scan()
	{
		tests::sqlite_contract::scratch_root scratch{ "protocol-provenance" };
		const auto                           config = configuration(scratch.path());
		auto                                 store  = open(config);
		const auto                           thread = create(*store);
		auto                                 write  = pending_run(*store, thread.Thread);
		auto&                                run    = write.Runs.front();
		run.Protocol                                = agent_protocol::claude_stream_json;
		run.Metadata.Requested                      = { "request-fact", "high" };
		run.Metadata.Configured                     = { "default-fact", { } };
		expect(store->commit(write).has_value(), "exact run admitted before protocol metadata");
		agent_protocol_decoder decoder{ run.Protocol };
		expect(decoder.push(R"({"type":"system","subtype":"init","session_id":"remote-session","model":"reported-fact","claude_code_version":"fixture-1","apiKeySource":"not-retained-env-canary","extra":"not-retained-secret-canary"})"
							"\n")
				   .has_value(),
			   "protocol decodes allowlisted facts from richer raw envelope");
		for (const auto& report : decoder.metadata().ProtocolReported) {
			run.Metadata = *append_runtime_report(run.Metadata, report);
		}
		run.Outcome                = private_run_outcome::cancelled;
		write.ExpectedNextIdentity = store->frontier()->NextIdentity;
		expect(store->commit(write).has_value(), "partial cancelled-run metadata commits independently of complete assistant output");
		const auto expected = run;
		store.reset();
		store             = open(config);
		const auto loaded = store->load_run_details(std::array{ expected.Run });
		expect(loaded && loaded->size() == 1 && loaded->front() == expected
				   && inspect_runtime_metadata(loaded->front().Metadata)->size() == 3,
			   "actual decoder facts and conflicts survive SQLite restart");
		for (const auto& entry : std::filesystem::recursive_directory_iterator(scratch.path())) {
			if (!entry.is_regular_file()) {
				continue;
			}
			std::ifstream     input{ entry.path(), std::ios::binary };
			const std::string bytes{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
			expect(bytes.find("not-retained-env-canary") == std::string::npos && bytes.find("not-retained-secret-canary") == std::string::npos,
				   "database, WAL and evidence files contain no discarded credential/environment carrier");
		}
	}

	auto new_thread_write(private_review_store_port& aStore) -> private_store_write
	{
		auto f        = *aStore.frontier();
		auto binding  = private_review_fixture::binding();
		binding.Scope = f.Scope;
		auto n        = f.NextIdentity;
		auto proposal = create_private_thread(private_review_thread_id{ n }, { binding, private_pr_subject{ } }, { private_review_turn_id{ n + 1 }, private_review_turn_revision_id{ n + 2 }, private_vertex_order{ n + 2 }, private_reviewer_actor{ }, "next", private_revision_completion::complete });
		expect(proposal.has_value(), "prepare quota thread");
		private_store_write w;
		w.ExpectedNextIdentity = n;
		w.Threads              = { { proposal->Thread, 0 } };
		w.Revisions            = { proposal->Revision };
		return w;
	}
	void retained_draft_schema_migration()
	{
		tests::sqlite_contract::scratch_root scratch{ "retained-draft-migration" };
		const auto                           config   = configuration(scratch.path());
		auto                                 store    = open(config);
		auto                                 seed     = create(*store);
		auto                                 composed = compose(*store, seed.Revision);
		expect(store->commit(composed.Write).has_value(), "migration fixture retains authored draft and exact composition source");
		const auto before   = store->list_drafts()->front();
		const auto frontier = *store->frontier();
		store.reset();
		{
			auto raw = detail::review_ledger_database::open(ledger_path(config), { });
			expect(raw.has_value(), "test-owned schema9 ledger opens for canonical schema8 fixture");
			// [[BPR-1591]]. The shared fixture: schema 10's note line table
			// goes first, then schema 8's counter definition.
			tests::sqlite_contract::restore_v8_fixture(raw->handle());
		}
		store            = open(config);
		const auto after = store->list_drafts()->front();
		expect(after.Binding == before.Binding && after.Draft == before.Draft && store->frontier()->Scope == frontier.Scope && store->frontier()->NextIdentity == frontier.NextIdentity, "schema8 to9 preserves authored bytes, linkage, authority, incarnation and allocation frontier");
		auto       raw     = detail::review_ledger_database::open(ledger_path(config), { });
		const auto version = detail::scalar_integer(raw->handle(), "PRAGMA user_version");
		expect(raw && version && *version == detail::review_ledger_schema_version, "retention migration publishes the current schema");
	}
	void quota_boundaries()
	{
		tests::sqlite_contract::scratch_root scratch{ "private-quotas" };
		auto                                 config = configuration(scratch.path());
		auto                                 store  = open(config);
		auto                                 first  = create(*store);
		auto                                 path   = ledger_path(config);
		const auto                           check  = [&](std::string_view aColumn, std::uint64_t aLimit, const private_store_write& aWrite) {
			auto raw = detail::review_ledger_database::open(path, { });
			expect(raw.has_value(), "quota fixture observes counter");
			auto original = detail::scalar_integer(raw->handle(), "SELECT " + std::string{ aColumn } + " FROM private_scope_state");
			expect(original.has_value(), "counter observed");
			raw = { };
			fixture_sql(path, "UPDATE private_scope_state SET " + std::string{ aColumn } + "=" + std::to_string(aLimit));
			auto before  = *store->frontier();
			auto refused = store->commit(aWrite);
			expect(!refused && refused.error() == private_store_failure::quota && store->frontier()->NextIdentity == before.NextIdentity, "full counter refuses whole atomic write without allocating");
			fixture_sql(path, "UPDATE private_scope_state SET " + std::string{ aColumn } + "=" + std::to_string(*original));
		};
		auto next = new_thread_write(*store);
		check("threads", 4096, next);
		check("revisions", 65536, next);
		check("thread_bytes", 67108864, next);
		check("payload_bytes", 67108864, next);
		auto run = pending_run(*store, first.Thread);
		check("runs", 65536, run);
		auto composed = compose(*store, first.Revision);
		check("compositions", 4096, composed.Write);
		check("candidates", 65536, composed.Write);
		check("drafts", maximum_retained_private_drafts, composed.Write);
		expect(store->commit(composed.Write).has_value(), "counter refusals left source and prepared composition usable");
		auto                       id = store->frontier()->NextIdentity;
		private_publication_record publication{ .Id = review_publication_lineage_id{ id }, .Draft = composed.Draft.Draft.Draft.Id, .DraftRevision = 1, .Composition = composed.Draft.Draft.Composition, .Target = { composed.Draft.Binding, composed.Draft.Draft.Draft.Anchor }, .Actor = github_account_id{ 3 }, .ApprovedProseDigest = private_digest(composed.Draft.Draft.Draft.Body), .WireBodyDigest = private_digest(composed.Draft.Draft.Draft.Body), .LogicalPublication = "quota", .Representation = "native", .ServerComment = { }, .ReconciledIdentity = { } };
		private_store_write        journal;
		journal.ExpectedNextIdentity = id;
		journal.Publications         = { publication };
		check("publications", 65536, journal);
		auto oversized                              = new_thread_write(*store);
		oversized.Revisions.front().Body            = std::string(16385, 'x');
		oversized.Revisions.front().Identity.Digest = private_digest(oversized.Revisions.front().Body);
		expect(!store->commit(oversized), "reviewer byte bound plus one refused");
		auto before  = *store->frontier();
		auto tooMany = new_thread_write(*store);
		tooMany.Revisions.resize(257, tooMany.Revisions.front());
		expect(!store->commit(tooMany) && store->frontier()->NextIdentity == before.NextIdentity, "batch count limit refuses whole");
		auto runIds = std::vector<agent_run_id>(17, agent_run_id{ 1 });
		expect(!store->load_run_details(runIds), "selected provenance request count bound");
		auto sources = std::vector<private_source_reference>(257, source_of(first.Revision));
		expect(!store->load_sources(sources), "selected source count bound");
		fixture_sql(path, "UPDATE private_scope_state SET last_identity=9223372036854775807");
		expect(store->frontier()->NextIdentity == 0 && !store->commit(next), "exhausted durable allocator never wraps");
	}
	void full_thread_byte_budget()
	{
		tests::sqlite_contract::scratch_root scratch{ "private-total-bytes" };
		auto                                 config       = configuration(scratch.path());
		auto                                 store        = open(config);
		auto                                 first        = create(*store, "x");
		const auto                           initialBytes = std::filesystem::file_size(ledger_path(config));
		auto                                 header       = first.Thread;
		std::string                          body(maximum_private_assistant_body_bytes, 'z');
		for (int i = 0; i < 64; ++i) {
			if (i == 63) {
				body.pop_back();
			}
			auto write = assistant_write(*store, header, body);
			expect(store->commit(write).has_value(), "real assistant bodies reach exact 64 MiB aggregate");
			header = write.Threads.front().Thread;
		}
		auto overflow = assistant_write(*store, header, "x");
		auto before   = *store->frontier();
		auto refused  = store->commit(overflow);
		expect(!refused && refused.error() == private_store_failure::quota && store->frontier()->NextIdentity == before.NextIdentity, "one real body byte beyond total refuses whole terminal/body transaction");
		auto page = store->list_revisions(first.Thread.Id);
		expect(page && page->Revisions.size() == 64 && page->Next, "full body budget remains paged");
		auto stats = std::filesystem::file_size(ledger_path(config));
		std::cout << "private-store-body-budget:67108864 database-initial-bytes:" << initialBytes << " database-final-bytes:" << stats << " database-growth-bytes:" << stats - initialBytes << '\n';
	}
	void findings_and_run_retention()
	{
		tests::sqlite_contract::scratch_root scratch{ "private-finding" };
		auto                                 config  = configuration(scratch.path());
		auto                                 store   = open(config);
		auto                                 first   = create(*store);
		auto                                 pending = pending_run(*store, first.Thread);
		pending.Runs.front().SelectedPrivateSources  = { { source_of(first.Revision), source_of(first.Revision).Key, false } };
		pending.Runs.front().InputSelectionDigest    = *private_selection_digest(pending.Runs.front().SelectedPrivateSources);
		expect(store->commit(pending).has_value(), "selected sources pinned by admitted run");
		auto run = pending.Runs.front();
		expect(store->erase_thread(first.Thread.Id, first.Thread.Version).error() == private_store_failure::references_present && store->erase_local_object({ private_object_kind::run, run.Run.value() }).error() == private_store_failure::references_present, "active run retains source and cannot be erased");
		auto next   = store->frontier()->NextIdentity;
		auto edited = edit_private_revision(first.Thread, first.Thread.Version, first.Revision, first.Revision.Identity.Revision, { first.Revision.Identity.Turn, private_review_turn_revision_id{ next }, private_vertex_order{ next }, private_reviewer_actor{ }, "edited while running", private_revision_completion::complete });
		expect(edited.has_value(), "core prepares source edit during an admitted run");
		private_store_write editWrite;
		editWrite.ExpectedNextIdentity = next;
		editWrite.Threads              = { { edited->Thread, first.Thread.Version } };
		editWrite.Revisions            = { edited->Revision };
		expect(store->commit(editWrite).has_value(), "immutable source successor may commit during run");
		run.Outcome                  = private_run_outcome::succeeded;
		run.Completion               = private_revision_completion::complete;
		run.OutputDigest             = private_digest("finding response");
		pending.ExpectedNextIdentity = store->frontier()->NextIdentity;
		pending.Runs                 = { run };
		expect(store->commit(pending).has_value(), "terminal preserves original selected identity despite current-tip change");
		agent_finding finding{ .Provider = run.Profile, .Severity = defect_severity::minor, .Confidence = agent_finding_confidence::unstated, .Category = defect_category::correctness, .Path = "file.cpp", .Lines = { 0, 1 }, .Anchor = { .Line = source_line_ref{ private_review_fixture::opaque<file_comparison_digest>(8), diff_side::right, 0 } }, .Claim = "claim", .Evidence = "evidence", .Consequence = "consequence", .SuggestedComment = "suggested", .RequiredContext = "context", .AnalyzedBaseOid = run.Binding.Revision.BaseOid, .AnalyzedHeadOid = run.Binding.Revision.HeadOid, .AnalyzedCommits = { } };
		auto          retained = retain_agent_finding(run, 0, private_vertex_order{ store->frontier()->NextIdentity }, finding, { run.Binding.Revision, { } });
		expect(retained.has_value(), "core retains exact structured finding with unknown-path verdict");
		private_store_write write;
		write.ExpectedNextIdentity              = store->frontier()->NextIdentity;
		write.Findings                          = { *retained };
		auto poisoned                           = write;
		poisoned.Findings.front().Finding.Claim = "changed";
		expect(!store->commit(poisoned), "finding canonical digest mismatch refuses");
		fixture_sql(ledger_path(config), "UPDATE private_scope_state SET findings=65536");
		expect(store->commit(write).error() == private_store_failure::quota, "finding count cap refuses whole");
		fixture_sql(ledger_path(config), "UPDATE private_scope_state SET findings=0");
		expect(store->commit(write).has_value(), "finding snapshot commits with durable run identity");
		expect(store->load_finding(retained->Source)->Finding == finding, "full finding survives source load");
		std::vector<private_source_reference> extras;
		for (std::uint32_t ordinal = 1; ordinal <= 65; ++ordinal) {
			auto more = retain_agent_finding(run, ordinal, private_vertex_order{ store->frontier()->NextIdentity }, finding, { run.Binding.Revision, { } });
			expect(more.has_value(), "prepare independent retained finding identity");
			expect(store->commit({ .ExpectedNextIdentity = store->frontier()->NextIdentity, .Findings = { *more } }).has_value(), "retain paginated finding");
			extras.push_back(more->Source);
		}
		store.reset();
		store          = open(config);
		auto firstPage = store->list_findings();
		expect(firstPage && firstPage->Sources.size() == 64 && firstPage->Next, "restart first finding page is bounded");
		auto secondPage = store->list_findings(firstPage->Next);
		expect(secondPage && secondPage->Sources.size() == 2 && !secondPage->Next, "finding continuation returns exact remaining identities");
		expect(firstPage->Sources.front() == retained->Source && secondPage->Sources.back() == extras.back(), "pagination does not substitute source revision");
		for (const auto& extra : extras) {
			expect(store->erase_local_object({ private_object_kind::finding, extra.Order.value() }).has_value(), "clean unreferenced page fixture");
		}
		expect(store->list_findings(firstPage->Next).error() == private_store_failure::stale_cursor, "changed finding catalog refuses stale continuation");
		expect(store->erase_local_object({ private_object_kind::run, run.Run.value() }).error() == private_store_failure::references_present, "retained finding keeps run provenance");
		expect(store->erase_local_object({ private_object_kind::finding, retained->Source.Order.value() }).has_value() && store->erase_local_object({ private_object_kind::run, run.Run.value() }).has_value(), "explicit unreferenced finding then run cleanup");
		expect(store->erase_thread(first.Thread.Id, edited->Thread.Version).has_value(), "source can be explicitly deleted after dependents removed");
	}
#if !defined(_WIN32)
	std::string KillPrefix;
	bool        KillArmed       = false;
	bool        KillAfterCommit = false;
	auto        kill_trace(unsigned aEvent, void*, void* aStatement, void*) -> int
	{
		auto sql = std::string_view{ sqlite3_sql(static_cast<sqlite3_stmt*>(aStatement)) };
		if (aEvent == SQLITE_TRACE_STMT && sql.starts_with(KillPrefix)) {
			KillArmed = true;
		}
		if (KillArmed && sql == "COMMIT" && aEvent == (KillAfterCommit ? SQLITE_TRACE_PROFILE : SQLITE_TRACE_STMT)) {
			kill(getpid(), SIGKILL);
			_exit(99);
		}
		return 0;
	}
	auto install_kill(sqlite3* aDatabase, char**, const sqlite3_api_routines*) -> int { return sqlite3_trace_v2(aDatabase, SQLITE_TRACE_STMT | SQLITE_TRACE_PROFILE, kill_trace, nullptr); }
	template <typename F>
	void killed(std::string aPrefix, bool aAfter, F aFunction)
	{
		auto child = fork();
		expect(child >= 0, "fork isolated crash writer");
		if (child == 0) {
			KillPrefix      = std::move(aPrefix);
			KillArmed       = false;
			KillAfterCommit = aAfter;
			expect(sqlite3_auto_extension(reinterpret_cast<void (*)()>(install_kill)) == SQLITE_OK, "install child-only transaction observer");
			aFunction();
			_exit(90);
		}
		int status = 0;
		expect(waitpid(child, &status, 0) == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL, "actual SIGKILL crosses selected transaction boundary");
	}
	void crash_transactions()
	{
		for (bool after : { false, true }) {
			tests::sqlite_contract::scratch_root scratch{ "private-kill" };
			auto                                 config = configuration(scratch.path());
			{
				auto initial = open(config);
			}
			killed("INSERT INTO private_content", after, [&] {
				auto child = open(config);
				(void)create(*child, "killed exact");
			});
			auto store = open(config);
			auto list  = store->list_threads();
			expect(list && list->Threads.size() == (after ? 1U : 0U), "WAL restart retains whole committed turn or no uncommitted turn");
			auto first    = after ? private_revision_commit{ list->Threads.front(), store->list_revisions(list->Threads.front().Id)->Revisions.front().Revision } : create(*store);
			auto prepared = compose(*store, first.Revision);
			store.reset();
			killed("INSERT INTO private_draft", after, [&] {
				auto child = open(config);
				expect(child->commit(prepared.Write).has_value(), "child commits prepared composition");
			});
			store       = open(config);
			auto drafts = store->list_drafts();
			expect(drafts && drafts->size() == (after ? 1U : 0U), "kill at draft link commit never leaves partial candidate or link");
			if (!after) {
				expect(store->commit(prepared.Write).has_value(), "aborted composition safely reuses uncommitted frontier");
			}
			auto draft = store->load_draft(prepared.Draft.Draft.Draft.Id);
			expect(draft && draft->Draft.Draft.Body == prepared.Candidate.Body, "durable exact draft recovered");
			auto                       id = store->frontier()->NextIdentity;
			private_publication_record p{ .Id = review_publication_lineage_id{ id }, .Draft = draft->Draft.Draft.Id, .DraftRevision = draft->Draft.Revision, .Composition = draft->Draft.Composition, .Target = { draft->Binding, draft->Draft.Draft.Anchor }, .Actor = github_account_id{ 3 }, .ApprovedProseDigest = private_digest(draft->Draft.Draft.Body), .WireBodyDigest = private_digest(draft->Draft.Draft.Body), .LogicalPublication = "kill-logical", .Representation = "native", .ServerComment = "server-kill", .ReconciledIdentity = "receipt-kill" };
			private_store_write        write;
			write.ExpectedNextIdentity = id;
			write.Publications         = { p };
			store.reset();
			killed("INSERT INTO private_publication", after, [&] {
				auto child = open(config);
				expect(child->commit(write).has_value(), "child commits local receipt");
			});
			store             = open(config);
			auto publications = store->find_publication();
			expect(publications && publications->Records.size() == (after ? 1U : 0U), "kill at lineage commit is atomic and does not invoke any remote capability");
		}
		tests::sqlite_contract::scratch_root scratch{ "private-migration-kill" };
		auto                                 config = configuration(scratch.path());
		{
			auto ledger = make_review_ledger_store(config.Ledger);
			expect(ledger && (*ledger)->store_note({ .Id = local_note_id{ 1 }, .Note = { .Text = "migration exact" } }).has_value(), "legacy migration seed");
			ledger->reset();
			auto raw = detail::review_ledger_database::open(ledger_path(config), { });
			tests::sqlite_contract::restore_v6_fixture(raw->handle());
		}
		killed("CREATE TABLE private_composition", false, [&] { auto child = open(config); });
		sqlite3* raw = nullptr;
		expect(sqlite3_open(ledger_path(config).string().c_str(), &raw) == SQLITE_OK, "inspect killed migration");
		expect(*detail::scalar_integer(raw, "PRAGMA user_version") == 6 && *detail::scalar_integer(raw, "SELECT COUNT(*) FROM local_note") == 1, "kill rolls back complete schema and legacy migration");
		sqlite3_close(raw);
		auto recovered = open(config);
		expect(recovered->list_threads()->Threads.size() == 1, "migration retries atomically after kill");
	}
#endif
	void database_loss()
	{
		tests::sqlite_contract::scratch_root scratch{ "private-loss" };
		auto                                 config = configuration(scratch.path());
		auto                                 store  = open(config);
		auto                                 first  = create(*store);
		auto                                 old    = *store->frontier();
		store.reset();
		const auto neighboring = scratch.path() / "github-fixture-authority.txt";
		{
			std::ofstream out{ neighboring, std::ios::binary };
			out << "server comment 17 exact public body";
		}
		std::filesystem::remove(ledger_path(config));
		std::filesystem::remove(ledger_path(config).string() + "-wal");
		std::filesystem::remove(ledger_path(config).string() + "-shm");
		store = open(config);
		expect(store->frontier()->Scope.Incarnation != old.Scope.Incarnation && store->list_threads()->Threads.empty(), "ledger replacement creates new incarnation and no recovered private text");
		expect(!store->load_sources(std::array{ source_of(first.Revision) }), "old references cannot bind replacement ledger");
		std::ifstream input{ neighboring, std::ios::binary };
		std::string   server{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
		expect(server == "server comment 17 exact public body", "independently observed neighboring synthetic shared authority unchanged");
	}

	void simultaneous_allocations()
	{
		tests::sqlite_contract::scratch_root         scratch{ "private-race" };
		auto                                         config = configuration(scratch.path());
		auto                                         left   = open(config);
		auto                                         right  = open(config);
		auto                                         write  = new_thread_write(*left);
		std::barrier                                 ready{ 3 };
		private_store_result<private_store_frontier> a = std::unexpected(private_store_failure::unavailable), b = a;
		std::thread                                  first{ [&] {
			ready.arrive_and_wait();
			a = left->commit(write);
		} };
		std::thread second{ [&] {
			ready.arrive_and_wait();
			b = right->commit(write);
		} };
		ready.arrive_and_wait();
		first.join();
		second.join();
		expect(a.has_value() != b.has_value(), "concurrent writers have one allocation winner");
		expect((a ? b.error() : a.error()) == private_store_failure::conflict && left->list_threads()->Threads.size() == 1 && left->frontier()->NextIdentity == 4, "loser observes exact frontier conflict with no duplicate or partial object");
		auto missing = new_thread_write(*left);
		missing.Revisions.clear();
		auto before = *left->frontier();
		expect(!left->commit(missing) && left->frontier()->NextIdentity == before.NextIdentity && left->list_threads()->Threads.size() == 1, "missing required revision rejects partial header and allocation");
		auto wrong    = config;
		wrong.Account = github_account_id{ 99 };
		expect(make_private_review_store(wrong).error() == private_store_failure::conflict, "numeric scope cannot replace already bound ledger authority");
	}

}
auto main() -> int
{
	protocol_metadata_storage_and_secret_scan();
	run_codec_compatibility();
	publication_codec_compatibility();
	smoke();
	facade_thread_coherence();
	simultaneous_allocations();
	body_pages_and_cursors();
	exact_copy_and_late_draft_terminal();
	draft_lineage_and_conflicts();
	migration_bytes();
	retained_draft_schema_migration();
	quota_boundaries();
	findings_and_run_retention();
	full_thread_byte_budget();
	database_loss();
#if !defined(_WIN32)
	crash_transactions();
#endif
	std::cout << "private-review-storage-tests:pass\n";
}
