#include "bpr/application/agent_invocation_host.hpp"

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <ranges>
#include <stop_token>
#include <thread>
#include <utility>

namespace bpr::application {
	namespace {
		/// \brief Why the host is ending whatever is running.
		///
		/// \details Two causes, told apart because one of them is not news
		/// about a continuation. `review_changed` means the reviewer is no
		/// longer looking at the review this invocation belongs to, which ends
		/// every turn. `binding_moved` means the revision under review moved
		/// while the review stayed, which ends a turn asked about the revision
		/// under review and deliberately does not end one asked about an
		/// older one.
		enum class supersession_cause : std::uint8_t {
			binding_moved,
			review_changed,
		};

		[[nodiscard]] auto binding_matches_authority(
			const core::agent_revision_binding& aBinding,
			const core::pull_request_authority& aAuthority) noexcept -> bool
		{
			return aBinding.Host == aAuthority.Host
				&& aBinding.Repository == aAuthority.Repository
				&& aBinding.PullRequest == aAuthority.PullRequest;
		}

		[[nodiscard]] auto workspace_outcome(
			const agent_workspace_error& aFailure) noexcept -> agent_run_outcome
		{
			switch (aFailure.Code) {
			case agent_workspace_failure::cancelled:
				return agent_run_outcome::cancelled;
			case agent_workspace_failure::stale:
				return agent_run_outcome::stale_result;
			case agent_workspace_failure::unavailable:
				// The workspace could not be materialized. Nothing was launched,
				// so `failed_to_start` would point the operator at the agent
				// when the failure is in the cache. BPR-0348.
				return agent_run_outcome::workspace_unavailable;
			case agent_workspace_failure::invalid_request:
			case agent_workspace_failure::cleanup_failed:
				return agent_run_outcome::failed_to_start;
			}
			return agent_run_outcome::failed_to_start;
		}
	} // namespace

