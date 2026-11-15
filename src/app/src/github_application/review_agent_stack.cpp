#include "github_application/review_agent_stack.hpp"

#if defined(BPR_AGENT_INTEGRATIONS)
	#include <QString>
	#include <QtGlobal>

	#include <utility>

namespace bpr::app {
	/// The stack's epoch host presents nothing; each update tells the stack
	/// the review it holds may have moved.
	struct review_agent_stack::epoch_observer final : application::epoch_projection_port
	{
		explicit epoch_observer(review_agent_stack& aOwner)
			: Owner(aOwner)
		{ }
		void                publish(const application::epoch_presentation_update&) override { Owner.observe_review(); }
		review_agent_stack& Owner;
	};

	/// The stack's ledger host is only its session catalog: no receipt is
	/// shown, so none is published.
	struct review_agent_stack::receipt_sink final : application::receipt_projection_port
	{
		void publish(const application::receipt_projection&) override { }
		void fail(const application::receipt_failure_publication&) override { }
	};

	review_agent_stack::review_agent_stack(const core::application_session_id aSession, review_agent_stack_seed aSeed, const review_agent_stack_ports& aPorts, std::function<void()> aRetired, std::function<void()> aReady)
		: Session(aSession)
		, Seed(std::move(aSeed))
		, DecisionLane(aPorts.DecisionLane)
		, Retired(std::move(aRetired))
		, Ready(std::move(aReady))
	{ }

	auto review_agent_stack::make(const core::application_session_id aSession, review_agent_stack_seed aSeed, const review_agent_stack_ports& aPorts,
								  application::agent_invocation_projection_port& aProjection, std::function<void()> aRetired, std::function<void()> aReady)
		-> std::unique_ptr<review_agent_stack>
	{
		auto  stack = std::unique_ptr<review_agent_stack>{ new review_agent_stack(aSession, std::move(aSeed), aPorts, std::move(aRetired), std::move(aReady)) };
		auto& self  = *stack;
		// The process chain, as a window composes its own: the Claude wrapper,
		// the located Codex and plain-text programs, the reviewer's added
		// agents, and each run's diagnostic line.
		self.Launcher                         = std::make_unique<adapters::agent::agent_process_launcher>();
		application::agent_process_port* base = self.Launcher.get();
	#if defined(BPR_BUILD_TESTING)
		self.Probe = std::make_unique<composition_agent_process_launcher_probe>(*self.Launcher);
		if (self.Seed.Admission) {
			self.Probe->set_admission(self.Seed.Admission);
		}
		base = self.Probe.get();
	#endif
		self.ClaudeProcess    = std::make_unique<application::wrapped_claude_process>(*base, self.Seed.ClaudeWrapper, &adapters::agent::inspect_claude_process_wrapper, self.Seed.LocateClaude);
		self.LocatedProcess   = std::make_unique<application::located_agent_process>(*self.ClaudeProcess, self.Seed.Programs, &adapters::agent::inspect_claude_process_wrapper, self.Seed.LocateProgram);
		self.EntryProcess     = std::make_unique<application::agent_entry_process>(*base, *self.LocatedProcess, self.Seed.Entries, &adapters::agent::inspect_claude_process_wrapper, self.Seed.LocateClaude, self.Seed.LocateProgram);
		self.DiagnosedProcess = std::make_unique<application::diagnosed_agent_process>(*self.EntryProcess, aPorts.Log);
		// Session admission over the review's own ledger file. Admission is
		// one immediate transaction there, so a window's ledger host on the
		// same file never hands out the same identity.
		self.Receipts = std::make_unique<receipt_sink>();
		self.Ledger   = std::make_unique<application::review_ledger_application_host>(
			[scope = self.Seed.Ledger]() -> std::expected<std::unique_ptr<application::review_ledger_store_port>, application::ledger_store_failure> { return adapters::sqlite::make_review_ledger_store(scope); },
			*self.Receipts, aPorts.LedgerEffects, aPorts.DecisionLane);
		// The review the stack reads, attached as the profile's own consumer
		// of its cache actors: no window's close decides it.
		self.EpochProjection = std::make_unique<epoch_observer>(self);
		try {
			self.Epoch = std::make_unique<application::epoch_application_host>(core::epoch_state{ .ApplicationSession = aSession }, aPorts.Actors, aPorts.Catalogs, aPorts.Entropy, aPorts.Clock,
																			   aPorts.EpochEffects, aPorts.DecisionLane, *self.EpochProjection, application::epoch_host_limits{ }, aPorts.Log);
		} catch (...) {
			self.Ledger->retire();
			return nullptr;
		}
		// Private runs over the stack's own store of the review, opened the
		// first time a run needs it, on the agent worker and under the gate.
		self.PrivateRuns = std::make_unique<application::scoped_private_agent_runs>(aPorts.LedgerFile, [&self](const core::private_review_scope& aScope) {
			const auto& configuration = self.Seed.Private;
			if (aScope.Profile != configuration.Profile || aScope.Host != configuration.Host || aScope.Account != configuration.Account || aScope.Repository != configuration.Repository
				|| aScope.PullRequest != configuration.PullRequest) {
				return application::scoped_private_agent_runs::scope_access{ };
			}
			if (self.PrivateStore == nullptr) {
				if (auto opened = adapters::sqlite::make_private_review_store(configuration); opened) {
					self.PrivateStore = std::move(*opened);
				}
			}
			if (self.PrivateStore == nullptr) {
				return application::scoped_private_agent_runs::scope_access{ };
			}
			return application::scoped_private_agent_runs::scope_access{
				.Store = self.PrivateStore.get(),
				.Open  = [configuration](std::stop_token aStop) { return adapters::sqlite::make_private_review_store(configuration, std::move(aStop)); },
			};
		});
		self.Agent       = std::make_unique<application::agent_invocation_host>(
			self.Seed.Profiles, self.Seed.Policies, aPorts.WorkspaceParent, self, *self.Epoch, *self.DiagnosedProcess,
			[](const std::string& aName) -> std::optional<std::string> {
				if (!qEnvironmentVariableIsSet(aName.c_str())) {
					return std::nullopt;
				}
				return qEnvironmentVariable(aName.c_str()).toStdString();
			},
			aPorts.AgentEffects, aPorts.DecisionLane, aProjection, self.PrivateRuns.get(), aPorts.NativeSessions, aPorts.Log.get());
		if (!self.Seed.ScratchRoot.empty()) {
			self.Agent->set_scratch_root(self.Seed.ScratchRoot);
		}
		self.Agent->set_retired_observer([alive = std::weak_ptr<bool>{ self.Alive }, &self] {
			if (const auto live = alive.lock(); live && *live && self.Retired) {
				self.Retired();
			}
		});
		return stack;
	}

