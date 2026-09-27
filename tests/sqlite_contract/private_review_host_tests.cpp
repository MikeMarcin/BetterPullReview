#include "../core/private_review_fixture.hpp"
#include "bpr/adapters/sqlite/private_review_store.hpp"
#include "bpr/application/agent_invocation_host.hpp"
#include "bpr/application/manual_executor.hpp"
#include "scratch_root.hpp"
#include <functional>
#include <iostream>
#include <sqlite3.h>
namespace {
	using namespace bpr;
	using namespace core;
	using namespace application;
	using namespace adapters::sqlite;
	using private_review_fixture::expect;
	struct fault_store : private_review_store_port
	{
		private_review_store_port& Real;
		bool                       FailCommit = false;
		std::size_t                Commits    = 0;
		explicit fault_store(private_review_store_port& aReal)
			: Real(aReal)
		{ }
		auto frontier() -> private_store_result<private_store_frontier> override { return Real.frontier(); }
		auto commit(const private_store_write& aWrite) -> private_store_result<private_store_frontier> override
		{
			++Commits;
			if (FailCommit) {
				return std::unexpected(private_store_failure::unavailable);
			}
			return Real.commit(aWrite);
		}
		auto claim_sources(std::span<const private_source_selection> aSelected) -> private_store_result<std::unique_ptr<private_source_use>> override { return Real.claim_sources(aSelected); }
		auto list_runs(std::optional<agent_run_id> aAfter) -> private_store_result<private_run_page> override { return Real.list_runs(aAfter); }
		auto list_threads(bool aArchived, std::optional<private_catalog_cursor> aCursor) -> private_store_result<private_catalog_page> override { return Real.list_threads(aArchived, aCursor); }
		auto load_thread(private_review_thread_id aId) -> private_store_result<private_review_thread> override { return Real.load_thread(aId); }
		auto list_revisions(private_review_thread_id aId, std::optional<private_revision_cursor> aCursor) -> private_store_result<private_revision_page> override { return Real.list_revisions(aId, aCursor); }
		auto read_revision_body(const private_body_cursor& aCursor) -> private_store_result<private_body_page> override { return Real.read_revision_body(aCursor); }
		auto load_run_details(std::span<const agent_run_id> aIds) -> private_store_result<std::vector<agent_run_provenance>> override { return Real.load_run_details(aIds); }
		auto load_sources(std::span<const private_source_reference> aSources) -> private_store_result<std::vector<private_source_observation>> override { return Real.load_sources(aSources); }
		auto load_finding(const private_source_reference& aSource) -> private_store_result<retained_agent_finding> override { return Real.load_finding(aSource); }
		auto load_composition(review_composition_id aId) -> private_store_result<private_composition_record> override { return Real.load_composition(aId); }
		auto load_candidate(const private_source_reference& aSource) -> private_store_result<review_composition_revision> override { return Real.load_candidate(aSource); }
		auto load_draft(review_draft_id aId) -> private_store_result<private_draft_record> override { return Real.load_draft(aId); }
		auto list_drafts() -> private_store_result<std::vector<private_draft_record>> override { return Real.list_drafts(); }
		auto find_publication(std::optional<std::string> aServer, std::optional<private_publication_cursor> aCursor) -> private_store_result<private_publication_page> override { return Real.find_publication(aServer, aCursor); }
		auto erase_thread(private_review_thread_id aId, std::uint64_t aVersion) -> private_store_result<void> override { return Real.erase_thread(aId, aVersion); }
		auto list_thread_dependents(private_review_thread_id aId, std::optional<private_dependency_cursor> aCursor) -> private_store_result<private_dependency_page> override { return Real.list_thread_dependents(aId, aCursor); }
		auto erase_local_object(private_object_reference aId, std::uint64_t aVersion) -> private_store_result<void> override { return Real.erase_local_object(aId, aVersion); }
		auto purge() -> private_store_result<void> override { return Real.purge(); }
	};
	struct sessions : agent_session_catalog_port
	{
		std::optional<agent_session> Latest;
		std::uint64_t                Next   = 1;
		review_ledger_store_port*    Ledger = nullptr;
		auto                         admit(const agent_session_admission& aRequest) -> std::expected<agent_session, ledger_store_failure> override
		{
			if (Ledger) {
				auto row = Ledger->admit_agent_session(aRequest);
				if (row) {
					Latest = *row;
				}
				return row;
			}
			Latest = agent_session{ agent_session_id{ Next++ }, aRequest.Profile, aRequest.Binding };
			return *Latest;
		}
		auto session_catalog() -> std::expected<agent_session_catalog_state, ledger_store_failure> override { return agent_session_catalog_state{ Latest, agent_session_id{ Next } }; }
	};
	struct workspace : agent_workspace_port
	{
		std::function<void()> OnMaterialize;
		auto                  materialize_agent_workspace(const agent_workspace_materialization_request& aRequest, std::stop_token) -> std::expected<owned_agent_workspace, agent_workspace_error> override
		{
			if (OnMaterialize) {
				OnMaterialize();
			}
			return owned_agent_workspace{ .Workspace = { .Policy = agent_workspace_policy::disposable_review_workspace, .PinnedHead = aRequest.Session.Binding.HeadOid, .Path = aRequest.ParentDirectory.string() }, .ApplicationSession = aRequest.ApplicationSession, .Authority = aRequest.Authority, .Session = aRequest.Session.Id };
		}
		auto cleanup_agent_workspace(const owned_agent_workspace&, std::stop_token) -> std::expected<void, agent_workspace_error> override { return { }; }
	};
	struct process : agent_process_port
	{
		std::size_t                                      Starts = 0;
		std::function<void(const agent_launch_request&)> OnStart;
		agent_run_outcome                                Outcome  = agent_run_outcome::success;
		bool                                             WrongRun = false;
		std::optional<agent_finding>                     Finding;
		std::function<void(agent_run_result&)>           ChangeResult;
		agent_run_result                                 Last;
		std::optional<agent_launch_request>              Request;
		auto                                             run(const agent_launch_request& aRequest, int, const agent_output_sink&, std::stop_token) -> agent_run_result override
		{
			++Starts;
			Request = aRequest;
			if (OnStart) {
				OnStart(aRequest);
			}
			if (aRequest.PrivateInputClaim) {
				aRequest.PrivateInputClaim->release();
			}
			Last = { .Session = aRequest.Session, .Outcome = Outcome, .ExitCode = 0, .Run = WrongRun ? agent_run_id{ aRequest.Run.value() + 1 } : aRequest.Run, .Metadata = aRequest.Metadata, .AssistantItems = { "Full answer: ", "detail beyond a finding\r\nα" } };
			if (Finding) {
				Last.Findings = agent_finding_payload{ aRequest.Session, { *Finding } };
			}
			if (ChangeResult) {
				ChangeResult(Last);
			}
			return Last;
		}
		void cancel() noexcept override { }
	};
	struct projection : agent_invocation_projection_port
	{
		std::vector<private_review_delta> Deltas;
		std::vector<private_run_error>    Errors;
		std::vector<agent_run_result>     Results;
		std::vector<std::string>          Refusals;
		std::function<void()>             OnDelta;
		void                              offer(std::span<const agent_profile>) override { }
		void                              refuse(std::string_view aReason) override { Refusals.emplace_back(aReason); }
		void                              admit(const agent_invocation_projection_identity&) override { }
		void                              start(const agent_invocation_projection_identity&, const prepared_workspace&) override { }
		void                              stream(const agent_invocation_projection_identity&, const agent_stream_event&) override { }
		void                              finish(const agent_invocation_projection_identity&, const agent_run_result& aResult) override { Results.push_back(aResult); }
		void                              present_session_catalog(const agent_session_catalog_projection&) override { }
		void                              private_changed(const agent_invocation_projection_identity&, const private_review_delta& aDelta) override
		{
			Deltas.push_back(aDelta);
			if (OnDelta) {
				OnDelta();
			}
		}
		void private_refused(const agent_invocation_projection_identity&, const private_run_error& aError) override { Errors.push_back(aError); }
	};
	struct fixture
	{
		tests::sqlite_contract::scratch_root       Scratch{ "private-host" };
		private_review_configuration               Config{ { Scratch.path(), "github.com", "octo", "example", 5 }, bpr_profile_id{ 1 }, git_host_id{ 2 }, github_account_id{ 3 }, repository_id{ 4 }, pull_request_id{ 5 } };
		std::unique_ptr<private_review_store_port> Store  = std::move(*make_private_review_store(Config));
		std::unique_ptr<review_ledger_store_port>  Ledger = std::move(*make_review_ledger_store(Config.Ledger));
		std::unique_ptr<private_review_store_port> Peer   = std::move(*make_private_review_store(Config));
		fault_store                                Fault{ *Store };
		private_review_application_host            Private{ Fault };
		sessions                                   Sessions;
		workspace                                  Workspace;
		process                                    Process;
		manual_executor                            Effects, Decisions;
		projection                                 Projection;
		std::vector<agent_profile>                 Profiles = built_in_agent_profiles();
		private_review_binding                     Binding  = private_review_fixture::binding();
		pull_request_authority                     Authority;
		std::unique_ptr<agent_invocation_host>     Host;
		fixture()
		{
			Binding.Scope   = Store->frontier()->Scope;
			Sessions.Ledger = Ledger.get();
			Authority       = { .Profile = host_profile_id{ 1 }, .ProfileRevision = github_profile_revision{ 1 }, .Host = Binding.Scope.Host, .HostAuthority = Binding.HostAuthority, .Account = Binding.Scope.Account, .Viewer = github_viewer_actor_id{ 3 }, .Repository = Binding.Scope.Repository, .Cache = private_review_fixture::opaque<repository_cache_scope_id>(), .PullRequest = Binding.Scope.PullRequest, .Number = pull_request_number{ 5 }, .Scope = Binding.LeftAnchor.Authority };
			Host            = std::make_unique<agent_invocation_host>(Profiles, std::vector<agent_host_policy>{ built_in_agent_policy(Binding.Scope.Host, built_in_agent_host_kind::github_com) }, Scratch.path(), Sessions, Workspace, Process, [](const std::string&) -> std::optional<std::string> { return { }; }, Effects, Decisions, Projection, &Private);
			Host->observe(application_session_id{ 1 }, Authority, Binding.Revision);
		}
		auto request(std::string aPrompt = "Why is this changed?") -> agent_invocation_request
		{
			return { .ApplicationSession = application_session_id{ 1 }, .Authority = Authority, .Profile = Profiles.front().Id, .Context = { .ApplicationSession = application_session_id{ 1 }, .Binding = Binding.Revision, .PullRequestUrl = "https://github.com/example/repo/pull/5", .ChatMessage = std::move(aPrompt) }, .PrivateRun = private_run_intent{ .Binding = Binding } };
		}
		void drain()
		{
			for (unsigned i = 0; i < 20 && (Effects.pending() || Decisions.pending()); ++i) {
				Decisions.drain();
				Effects.drain();
			}
			expect(Host->is_quiescent(), "invocation retires after storage and cleanup");
		}
		void send(agent_invocation_request aRequest)
		{
			Host->dispatch(std::move(aRequest));
			drain();
		}
		auto seeded(std::string aBody = "SELECTED_CANARY") -> private_revision_commit
		{
			auto f    = *Store->frontier();
			auto next = f.NextIdentity;
			auto row  = *create_private_thread(private_review_thread_id{ next }, { Binding, private_pr_subject{ } }, { private_review_turn_id{ next + 1 }, private_review_turn_revision_id{ next + 2 }, private_vertex_order{ next + 2 }, private_reviewer_actor{ }, std::move(aBody) });
			expect(Store->commit({ .ExpectedNextIdentity = next, .Threads = { { row.Thread, 0 } }, .Revisions = { row.Revision } }).has_value(), "seed exact immutable source");
			return row;
		}
		auto edit(const private_revision_commit& aRow) -> private_store_result<private_store_frontier>
		{
			auto f = Peer->frontier();
			if (!f) {
				return std::unexpected(f.error());
			}
			auto thread = *Peer->load_thread(aRow.Thread.Id);
			auto row    = *edit_private_revision(thread, thread.Version, aRow.Revision, aRow.Revision.Identity.Revision, { aRow.Revision.Identity.Turn, private_review_turn_revision_id{ f->NextIdentity }, private_vertex_order{ f->NextIdentity }, private_reviewer_actor{ }, "EDITED" });
			return Peer->commit({ .ExpectedNextIdentity = f->NextIdentity, .Threads = { { row.Thread, thread.Version } }, .Revisions = { row.Revision } });
		}
	};
	void composition_and_ordering()
	{
		fixture f;
		auto    source = f.seeded();
		(void)f.seeded("UNSELECTED_SECRET_CANARY");
		auto input                = f.request();
		auto ref                  = source_of(source.Revision);
		input.PrivateRun->Sources = { { ref, ref.Key, false } };
		f.Process.OnStart         = [&](const agent_launch_request& aRequest) {
			expect(aRequest.Run.value() != 0 && aRequest.AdmittedRun && aRequest.PrivateInputClaim->permitted(), "existing process gets admitted run and active claim");
			auto run = f.Peer->load_run_details(std::array{ aRequest.Run });
			expect(run && run->front().Outcome == private_run_outcome::admitted, "separate connection observes committed admission before launch");
			auto threads = f.Peer->list_threads();
			expect(threads && threads->Threads.size() == 3, "prompt committed atomically before launch");
			expect(aRequest.DeferredContext.find("SELECTED_CANARY") != std::string::npos && aRequest.DeferredContext.find("UNSELECTED_SECRET_CANARY") == std::string::npos, "only selected body reaches actual invocation process input");
			auto blocked = f.edit(source);
			expect(!blocked && blocked.error() == private_store_failure::busy, "second connection cannot edit before deferred input acknowledgement");
			aRequest.PrivateInputClaim->release();
			expect(f.edit(source).has_value(), "source edit succeeds after exact input claim release");
		};
		f.send(input);
		expect(f.Process.Starts == 1 && f.Projection.Errors.empty() && f.Projection.Deltas.size() == 2, "one run and two changed identity deltas");
		auto prompt     = f.Projection.Deltas.front();
		auto terminal   = f.Projection.Deltas.back();
		auto transcript = f.Store->list_revisions(*prompt.Thread);
		expect(transcript && transcript->Revisions.size() == 2 && transcript->Revisions.back().Revision.Body == "Full answer: detail beyond a finding\r\nα", "complete response retained instead of finding summary");
		expect(std::get<private_agent_actor>(transcript->Revisions.back().Revision.Actor).Run == terminal.Run, "assistant actor names exact persisted run");
		auto retained = f.Store->load_run_details(std::array{ terminal.Run });
		expect(retained && retained->front().OutputItemBytes == std::vector<std::uint64_t>{ 13, std::string{ "detail beyond a finding\r\nα" }.size() }, "item boundaries survive durable provenance");
		// A continued session still gets a new run, with an independently committed prompt.
		f.Process.OnStart                        = { };
		auto second                              = f.request("A follow-up");
		second.ContinuedSession                  = *f.Sessions.Latest;
		second.PrivateRun->Thread                = prompt.Thread;
		second.PrivateRun->ExpectedThreadVersion = f.Store->load_thread(*prompt.Thread)->Version;
		f.send(second);
		expect(f.Process.Request->Run != terminal.Run && f.Process.Request->Session == retained->front().Session, "continued session has distinct durable runs");
		for (unsigned i = 0; i < maximum_retained_agent_sessions + 1; ++i) {
			expect(f.Ledger->admit_agent_session({ f.Profiles.front().Id, f.Binding.Revision }).has_value(), "catalog pruning through production admission");
		}
		f.Sessions.Latest.reset();
		auto reopened = make_private_review_store(f.Config);
		expect(reopened && (*reopened)->load_run_details(std::array{ terminal.Run })->front() == retained->front(), "reopen and catalog prune preserve exact run independently");
		expect((*reopened)->list_revisions(*prompt.Thread)->Revisions.size() == 4, "reopen retains both prompt and complete response pairs");
	}
	void refusals_and_lifecycle()
	{
		{
			fixture f;
			f.Fault.FailCommit = true;
			f.send(f.request());
			expect(f.Process.Starts == 0 && f.Store->list_threads()->Threads.empty() && f.Store->list_runs()->Runs.empty(), "atomic admission failure starts no process and leaves no prompt");
		}
		{
			fixture f;
			auto    source            = f.seeded();
			auto    input             = f.request();
			auto    ref               = source_of(source.Revision);
			input.PrivateRun->Sources = { { ref, ref.Key, false } };
			f.Workspace.OnMaterialize = [&] { expect(f.edit(source).has_value(), "edit after admission before claim"); };
			f.send(input);
			expect(f.Process.Starts == 0 && !f.Projection.Errors.empty(), "last dispatch source recheck refuses intervening edit");
		}
		{
			fixture f;
			f.Process.OnStart = [&](const agent_launch_request&) { f.Fault.FailCommit = true; };
			f.send(f.request());
			expect(f.Projection.Results.back().Outcome == agent_run_outcome::private_storage_failure && f.Store->list_runs()->Runs.front().Outcome == private_run_outcome::admitted, "terminal storage failure cannot claim durable success");
			f.Fault.FailCommit = false;
			auto recovery      = f.Private.recover();
			expect(recovery && recovery->Runs.front().Outcome == private_run_outcome::interrupted && f.Process.Starts == 1, "restart recovery marks unknown execution without retrying");
		}
		{
			fixture f;
			f.Process.WrongRun = true;
			f.send(f.request());
			expect(f.Store->list_runs()->Runs.front().Outcome == private_run_outcome::protocol_failure && f.Store->list_revisions(*f.Projection.Deltas.front().Thread)->Revisions.size() == 1, "mismatched run cannot create assistant turn");
		}
		for (auto outcome : { agent_run_outcome::cancelled, agent_run_outcome::timeout, agent_run_outcome::nonzero_exit, agent_run_outcome::protocol_failure }) {
			fixture f;
			f.Process.Outcome = outcome;
			f.send(f.request());
			auto turns = f.Store->list_revisions(*f.Projection.Deltas.front().Thread);
			expect(turns && turns->Revisions.size() == 2 && !eligible_private_revision(turns->Revisions.back().Revision), "partial failure/cancel/timeout never becomes complete eligible source");
		}
		{
			fixture f;
			f.Host->dispatch(f.request());
			f.Decisions.drain();
			f.Host->cancel();
			f.drain();
			expect(f.Process.Starts == 0 && f.Store->list_runs()->Runs.empty(), "cancel before admission dispatches nothing");
		}
		{
			fixture f;
			f.Process.OnStart = [&](const agent_launch_request&) {auto moved=f.Binding.Revision; moved.HeadOid=*parse_git_commit_oid("3333333333333333333333333333333333333333");f.Host->observe(application_session_id{1},f.Authority,moved); };
			f.send(f.request());
			expect(f.Projection.Results.back().Outcome == agent_run_outcome::stale_result && f.Store->list_runs()->Runs.front().Outcome == private_run_outcome::succeeded, "validated original complete result persists before stale presentation");
		}
		{
			fixture f;
			f.Projection.OnDelta = [&] { f.Host->clear(); };
			f.send(f.request());
			expect(f.Process.Starts == 1 && f.Store->list_runs()->Runs.size() == 1, "reentrant projection keeps original durability identity");
		}
		{
			fixture f;
			f.Host->dispatch(f.request());
			f.Decisions.drain();
			f.Host->shutdown();
			f.drain();
			expect(f.Process.Starts == 0, "shutdown drains queued work without launch");
		}
	}
	void cancellation_wins_over_late_success()
	{
		fixture f;
		f.Process.OnStart = [&](const agent_launch_request&) { f.Host->cancel(); };
		f.send(f.request());
		auto turns = f.Store->list_revisions(*f.Projection.Deltas.front().Thread);
		expect(f.Store->list_runs()->Runs.front().Outcome == private_run_outcome::cancelled && !eligible_private_revision(turns->Revisions.back().Revision), "explicit cancellation cannot be overwritten by a late success response");
	}
	void malformed_terminal()
	{
		for (unsigned mode = 0; mode < 5; ++mode) {
			fixture f;
			f.Process.ChangeResult = [mode](agent_run_result& r) {
				if (mode == 0) {
					r.AssistantItems.resize(257, "x");
				}
				if (mode == 1) {
					r.AssistantItems = { std::string(maximum_private_assistant_body_bytes + 1, 'x') };
				}
				if (mode == 2) {
					r.Findings = agent_finding_payload{ agent_session_id{ 999 }, { } };
				}
				if (mode == 3) {
					r.Findings = agent_finding_payload{ r.Session, { agent_finding{ } } };
				}
				if (mode == 4) {
					r.Metadata.Requested.Model = "wrong";
				}
			};
			f.send(f.request());
			expect(f.Projection.Results.back().Outcome == agent_run_outcome::protocol_failure && f.Store->list_runs()->Runs.front().Outcome == private_run_outcome::protocol_failure, "malformed terminal becomes durable protocol failure rather than stranded admission");
			auto turns = f.Store->list_revisions(*f.Projection.Deltas.front().Thread);
			expect(turns && (turns->Revisions.size() == 1 || !eligible_private_revision(turns->Revisions.back().Revision)), "invalid terminal cannot retain eligible complete response");
		}
		fixture f;
		f.Process.ChangeResult = [](agent_run_result& r) { r.AssistantItems.clear(); };
		f.send(f.request());
		expect(f.Store->list_revisions(*f.Projection.Deltas.front().Thread)->Revisions.size() == 2, "empty validated response retains exact empty assistant turn");
	}
	void findings_and_duplicate_terminal()
	{
		fixture f;
		f.Process.Finding = agent_finding{ .Provider = f.Profiles.front().Id, .Path = "file.cpp", .Lines = { 1, 2 }, .Anchor = { .Line = source_line_ref{ private_review_fixture::opaque<file_comparison_digest>(), diff_side::right, 1 } }, .Claim = "finding claim", .Evidence = "synthetic evidence", .Consequence = "consequence", .SuggestedComment = "suggestion", .RequiredContext = "context", .AnalyzedBaseOid = f.Binding.Revision.BaseOid, .AnalyzedHeadOid = f.Binding.Revision.HeadOid };
		f.send(f.request());
		expect(f.Projection.Errors.empty() && f.Projection.Deltas.back().Findings.size() == 1, "validated finding persisted with its exact run");
		auto source  = f.Projection.Deltas.back().Findings.front();
		auto finding = f.Store->load_finding(source);
		expect(finding && finding->Finding == *f.Process.Finding, "all finding facts retain without anonymous promotion");
		auto linked = f.Private.reply_to_finding({ source, source.Key, false }, "Reviewer asks about the finding");
		expect(linked && linked->Outcome == private_run_outcome::succeeded, "explicit reviewer reply links original finding without changing run outcome");
		auto header = f.Store->load_thread(*linked->Thread);
		expect(header && std::get<private_finding_reference>(std::get<exact_private_subject>(header->Binding).Subject).Run == std::get<private_finding_source>(source.Key).Run, "thread subject retains original run and finding identity");
		expect(!f.Store->erase_local_object({ private_object_kind::finding, source.Order.value() }), "retained finding link prevents deletion of its source");
		auto input                = f.request("Use exact finding");
		input.PrivateRun->Sources = { { source, source.Key, false } };
		f.send(input);
		expect(f.Process.Starts == 2 && f.Process.Request->DeferredContext.find("synthetic evidence") != std::string::npos && f.Process.Request->DeferredContext.find("finding claim") != std::string::npos, "full selected finding snapshot is dispatched through same path");
		private_run_admission admission{ .Launch = { .AdmittedRun = f.Process.Request->AdmittedRun } };
		expect(!f.Private.finish(admission, f.Process.Last, { .Binding = f.Binding.Revision }), "duplicate terminal cannot allocate a second response or finding");
		auto reopened = make_private_review_store(f.Config);
		expect(reopened && (*reopened)->load_finding(source)->Finding == finding->Finding, "finding readable after restart");
	}