	struct agent_invocation_host::implementation final
		: std::enable_shared_from_this<implementation>
	{
		struct invocation final
		{
			invocation(
				agent_invocation_projection_identity aIdentity,
				core::agent_finding_snapshot         aSnapshot,
				const bool                           aContinued,
				std::shared_ptr<std::stop_source>    aStop)
				: Identity{ std::move(aIdentity) }
				, Snapshot{ std::move(aSnapshot) }
				, Continued{ aContinued }
				, Stop{ std::move(aStop) }
			{
			}

			agent_invocation_projection_identity Identity;
			/// Frozen at admission and never rebuilt. Const in effect: nothing
			/// below writes it, so the verdict a payload receives is the one
			/// the revision it was asked about produces.
			const core::agent_finding_snapshot Snapshot;
			/// Whether this turn continues a session the reviewer kept.
			///
			/// \details Frozen at admission beside the snapshot, and read
			/// wherever a moved revision would otherwise end the turn. A
			/// continuation is bound by BPR-0521's design to a revision the
			/// review may already have moved away from, so "the binding
			/// changed" is not news about it.
			const bool                           Continued = false;
			std::shared_ptr<std::stop_source>    Stop;
			std::mutex                           WorkspaceMutex;
			std::optional<owned_agent_workspace> OwnedWorkspace;
			std::optional<private_run_admission> PrivateAdmission;
			std::atomic_uint64_t                 DurableRun          = 0;
			std::atomic_bool                     Superseded          = false;
			std::atomic_bool                     TerminalComplete    = false;
			std::atomic_bool                     CleanupScheduled    = false;
			std::atomic_bool                     Retired             = false;
			std::atomic_bool                     ExplicitlyCancelled = false;
		};

		implementation(
			std::vector<core::agent_profile>     aProfiles,
			std::vector<core::agent_host_policy> aPolicies,
			std::filesystem::path                aWorkspaceParent,
			agent_session_catalog_port&          aSessions,
			agent_workspace_port&                aWorkspaces,
			agent_process_port&                  aProcesses,
			environment_reader                   aEnvironment,
			effect_executor&                     aEffects,
			decision_executor&                   aDecisionLane,
			agent_invocation_projection_port&    aProjection,
			private_agent_run_port*              aPrivateRuns)
			: Profiles{ std::move(aProfiles) }
			, Policies{ std::move(aPolicies) }
			, WorkspaceParent{ std::move(aWorkspaceParent) }
			, Sessions{ aSessions }
			, Workspaces{ aWorkspaces }
			, Processes{ aProcesses }
			, Environment{ std::move(aEnvironment) }
			, Effects{ aEffects }
			, DecisionLane{ aDecisionLane }
			, Projection{ aProjection }
			, PrivateRuns{ aPrivateRuns }
		{
		}

		[[nodiscard]] auto enter_callback() -> bool
		{
			std::scoped_lock lock{ GateMutex };
			if (Destroying) {
				return false;
			}
			++CallbacksInFlight;
			++CallbacksByThread[std::this_thread::get_id()];
			return true;
		}

		void leave_callback()
		{
			auto       lock   = std::unique_lock{ GateMutex };
			const auto thread = CallbacksByThread.find(std::this_thread::get_id());
			if (thread != CallbacksByThread.end() && --thread->second == 0U) {
				CallbacksByThread.erase(thread);
			}
			--CallbacksInFlight;
			if (Destroying || CallbacksInFlight == 0U) {
				lock.unlock();
				GateChanged.notify_all();
			}
		}

		struct callback_guard final
		{
			std::shared_ptr<implementation> Owner;

			~callback_guard()
			{
				Owner->leave_callback();
			}
		};

		void project_offer(
			const std::span<const core::agent_profile> aProfiles) noexcept
		{
			try {
				Projection.offer(aProfiles);
			} catch (...) {
			}
		}

		void project_refuse(const std::string_view aReason) noexcept
		{
			try {
				Projection.refuse(aReason);
			} catch (...) {
			}
		}

		void project_admit(
			const agent_invocation_projection_identity& aIdentity) noexcept
		{
			try {
				Projection.admit(aIdentity);
			} catch (...) {
			}
		}

		void project_start(
			const agent_invocation_projection_identity& aIdentity,
			const prepared_workspace&                   aWorkspace) noexcept
		{
			try {
				Projection.start(aIdentity, aWorkspace);
			} catch (...) {
			}
		}

		void project_stream(
			const agent_invocation_projection_identity& aIdentity,
			const agent_stream_event&                   aEvent) noexcept
		{
			try {
				Projection.stream(aIdentity, aEvent);
			} catch (...) {
			}
		}

		void project_finish(
			const agent_invocation_projection_identity& aIdentity,
			const agent_run_result&                     aResult) noexcept
		{
			try {
				Projection.finish(aIdentity, aResult);
			} catch (...) {
			}
		}

		void project_findings(
			const agent_invocation_projection_identity& aIdentity,
			const agent_finding_delivery&               aDelivery) noexcept
		{
			try {
				Projection.deliver_findings(aIdentity, aDelivery);
			} catch (...) {
			}
		}

		void project_session_catalog(
			const agent_session_catalog_projection& aProjection) noexcept
		{
			try {
				Projection.present_session_catalog(aProjection);
			} catch (...) {
			}
		}

		void publish_session_catalog(
			const core::agent_revision_binding& aObserved)
		{
			// [[BPR-1211]]. The catalog is read off the decision lane and
			// answered on it; an answer for a binding this host has since
			// left is not presented.
			Sessions.session_catalog([weak = weak_from_this(), aObserved](
										 std::expected<agent_session_catalog_state, ledger_store_failure> aLoaded) {
				auto self = weak.lock();
				if (!self || !self->enter_callback()) {
					return;
				}
				auto guard = callback_guard{ self };
				{
					std::scoped_lock lock{ self->StateMutex };
					if (self->Binding != std::optional{ aObserved }) {
						return;
					}
				}
				if (!aLoaded) {
					self->project_refuse(
						std::string{ "agent_session_catalog_" }
						+ std::string{ name(aLoaded.error()) });
					return;
				}
				auto projection = agent_session_catalog_projection{
					.Latest = aLoaded->Latest,
					.Next   = aLoaded->Next,
				};
				if (aLoaded->Latest.has_value()) {
					projection.LatestStatus = core::observe_agent_session(
						*aLoaded->Latest, aObserved);
				}
				self->project_session_catalog(projection);
			});
		}

		void publish_offerings()
		{
			auto authority = std::optional<core::pull_request_authority>{ };
			{
				std::scoped_lock lock{ StateMutex };
				authority = Authority;
			}
			auto offered = std::vector<core::agent_profile>{ };
			if (authority.has_value()) {
				const auto policy = core::policy_for_host(Policies, authority->Host);
				for (const auto& profile : Profiles) {
					if (core::decide_agent_invocation(profile, policy).Permitted) {
						offered.push_back(profile);
					}
				}
			}
			project_offer(offered);
		}

		void observe(
			const core::application_session_id aApplicationSession,
			core::pull_request_authority       aAuthority,
			core::agent_revision_binding       aBinding)
		{
			auto changed = false;
			auto cause   = supersession_cause::review_changed;
			{
				std::scoped_lock lock{ StateMutex };
				const auto       reviewChanged = !ApplicationSession.has_value()
					|| *ApplicationSession != aApplicationSession
					|| Authority != std::optional{ aAuthority };
				const auto bindingChanged = Binding != std::optional{ aBinding };
				changed                   = reviewChanged || bindingChanged;
				// Only when the review itself stayed put is a moved binding
				// the whole of the change; anything else is reported as the
				// larger cause, so a continuation is never carried across a
				// review it was not asked about.
				cause              = reviewChanged
					? supersession_cause::review_changed
					: supersession_cause::binding_moved;
				ApplicationSession = aApplicationSession;
				Authority          = std::move(aAuthority);
				Binding            = aBinding;
			}
			if (changed) {
				supersede(cause);
			}
			publish_session_catalog(aBinding);
			publish_offerings();
		}

		void clear()
		{
			{
				std::scoped_lock lock{ StateMutex };
				ApplicationSession.reset();
				Authority.reset();
				Binding.reset();
			}
			supersede(supersession_cause::review_changed);
			project_session_catalog({ });
			publish_offerings();
		}

		[[nodiscard]] auto current_matches(
			const std::shared_ptr<invocation>& aInvocation) const -> bool
		{
			std::scoped_lock lock{ StateMutex };
			return ActiveInvocation == aInvocation
				&& ApplicationSession
				== std::optional{ aInvocation->Identity.ApplicationSession }
			&& Authority == std::optional{ aInvocation->Identity.Authority }
			&& Binding == std::optional{ aInvocation->Identity.Session.Binding };
		}

		[[nodiscard]] auto still_active(
			const std::shared_ptr<invocation>& aInvocation) const -> bool
		{
			std::scoped_lock lock{ StateMutex };
			return ActiveInvocation == aInvocation;
		}

		void dispatch(agent_invocation_request aRequest)
		{
			if (Closing.load(std::memory_order_acquire)) {
				project_refuse("agent host is closing");
				return;
			}
			const auto profile = std::ranges::find(
				Profiles,
				aRequest.Profile,
				&core::agent_profile::Id);
			if (profile == Profiles.end()) {
				project_refuse("profile_unknown");
				return;
			}
			if (aRequest.WritingOnly && (profile->Protocol != core::agent_protocol::claude_stream_json || aRequest.ExistingCheckoutChosen || aRequest.ContinuedSession)) {
				project_refuse("selected_agent_has_no_qualified_writing_protocol");
				return;
			}
			if (aRequest.WritingOnly ? aRequest.PrivateRun.has_value() : ((PrivateRuns != nullptr) != aRequest.PrivateRun.has_value())) {
				project_refuse("private_run_admission_unavailable");
				return;
			}
			if (aRequest.PrivateRun) {
				const auto& b = aRequest.PrivateRun->Binding;
				if (b.Scope.Host != aRequest.Authority.Host || b.Scope.Account != aRequest.Authority.Account || b.Scope.Repository != aRequest.Authority.Repository || b.Scope.PullRequest != aRequest.Authority.PullRequest || b.HostAuthority != aRequest.Authority.HostAuthority || b.LeftAnchor.Authority != aRequest.Authority.Scope || b.RightAnchor.Authority != aRequest.Authority.Scope) {
					project_refuse("private_run_authority_mismatch");
					return;
				}
			}
			const auto policy = core::policy_for_host(Policies, aRequest.Authority.Host);
			++CoreDecisions;
			const auto verdict = core::decide_agent_invocation(*profile, policy);
			if (!verdict.Permitted) {
				project_refuse(core::stable_name(verdict.Refusal));
				return;
			}
			const auto provisionalSession = core::agent_session{
				.Id      = core::agent_session_id{ 1U },
				.Profile = profile->Id,
				.Binding = aRequest.Context.Binding,
			};
			auto supplement = build_agent_context_supplement(
				aRequest.Context, provisionalSession);
			if (!supplement.has_value()) {
				project_refuse("invalid_agent_context");
				return;
			}
			if (aRequest.FrozenFiles.size() > core::maximum_supplement_paths) {
				project_refuse("invalid_agent_finding_snapshot");
				return;
			}
			// A continuation says two things about one revision, and they are
			// checked against each other before anything is run. The session
			// the turn continues carries its own binding; the context the
			// caller composed carries one too. A turn whose two answers differ
			// would freeze a snapshot under one revision and ask about the
			// other, which is exactly the mismatch
			// `continue_against_old_revision` exists to prevent, so it is
			// refused by name rather than resolved in favour of either.
			if (aRequest.ContinuedSession.has_value()) {
				if (!core::valid_agent_session(*aRequest.ContinuedSession)
					|| aRequest.ContinuedSession->Profile != profile->Id) {
					project_refuse("agent_continuation_invalid_session");
					return;
				}
				if (aRequest.ContinuedSession->Binding
					!= aRequest.Context.Binding) {
					project_refuse("agent_continuation_binding_mismatch");
					return;
				}
			}

			auto admission = std::optional<agent_session_admission>{ };
			if (!admissible(aRequest)) {
				return;
			}
			// Continuing is the one path that allocates nothing. The
			// identity already exists in the catalog, which is what makes
			// it a continuation rather than a second conversation, and
			// re-admitting it would mint an identity the reviewer's own
			// session is not keyed to.
			if (aRequest.ContinuedSession.has_value()) {
				const auto continued = *aRequest.ContinuedSession;
				start(std::move(aRequest), *profile, std::move(*supplement), continued);
				return;
			}
			{
				std::scoped_lock lock{ StateMutex };
				AdmissionPending   = true;
				AdmissionCancelled = false;
				admission          = agent_session_admission{ .Profile = profile->Id, .Binding = *Binding };
				ActiveJobs.store(1U, std::memory_order_release);
			}
			// [[BPR-1211]]. The new identity is allocated durably off the
			// decision lane; the turn starts when its answer returns there,
			// and only if the review it was asked about is still the one here.
			++SessionAdmissions;
			Sessions.admit(*admission, [weak = weak_from_this(), request = std::move(aRequest), turnProfile = *profile, turnSupplement = std::move(*supplement), expected = admission->Binding](std::expected<core::agent_session, ledger_store_failure> aAdmitted) mutable {
				auto self = weak.lock();
				if (!self || !self->enter_callback()) {
					return;
				}
				auto guard     = callback_guard{ self };
				auto cancelled = false;
				{
					std::scoped_lock lock{ self->StateMutex };
					self->AdmissionPending = false;
					cancelled              = std::exchange(self->AdmissionCancelled, false);
				}
				const auto idle = [&self] { self->ActiveJobs.store(0U, std::memory_order_release); };
				if (!aAdmitted) {
					++self->SessionAdmissionFailures;
					idle();
					self->project_refuse(
						std::string{ "agent_session_admission_" }
						+ std::string{ name(aAdmitted.error()) });
					return;
				}
				{
					std::scoped_lock lock{ self->StateMutex };
					if (self->Binding != std::optional{ expected }) {
						idle();
						self->project_refuse("stale_or_mismatched_agent_context");
						return;
					}
				}
				if (!self->admissible(request)) {
					idle();
					return;
				}
				self->start(std::move(request), turnProfile, std::move(turnSupplement), *aAdmitted);
				// A cancel or shutdown while the identity was allocated applies
				// to the turn it was allocated for, before any effect.
				if (cancelled || self->Closing.load(std::memory_order_acquire)) {
					self->cancel();
				}
			});
		}

		/// \brief Whether the review this host holds still admits the turn.
		[[nodiscard]] auto admissible(const agent_invocation_request& aRequest) -> bool
		{
			std::scoped_lock lock{ StateMutex };
			// The revision this turn is about: the one under review, or —
			// for a continuation — the one the continued session was
			// bound to. Both are values this host holds or was handed
			// whole; neither is a revision a caller named on its own.
			const auto turnBinding = aRequest.ContinuedSession.has_value()
				? aRequest.ContinuedSession->Binding
				: Binding.value_or(core::agent_revision_binding{ });
			if (
				!ApplicationSession.has_value() || !Authority.has_value()
				|| !Binding.has_value() || ActiveInvocation || AdmissionPending
				|| aRequest.ApplicationSession != *ApplicationSession
				|| aRequest.Context.ApplicationSession
					!= aRequest.ApplicationSession
				|| aRequest.Authority != *Authority
				|| aRequest.Context.Binding != turnBinding
				|| !binding_matches_authority(turnBinding, *Authority)
				|| !binding_matches_authority(*Binding, *Authority)) {
				project_refuse(
					ActiveInvocation || AdmissionPending ? "agent_invocation_already_running"
														 : "stale_or_mismatched_agent_context");
				return false;
			}
			return true;
		}

		/// \brief Starts one admitted turn under its exact session.
		void start(agent_invocation_request       aRequest,
				   const core::agent_profile&     aProfile,
				   core::agent_context_supplement aSupplement,
				   const core::agent_session&     aSession)
		{
			aSupplement.Session = aSession;
			auto job            = std::shared_ptr<invocation>{ };
			{
				std::scoped_lock lock{ StateMutex };
				if (ActiveInvocation) {
					project_refuse("agent_invocation_already_running");
					return;
				}
				job = std::make_shared<invocation>(
					agent_invocation_projection_identity{
						.ApplicationSession   = aRequest.ApplicationSession,
						.Authority            = aRequest.Authority,
						.Session              = aSession,
						.InvocationGeneration = ++InvocationGeneration,
					},
					// The binding comes from the session this turn runs under,
					// never from the request beside it. A caller cannot name a
					// revision that session was not started against, because
					// there is nowhere for it to say one and the one place it
					// could restate it has already been checked equal.
					core::agent_finding_snapshot{
						.Binding = aSession.Binding,
						.Files   = aRequest.FrozenFiles,
					},
					aRequest.ContinuedSession.has_value(),
					std::make_shared<std::stop_source>());
				ActiveInvocation = job;
				ActiveJobs.store(1U, std::memory_order_release);
			}
			project_session_catalog(agent_session_catalog_projection{
				.Latest       = job->Identity.Session,
				.LatestStatus = core::observe_agent_session(
					job->Identity.Session, aRequest.Context.Binding),
			});

			const auto existing = aRequest.ExistingCheckoutChosen
				? prepare_agent_workspace(
					  aProfile,
					  job->Identity.Session,
					  aRequest.ExistingCheckout,
					  true,
					  { })
				: workspace_preparation{ };
			post_admission(
				std::move(aRequest),
				aProfile,
				std::move(aSupplement),
				existing,
				job);
		}

		void post_admission(
			agent_invocation_request           aRequest,
			core::agent_profile                aProfile,
			core::agent_context_supplement     aSupplement,
			workspace_preparation              aExisting,
			const std::shared_ptr<invocation>& aInvocation)
		{
			const auto weak = weak_from_this();
			try {
				DecisionLane.post(
					[weak,
					 request    = std::move(aRequest),
					 profile    = std::move(aProfile),
					 supplement = std::move(aSupplement),
					 existing   = std::move(aExisting),
					 aInvocation]() mutable {
						auto self = weak.lock();
						if (!self || !self->enter_callback()) {
							return;
						}
						auto guard = callback_guard{ self };
						if (!self->still_active(aInvocation)) {
							return;
						}
						self->project_admit(aInvocation->Identity);
						{
							std::scoped_lock lock{ self->GateMutex };
							if (self->Destroying) {
								return;
							}
						}
						if (
							request.ExistingCheckoutChosen
							&& !existing.Workspace.has_value()) {
							self->complete(
								{ .Session = aInvocation->Identity.Session.Id,
								  .Outcome = agent_run_outcome::failed_to_start },
								aInvocation);
							return;
						}
						try {
							self->Effects.post(
								[weak,
								 runRequest    = std::move(request),
								 runProfile    = std::move(profile),
								 runSupplement = std::move(supplement),
								 runExisting   = std::move(existing),
								 aInvocation]() mutable {
									auto effectOwner = weak.lock();
									if (!effectOwner || !effectOwner->enter_callback()) {
										return;
									}
									auto effectGuard = callback_guard{ effectOwner };
									effectOwner->run(
										std::move(runRequest),
										std::move(runProfile),
										std::move(runSupplement),
										std::move(runExisting),
										aInvocation);
								});
						} catch (...) {
							self->complete(
								{ .Session = aInvocation->Identity.Session.Id,
								  .Outcome = agent_run_outcome::failed_to_start },
								aInvocation);
						}
					});
			} catch (...) {
				retire(aInvocation);
			}
		}

		void track_workspace(
			const std::shared_ptr<invocation>& aInvocation,
			owned_agent_workspace              aWorkspace)
		{
			std::scoped_lock lock{ aInvocation->WorkspaceMutex };
			aInvocation->OwnedWorkspace = std::move(aWorkspace);
		}

		[[nodiscard]] auto cleanup_workspace(
			const std::shared_ptr<invocation>& aInvocation) -> bool
		{
			auto workspace = std::optional<owned_agent_workspace>{ };
			{
				std::scoped_lock lock{ aInvocation->WorkspaceMutex };
				workspace = aInvocation->OwnedWorkspace;
			}
			if (!workspace.has_value()) {
				return true;
			}
			// The stop-stateless token here is deliberate, and saying so is the
			// point: BPR-0390 found a `std::stop_token{ }` that made a wait look
			// cancellable when it never could be. This one must not be
			// cancellable. Cleanup runs *because* the invocation ended, and
			// `destroy` requests the invocation's stop before calling here, so
			// passing that token would abandon every shutdown cleanup and leak
			// the workspace it exists to remove. What makes this wait terminable
			// is its bound, not a token. BPR-0397.
			const auto cleaned = Workspaces.cleanup_agent_workspace(*workspace, { });
			if (!cleaned.has_value()) {
				++WorkspaceCleanupFailures;
				return false;
			}
			{
				std::scoped_lock lock{ aInvocation->WorkspaceMutex };
				if (aInvocation->OwnedWorkspace == workspace) {
					aInvocation->OwnedWorkspace.reset();
				}
			}
			++WorkspaceCleanups;
			return true;
		}

		void run(
			agent_invocation_request           aRequest,
			core::agent_profile                aProfile,
			core::agent_context_supplement     aSupplement,
			workspace_preparation              aExisting,
			const std::shared_ptr<invocation>& aInvocation)
		{
			try {
				if (aRequest.PrivateRun && !aInvocation->Stop->stop_requested()) {
					auto admitted = PrivateRuns->admit(*aRequest.PrivateRun, aProfile, aInvocation->Identity.Session, aRequest.Context, aRequest.RequestedSettings);
					if (!admitted) {
						post_private(aInvocation, { }, admitted.error());
						finish_run({ .Session = aInvocation->Identity.Session.Id, .Outcome = agent_run_outcome::failed_to_start }, aInvocation);
						return;
					}
					aInvocation->PrivateAdmission = std::move(*admitted);
					aInvocation->DurableRun.store(aInvocation->PrivateAdmission->Delta.Run.value(), std::memory_order_release);
					post_private(aInvocation, aInvocation->PrivateAdmission->Delta, { });
				}
				auto workspace     = std::optional<prepared_workspace>{ };
				auto lifetimeGuard = std::optional<agent_workspace_lifetime_guard>{ };
				if (aInvocation->Stop->stop_requested()) {
					finish_run(
						{ .Session = aInvocation->Identity.Session.Id,
						  .Outcome = agent_run_outcome::cancelled },
						aInvocation);
					return;
				}
				if (aRequest.ExistingCheckoutChosen) {
					workspace = std::move(aExisting.Workspace);
				} else {
					++WorkspaceStarts;
					auto materialized = Workspaces.materialize_agent_workspace(
						agent_workspace_materialization_request{
							.ApplicationSession = aRequest.ApplicationSession,
							.Authority          = aRequest.Authority,
							.Session            = aInvocation->Identity.Session,
							.ParentDirectory    = WorkspaceParent,
							.HistoryClone       = aRequest.HistoryClone,
						},
						aInvocation->Stop->get_token());
					if (!materialized.has_value()) {
						auto failed = agent_run_result{
							.Session       = aInvocation->Identity.Session.Id,
							.Outcome       = workspace_outcome(materialized.error()),
							.StandardError = std::move(materialized.error().Detail),
						};
						// [[BPR-1738]]. The reason travels with the outcome
						// so the reviewer is told what happened and what to do;
						// Git's own text stays in the log.
						if (failed.Outcome == agent_run_outcome::workspace_unavailable) {
							failed.WorkspaceReason = materialized.error().Reason;
						}
						finish_run(std::move(failed), aInvocation);
						return;
					}
					workspace     = materialized->Workspace;
					lifetimeGuard = materialized->LifetimeGuard;
					track_workspace(aInvocation, std::move(*materialized));
				}

				if (aInvocation->Stop->stop_requested()) {
					finish_run(
						{ .Session = aInvocation->Identity.Session.Id,
						  .Outcome = agent_run_outcome::cancelled },
						aInvocation);
					return;
				}
				auto options = aInvocation->PrivateAdmission ? aInvocation->PrivateAdmission->Launch : agent_launch_options{ .Requested = aRequest.RequestedSettings, .EntryPoint = aRequest.Context.EntryPoint, .WritingOnly = aRequest.WritingOnly };
				// [[BPR-1752]]. The brief says what the workspace that
				// was just made is, so the agent knows whether history and the
				// base are there to compare with.
				auto framed = std::optional<core::agent_context_supplement>{ };
				if (aSupplement.Brief && workspace) {
					framed                   = aSupplement;
					framed->Brief->Workspace = workspace->Shallow ? core::brief_workspace::private_snapshot
						: workspace->BorrowsHistory               ? core::brief_workspace::linked_clone_history
																  : core::brief_workspace::private_copy;
				}
				auto launch = build_agent_launch(aProfile, framed ? *framed : aSupplement, Environment, options);
				options.PrivateSources.clear();
				if (aInvocation->PrivateAdmission) {
					aInvocation->PrivateAdmission->Launch.PrivateSources.clear();
				}
				if (!launch.has_value() || !workspace.has_value()) {
					finish_run(
						{ .Session = aInvocation->Identity.Session.Id,
						  .Outcome = agent_run_outcome::failed_to_start },
						aInvocation);
					return;
				}
				launch->WorkingDirectory       = workspace->Path;
				launch->WorkspaceLifetimeGuard = std::move(lifetimeGuard);
				if (aInvocation->Stop->stop_requested()) {
					finish_run(
						{ .Session = aInvocation->Identity.Session.Id,
						  .Outcome = agent_run_outcome::cancelled },
						aInvocation);
					return;
				}
				if (aInvocation->PrivateAdmission) {
					auto claim = PrivateRuns->claim(*aInvocation->PrivateAdmission, aInvocation->Stop->get_token());
					if (!claim) {
						post_private(aInvocation, { }, claim.error());
						finish_run({ .Session = aInvocation->Identity.Session.Id, .Outcome = agent_run_outcome::failed_to_start }, aInvocation);
						return;
					}
					launch->PrivateInputClaim = std::move(*claim);
				}
				post_start(aInvocation, *workspace);
				++ProcessStarts;
				const auto weak = weak_from_this();
				// Owner decision 2026-10-23: no time limit unless the reviewer
				// set one; Stop is always there.
				auto result = Processes.run(
					*launch,
					aRequest.RunLimitMilliseconds.value_or(agent_run_without_limit),
					[weak, aInvocation](const agent_stream_event& aEvent) {
						if (auto self = weak.lock()) {
							self->post_stream(aInvocation, aEvent);
						}
					},
					aInvocation->Stop->get_token());
				if (launch->PrivateInputClaim) {
					launch->PrivateInputClaim->release();
				}
				finish_run(std::move(result), aInvocation, true);
			} catch (...) {
				finish_run({ .Session = aInvocation->Identity.Session.Id, .Outcome = agent_run_outcome::failed_to_start }, aInvocation);
			}
		}

		void post_private(const std::shared_ptr<invocation>& aInvocation, std::optional<private_review_delta> aDelta, std::optional<private_run_error> aError)
		{
			const auto weak = weak_from_this();
			try {
				DecisionLane.post([weak, aInvocation, delta = std::move(aDelta), error = std::move(aError)] {
					auto self = weak.lock();
					if (!self || !self->enter_callback()) {
						return;
					}
					auto guard = callback_guard{ self };
					try {
						if (delta) {
							self->Projection.private_changed(aInvocation->Identity, *delta);
						}
						if (error) {
							self->Projection.private_refused(aInvocation->Identity, *error);
						}
					} catch (...) { }
				});
			} catch (...) { }
		}
		void finish_run(agent_run_result aResult, const std::shared_ptr<invocation>& aInvocation, bool aProcessResult = false)
		{
			if (aInvocation->PrivateAdmission) {
				if (aInvocation->ExplicitlyCancelled.load(std::memory_order_acquire) && aResult.Outcome == agent_run_outcome::success) {
					aResult.Outcome = agent_run_outcome::cancelled;
					aResult.Events.clear();
					aResult.Findings.reset();
				}
				if (!aProcessResult) {
					aResult.Run      = aInvocation->PrivateAdmission->Delta.Run;
					aResult.Metadata = aInvocation->PrivateAdmission->Launch.AdmittedRun->Metadata;
				}
				auto stored = PrivateRuns->finish(*aInvocation->PrivateAdmission, aResult, aInvocation->Snapshot);
				if (stored) {
					post_private(aInvocation, *stored, { });
					if (stored->Outcome == core::private_run_outcome::protocol_failure) {
						aResult.Outcome = agent_run_outcome::protocol_failure;
						aResult.Events.clear();
						aResult.Findings.reset();
					}
				} else {
					post_private(aInvocation, { }, stored.error());
					aResult.Outcome = agent_run_outcome::private_storage_failure;
					aResult.Events.clear();
					aResult.Findings.reset();
				}
			}
			complete(std::move(aResult), aInvocation);
		}

		void post_start(
			const std::shared_ptr<invocation>& aInvocation,
			prepared_workspace                 aWorkspace)
		{
			const auto weak = weak_from_this();
			try {
				DecisionLane.post(
					[weak, aInvocation, workspace = std::move(aWorkspace)] {
						auto self = weak.lock();
						if (!self || !self->enter_callback()) {
							return;
						}
						auto guard = callback_guard{ self };
						if (self->current_matches(aInvocation)) {
							self->project_start(aInvocation->Identity, workspace);
						}
					});
			} catch (...) {
			}
		}

		void post_stream(
			const std::shared_ptr<invocation>& aInvocation,
			agent_stream_event                 aEvent)
		{
			const auto weak = weak_from_this();
			try {
				DecisionLane.post(
					[weak, aInvocation, event = std::move(aEvent)] {
						auto self = weak.lock();
						if (!self || !self->enter_callback()) {
							return;
						}
						auto guard = callback_guard{ self };
						if (
							self->current_matches(aInvocation)
							&& event.Session == aInvocation->Identity.Session.Id
							&& event.Run.value() == aInvocation->DurableRun.load(std::memory_order_acquire)) {
							self->project_stream(aInvocation->Identity, event);
						}
					});
			} catch (...) {
			}
		}

		void complete(
			agent_run_result                   aResult,
			const std::shared_ptr<invocation>& aInvocation)
		{
			const auto weak = weak_from_this();
			try {
				DecisionLane.post(
					[weak, aInvocation, result = std::move(aResult)]() mutable {
						auto self = weak.lock();
						if (!self || !self->enter_callback()) {
							return;
						}
						auto guard = callback_guard{ self };
						if (!self->still_active(aInvocation)) {
							return;
						}
						auto observed = std::optional<core::agent_revision_binding>{ };
						{
							std::scoped_lock lock{ self->StateMutex };
							if (
								self->ApplicationSession
									== std::optional{ aInvocation->Identity.ApplicationSession }
								&& self->Authority
									== std::optional{ aInvocation->Identity.Authority }) {
								observed = self->Binding;
							}
						}
						// Lifted off the result before any staleness decision.
						// A terminal that supersession replaces wholesale would
						// otherwise take the turn's findings with it, and a
						// payload the reviewer can no longer act on is still
						// one they are entitled to see refused by revision
						// rather than to never learn about.
						auto payload = std::move(result.Findings);
						result.Findings.reset();
						// [[BPR-1753]], owner decision 2026-10-23. A
						// harness writes its own result frame, so a real agent
						// returns its review findings in its answer, fenced as
						// the review skill asks; they are read against this
						// turn's frozen snapshot and delivered like any other.
						if (!payload.has_value() && result.Outcome == agent_run_outcome::success) {
							auto answer = std::string{ };
							for (const auto& event : result.Events) {
								answer += event.Text;
								answer += '\n';
							}
							payload = decode_agent_review_findings(answer, aInvocation->Identity.Session, aInvocation->Snapshot);
						}
						// Which revision this turn's outcome is judged
						// against. A fresh turn is judged against the revision
						// under review, unchanged. A continuation is judged
						// against the revision its session was started on: the
						// reviewer asked about that one, the frozen snapshot
						// every finding below is validated against is that
						// one's, and judging the answer against the head on
						// screen would report every continuation as stale by
						// construction. The delivery below still states its
						// staleness against `observed`, so how far the review
						// has moved is reported rather than hidden.
						const auto judged = aInvocation->Continued
								&& observed.has_value()
							? std::optional{ aInvocation->Identity.Session.Binding }
							: observed;
						auto       final  = aInvocation->Superseded.load(std::memory_order_acquire)
							? agent_run_result{
								  .Session = aInvocation->Identity.Session.Id,
								  .Outcome = agent_run_outcome::stale_result,
							  }
							: judged.has_value() ? finalize_agent_run(std::move(result), aInvocation->Identity.Session, *judged, core::agent_run_id{ aInvocation->DurableRun.load(std::memory_order_acquire) })
												 : agent_run_result{
													   .Session = aInvocation->Identity.Session.Id,
													   .Outcome = agent_run_outcome::stale_result,
												   };
						// The terminal carries no undecoded payload. Findings
						// leave the host exactly once, with a verdict each, and
						// through the call below; there is no second route by
						// which an unvalidated finding could reach a
						// presentation.
						final.Findings.reset();
						if (payload.has_value()) {
							self->project_findings(
								aInvocation->Identity,
								deliver_agent_findings(
									*payload,
									aInvocation->Identity.Session,
									aInvocation->Snapshot,
									observed));
						}
						self->project_finish(aInvocation->Identity, final);
						aInvocation->TerminalComplete.store(
							true, std::memory_order_release);
						{
							std::scoped_lock lock{ self->GateMutex };
							if (self->Destroying) {
								return;
							}
						}
						self->post_cleanup(aInvocation);
					});
			} catch (...) {
				aInvocation->TerminalComplete.store(true, std::memory_order_release);
				if (cleanup_workspace(aInvocation)) {
					retire(aInvocation);
				}
			}
		}

		void post_cleanup(const std::shared_ptr<invocation>& aInvocation)
		{
			if (!aInvocation->TerminalComplete.load(std::memory_order_acquire)) {
				return;
			}
			auto expected = false;
			if (!aInvocation->CleanupScheduled.compare_exchange_strong(
					expected, true, std::memory_order_acq_rel)) {
				return;
			}
			const auto weak = weak_from_this();
			try {
				Effects.post([weak, aInvocation] {
					auto self = weak.lock();
					if (!self || !self->enter_callback()) {
						return;
					}
					auto guard   = callback_guard{ self };
					auto cleaned = false;
					try {
						cleaned = self->cleanup_workspace(aInvocation);
					} catch (...) {
						++self->WorkspaceCleanupFailures;
					}
					if (cleaned) {
						self->retire(aInvocation);
					} else {
						aInvocation->CleanupScheduled.store(
							false, std::memory_order_release);
					}
				});
			} catch (...) {
				aInvocation->CleanupScheduled.store(false, std::memory_order_release);
			}
		}

		void retry_cleanup()
		{
			auto active = std::shared_ptr<invocation>{ };
			{
				std::scoped_lock lock{ StateMutex };
				active = ActiveInvocation;
			}
			if (active) {
				post_cleanup(active);
			}
		}

		void retire(const std::shared_ptr<invocation>& aInvocation)
		{
			if (aInvocation->Retired.exchange(true, std::memory_order_acq_rel)) {
				return;
			}
			{
				std::scoped_lock lock{ StateMutex };
				if (ActiveInvocation == aInvocation) {
					ActiveInvocation.reset();
				}
			}
			ActiveJobs.store(0U, std::memory_order_release);
		}

		void cancel()
		{
			auto active = std::shared_ptr<invocation>{ };
			{
				std::scoped_lock lock{ StateMutex };
				active             = ActiveInvocation;
				AdmissionCancelled = AdmissionCancelled || AdmissionPending;
			}
			if (!active) {
				return;
			}
			active->ExplicitlyCancelled.store(true, std::memory_order_release);
			static_cast<void>(active->Stop->request_stop());
			Processes.cancel();
		}

		void supersede(const supersession_cause aCause)
		{
			auto active = std::shared_ptr<invocation>{ };
			{
				std::scoped_lock lock{ StateMutex };
				active = ActiveInvocation;
			}
			if (!active) {
				return;
			}
			// The guard, and the whole of it. A continuation was dispatched
			// against the revision its session was started on, which BPR-0521
			// permits to differ from the one under review — that difference is
			// what the reviewer chose when they kept the session. Stopping it
			// because the binding moved would kill the turn for the condition
			// it was created under, and the reviewer would lose an answer they
			// asked for on purpose. A fresh turn is superseded exactly as
			// before: it was asked about the revision that just moved.
			if (aCause == supersession_cause::binding_moved
				&& active->Continued) {
				return;
			}
			active->Superseded.store(true, std::memory_order_release);
			static_cast<void>(active->Stop->request_stop());
			Processes.cancel();
		}

		void destroy()
		{
			Closing.store(true, std::memory_order_release);
			cancel();
			{
				auto lock          = std::unique_lock{ GateMutex };
				Destroying         = true;
				const auto current = CallbacksByThread.find(
					std::this_thread::get_id());
				const auto currentDepth = current == CallbacksByThread.end()
					? 0U
					: current->second;
				GateChanged.wait(lock, [this, currentDepth] {
					return CallbacksInFlight <= currentDepth;
				});
			}
			auto active = std::shared_ptr<invocation>{ };
			{
				std::scoped_lock lock{ StateMutex };
				active = ActiveInvocation;
			}
			if (active) {
				if (cleanup_workspace(active)) {
					retire(active);
				}
			}
		}

		std::vector<core::agent_profile>     Profiles;
		std::vector<core::agent_host_policy> Policies;
		std::filesystem::path                WorkspaceParent;
		agent_session_catalog_port&          Sessions;
		agent_workspace_port&                Workspaces;
		agent_process_port&                  Processes;
		environment_reader                   Environment;
		effect_executor&                     Effects;
		decision_executor&                   DecisionLane;
		agent_invocation_projection_port&    Projection;
		private_agent_run_port*              PrivateRuns;

		mutable std::mutex                          StateMutex;
		std::optional<core::application_session_id> ApplicationSession;
		std::optional<core::pull_request_authority> Authority;
		std::optional<core::agent_revision_binding> Binding;
		std::shared_ptr<invocation>                 ActiveInvocation;
		std::uint64_t                               InvocationGeneration = 0U;
		/// [[BPR-1211]]. A new session is being admitted for a turn; no
		/// second turn starts meanwhile, and a cancel reaches that turn.
		bool AdmissionPending   = false;
		bool AdmissionCancelled = false;

		std::atomic_bool   Closing    = false;
		std::atomic_size_t ActiveJobs = 0U;
		/// [[BPR-1751]]. Workspace probes posted and not yet answered.
		std::atomic_size_t                     Probes        = 0U;
		std::atomic_uint64_t                   ProbeSessions = 0U;
		std::mutex                             GateMutex;
		std::condition_variable                GateChanged;
		std::size_t                            CallbacksInFlight = 0U;
		std::map<std::thread::id, std::size_t> CallbacksByThread;
		bool                                   Destroying = false;

		std::atomic_uint64_t CoreDecisions            = 0U;
		std::atomic_uint64_t WorkspaceStarts          = 0U;
		std::atomic_uint64_t ProcessStarts            = 0U;
		std::atomic_uint64_t WorkspaceCleanups        = 0U;
		std::atomic_uint64_t WorkspaceCleanupFailures = 0U;
		std::atomic_uint64_t SessionAdmissions        = 0U;
		std::atomic_uint64_t SessionAdmissionFailures = 0U;
	};

