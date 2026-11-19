#include "bpr/application/agent_invocation_host.hpp"
#include "bpr/application/agent_conversation_host.hpp"
#include "bpr/application/agent_scratch_folder.hpp"
#include "bpr/application/operation_diagnostics.hpp"
#include "bpr/application/tour_agent_conversation.hpp"

#include "bpr/core/hex.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <ranges>
#include <stop_token>
#include <system_error>
#include <thread>
#include <utility>

namespace bpr::application {
	[[nodiscard]] auto native_scope_key(const core::pull_request_authority& aAuthority, const std::string& aAgent, const std::string& aThread) -> std::string
	{
		return std::to_string(aAuthority.Profile.value()) + ":" + std::to_string(aAuthority.Account.value()) + ":"
			+ std::to_string(aAuthority.Viewer.value()) + ":" + std::to_string(aAuthority.Host.value()) + ":" + std::to_string(aAuthority.Repository.value())
			+ ":" + std::to_string(aAuthority.PullRequest.value()) + ":" + std::to_string(aAuthority.ProfileRevision.value()) + ":"
			+ std::to_string(static_cast<unsigned>(aAuthority.Mode)) + ":" + aAgent + ":" + aThread;
	}

	auto agent_workspace_key(const core::pull_request_authority& aAuthority, const core::agent_revision_binding& aBinding) -> std::string
	{
		return native_scope_key(aAuthority, "", "") + ":" + std::to_string(aAuthority.Number.value()) + ":" + core::lower_hex(aAuthority.HostAuthority.bytes()) + ":"
			+ core::lower_hex(aAuthority.Cache.bytes()) + ":" + core::lower_hex(aAuthority.Scope.bytes()) + ":" + std::string{ core::canonical_hex(aBinding.BaseOid).view() } + ":"
			+ std::string{ core::canonical_hex(aBinding.HeadOid).view() };
	}

	namespace {
		/// [[BPR-2153]]. The messages steered into a run, as its stored turn
		/// keeps them: each where it went in, as bytes of the stored answer
		/// (the assistant items written before it; with only the last item
		/// kept, before it unless that item had begun).
		[[nodiscard]] auto steered_messages(const agent_run_result& aResult, const bool aLastItemOnly, const std::string& aAnswer) -> std::vector<agent_steered_message>
		{
			auto messages = std::vector<agent_steered_message>{ };
			for (const auto& input : aResult.SteeredInputs) {
				if (messages.size() >= core::maximum_steers_per_run || input.Text.empty()) {
					break;
				}
				auto at = std::size_t{ 0U };
				if (aLastItemOnly) {
					at = input.AfterItems >= aResult.AssistantItems.size() ? aAnswer.size() : 0U;
				} else {
					const auto written = std::min(input.AfterItems, aResult.AssistantItems.size());
					for (std::size_t item = 0U; item < written; ++item) {
						at += aResult.AssistantItems[item].size();
					}
				}
				messages.push_back({ .Text = input.Text, .At = std::min(at, aAnswer.size()), .AfterSteps = input.AfterSteps, .Disposition = input.Disposition });
			}
			return messages;
		}