	void source_refusals()
	{
		for (unsigned mode = 0; mode < 7; ++mode) {
			fixture f;
			auto    source            = f.seeded();
			auto    ref               = source_of(source.Revision);
			auto    input             = f.request();
			input.PrivateRun->Sources = { { ref, ref.Key, false } };
			if (mode == 0) {
				input.PrivateRun->Sources.push_back(input.PrivateRun->Sources.front());
			}
			if (mode == 1) {
				input.PrivateRun->Sources.front().Source.Digest = private_digest("tampered");
			}
			if (mode == 2) {
				input.PrivateRun->Sources.front().Source.Scope.Account = github_account_id{ 99 };
			}
			if (mode == 3) {
				expect(f.Store->erase_thread(source.Thread.Id, source.Thread.Version).has_value(), "delete selected source");
			}
			if (mode == 4) {
				expect(f.edit(source).has_value(), "edit selected source");
			}
			if (mode == 5) {
				input.PrivateRun->Sources.resize(257, input.PrivateRun->Sources.front());
			}
			if (mode == 6) {
				input.Context.IncludedNotes = { local_note_id{ 1 } };
			}
			f.send(input);
			expect(f.Process.Starts == 0, "unchecked/deleted/edited/duplicate/cross-review/overlimit source never launches");
		}
	}
}
int main()
{
	composition_and_ordering();
	refusals_and_lifecycle();
	source_refusals();
	cancellation_wins_over_late_success();
	malformed_terminal();
	findings_and_duplicate_terminal();
	std::cout << "private-review-host-tests:pass\n";
}