	review_agent_stack::~review_agent_stack()
	{
		*Alive = false;
		Lease.release();
	}

	void review_agent_stack::lend(application::lent_review_authority aLent, const core::agent_revision_binding aFor)
	{
		if (Closing || Epoch == nullptr) {
			return;
		}
		Lease   = std::move(aLent.Lease);
		LentFor = aFor;
		Epoch->accept(std::move(aLent.Handoff));
	}

	auto review_agent_stack::lent_for(const core::agent_revision_binding& aBinding) const -> bool { return !Closing && Lease.live() && LentFor == std::optional{ aBinding }; }

	auto review_agent_stack::statistics() const -> application::agent_invocation_host_statistics { return Agent != nullptr ? Agent->statistics() : application::agent_invocation_host_statistics{ }; }

	auto review_agent_stack::session() const noexcept -> core::application_session_id { return Session; }

	/// The review the epoch host accepted, as the agent host must observe it
	/// before any turn: the stack's own session, the lent authority and the
	/// accepted revision's binding.
	void review_agent_stack::observe_review()
	{
		if (Closing || Epoch == nullptr || Agent == nullptr) {
			return;
		}
		const auto& state = Epoch->state();
		if (!state.Authority || !state.AcceptedRevision || !state.AcceptedComparison) {
			return;
		}
		const auto binding = core::agent_revision_binding{
			.Host        = state.Authority->Host,
			.Repository  = state.Authority->Repository,
			.PullRequest = state.Authority->PullRequest,
			.BaseOid     = state.AcceptedRevision->Base,
			.HeadOid     = state.AcceptedRevision->Head,
			.LeftEpoch   = state.AcceptedComparison->Left.Identity.Epoch,
			.RightEpoch  = state.AcceptedComparison->Right.Identity.Epoch,
		};
		if (Observed == std::optional{ binding }) {
			return;
		}
		Observed = binding;
		Agent->observe(Session, *state.Authority, binding);
		if (Ready) {
			Ready();
		}
	}