		// Read only this selected catalog generation, never an in-memory cache of other chats.
		auto prose_replay(const native_agent_history_projection& aHistory, const bool aTour) -> core::transcript_replay
		{
			auto turns      = std::vector<core::brief_turn>{ };
			auto unanswered = std::size_t{ 0U };
			for (const auto& saved : aHistory.Turns) {
				if (saved.Outcome != core::agent_turn_outcome::answered) {
					++unanswered;
					continue;
				}
				auto answer = saved.Answered;
				if (aTour) {
					if (const auto parsed = decode_tour_ask_answer(answer)) {
						answer = parsed->Reply;
					} else if (decode_tour_draft_answer(answer)) {
						answer = "Draft Tour proposal (history does not restore or accept proposals).";
					}
				}
				turns.push_back({ saved.Asked, std::move(answer) });
			}
			auto result = core::bound_transcript_replay(std::move(turns), aTour);
			result.Omitted += unanswered;
			return result;
		}

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
			const bool Continued = false;
			/// [[BPR-2241]]. When the turn was admitted, as the Agent tab
			/// starts its clock: the stored turn keeps how long it took.
			const std::chrono::steady_clock::time_point Admitted = std::chrono::steady_clock::now();
			std::shared_ptr<std::stop_source>           Stop;
			std::mutex                                  WorkspaceMutex;
			std::optional<owned_agent_workspace>        OwnedWorkspace;
			std::optional<native_agent_session>         Native;
			bool                                        RetainWorkspace = false;
			/// [[BPR-1999]]. The run resumed the runtime's own session (its
			/// handle was known at launch), so its reported cost is that
			/// session's running total.
			bool                                 ResumedNative   = false;
			bool                                 NativeSubmitted = false;
			std::optional<private_run_admission> PrivateAdmission;
			/// [[BPR-2007]]. The turn's scratch folder, its key under the
			/// root, whether it is kept with a native session, and what it
			/// held when the turn started.
			std::filesystem::path             ScratchFolder;
			std::string                       ScratchKey;
			bool                              ScratchKept = false;
			agent_scratch_listing             ScratchBefore;
			std::optional<agent_launch_grant> Grant;
			/// Re-review N3. A folder was asked for and none could be made.
			bool ScratchUnmade = false;
			/// RFC-BPR-0104 as amended 2026-11-11. The folder's limit, when
			/// it is watched during the turn (it was below it at the start;
			/// a turn starting full is given no write and, for Claude, no run
			/// allowance there, so nothing of its grant writes it);
			/// when it was last measured; whether the turn passed it.
			std::uint64_t        ScratchLimit        = 0U;
			std::atomic_int64_t  ScratchMeasuredAt   = 0;
			std::atomic_bool     ScratchOverflowed   = false;
			std::atomic_uint64_t DurableRun          = 0;
			std::atomic_bool     Superseded          = false;
			std::atomic_bool     TerminalComplete    = false;
			std::atomic_bool     CleanupScheduled    = false;
			std::atomic_bool     Retired             = false;
			std::atomic_bool     ExplicitlyCancelled = false;
			/// [[BPR-2153]]. While the process runs: its protocol, and the kept
			/// native connection and turn a correction is handed to (none for
			/// a one-shot or replayed run).
			mutable std::mutex                  SteerMutex;
			std::optional<core::agent_protocol> SteerProtocol;
			std::optional<agent_steer_request>  SteerTarget;
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
			private_agent_run_port* aPrivateRuns, native_agent_session_store_port* aNativeSessions, diagnostic_log_port* aLog)
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
			, NativeSessions{ aNativeSessions }
			, Log{ aLog }
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
			NativeRestoreKey.clear();
			++NativeRestoreGeneration;
			NativeDrainRequested = true;
			Processes.cancel();
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
			if (NativeDrainRequested && !Processes.native_connections_quiescent()) {
				project_refuse("native_connection_still_stopping");
				return;
			}
			NativeDrainRequested = false;
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
			// [[BPR-1900]]. A tour drafting run reads the workspace with
			// read-only tools, for the two agents whose allowlist is recorded.
			if (aRequest.ReadOnlyTools
				&& (aRequest.WritingOnly || aRequest.ExistingCheckoutChosen || aRequest.MeasurePaths.size() > maximum_measured_paths
					|| (profile->Protocol != core::agent_protocol::claude_stream_json && profile->Protocol != core::agent_protocol::codex_jsonl_exec && profile->Protocol != core::agent_protocol::codex_app_server))) {
				project_refuse("selected_agent_has_no_qualified_read_only_protocol");
				return;
			}
			if (aRequest.WritingOnly || aRequest.ReadOnlyTools ? aRequest.PrivateRun.has_value() : ((PrivateRuns != nullptr) != aRequest.PrivateRun.has_value())) {
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
			// [[BPR-1935]]. A read-only run is a tour run, and its prompt
			// travels in the tour prompt's own carrier; a refusal for size or
			// for a misplaced prompt says so by name.
			auto supplement = build_agent_context_supplement(
				aRequest.Context, provisionalSession,
				aRequest.ReadOnlyTools                                             ? agent_context_run::read_only_tour
					: aRequest.WritingOnly && !aRequest.Context.TourPrompt.empty() ? agent_context_run::tool_free_tour
																				   : agent_context_run::ordinary);
			if (!supplement.has_value()) {
				project_refuse(refusal_name(supplement.error()));
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
				const auto idle = [&self] {
					self->ActiveJobs.store(0U, std::memory_order_release);
					self->announce_retirement();
				};
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
			if (aInvocation->RetainWorkspace) {
				return true;
			}
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
				// [[BPR-2138]]. Only a resumed path keeps a native session.
				const auto native = NativeSessions != nullptr && !aRequest.NativeAgentIdentity.empty() && !aRequest.NativeScope.empty()
					&& !aRequest.WritingOnly && !aRequest.ExistingCheckoutChosen && core::session_mode_of(aRequest.Path) == core::agent_session_mode::resumed;
				const auto refuseNative = [&](const std::string& aReason, const core::operation_failure aFailure,
											  const agent_run_outcome                                 aOutcome         = agent_run_outcome::private_storage_failure,
											  const std::optional<agent_workspace_unavailable_reason> aWorkspaceReason = std::nullopt) {
					record_operation(Log, { .Action = core::operation_action::native_continuation, .Failure = aFailure });
					finish_run({ .Session = aInvocation->Identity.Session.Id, .Outcome = aOutcome, .StandardError = aReason, .WorkspaceReason = aWorkspaceReason, .RefusalReason = aReason }, aInvocation);
				};
				auto       workspaceLease     = std::unique_ptr<agent_workspace_turn_lease>{ };
				auto       workspacePathLease = std::unique_ptr<agent_workspace_turn_lease>{ };
				const auto workspaceKey       = native ? agent_workspace_key(aRequest.Authority, aInvocation->Identity.Session.Binding) : std::string{ };
				if (native) {
					workspaceLease = NativeSessions->try_lock_workspace(workspaceKey);
					if (!workspaceLease) {
						refuseNative("Another agent is using this pull request's workspace. Wait for it to finish or stop it, then send again. No question was sent.", core::operation_failure::changed_candidate);
						return;
					}
					// Application/window and invocation IDs are deliberately absent.
					const auto scope    = native_scope_key(aRequest.Authority, aRequest.NativeAgentIdentity, aRequest.NativeScope);
					auto       selected = NativeSessions->select(scope, std::to_string(static_cast<unsigned>(aProfile.Protocol)) + ":" + aRequest.NativeAgentIdentity + (aRequest.ReadOnlyTools ? ":read-only" : ""),
																 aInvocation->Identity.Session.Binding, aRequest.NativeStartAtCurrentRevision);
					if (!selected) {
						refuseNative("Native session storage is unavailable, full, or changed in another window. No question was sent.", core::operation_failure::storage_failed);
						return;
					}
					if ((!aRequest.SelectedConversation.empty() && selected->Id != aRequest.SelectedConversation) || selected->Replay != aRequest.TranscriptReplay) {
						refuseNative("The selected continuation changed. Open History and choose it again; no question was sent.", core::operation_failure::changed_candidate);
						return;
					}
					if (aProfile.Protocol == core::agent_protocol::external_terminal && !selected->Replay) {
						refuseNative("This backend has no qualified native continuation. Choose Transcript replay or New session in History; no question was sent.", core::operation_failure::unsupported);
						return;
					}
					if (selected->Pending) {
						refuseNative("The previous question may have been sent before interruption. It was not resent. Start a new session or inspect the native history before continuing.", core::operation_failure::outcome_unknown);
						return;
					}
					auto bound    = aInvocation->Identity.Session;
					bound.Binding = selected->Binding;
					if (!core::may_treat_as_current(bound, aInvocation->Identity.Session.Binding)) {
						refuseNative("This native session is pinned to an earlier revision. Explicitly continue that revision or start a new session.", core::operation_failure::stale);
						return;
					}
					selected->WorkspaceKey = workspaceKey;
					const auto shared      = NativeSessions->shared_workspace(workspaceKey, *selected);
					if (!shared) {
						refuseNative(shared.error() == native_agent_storage_failure::conflict
										 ? "This older session has a different workspace. Its files and history are kept. Choose New session to use the shared PR workspace; no question was sent."
										 : "The shared PR workspace record could not be opened. Its files and history are kept; no question was sent.",
									 shared.error() == native_agent_storage_failure::conflict ? core::operation_failure::changed_candidate : core::operation_failure::storage_failed);
						return;
					}
					selected->Workspace = *shared;
					// [[BPR-2293]]. The retained workspace is held by its path
					// too, so expiry never removes one a turn is using, even
					// one kept before its pull request key was recorded.
					if (selected->Workspace) {
						workspacePathLease = NativeSessions->try_lock_workspace(agent_workspace_path_lease(selected->Workspace->Workspace.Path));
						if (!workspacePathLease) {
							refuseNative("Another agent is using this pull request's workspace. Wait for it to finish or stop it, then send again. No question was sent.", core::operation_failure::changed_candidate);
							return;
						}
					}
					aInvocation->Native = std::move(*selected);
					if (aInvocation->Native->Replay) {
						const auto history = NativeSessions->history(scope, aInvocation->Native->Configuration);
						if (!history || history->Selected != aInvocation->Native->Id) {
							refuseNative("Replay history changed or is unavailable. No question was sent.", core::operation_failure::storage_failed);
							return;
						}
						aSupplement.Replay = prose_replay(*history, aRequest.ReadOnlyTools);
					}
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
				if (aInvocation->Native && aInvocation->Native->Workspace) {
					auto retained               = *aInvocation->Native->Workspace;
					retained.Authority          = aRequest.Authority;
					retained.ApplicationSession = aRequest.ApplicationSession;
					// [[BPR-2261]]. The turn says it checks its workspace,
					// not that it makes one: nothing is made.
					post_stream(aInvocation, { .Session = aInvocation->Identity.Session.Id, .Run = core::agent_run_id{ aInvocation->DurableRun.load(std::memory_order_acquire) }, .Kind = agent_stream_kind::workspace_check });
					const auto inspected = Workspaces.inspect_agent_workspace(retained, aInvocation->Stop->get_token());
					if (!inspected) {
						if (inspected.error().Code == agent_workspace_failure::cancelled) {
							refuseNative("The retained workspace check was stopped. No question was sent.", core::operation_failure::cancelled, agent_run_outcome::cancelled);
							return;
						}
						using reason      = agent_workspace_unavailable_reason;
						const auto why    = inspected.error().Code == agent_workspace_failure::unavailable ? inspected.error().Reason : reason::retained_unavailable;
						const auto detail = why == reason::inspection_output_limit ? "The retained workspace check reached its output limit. No question was sent; its transcript and files are kept."
							: why == reason::inspection_timed_out                  ? "The retained workspace check reached its time limit. No question was sent; its transcript and files are kept."
							: why == reason::retained_objects_missing              ? "The retained workspace is missing a required Git commit or tree. No question was sent; its transcript and files are kept."
																				   : "The retained native workspace or required Git objects could not be verified. No question was sent; its transcript and files are kept.";
						refuseNative(detail, operation_failure_of(agent_workspace_error{ agent_workspace_failure::unavailable, { }, why }),
									 agent_run_outcome::workspace_unavailable, why);
						return;
					}
					// [[BPR-2118]]. The run uses the identities the journal
					// records now; the agent launcher checks the guard by them.
					workspace                    = inspected->Workspace;
					lifetimeGuard                = inspected->LifetimeGuard;
					aInvocation->RetainWorkspace = true;
				} else if (aRequest.ExistingCheckoutChosen) {
					workspace = std::move(aExisting.Workspace);
				} else {
					++WorkspaceStarts;
					auto materialized = Workspaces.materialize_agent_workspace(
						agent_workspace_materialization_request{
							.ApplicationSession  = aRequest.ApplicationSession,
							.Authority           = aRequest.Authority,
							.Session             = aInvocation->Identity.Session,
							.ParentDirectory     = WorkspaceParent,
							.HistoryClone        = aRequest.HistoryClone,
							.HistoryCloneManaged = aRequest.HistoryCloneManaged,
							.RetainForSession    = native,
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
					if (aInvocation->Native) {
						aInvocation->Native->Workspace = *materialized;
					}
					track_workspace(aInvocation, std::move(*materialized));
				}

				if (aInvocation->Stop->stop_requested()) {
					finish_run(
						{ .Session = aInvocation->Identity.Session.Id,
						  .Outcome = agent_run_outcome::cancelled },
						aInvocation);
					return;
				}
				auto options               = aInvocation->PrivateAdmission ? aInvocation->PrivateAdmission->Launch : agent_launch_options{ .Requested = aRequest.RequestedSettings, .EntryPoint = aRequest.Context.EntryPoint, .WritingOnly = aRequest.WritingOnly, .ReadOnlyTools = aRequest.ReadOnlyTools, .RunChecks = aRequest.RunChecks };
				options.Path               = aRequest.Path;
				options.McpServers         = aRequest.McpServers;
				options.SystemInstructions = aRequest.SystemInstructions;
				options.Instructions       = aRequest.Instructions;
				options.WritingStyle       = aRequest.WritingStyle;
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
				// [[BPR-2007]], [[BPR-2008]], RFC-BPR-0104. The grant this
				// turn launches with: a scratch folder kept with a native
				// session (or this turn's own), and the disposable checkout to
				// build in. A folder that cannot be made, or named in a rule,
				// is no grant; the turn reads only.
				// Re-review N1a (2026-11-11): Codex in the reviewer's own
				// checkout reads that checkout's trusted `.codex/config.toml`,
				// which BPR's profile would replace, so it is given nothing
				// there (the picker says why).
				// F1 (third round, 2026-11-11). A checkout BPR made holds the
				// pull request's own `.codex` and `.claude` settings; the
				// launch names it so they are never loaded, by the path the
				// agent runs in and the canonical one Codex matches.
				// Fourth review (2026-11-11): Codex keys the project by the
				// kernel's name for its working folder, which a firmlink
				// spelling's canonical path is not, so the opened folder's
				// path is named too; a checkout that cannot be resolved is
				// never launched in (it failed open before).
				if (!aRequest.ExistingCheckoutChosen && workspace) {
					auto spellings = agent_checkout_spellings(workspace->Path);
					if (!spellings) {
						finish_run(
							{ .Session       = aInvocation->Identity.Session.Id,
							  .Outcome       = agent_run_outcome::failed_to_start,
							  .StandardError = "The pull request's checkout could not be resolved, so its own agent settings could not be shut out. No question was sent." },
							aInvocation);
						return;
					}
					options.PullRequestCheckout = std::move(*spellings);
				}
				const auto codexOwn = aRequest.ExistingCheckoutChosen
					&& (aProfile.Protocol == core::agent_protocol::codex_jsonl_exec || aProfile.Protocol == core::agent_protocol::codex_app_server);
				if ((aRequest.ScratchFolder || aRequest.BuildsAndTests) && !aRequest.WritingOnly && !aRequest.ReadOnlyTools && !codexOwn) {
					auto grant                 = agent_launch_grant{ };
					aInvocation->ScratchUnmade = aRequest.ScratchFolder;
					if (aRequest.ScratchFolder && !ScratchRoot.empty()) {
						const auto kept = aInvocation->Native.has_value() && valid_agent_scratch_key(aInvocation->Native->Id);
						const auto key  = kept ? aInvocation->Native->Id
											   : "run-" + std::to_string(aRequest.ApplicationSession.value()) + "-" + std::to_string(aInvocation->Identity.Session.Id.value());
						if (const auto folder = prepare_agent_scratch_folder(ScratchRoot, key); folder && valid_agent_grant_path(folder->string())) {
							aInvocation->ScratchUnmade = false;
							aInvocation->ScratchFolder = *folder;
							aInvocation->ScratchKey    = key;
							aInvocation->ScratchKept   = kept;
							aInvocation->ScratchBefore = list_agent_scratch_folder(*folder);
							grant.ScratchFolder        = folder->string();
							grant.ScratchLimitMiB      = core::agent_scratch_limit_mib(aRequest.ScratchLimitMiB);
							grant.ScratchFull          = agent_scratch_full(aInvocation->ScratchBefore, core::agent_scratch_limit_bytes(aRequest.ScratchLimitMiB));
							aInvocation->ScratchLimit  = grant.ScratchFull ? 0U : core::agent_scratch_limit_bytes(aRequest.ScratchLimitMiB);
						}
					}
					if (aRequest.BuildsAndTests && !aRequest.ExistingCheckoutChosen && workspace && valid_agent_build_path(workspace->Path)) {
						grant.BuildWorkspace = workspace->Path;
					}
					if (!grant.ScratchFolder.empty() || !grant.BuildWorkspace.empty()) {
						options.Grant      = grant;
						aInvocation->Grant = grant;
					}
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
				if (aInvocation->Native) {
					auto& session   = *aInvocation->Native;
					auto  pending   = session;
					pending.Pending = true;
					if (!NativeSessions->store(pending)) {
						refuseNative("The native session could not be saved. No question was sent.", core::operation_failure::storage_failed);
						return;
					}
					session                      = std::move(pending);
					aInvocation->RetainWorkspace = true;
					if (!session.Replay) {
						launch->NativeConversation = session.Id;
						launch->NativeHandle       = session.Handle;
						aInvocation->ResumedNative = !session.Handle.empty();
						launch->NativeTurn         = session.Turn;
						launch->SaveNativeHandle   = std::make_shared<std::function<bool(const std::string&)>>([this, aInvocation](const std::string& aHandle) {
							aInvocation->Native->Handle = aHandle;
							return NativeSessions->store(*aInvocation->Native);
						});
						if (launch->Protocol == core::agent_protocol::codex_jsonl_exec) {
							auto prepared = prepare_native_agent_turn(std::move(*launch), false);
							if (!prepared) {
								refuseNative("Native resume is unavailable for this configuration. No replay was attempted.", core::operation_failure::unsupported);
								return;
							}
							launch = std::move(*prepared);
							// JSONL has exact-ID native resume, not persistent process reuse.
							launch->NativeConversation.clear();
						}
					}
				}
				launch->WorkingDirectory       = workspace->Path;
				launch->WorkspaceLifetimeGuard = std::move(lifetimeGuard);
				// [[BPR-2004]], RFC-BPR-0103. A run that shows a turn shows
				// what it is doing; a writing run shows no turn.
				launch->ReportActivity = !aRequest.WritingOnly;
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
				// [[BPR-1900]]. The head lines a proposed stop is checked
				// against, counted before the agent can touch anything.
				auto measured = aRequest.MeasurePaths.empty() ? std::vector<std::pair<std::string, std::uint32_t>>{ } : measure_workspace_lines(std::filesystem::path{ workspace->Path }, aRequest.MeasurePaths);
				post_start(aInvocation, *workspace);
				++ProcessStarts;
				const auto weak = weak_from_this();
				// Owner decision 2026-10-23: no time limit unless the reviewer
				// set one; Stop is always there.
				aInvocation->NativeSubmitted = true;
				{
					const auto lock            = std::scoped_lock{ aInvocation->SteerMutex };
					aInvocation->SteerProtocol = launch->Protocol;
					if (!launch->NativeConversation.empty()) {
						aInvocation->SteerTarget = agent_steer_request{ .Conversation = launch->NativeConversation, .Run = launch->Run, .NativeTurn = launch->NativeTurn };
					}
				}
				auto result = Processes.run(
					*launch,
					aRequest.RunLimitMilliseconds.value_or(agent_run_without_limit),
					[weak, aInvocation](const agent_stream_event& aEvent) {
						watch_scratch(*aInvocation);
						auto self = weak.lock();
						if (!self) {
							return;
						}
						if (aEvent.Kind != agent_stream_kind::cost) {
							self->post_stream(aInvocation, aEvent);
							return;
						}
						// [[BPR-2252]]. The turn's cost so far, accounted as
						// its end accounts it: on this thread, which alone
						// touches the native session until the run returns.
						auto event = aEvent;
						event.Cost = running_turn_cost(*aInvocation, event.Text);
						self->post_stream(aInvocation, std::move(event));
					},
					aInvocation->Stop->get_token());
				{
					const auto lock = std::scoped_lock{ aInvocation->SteerMutex };
					aInvocation->SteerProtocol.reset();
					aInvocation->SteerTarget.reset();
				}
				if (launch->PrivateInputClaim) {
					launch->PrivateInputClaim->release();
				}
				// [[BPR-1999]], RFC-BPR-0101. The turn's cost, accounted by the
				// core: a run that resumed the runtime's own session reports
				// that session's running total, so the turn cost the
				// difference; any other run's figure is its own.
				const auto reported = result.ReportedCostUsd ? core::parse_agent_cost(*result.ReportedCostUsd) : std::nullopt;
				result.TurnCost     = reported;
				if (aInvocation->Native) {
					auto&      session  = *aInvocation->Native;
					const auto resumed  = aInvocation->ResumedNative;
					const auto cost     = core::account_agent_turn_cost(session.Cost, reported, resumed);
					session.Cost        = cost.Ledger;
					result.TurnCost     = cost.Turn;
					result.SessionCost  = cost.Ledger.Known ? std::optional{ cost.Ledger.Total } : std::nullopt;
					result.Conversation = session.Id;
					if (!session.Replay && !result.NativeHandle.empty()) {
						session.Handle = result.NativeHandle;
					}
					session.Pending = !session.Replay && result.NativeHandle.empty() && session.Handle.empty() && result.Outcome != agent_run_outcome::failed_to_start;
					++session.Turn;
					auto answer = std::string{ };
					if (aRequest.ReadOnlyTools && !result.AssistantItems.empty()) {
						answer = result.AssistantItems.back();
					} else {
						// [[BPR-2261]]. Each item is its own paragraph, as the
						// live turn showed it ([[BPR-2219]]): joined with
						// nothing, the narration ran into the report's title
						// and a reload drew no report card.
						for (const auto& item : result.AssistantItems) {
							if (!answer.empty() && !item.empty() && item.front() != '\n') {
								answer += "\n\n";
							}
							answer += item;
						}
					}
					if (answer.empty()) {
						for (const auto& event : result.Events) {
							answer += event.Text;
						}
					}
					auto turn     = agent_conversation_turn{ };
					turn.Asked    = aRequest.NativeQuestion.empty() ? aRequest.Context.ChatMessage : aRequest.NativeQuestion;
					turn.Steered  = steered_messages(result, aRequest.ReadOnlyTools && !result.AssistantItems.empty(), answer);
					turn.Answered = std::move(answer);
					turn.Ending   = std::string{ name(result.Outcome) };
					// [[BPR-2261]]. A run the app's closing stopped is kept as
					// that, not as an error to explain. One stopped after it
					// answered with no helper out is already a success.
					turn.Outcome = result.Outcome == agent_run_outcome::success                                     ? core::agent_turn_outcome::answered
						: Closing.load(std::memory_order_acquire) && result.Outcome == agent_run_outcome::cancelled ? core::agent_turn_outcome::closed_with_app
																													: core::agent_turn_outcome::ended_without_answer;
					turn.Seconds = agent_turn_seconds(aInvocation->Admitted);
					auto bounded = bound_conversation({ std::move(turn) });
					for (auto& steered : bounded.front().Steered) {
						steered.At = std::min(steered.At, bounded.front().Answered.size());
					}
					if (!NativeSessions->append(session.Id, bounded.front()) || !NativeSessions->store(session)) {
						result.Outcome       = agent_run_outcome::private_storage_failure;
						result.StandardError = "The question ended, but native session storage failed. Its submission remains uncertain; it will not be resent.";
					}
				}
				result.MeasuredLines = std::move(measured);
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
			if (aInvocation->Native && aInvocation->Native->Pending && !aInvocation->NativeSubmitted) {
				aInvocation->Native->Pending = false;
				if (!NativeSessions->store(*aInvocation->Native)) {
					aResult.Outcome = agent_run_outcome::private_storage_failure;
				}
			}
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
			// [[BPR-2007]]. What the turn left in its scratch folder; a turn's
			// own folder goes with it.
			aResult.Grant         = aInvocation->Grant;
			aResult.ScratchUnmade = aInvocation->ScratchUnmade;
			if (!aInvocation->ScratchFolder.empty()) {
				const auto after          = list_agent_scratch_folder(aInvocation->ScratchFolder);
				aResult.ScratchFolder     = aInvocation->ScratchFolder.string();
				aResult.ScratchFiles      = agent_scratch_written(aInvocation->ScratchBefore, after, core::maximum_agent_activity_listed);
				aResult.ScratchFull       = aInvocation->Grant && agent_scratch_full(after, core::agent_scratch_limit_bytes(aInvocation->Grant->ScratchLimitMiB));
				aResult.ScratchOverflowed = aInvocation->ScratchOverflowed.load(std::memory_order_acquire);
				if (!aInvocation->ScratchKept) {
					static_cast<void>(remove_agent_scratch_folder(ScratchRoot, aInvocation->ScratchKey));
					aResult.ScratchFolder.clear();
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

		/// RFC-BPR-0104 as amended 2026-11-11 (owner look e97c6a3d). A
		/// turn's folder is measured as it reports its steps, at most every
		/// half second; the turn is stopped once the folder reaches its limit.
		/// A step that writes past the limit finishes before the stop, so the
		/// folder can end above it by what that step wrote.
		/// [[BPR-2252]]. What a running turn has cost so far, from the run's
		/// figure: a native session's turn is its share of the session's
		/// running total, as `account_agent_turn_cost` settles it at the end;
		/// any other run's figure is its own. None for no figure.
		[[nodiscard]] static auto running_turn_cost(const invocation& aInvocation, const std::string_view aFigure) -> std::optional<core::agent_cost>
		{
			const auto reported = aFigure.empty() ? std::nullopt : core::parse_agent_cost(aFigure);
			if (!reported || !aInvocation.Native) {
				return reported;
			}
			return core::account_agent_turn_cost(aInvocation.Native->Cost, reported, aInvocation.ResumedNative).Turn;
		}

		static void watch_scratch(invocation& aInvocation)
		{
			if (aInvocation.ScratchLimit == 0U || aInvocation.ScratchOverflowed.load(std::memory_order_acquire)) {
				return;
			}
			const auto now  = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			auto       last = aInvocation.ScratchMeasuredAt.load(std::memory_order_acquire);
			if (now - last < 500 || !aInvocation.ScratchMeasuredAt.compare_exchange_strong(last, now, std::memory_order_acq_rel)) {
				return;
			}
			if (agent_scratch_full(list_agent_scratch_folder(aInvocation.ScratchFolder), aInvocation.ScratchLimit)) {
				aInvocation.ScratchOverflowed.store(true, std::memory_order_release);
				static_cast<void>(aInvocation.Stop->request_stop());
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
						self->NativeRestoreKey.clear();
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
			announce_retirement();
			if (HasDeferredNativeHistory.exchange(false)) {
				++Probes;
				try {
					DecisionLane.post([self = shared_from_this()] {
						--self->Probes;
						self->announce_retirement();
						if (!self->Closing) {
							auto choice      = std::exchange(self->DeferredNativeChoice, { });
							auto observation = std::exchange(self->DeferredNativeHistory, { });
							if (choice) {
								choice();
							}
							if (observation) {
								observation();
							}
						}
					});
				} catch (...) {
					--Probes;
				}
			}
		}

		void cancel()
		{
			auto active = std::shared_ptr<invocation>{ };
			{
				std::scoped_lock lock{ StateMutex };
				active             = ActiveInvocation;
				AdmissionCancelled = AdmissionCancelled || AdmissionPending;
			}
			NativeDrainRequested = true;
			Processes.cancel();
			if (!active) {
				return;
			}
			active->ExplicitlyCancelled.store(true, std::memory_order_release);
			static_cast<void>(active->Stop->request_stop());
		}

		/// [[BPR-2153]]. The steering channel of `aRun`, which must be the
		/// active run, or why it has none. The version is the one its runtime
		/// reported to the process port, never assumed.
		[[nodiscard]] auto steer_target(const agent_invocation_projection_identity& aRun) const -> std::expected<agent_steer_request, core::agent_steer_support>
		{
			auto active = std::shared_ptr<invocation>{ };
			{
				std::scoped_lock lock{ StateMutex };
				active = ActiveInvocation;
			}
			if (!active) {
				return std::unexpected(core::agent_steer_support::not_running);
			}
			if (active->Identity != aRun) {
				return std::unexpected(core::agent_steer_support::other_run);
			}
			if (active->Stop->stop_requested()) {
				return std::unexpected(core::agent_steer_support::not_running);
			}
			auto target = std::optional<agent_steer_request>{ };
			{
				const auto lock = std::scoped_lock{ active->SteerMutex };
				if (!active->SteerProtocol) {
					return std::unexpected(core::agent_steer_support::not_running);
				}
				// Driver and connection first; the version is the port's to say.
				if (const auto shape = core::agent_steer_support_of(*active->SteerProtocol, active->SteerTarget.has_value(), core::qualified_claude_steer_version); shape != core::agent_steer_support::available) {
					return std::unexpected(shape);
				}
				target = active->SteerTarget;
			}
			if (const auto support = Processes.steer_support(*target); support != core::agent_steer_support::available) {
				return std::unexpected(support);
			}
			return std::move(*target);
		}

		void steer(const agent_invocation_projection_identity& aRun, std::string aText, agent_invocation_host::steer_answer aAnswer)
		{
			const auto weak    = weak_from_this();
			auto       deliver = [weak, answer = std::move(aAnswer)](const agent_steer_receipt& aReceipt) {
				auto self = weak.lock();
				if (!self || !answer) {
					return;
				}
				++self->Probes;
				try {
					self->DecisionLane.post([weak, answer, aReceipt] {
						if (auto lane = weak.lock()) {
							--lane->Probes;
							if (!lane->Closing) {
								answer(aReceipt);
							}
						}
					});
				} catch (...) {
					--self->Probes;
				}
			};
			if (aText.find_first_not_of(" \t\r\n") == std::string::npos) {
				deliver({ .Support = core::agent_steer_support::empty_text });
				return;
			}
			if (aText.size() > core::maximum_chat_message_bytes) {
				deliver({ .Support = core::agent_steer_support::text_too_long });
				return;
			}
			auto target = steer_target(aRun);
			if (!target) {
				deliver({ .Support = target.error() });
				return;
			}
			auto request = std::move(*target);
			request.Text = std::move(aText);
			Processes.steer(std::move(request), std::move(deliver));
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
		std::filesystem::path                ScratchRoot;
		agent_session_catalog_port&          Sessions;
		agent_workspace_port&                Workspaces;
		agent_process_port&                  Processes;
		environment_reader                   Environment;
		effect_executor&                     Effects;
		decision_executor&                   DecisionLane;
		agent_invocation_projection_port&    Projection;
		private_agent_run_port*              PrivateRuns;
		void                                 restore_native(const core::agent_profile_id aProfile, std::string aAgentIdentity, std::string aScope, std::optional<std::string> aChoice, std::optional<agent_conversation_key> aLegacy)
		{
			const auto self = shared_from_this();
			if (self->Closing || !self->NativeSessions || aAgentIdentity.empty() || !self->Authority || !self->Binding || !self->ApplicationSession) {
				return;
			}
			{
				std::unique_lock lock{ self->StateMutex };
				if (self->ActiveInvocation) {
					if ((aChoice && !self->ActiveInvocation->TerminalComplete) || (aChoice && self->DeferredNativeChoice)) {
						lock.unlock();
						self->project_refuse("Stop the current turn and wait for it to finish before starting or opening a session.");
						return;
					}
					const auto explicitChoice = aChoice.has_value();
					auto       deferred       = [weak = weak_from_this(), aProfile, agent = std::move(aAgentIdentity), scope = std::move(aScope), choice = std::move(aChoice), legacy = aLegacy, application = self->ApplicationSession, authority = self->Authority, binding = self->Binding]() mutable {
						if (auto owner = weak.lock(); owner && owner->ApplicationSession == application && owner->Authority == authority && owner->Binding == binding) {
							owner->restore_native(aProfile, std::move(agent), std::move(scope), std::move(choice), std::move(legacy));
						}
					};
					if (explicitChoice) {
						self->DeferredNativeChoice = std::move(deferred);
					} else {
						self->DeferredNativeHistory = std::move(deferred);
					}
					self->HasDeferredNativeHistory = true;
					return;
				}
			}

			const auto profile = std::ranges::find(self->Profiles, aProfile, &core::agent_profile::Id);
			if (profile == self->Profiles.end()) {
				return;
			}
			const auto scope      = native_scope_key(*self->Authority, aAgentIdentity, aScope);
			const auto activation = std::to_string(self->ApplicationSession->value()) + ":" + scope;
			if (!aChoice && self->NativeRestoreKey == activation) {
				return;
			}
			self->NativeRestoreKey     = activation;
			const auto generation      = ++self->NativeRestoreGeneration;
			const auto observed        = *self->Binding;
			const auto nativeSupported = profile->Protocol != core::agent_protocol::external_terminal;
			const auto configuration   = std::to_string(static_cast<unsigned>(profile->Protocol)) + ":" + aAgentIdentity + (aScope.starts_with("tour:") ? ":read-only" : "");
			++self->Probes;
			try {
				if (!aChoice || !aChoice->starts_with("view:")) {
					self->Projection.present_native_history({ .Scope = aScope, .Loading = true });
				}
			} catch (...) { }
			try {
				self->Effects.post([self, scope, configuration, observed, aProfile, generation, aScope, choice = std::move(aChoice), legacy = aLegacy, nativeSupported] {
					auto loaded  = std::expected<std::optional<native_agent_session>, native_agent_storage_failure>{ std::unexpected(native_agent_storage_failure::unavailable) };
					auto history = std::expected<native_agent_history_projection, native_agent_storage_failure>{ std::unexpected(native_agent_storage_failure::unavailable) };
					try {
						if (choice && !choice->starts_with("view:")) {
							auto selected = choice->empty() && !nativeSupported ? self->NativeSessions->replay(scope, configuration, observed, "", legacy) : choice->starts_with("replay:") ? self->NativeSessions->replay(scope, configuration, observed, choice->substr(7U), legacy)
																																															: self->NativeSessions->choose(scope, configuration, observed, *choice);
							if (selected) {
								loaded = std::optional{ std::move(*selected) };
							} else {
								loaded = std::unexpected(selected.error());
							}
						} else {
							loaded = self->NativeSessions->find(scope, configuration, observed);
						}
						history = self->NativeSessions->history(scope, configuration, choice && choice->starts_with("view:") ? choice->substr(5U) : std::string{ }, legacy);
					} catch (...) { }
					try {
						self->DecisionLane.post([self, outcome = std::move(loaded), savedHistory = std::move(history), aScope, aProfile, generation, nativeSupported] {
							--self->Probes;
							if (self->Closing || generation != self->NativeRestoreGeneration || !self->Binding || self->ActiveInvocation) {
								return;
							}
							auto view               = savedHistory ? std::move(*savedHistory) : native_agent_history_projection{ };
							view.Scope              = aScope;
							view.NativeSupported    = nativeSupported;
							const auto replay       = prose_replay(view, aScope.starts_with("tour:"));
							view.ReplayTurnsOmitted = replay.Omitted;
							view.ReplayTextCutBytes = replay.CutBytes;
							if (!outcome || !savedHistory) {
								view.Failure = "Session history could not be saved or opened. The previous session was kept; no question was sent.";
							}
							try {
								self->Projection.present_native_history(view);
							} catch (...) { }
							if (view.ReadOnly) {
								return;
							}
							if (!outcome) {
								self->NativeRestoreKey.clear();
								self->project_refuse("native_session_history_unavailable");
								return;
							}
							if (*outcome && (**outcome).Pending) {
								self->project_refuse("The previous question may have been sent before interruption. It was not resent. Choose New session or explicitly create Transcript replay.");
							}
							if (!*outcome || (**outcome).Replay || !(**outcome).Workspace || (**outcome).Handle.empty()) {
								return;
							}
							const auto& saved   = **outcome;
							auto        session = core::agent_session{ };
							session.Id          = saved.Workspace->Session;
							session.Profile     = aProfile;
							session.Binding     = saved.Binding;
							if (!core::valid_agent_session(session)) {
								self->project_refuse("native_session_history_invalid");
								return;
							}
							try {
								self->Projection.restore_native_session(session);
							} catch (...) { }
							self->project_session_catalog({ .Latest = session, .LatestStatus = core::observe_agent_session(session, *self->Binding) });
						});
					} catch (...) {
						--self->Probes;
					}
				});
			} catch (...) {
				--self->Probes;
			}
		}

		/// [[BPR-2181]]. Told on the decision lane each time the host is
		/// quiescent after a run retired: the run's own retirement event,
		/// so nothing watches for it on a timer. Decision lane only.
		std::function<void()> RetiredObserver;

		[[nodiscard]] auto quiescent() const noexcept -> bool
		{
			return ActiveJobs.load(std::memory_order_acquire) == 0U && Probes.load(std::memory_order_acquire) == 0U
				&& (!NativeDrainRequested || Processes.native_connections_quiescent());
		}

		void announce_retirement() noexcept
		{
			try {
				DecisionLane.post([weak = weak_from_this()] {
					const auto self = weak.lock();
					if (self && !self->Closing.load(std::memory_order_acquire) && self->RetiredObserver && self->quiescent()) {
						self->RetiredObserver();
					}
				});
			} catch (...) { }
		}

		std::function<void()> DeferredNativeHistory;
		std::function<void()> DeferredNativeChoice;
		std::atomic_bool      HasDeferredNativeHistory = false;

		native_agent_session_store_port* NativeSessions;
		diagnostic_log_port*             Log;

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

		std::string        NativeRestoreKey;
		std::uint64_t      NativeRestoreGeneration = 0U;
		std::atomic_bool   NativeDrainRequested    = false;
		std::atomic_bool   Closing                 = false;
		std::atomic_size_t ActiveJobs              = 0U;
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
		private_agent_run_port* aPrivateRuns, native_agent_session_store_port* aNativeSessions, diagnostic_log_port* aLog)
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
			  aProjection, aPrivateRuns, aNativeSessions, aLog) }
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

	void agent_invocation_host::restore_native(const core::agent_profile_id aProfile, std::string aAgentIdentity, std::string aScope, std::optional<std::string> aChoice, std::optional<agent_conversation_key> aLegacy)
	{
		Implementation->restore_native(aProfile, std::move(aAgentIdentity), std::move(aScope), std::move(aChoice), std::move(aLegacy));
	}

	void agent_invocation_host::native_history_changed()
	{
		Implementation->NativeRestoreKey.clear();
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

	auto agent_invocation_host::steer_support(const agent_invocation_projection_identity& aRun) const -> core::agent_steer_support
	{
		const auto target = Implementation->steer_target(aRun);
		return target ? core::agent_steer_support::available : target.error();
	}

	void agent_invocation_host::steer(const agent_invocation_projection_identity& aRun, std::string aText, steer_answer aAnswer)
	{
		const auto implementation = Implementation;
		implementation->steer(aRun, std::move(aText), std::move(aAnswer));
	}

	void agent_invocation_host::shutdown()
	{
		const auto implementation = Implementation;
		implementation->Closing.store(true, std::memory_order_release);
		implementation->cancel();
	}

	void agent_invocation_host::set_scratch_root(std::filesystem::path aRoot)
	{
		Implementation->ScratchRoot = std::move(aRoot);
	}

	void agent_invocation_host::retry_cleanup()
	{
		const auto implementation = Implementation;
		implementation->retry_cleanup();
	}

	auto agent_invocation_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->quiescent();
	}

	void agent_invocation_host::set_retired_observer(std::function<void()> aObserver)
	{
		Implementation->RetiredObserver = std::move(aObserver);
	}

	void agent_invocation_host::probe_workspace(const core::application_session_id aApplicationSession, core::pull_request_authority aAuthority,
												core::agent_revision_binding aBinding, std::optional<std::filesystem::path> aHistoryClone, workspace_probe_answer aAnswer, std::stop_token aStop)
	{
		const auto host = Implementation;
		if (host->Closing.load(std::memory_order_acquire) || aStop.stop_requested()) {
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
			host->Effects.post([weak = std::weak_ptr{ host }, probe = std::move(request), answer = std::move(aAnswer), stop = std::move(aStop)]() mutable {
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
					// [[BPR-2261]]. The pull request's retained workspace, when
					// there is one, is checked as a turn reattaching checks
					// it, and nothing is cloned again; one a turn holds now is
					// in use, and that turn checked it. Only with none is one
					// made, and removed again.
					auto retained  = std::optional<owned_agent_workspace>{ };
					auto busy      = false;
					auto lease     = std::unique_ptr<agent_workspace_turn_lease>{ };
					auto pathLease = std::unique_ptr<agent_workspace_turn_lease>{ };
					if (self->NativeSessions != nullptr) {
						const auto key = agent_workspace_key(probe.Authority, probe.Session.Binding);
						lease          = self->NativeSessions->try_lock_workspace(key);
						auto bound     = native_agent_session{ };
						bound.Binding  = probe.Session.Binding;
						if (auto shared = self->NativeSessions->shared_workspace(key, bound); shared && *shared) {
							retained = std::move(**shared);
							// [[BPR-2293]]. Expiry holds it by its path while it removes it.
							pathLease = self->NativeSessions->try_lock_workspace(agent_workspace_path_lease(retained->Workspace.Path));
							busy      = lease == nullptr || pathLease == nullptr;
						}
					}
					if (retained && busy) {
						result = retained->Workspace;
					} else if (retained) {
						retained->Authority          = probe.Authority;
						retained->ApplicationSession = probe.ApplicationSession;
						auto checked                 = self->Workspaces.inspect_agent_workspace(*retained, stop);
						if (checked) {
							result = checked->Workspace;
						} else {
							result = std::unexpected(std::move(checked.error()));
						}
					} else {
						lease.reset();
						auto made = self->Workspaces.materialize_agent_workspace(probe, stop);
						if (made) {
							result = made->Workspace;
							static_cast<void>(self->Workspaces.cleanup_agent_workspace(*made, { }));
						} else {
							result = std::unexpected(std::move(made.error()));
						}
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

	auto measure_workspace_lines(const std::filesystem::path& aRoot, const std::vector<std::string>& aPaths) -> std::vector<std::pair<std::string, std::uint32_t>>
	{
		auto measured = std::vector<std::pair<std::string, std::uint32_t>>{ };
		for (const auto& given : aPaths | std::views::take(maximum_measured_paths)) {
			const auto relative = std::filesystem::path{ given }.lexically_normal();
			if (given.empty() || relative.is_absolute() || relative.has_root_name() || std::ranges::any_of(relative, [](const auto& aPart) { return aPart == ".."; })) {
				continue;
			}
			// Every component is looked at without following it.
			auto       at        = aRoot;
			auto       error     = std::error_code{ };
			bool       reachable = true;
			const auto parts     = std::vector<std::filesystem::path>{ relative.begin(), relative.end() };
			for (std::size_t index = 0U; index < parts.size() && reachable; ++index) {
				at               = at / parts[index];
				const auto state = std::filesystem::symlink_status(at, error);
				reachable        = !error && (index + 1U == parts.size() ? std::filesystem::is_regular_file(state) : std::filesystem::is_directory(state));
			}
			if (!reachable) {
				continue;
			}
			const auto size = std::filesystem::file_size(at, error);
			if (error || size > maximum_measured_bytes) {
				continue;
			}
			auto file = std::ifstream{ at, std::ios::binary };
			if (!file) {
				continue;
			}
			auto lines  = std::uint32_t{ 0U };
			auto last   = '\n';
			auto buffer = std::array<char, 65536>{ };
			while (file) {
				file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
				const auto read = static_cast<std::size_t>(file.gcount());
				for (std::size_t index = 0U; index < read; ++index) {
					lines += buffer[index] == '\n' ? 1U : 0U;
				}
				if (read > 0U) {
					last = buffer[read - 1U];
				}
			}
			measured.emplace_back(given, lines + (last != '\n' ? 1U : 0U));
		}
		return measured;
	}
} // namespace bpr::application
