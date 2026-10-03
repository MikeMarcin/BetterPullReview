#include "../core/private_review_fixture.hpp"
#include "bpr/adapters/sqlite/private_review_store.hpp"
#include "bpr/application/agent_invocation_host.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_composition_application_host.hpp"
#include "bpr/application/review_mutation_port.hpp"
#include "bpr/application/review_submission_port.hpp"
#include "bpr/application/shared_draft_application_host.hpp"
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
		/// [[BPR-1211]]. The catalog answers later, on the decision lane.
		decision_executor* Lane = nullptr;
		void               admit(const agent_session_admission& aRequest, std::function<void(std::expected<agent_session, ledger_store_failure>)> aDone) override
		{
			auto answer = std::expected<agent_session, ledger_store_failure>{ };
			if (Ledger) {
				answer = Ledger->admit_agent_session(aRequest);
				if (answer) {
					Latest = *answer;
				}
			} else {
				Latest = agent_session{ agent_session_id{ Next++ }, aRequest.Profile, aRequest.Binding };
				answer = *Latest;
			}
			deliver([done = std::move(aDone), answer] { done(answer); });
		}
		void session_catalog(std::function<void(std::expected<agent_session_catalog_state, ledger_store_failure>)> aDone) override
		{
			deliver([done = std::move(aDone), answer = agent_session_catalog_state{ Latest, agent_session_id{ Next } }] { done(answer); });
		}
		void deliver(std::function<void()> aAnswer)
		{
			if (Lane != nullptr) {
				Lane->post(std::move(aAnswer));
			} else {
				aAnswer();
			}
		}
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
	// Every shared write capability lives beside the local composition. No local
	// operation receives one. Counters make an accidental join observable.
	struct mutation_canary : review_mutation_port, review_submission_port
	{
		using review_mutation_port::submit_and_reconcile;
		std::size_t Inline = 0, Resolve = 0, WholeReview = 0, Tour = 0;
		auto        observe_target() -> std::expected<review_mutation_target, review_mutation_failure> override { return std::unexpected(review_mutation_failure::unavailable); }
		auto        capabilities() const -> review_provider_capabilities override { return { }; }
		auto        submit(const review_mutation_request&) -> std::expected<review_mutation_receipt, review_mutation_failure> override
		{
			++Inline;
			return std::unexpected(review_mutation_failure::unavailable);
		}
		auto resolve_thread(const review_thread_resolution_request&, std::stop_token) -> std::expected<review_thread_resolution_receipt, review_mutation_failure> override
		{
			++Resolve;
			return std::unexpected(review_mutation_failure::unavailable);
		}
		auto pending(std::stop_token) -> std::expected<std::vector<review_submission_request>, review_mutation_failure> override { return std::vector<review_submission_request>{ }; }
		auto submit_and_reconcile(const review_submission_request& aRequest, std::stop_token) -> review_submission_result override
		{
			++WholeReview;
			return { .Request = aRequest };
		}
		auto reconcile(const review_submission_request& aRequest, std::stop_token) -> review_submission_result override { return { .Request = aRequest }; }
		auto publish_tour(const staged_review_plan&, std::stop_token, staged_child_authorizer) -> staged_publication_result override
		{
			++Tour;
			return { };
		}
		void assert_unused() const { expect(Inline == 0 && Resolve == 0 && WholeReview == 0 && Tour == 0, "all inline/resolve/whole-review/tour mutation capabilities receive zero calls"); }
	};
	struct fixture
	{
		mutation_canary                            Mutations;
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
			Sessions.Lane   = &Decisions;
			Authority       = { .Profile = host_profile_id{ 1 }, .ProfileRevision = core::github_profile_revision{ 1 }, .Host = Binding.Scope.Host, .HostAuthority = Binding.HostAuthority, .Account = Binding.Scope.Account, .Viewer = github_viewer_actor_id{ 3 }, .Repository = Binding.Scope.Repository, .Cache = private_review_fixture::opaque<repository_cache_scope_id>(), .PullRequest = Binding.Scope.PullRequest, .Number = pull_request_number{ 5 }, .Scope = Binding.LeftAnchor.Authority };
			Host            = std::make_unique<agent_invocation_host>(Profiles, std::vector<agent_host_policy>{ built_in_agent_policy(Binding.Scope.Host, built_in_agent_host_kind::github_com) }, Scratch.path(), Sessions, Workspace, Process, [](const std::string&) -> std::optional<std::string> { return { }; }, Effects, Decisions, Projection, &Private);
			Host->observe(application_session_id{ 1 }, Authority, Binding.Revision);
		}
		auto request(std::string aPrompt = "Why is this changed?") -> agent_invocation_request
		{
			return { .ApplicationSession = application_session_id{ 1 }, .Authority = Authority, .Profile = Profiles.front().Id, .Context = { .ApplicationSession = application_session_id{ 1 }, .Binding = Binding.Revision, .PullRequestUrl = "https://github.com/example/repo/pull/5", .ChatMessage = std::move(aPrompt) }, .PrivateRun = private_run_intent{ .Binding = Binding } };
		}
		~fixture() { Mutations.assert_unused(); }
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
	void durable_left_reply()
	{
		fixture f;
		auto    index        = private_review_fixture::index();
		f.Binding.Comparison = index.diff().Alignment.Digest;
		review_composition_application_host host(*f.Store);
		const auto                          line = source_line_ref{ *f.Binding.Comparison, diff_side::left, 1 };
		expect(!make_composition_target(f.Binding, line, index), "private compositions still require current-head right targets");
		expect(!host.allocate_reply(f.Binding, line, index, ""), "reply requires an explicit thread identity");
		auto reply = host.allocate_reply(f.Binding, line, index, "thread-left");
		expect(reply && reply->records().size() == 1 && reply->records().front().Draft.Draft.ReplyToThread == "thread-left"
				   && !reply->records().front().Draft.Composition,
			   "ordinary durable reply preserves its exact left side without composition lineage");
		auto reopened = host.restore();
		expect(reopened && reopened->records().front().Draft == reply->records().front().Draft, "ordinary reply restores its durable identity");
	}

	void local_compositions()
	{
		fixture f;
		auto    index                              = private_review_fixture::index();
		f.Binding.Comparison                       = index.diff().Alignment.Digest;
		auto                                target = *make_composition_target(f.Binding, { *f.Binding.Comparison, diff_side::right, 1 }, index);
		auto                                source = f.seeded("Exact copy\r\nα");
		auto                                ref    = source_of(source.Revision);
		review_composition_application_host compositions(f.Fault);
		auto                                blank = compositions.create({ f.Binding, review_composition_mode::reviewer_authored, { { ref, ref.Key, false } }, target, { } });
		expect(blank && !blank->State.Candidate && compositions.revise(blank->State.Id, blank->Version, "Written by reviewer").has_value(), "manual workspace opens empty then stores reviewer prose without an agent");
		auto copied = compositions.create({ f.Binding, review_composition_mode::exact_copy, { { ref, ref.Key, false } }, target, { } });
		expect(copied.has_value(), "exact copy creates persisted workspace and candidate");
		expect(f.Store->load_candidate(*copied->State.Candidate)->Body == source.Revision.Body, "exact copy preserves every byte");
		auto edited = compositions.revise(copied->State.Id, copied->Version, "Reviewed prose\r\nα");
		expect(edited.has_value(), "reviewer edit creates successor");
		auto candidate = *f.Store->load_candidate(*edited->State.Candidate);
		expect(std::holds_alternative<private_reviewer_actor>(candidate.Actor) && candidate.Derivation.front().Source == *copied->State.Candidate, "edit retains immutable predecessor provenance");
		std::array requests{ review_composition_acceptance{ edited->State.Id, edited->Version, candidate.Identity } };
		f.Fault.FailCommit = true;
		expect(!compositions.accept(requests, f.Binding, index, review_draft_scope_id{ 9 }) && f.Store->list_drafts()->empty(), "failed acceptance stores neither accepted state nor draft");
		expect(f.Store->load_composition(edited->State.Id)->State.Lifecycle == review_composition_lifecycle::editing, "workspace remains editable after failure");
		f.Fault.FailCommit = false;
		auto accepted      = compositions.accept(requests, f.Binding, index, review_draft_scope_id{ 9 });
		expect(accepted && accepted->records().size() == 1, "accept atomically creates existing revisioned draft");
		auto draft = accepted->records().front().Draft;
		expect(draft.Composition && draft.Draft.Body == candidate.Body && draft.Scope.value() == 0 && draft.Draft.State == review_draft_state::drafting, "durable draft retains exact linkage and no runtime scope");
		expect(!compositions.accept(requests, f.Binding, index, review_draft_scope_id{ 9 }), "duplicate acceptance does not create second draft");
		auto                                reopened = std::move(*make_private_review_store(f.Config));
		review_composition_application_host restart(*reopened);
		auto                                restored = restart.restore();
		expect(restored && restored->records().front().Draft == draft, "restart recovers exact accepted draft");
		struct contexts : shared_draft_context_port
		{
			auto current(const bpr_window_context&) const noexcept -> bool override { return true; }
		} current;
		shared_draft_authority        authority{ .BprProfile = f.Binding.Scope.Profile, .ProfileAuthority = { .Profile = host_profile_id{ 1 }, .ProfileRevision = application::github_profile_revision{ 1 }, .Host = f.Binding.Scope.Host, .Hostname = github_canonical_hostname{ "github.com" }, .ProbeContractRevision = github_probe_contract_revision{ 1 } }, .HostAuthority = f.Binding.HostAuthority, .Session = application_session_id{ 1 }, .Generation = github_identity_generation{ 1 }, .Account = f.Binding.Scope.Account, .Mode = github_access_mode::authenticated, .Target = { f.Binding.Scope.Host, f.Binding.Scope.Repository, f.Binding.Scope.PullRequest, f.Binding.Revision.HeadOid } };
		shared_draft_application_host shared(authority, review_draft_scope_id{ 9 }, current);
		std::size_t                   publications = 0;
		auto                          consumer     = *shared.attach(authority, { f.Binding.Scope.Profile, bpr_window_id{ 1 }, bpr_window_generation{ 1 } }, [&](const auto& d) { ++publications; expect(reopened->load_draft(d.Draft.Id)->Draft.Revision == d.Revision, "persistence precedes shared draft projection"); });
		expect(shared.adopt(authority, *restored).has_value() && publications == 1, "existing draft owner reattaches and fans out durable value");
		auto command = revisioned_review_draft_command{ { draft.Draft.Id, review_draft_command_kind::edit, "Final reviewer edit" }, draft.Revision, false, { }, review_draft_scope_id{ 9 } };
		expect(shared.apply(consumer, command).Failure == shared_draft_failure::durable_required, "volatile path cannot overwrite durable draft");
		auto changed = restart.change_draft(command);
		expect(changed && shared.adopt(authority, *changed).has_value(), "linked edit commits reviewer successor before adoption");
		expect(changed->records().front().Draft.Composition->Revision > draft.Composition->Revision, "linked draft edit advances immutable candidate identity");
		expect(!shared.adopt(authority, *restored), "late old restore cannot overwrite edited draft");
		expect(reopened->load_candidate(candidate.Identity)->Body == candidate.Body && reopened->read_revision_body({ ref, 0 })->Bytes == source.Revision.Body, "accept/edit/restart preserve exact source and prior candidate bytes");
		expect(f.Process.Starts == 0 && f.Store->find_publication()->Records.empty(), "manual copy edit accept restart dispatch no agent or publication");
	}
	void agent_composition()
	{
		fixture f;
		auto    source = f.seeded();
		(void)f.seeded("UNSELECTED_PRIVATE");
		auto                                ref = source_of(source.Revision);
		review_composition_application_host compositions(f.Fault);
		auto                                workspace = compositions.create({ f.Binding, review_composition_mode::agent_assisted, { { ref, ref.Key, false } }, { }, { } });
		expect(workspace && !workspace->State.Candidate, "agent composition creation never auto invokes");
		auto request               = f.request("");
		request.Context.EntryPoint = agent_entry_point::workshop_comment;
		request.PrivateRun         = *compositions.agent_intent(workspace->State.Id, workspace->Version);
		f.Process.OnStart          = [&](const auto& launch) { expect(launch.DeferredContext.find("SELECTED_CANARY") != std::string::npos && launch.DeferredContext.find("UNSELECTED_PRIVATE") == std::string::npos, "squash uses exact selected input through existing invocation"); };
		f.send(request);
		auto saved = f.Store->load_composition(workspace->State.Id);
		expect(f.Process.Starts == 1 && saved && saved->State.Candidate && saved->State.Lifecycle == review_composition_lifecycle::editing && f.Store->list_drafts()->empty(), "one explicit run produces only an unaccepted proposal");
		auto candidate = f.Store->load_candidate(*saved->State.Candidate);
		expect(candidate && std::get<private_agent_actor>(candidate->Actor).Run == f.Process.Last.Run && candidate->Body == "Full answer: detail beyond a finding\r\nα", "proposal retains complete output and exact run actor");
		expect(f.Projection.Deltas.back().Candidate == saved->State.Candidate, "terminal delta names immutable candidate only");
		expect(!compositions.agent_intent(saved->State.Id, workspace->Version), "stale invocation intent refused");
	}

	/// [[BPR-1245]], BPR-1245-workshop-v001. A comment-workshop press records the
	/// stored draft as one immutable reviewer revision and asks with it alone.
	void workshop_press()
	{
		fixture f;
		auto    index        = private_review_fixture::index();
		f.Binding.Comparison = index.diff().Alignment.Digest;
		review_composition_application_host drafts(*f.Store);
		auto                                allocated = drafts.allocate_draft(*make_composition_target(f.Binding, { *f.Binding.Comparison, diff_side::right, 1 }, index), index, "WORKSHOP_FIRST");
		expect(allocated && allocated->records().size() == 1, "seed one durable inline draft");
		const auto draft  = allocated->records().front().Draft;
		auto       edited = drafts.change_draft({ { draft.Draft.Id, review_draft_command_kind::edit, "WORKSHOP_DRAFT\tat press" }, draft.Revision, false, { }, review_draft_scope_id{ 1 } });
		expect(edited.has_value(), "reviewer edits the draft before the press");
		const auto threadsBefore = f.Store->list_threads()->Threads.size();
		auto       press         = f.Private.record_workshop_press(f.Binding, draft.Draft.Id, index);
		expect(press && press->Run.Sources.size() == 1 && press->Run.Purpose == private_run_purpose::findings && !press->Run.Thread, "press yields one findings run over one source");
		expect(f.Store->list_threads()->Threads.size() == threadsBefore + 1, "press adds one thread to Private history");
		const auto source   = press->Run.Sources.front().Source;
		auto       recorded = f.Store->read_revision_body({ source, 0 });
		expect(recorded && recorded->Bytes == "WORKSHOP_DRAFT\tat press", "the revision holds the draft's stored text at the press");
		auto header = f.Store->load_thread(press->Thread);
		expect(header && std::get<source_line_ref>(std::get<exact_private_subject>(header->Binding).Subject) == draft.Draft.Anchor, "the thread is about the draft's line");
		auto turns = f.Store->list_revisions(press->Thread);
		expect(turns && turns->Revisions.size() == 1 && std::holds_alternative<private_reviewer_actor>(turns->Revisions.front().Revision.Actor), "the revision is the reviewer's");

		const auto workshop = [&f](private_run_intent aRun) {
			auto request                      = f.request("");
			request.Context.EntryPoint        = agent_entry_point::workshop_comment;
			request.Context.WorkshopOperation = workshop_operation::clarify;
			request.PrivateRun                = std::move(aRun);
			return request;
		};
		f.Process.OnStart = [&](const agent_launch_request& aRequest) {
			const auto& context = aRequest.DeferredContext;
			expect(context.find("private.count\t1\n") != std::string::npos && context.find("private.0.body\tWORKSHOP_DRAFT\\tat press\n") != std::string::npos, "the agent reads the recorded revision");
			expect(context.find("draft.count\t0\n") != std::string::npos && context.find("note.count\t0\n") != std::string::npos && context.find("workshop_operation\tclarify\n") != std::string::npos, "no mutable draft or note crosses; the operation does");
		};
		f.send(workshop(press->Run));
		expect(f.Process.Starts == 1 && f.Projection.Errors.empty(), "the press's run starts with its one source");
		auto runs = f.Store->list_runs();
		expect(runs && runs->Runs.size() == 1 && runs->Runs.front().EntryPoint == agent_entry_point::workshop_comment && runs->Runs.front().SelectedPrivateSources == press->Run.Sources, "the run's provenance names the press and its one source");
		f.Process.OnStart = { };

		auto later = drafts.change_draft({ { draft.Draft.Id, review_draft_command_kind::edit, "WORKSHOP_LATER" }, edited->records().front().Draft.Revision, false, { }, review_draft_scope_id{ 1 } });
		expect(later && f.Store->read_revision_body({ source, 0 })->Bytes == "WORKSHOP_DRAFT\tat press", "a later edit leaves the recorded revision unchanged");

		// Negative controls: each is refused before any process starts.
		auto none    = press->Run;
		none.Sources = { };
		f.send(workshop(none));
		expect(f.Process.Starts == 1, "a press without a recorded revision never launches");
		auto extra = f.seeded("SECOND_SOURCE");
		auto ref   = source_of(extra.Revision);
		auto two   = press->Run;
		two.Sources.push_back({ ref, ref.Key, false });
		f.send(workshop(two));
		expect(f.Process.Starts == 1, "a press with a second source never launches");
		auto unnamed                      = workshop(press->Run);
		unnamed.Context.WorkshopOperation = std::nullopt;
		f.send(unnamed);
		expect(f.Process.Starts == 1, "a workshop run names its operation");
		auto conversation    = press->Run;
		conversation.Purpose = private_run_purpose::conversation;
		f.send(workshop(conversation));
		expect(f.Process.Starts == 1, "a workshop run is a findings run");
		auto withDraft                  = workshop(press->Run);
		withDraft.Context.IncludedDraft = agent_draft_choice{ .Id = draft.Draft.Id, .Revision = 1 };
		f.send(withDraft);
		expect(f.Process.Starts == 1, "the mutable draft never rides beside its revision");

		auto foreign                = f.Binding;
		foreign.Revision.RightEpoch = epoch_ordinal{ 99 };
		expect(!f.Private.record_workshop_press(foreign, draft.Draft.Id, index), "a draft of another revision is not recorded");
		expect(!f.Private.record_workshop_press(f.Binding, review_draft_id{ 999 }, index), "an unknown draft is not recorded");
		auto second = f.Private.record_workshop_press(f.Binding, draft.Draft.Id, index);
		expect(second && second->Thread != press->Thread && f.Store->read_revision_body({ second->Run.Sources.front().Source, 0 })->Bytes == "WORKSHOP_LATER", "a second press records its own revision of the text then");
	}

	void composition_adversaries()
	{
		fixture f;
		auto    index        = private_review_fixture::index();
		f.Binding.Comparison = index.diff().Alignment.Digest;
		std::vector<private_source_selection> sources;
		for (std::uint32_t line = 0; line < 2; ++line) {
			auto next = f.Store->frontier()->NextIdentity;
			auto row  = *create_private_thread(private_review_thread_id{ next }, { f.Binding, source_line_ref{ *f.Binding.Comparison, diff_side::right, line } }, { private_review_turn_id{ next + 1 }, private_review_turn_revision_id{ next + 2 }, private_vertex_order{ next + 2 }, private_reviewer_actor{ }, "partition source" }, &index);
			expect(f.Store->commit({ .ExpectedNextIdentity = next, .Threads = { { row.Thread, 0 } }, .Revisions = { row.Revision } }).has_value(), "seed two exact line targets");
			auto source = source_of(row.Revision);
			sources.push_back({ source, source.Key, false });
		}
		auto local = f.seeded("unanchored");
		auto ref   = source_of(local.Revision);
		sources.push_back({ ref, ref.Key, false });
		review_composition_application_host host(f.Fault);
		review_composition_request          request{ f.Binding, review_composition_mode::reviewer_authored, sources, { }, "manual proposal" };
		auto                                ambiguous = host.create(request);
		expect(ambiguous && !ambiguous->State.Target, "mixed and unanchored sources never guess target");
		std::array unresolved{ review_composition_acceptance{ ambiguous->State.Id, ambiguous->Version, *ambiguous->State.Candidate } };
		expect(!host.accept(unresolved, f.Binding, index, review_draft_scope_id{ 1 }), "ambiguous workspace cannot become draft");
		auto partitions = host.split(request);
		expect(partitions && partitions->Compositions.size() == 2 && partitions->Unassigned.size() == 1, "split names both exact targets and every unassigned source");
		std::vector<review_composition_acceptance> accept;
		for (const auto& c : partitions->Compositions) {
			accept.push_back({ c.State.Id, c.Version, *c.State.Candidate });
		}
		++accept.back().ExpectedVersion;
		expect(!host.accept(accept, f.Binding, index, review_draft_scope_id{ 1 }) && f.Store->list_drafts()->empty(), "one stale split member refuses the entire batch");
		--accept.back().ExpectedVersion;
		auto accepted = host.accept(accept, f.Binding, index, review_draft_scope_id{ 1 });
		expect(accepted && accepted->records().size() == 2, "exact split accepts both drafts in one transaction");
		expect(f.Store->read_revision_body({ sources[0].Source, 0 })->Bytes == "partition source", "split does not consume source");
		auto target = *make_composition_target(f.Binding, { *f.Binding.Comparison, diff_side::right, 2 }, index);
		expect(host.target(ambiguous->State.Id, ambiguous->Version, target, index).has_value(), "explicit exact target resolves otherwise unanchored workspace");
		auto changed                = f.Binding;
		changed.Revision.RightEpoch = epoch_ordinal{ 99 };
		unresolved[0].ExpectedVersion++;
		expect(!host.accept(unresolved, changed, index, review_draft_scope_id{ 1 }), "changed current source binding refuses draft acceptance");
		request.Mode = review_composition_mode::agent_assisted;
		request.Body.clear();
		request.Sources               = { sources.back() };
		auto agent                    = host.create(request);
		auto invocation               = f.request("");
		invocation.Context.EntryPoint = agent_entry_point::workshop_comment;
		invocation.PrivateRun         = *host.agent_intent(agent->State.Id, agent->Version);
		f.Process.OnStart             = [&](const auto& launch) { launch.PrivateInputClaim->release(); expect(f.edit(local).has_value(), "source advances after input delivery"); };
		f.send(invocation);
		expect(!f.Store->load_composition(agent->State.Id)->State.Candidate && f.Store->load_run_details(std::array{ f.Process.Last.Run })->front().Outcome == private_run_outcome::succeeded, "late source edit retains terminal run but refuses candidate");
	}

}
int main()
{
	durable_left_reply();
	composition_adversaries();
	local_compositions();
	agent_composition();
	workshop_press();
	composition_and_ordering();
	refusals_and_lifecycle();
	source_refusals();
	cancellation_wins_over_late_success();
	malformed_terminal();
	findings_and_duplicate_terminal();
	std::cout << "private-review-host-tests:pass\n";
}