	agent_invocation_host::agent_invocation_host(
		std::vector<core::agent_profile>     aProfiles,
		std::vector<core::agent_host_policy> aPolicies,
		std::filesystem::path                aWorkspaceParent,
		agent_session_catalog_port&          aSessions,
		agent_workspace_port&                aWorkspaces,
		agent_process_port&                  aProcesses,
		environment_reader                   aEnvironment,
		effect_executor&                     aEffects,
		decision_executor&                   aDecisionLane,
		agent_invocation_projection_port&    aProjection,
		private_agent_run_port*              aPrivateRuns)
		: Implementation{ std::make_shared<implementation>(
			  std::move(aProfiles),
			  std::move(aPolicies),
			  std::move(aWorkspaceParent),
			  aSessions,
			  aWorkspaces,
			  aProcesses,
			  std::move(aEnvironment),
			  aEffects,
			  aDecisionLane,
			  aProjection, aPrivateRuns) }
	{
	}

	agent_invocation_host::~agent_invocation_host()
	{
		if (Implementation) {
			Implementation->destroy();
			Implementation.reset();
		}
	}

	void agent_invocation_host::observe(
		const core::application_session_id aApplicationSession,
		core::pull_request_authority       aAuthority,
		core::agent_revision_binding       aBinding)
	{
		const auto implementation = Implementation;
		implementation->observe(
			aApplicationSession,
			std::move(aAuthority),
			std::move(aBinding));
	}