	void review_agent_stack::dispatch(application::agent_invocation_request aRequest)
	{
		if (Agent == nullptr) {
			return;
		}
		aRequest.ApplicationSession         = Session;
		aRequest.Context.ApplicationSession = Session;
		Agent->dispatch(std::move(aRequest));
		// A refusal starts nothing and so never retires: the runtime is told
		// at once, off this call, so its turn does not stay running.
		if (Agent->is_quiescent()) {
			DecisionLane.post([alive = std::weak_ptr<bool>{ Alive }, this] {
				if (const auto live = alive.lock(); live && *live && Retired) {
					Retired();
				}
			});
		}
	}

	void review_agent_stack::cancel()
	{
		if (Agent != nullptr) {
			Agent->cancel();
		}
	}

	/// The turn's machinery alone before shutdown, so background review work
	/// never holds a turn running; every host once shut down.
	auto review_agent_stack::is_quiescent() const -> bool
	{
		const auto agent = Agent == nullptr || Agent->is_quiescent();
		if (!Closing) {
			return agent;
		}
		return agent && ReviewHostsClosed && (Epoch == nullptr || Epoch->is_quiescent()) && (Ledger == nullptr || Ledger->is_quiescent());
	}

	void review_agent_stack::shutdown()
	{
		if (Closing) {
			return;
		}
		Closing = true;
		Lease.release();
		if (Agent != nullptr) {
			Agent->shutdown();
		}
		close_review_hosts();
	}

	/// The epoch host is the agent host's workspace port, so it closes only
	/// once the agent host is quiescent: a workspace whose removal was
	/// refused is removed on a retry, never abandoned to a closed port. The
	/// ledger host goes with it, as a window's shutdown orders its own.
	void review_agent_stack::close_review_hosts()
	{
		if (!Closing || ReviewHostsClosed || (Agent != nullptr && !Agent->is_quiescent())) {
			return;
		}
		ReviewHostsClosed = true;
		if (Epoch != nullptr) {
			Epoch->shutdown();
		}
		if (Ledger != nullptr) {
			Ledger->retire();
		}
	}

	auto review_agent_stack::authorized() const -> bool { return !Closing && Lease.live() && Observed.has_value(); }

	/// Lent, and the epoch host has not yet accepted the review: a window may
	/// already say ready, so its turn waits for `observe_review`.
	auto review_agent_stack::accepting() const -> bool { return !Closing && Lease.live() && !Observed.has_value(); }

	void review_agent_stack::retry_cleanup()
	{
		if (Agent != nullptr) {
			Agent->retry_cleanup();
		}
		close_review_hosts();
	}

	auto review_agent_stack::observed_review() const -> std::optional<std::pair<core::pull_request_authority, core::agent_revision_binding>>
	{
		if (Epoch == nullptr || !Observed || !Epoch->state().Authority) {
			return std::nullopt;
		}
		return std::pair{ *Epoch->state().Authority, *Observed };
	}

	void review_agent_stack::admit(const application::agent_session_admission&                                                aAdmission,
								   std::function<void(std::expected<core::agent_session, application::ledger_store_failure>)> aDone)
	{
		if (Closing || Ledger == nullptr) {
			DecisionLane.post([done = std::move(aDone)] { done(std::unexpected(application::ledger_store_failure::unavailable)); });
			return;
		}
		Ledger->admit(aAdmission, std::move(aDone));
	}

	void review_agent_stack::session_catalog(std::function<void(std::expected<application::agent_session_catalog_state, application::ledger_store_failure>)> aDone)
	{
		if (Closing || Ledger == nullptr) {
			DecisionLane.post([done = std::move(aDone)] { done(std::unexpected(application::ledger_store_failure::unavailable)); });
			return;
		}
		Ledger->session_catalog(std::move(aDone));
	}
} // namespace bpr::app
#endif