	void agent_invocation_host::clear()
	{
		const auto implementation = Implementation;
		implementation->clear();
	}

	void agent_invocation_host::dispatch(agent_invocation_request aRequest)
	{
		const auto implementation = Implementation;
		implementation->dispatch(std::move(aRequest));
	}

	void agent_invocation_host::cancel()
	{
		const auto implementation = Implementation;
		implementation->cancel();
	}

	void agent_invocation_host::shutdown()
	{
		const auto implementation = Implementation;
		implementation->Closing.store(true, std::memory_order_release);
		implementation->cancel();
	}

	void agent_invocation_host::retry_cleanup()
	{
		const auto implementation = Implementation;
		implementation->retry_cleanup();
	}

	auto agent_invocation_host::is_quiescent() const noexcept -> bool
	{
		const auto implementation = Implementation;
		return implementation->ActiveJobs.load(std::memory_order_acquire) == 0U && implementation->Probes.load(std::memory_order_acquire) == 0U;
	}

	void agent_invocation_host::probe_workspace(const core::application_session_id aApplicationSession, core::pull_request_authority aAuthority,
												core::agent_revision_binding aBinding, std::optional<std::filesystem::path> aHistoryClone, workspace_probe_answer aAnswer)
	{
		const auto host = Implementation;
		if (host->Closing.load(std::memory_order_acquire)) {
			if (aAnswer) {
				aAnswer(std::unexpected(agent_workspace_error{ agent_workspace_failure::cancelled }));
			}
			return;
		}
		// The reserved range: the high half of the session identities, which
		// the durable catalog's monotonic identities never reach.
		constexpr auto reserved    = std::uint64_t{ 1U } << 63U;
		auto           request     = agent_workspace_materialization_request{ };
		request.ApplicationSession = aApplicationSession;
		request.Authority          = std::move(aAuthority);
		request.Session.Id         = core::agent_session_id{ reserved + host->ProbeSessions.fetch_add(1U) + 1U };
		request.Session.Profile    = core::external_terminal_profile_id;
		request.Session.Binding    = std::move(aBinding);
		request.ParentDirectory    = host->WorkspaceParent;
		request.HistoryClone       = std::move(aHistoryClone);
		++host->Probes;
		try {
			host->Effects.post([weak = std::weak_ptr{ host }, probe = std::move(request), answer = std::move(aAnswer)]() mutable {
				auto self = weak.lock();
				if (!self) {
					return;
				}
				if (!self->enter_callback()) {
					--self->Probes;
					return;
				}
				auto guard  = implementation::callback_guard{ self };
				auto result = std::expected<prepared_workspace, agent_workspace_error>{ std::unexpected(agent_workspace_error{ agent_workspace_failure::invalid_request }) };
				try {
					auto made = self->Workspaces.materialize_agent_workspace(probe, { });
					if (made) {
						result = made->Workspace;
						static_cast<void>(self->Workspaces.cleanup_agent_workspace(*made, { }));
					} else {
						result = std::unexpected(std::move(made.error()));
					}
				} catch (...) { }
				try {
					self->DecisionLane.post([self, reply = std::move(answer), outcome = std::move(result)]() mutable {
						--self->Probes;
						if (!self->Closing.load(std::memory_order_acquire) && reply) {
							reply(std::move(outcome));
						}
					});
				} catch (...) {
					--self->Probes;
				}
			});
		} catch (...) {
			--host->Probes;
		}
	}

	auto agent_invocation_host::statistics() const noexcept
		-> agent_invocation_host_statistics
	{
		const auto implementation = Implementation;
		return {
			.CoreDecisions     = implementation->CoreDecisions.load(),
			.WorkspaceStarts   = implementation->WorkspaceStarts.load(),
			.ProcessStarts     = implementation->ProcessStarts.load(),
			.WorkspaceCleanups = implementation->WorkspaceCleanups.load(),
			.WorkspaceCleanupFailures
			= implementation->WorkspaceCleanupFailures.load(),
			.SessionAdmissions        = implementation->SessionAdmissions.load(),
			.SessionAdmissionFailures = implementation->SessionAdmissionFailures.load(),
		};
	}
} // namespace bpr::application
