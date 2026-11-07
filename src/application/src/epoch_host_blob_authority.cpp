#include "epoch_host_implementation.hpp"

#include "bpr/core/diff_transition.hpp"

#if defined(BPR_APPLICATION_TESTING)
	#include "bpr/application/testing.hpp"
	#include <new>
#endif

namespace bpr::application {
	namespace {
#if defined(BPR_APPLICATION_TESTING)
		// Consumes a matching arming exactly once so a single armed failure
		// cannot leak into an unrelated later admission.
		void fail_admission_at(
			const detail::testing::epoch_admission_allocation_stage aStage)
		{
			auto expected = static_cast<int>(aStage);
			if (ArmedAdmissionStage.compare_exchange_strong(
					expected, -1, std::memory_order_acq_rel)) {
				throw std::bad_alloc{ };
			}
		}
#endif
	}

	auto epoch_application_host::implementation::still_needed(const source_corpus_read_needed& aNeeded) noexcept -> bool
	{
		try {
			return aNeeded && aNeeded();
		} catch (...) {
			return false;
		}
	}

	/// Drops every prepared grant. Called under BlobAuthorityMutex.
	void epoch_application_host::implementation::forget_prepared_grants(authority_effect_actor& aActor) noexcept
	{
		aActor.PreparedGrants.clear();
		++aActor.PreparedGrantSequence;
		aActor.PreparedReadsSuspended = false;
	}

	/// Drops every retained blob. Called under BlobAuthorityMutex.
	void epoch_application_host::implementation::forget_retained_blobs(authority_effect_actor& aActor) noexcept
	{
		aActor.RetainedBlobs.clear();
		aActor.RetainedBlobBytes = 0U;
	}

	/// Retains verified bytes within the actor's bound, oldest first out.
	/// Called under BlobAuthorityMutex.
	void epoch_application_host::implementation::retain_blob(authority_effect_actor& aActor, const core::git_blob_oid& aBlob,
															 const std::vector<std::uint8_t>& aBytes, const bool aBackground)
	{
		if (aBytes.size() > maximum_retained_sparse_blob_bytes) {
			return;
		}
		const auto retained = std::ranges::find(aActor.RetainedBlobs, aBlob, &retained_sparse_blob::Blob);
		if (retained != aActor.RetainedBlobs.end()) {
			if (!aBackground) {
				retained->Background = false;
			}
			return;
		}
		while (!aActor.RetainedBlobs.empty()
			   && (aActor.RetainedBlobs.size() >= maximum_retained_sparse_blobs
				   || aActor.RetainedBlobBytes > maximum_retained_sparse_blob_bytes - aBytes.size())) {
			const auto victim = std::ranges::find(aActor.RetainedBlobs, true, &retained_sparse_blob::Background);
			if (victim != aActor.RetainedBlobs.end()) {
				aActor.RetainedBlobBytes -= victim->Bytes.size();
				aActor.RetainedBlobs.erase(victim);
			} else if (aBackground) {
				return;
			} else {
				aActor.RetainedBlobBytes -= aActor.RetainedBlobs.front().Bytes.size();
				aActor.RetainedBlobs.pop_front();
			}
		}
		aActor.RetainedBlobs.push_back(retained_sparse_blob{ .Blob = aBlob, .Bytes = aBytes, .Background = aBackground });
		aActor.RetainedBlobBytes += aBytes.size();
	}

	auto epoch_application_host::implementation::retained_blob(const authority_effect_actor& aActor, const core::git_blob_oid& aBlob)
		-> std::optional<std::vector<std::uint8_t>>
	{
		const auto found = std::ranges::find(aActor.RetainedBlobs, aBlob, &retained_sparse_blob::Blob);
		return found == aActor.RetainedBlobs.end() ? std::nullopt : std::optional{ found->Bytes };
	}

	epoch_application_host::implementation::implementation(
		core::epoch_state       aInitialState,
		git_cache_port&         aCaches,
		epoch_catalog_port&     aCatalogs,
		epoch_entropy_port&     aEntropy,
		epoch_clock_port&       aClock,
		effect_executor&        aEffects,
		decision_executor&      aDecisionLane,
		epoch_projection_port&  aProjections,
		const epoch_host_limits aLimits, std::shared_ptr<diagnostic_log_port> aDiagnosticLog,
		std::shared_ptr<profile_epoch_actor_consumer> aConsumer, const bool aLegacy)
		: State{ std::move(aInitialState) }
		, Caches{ aCaches }
		, Catalogs{ aCatalogs }
		, Entropy{ aEntropy }
		, Clock{ aClock }
		, Effects{ aEffects }
		, DecisionLane{ aDecisionLane }
		, Projections{ aProjections }
		, Limits{ validated_limits(aLimits) }
		, DiagnosticLog{ std::move(aDiagnosticLog) }
		, CompletionGate{ std::make_shared<completion_gate>() }
		, Consumer{ std::move(aConsumer) }
		, WindowApplicationSession{ State.ApplicationSession }
		, Legacy{ aLegacy }
	{
		Jobs.reserve(Limits.OutstandingJobs);
		CompletionGate->Host.store(this, std::memory_order_release);
	}

	epoch_application_host::implementation::~implementation()
	{
		const std::lock_guard barrier{ CompletionGate->Barrier };
		CompletionGate->Host.store(nullptr, std::memory_order_release);
	}

	auto epoch_application_host::implementation::blob_authority_for(
		const authority_context& aContext) noexcept -> blob_authority
	{
		return {
			.ApplicationSession = aContext.Handoff.ApplicationSession,
			.Authority          = aContext.Authority,
		};
	}

	auto epoch_application_host::implementation::blob_authority_for(
		const workbench_selected_diff_request& aRequest) noexcept
		-> blob_authority
	{
		return {
			.ApplicationSession = aRequest.Diff.ApplicationSession,
			.Authority          = aRequest.Diff.Authority,
		};
	}

	auto epoch_application_host::implementation::blob_authority_for(
		const diff_blob_request& aRequest) noexcept -> blob_authority
	{
		return {
			.ApplicationSession = aRequest.Context.ApplicationSession,
			.Authority          = aRequest.Context.Key.Authority,
		};
	}

	auto epoch_application_host::implementation::blob_authority_for(
		const agent_workspace_materialization_request& aRequest) noexcept
		-> blob_authority
	{
		return {
			.ApplicationSession = aRequest.ApplicationSession,
			.Authority          = aRequest.Authority,
		};
	}

	auto epoch_application_host::implementation::exact_selected_blob_request(
		const workbench_selected_diff_observation& aGrant,
		const diff_blob_request&                   aRequest) noexcept -> bool
	{
		if (aGrant.Status != workbench_selected_diff_status::available) {
			return false;
		}
		const auto& selected = aGrant.Request;
		const auto& key      = aRequest.Context.Key;
		return aRequest.Context.ApplicationSession == selected.Diff.ApplicationSession
			&& key.Authority == selected.Diff.Authority
			&& key.Repository == selected.Diff.Authority.Repository
			&& key.PullRequest == selected.Diff.Authority.PullRequest
			&& key.LeftAnchor == selected.Diff.Comparison.Left.Identity
			&& key.RightAnchor == selected.Diff.Comparison.Right.Identity
			&& key.LeftBlob == aGrant.LeftBlob
			&& key.RightBlob == aGrant.RightBlob
			&& key.File == selected.File.File
			&& key.Options == selected.Diff.OptionsDigest
			&& key.Algorithm == selected.Diff.Algorithm
			&& key.Filter == selected.Filter
			&& (aRequest.Side == core::diff_side::left
					? aGrant.LeftBlob == std::optional{ aRequest.Blob }
					: aGrant.RightBlob == std::optional{ aRequest.Blob });
	}

	void epoch_application_host::implementation::authorize_blobs(
		const std::shared_ptr<authority_effect_actor>& aActor,
		const blob_authority&                          aAuthority,
		const std::shared_ptr<authority_context>&      aContext) noexcept
	{
		std::scoped_lock lock{ aActor->BlobAuthorityMutex };
		++aActor->BlobAuthorityGeneration;
		aActor->SearchScope.reset();
		aActor->SearchCorpus.reset();
		if (aActor->SearchRead) {
			aActor->SearchRead->Stop.request_stop();
		}
		aActor->BlobAuthority = aAuthority;
		aActor->BlobContext   = aContext;
		aActor->SelectedDiffAuthority.reset();
		aActor->RealignmentAuthority.reset();
		aActor->FilterAuthority.reset();
		aActor->FilterCommits.clear();
		aActor->ChainTrees.reset();
		aActor->SelectedBlobGrant.reset();
		++aActor->SelectedBlobGrantSequence;
		forget_prepared_grants(*aActor);
		forget_retained_blobs(*aActor);
	}

	void epoch_application_host::implementation::authorize_selected_diff(
		const std::shared_ptr<authority_effect_actor>& aActor,
		const selected_diff_authority&                 aAuthority) noexcept
	{
		std::scoped_lock lock{ aActor->BlobAuthorityMutex };
		if (aActor->SelectedDiffAuthority == std::optional{ aAuthority }) {
			return;
		}
		++aActor->BlobAuthorityGeneration;
		aActor->SelectedDiffAuthority = aAuthority;
		aActor->SelectedBlobGrant.reset();
		++aActor->SelectedBlobGrantSequence;
		forget_prepared_grants(*aActor);
	}

	/// \brief Grants the one re-alignment authorization, without a bump.
	///
	/// \details Deliberately does not touch `BlobAuthorityGeneration`.
	/// The generation is what tells an in-flight resolve that the review
	/// moved under it, and a re-alignment is not the review moving; a bump
	/// here would refuse the reviewer's own selected diff for a turn they
	/// asked about a different revision. Replacement of the review's
	/// context still bumps, and still drops this.
	void epoch_application_host::implementation::authorize_realignment(
		const std::shared_ptr<authority_effect_actor>& aActor,
		const selected_diff_authority&                 aAuthority) noexcept
	{
		std::scoped_lock lock{ aActor->BlobAuthorityMutex };
		aActor->RealignmentAuthority = aAuthority;
	}

	void epoch_application_host::implementation::revoke_blob_authority(
		const std::shared_ptr<authority_effect_actor>& aActor,
		const blob_authority&                          aAuthority) noexcept
	{
		std::scoped_lock lock{ aActor->BlobAuthorityMutex };
		if (aActor->BlobAuthority == std::optional{ aAuthority }) {
			aActor->SearchScope.reset();
			aActor->SearchCorpus.reset();
			if (aActor->SearchRead) {
				aActor->SearchRead->Stop.request_stop();
			}
			aActor->BlobAuthority.reset();
			aActor->BlobContext.reset();
			aActor->SelectedBlobGrant.reset();
			++aActor->SelectedBlobGrantSequence;
			forget_prepared_grants(*aActor);
			forget_retained_blobs(*aActor);
			aActor->SelectedDiffAuthority.reset();
			aActor->RealignmentAuthority.reset();
			aActor->FilterAuthority.reset();
			aActor->FilterCommits.clear();
			aActor->ChainTrees.reset();
			++aActor->BlobAuthorityGeneration;
		}
	}

	auto epoch_application_host::implementation::selected_diff_authorization_generation(
		const std::shared_ptr<authority_effect_actor>& aActor,
		const workbench_selected_diff_request&         aRequest) noexcept
		-> std::optional<std::uint64_t>
	{
		const auto expected = selected_diff_authority{
			.Workbench = aRequest.WorkbenchAuthority,
			.Diff      = aRequest.Diff,
		};
		std::scoped_lock lock{ aActor->BlobAuthorityMutex };
		// Either slot answers, and both answer with the same generation:
		// a re-alignment is admitted under the review's own generation, so
		// the review's context being replaced under it refuses it exactly
		// as it refuses the workbench's.
		if (aActor->SelectedDiffAuthority != std::optional{ expected }
			&& aActor->RealignmentAuthority != std::optional{ expected }) {
			return std::nullopt;
		}
		// RFC-BPR-0089. A filtered request reads at two commits this
		// review's touches job diffed, under the same workbench authority.
		if (aRequest.Filter.has_value()
			&& (aActor->FilterAuthority != std::optional{ aRequest.WorkbenchAuthority } || !core::valid(*aRequest.Filter)
				|| !std::ranges::binary_search(aActor->FilterCommits, aRequest.Filter->Left)
				|| !std::ranges::binary_search(aActor->FilterCommits, aRequest.Filter->Right))) {
			return std::nullopt;
		}
		return aActor->BlobAuthorityGeneration;
	}

	auto epoch_application_host::implementation::blob_authorization_generation(
		const std::shared_ptr<authority_effect_actor>& aActor,
		const blob_authority&                          aAuthority) noexcept
		-> std::optional<std::uint64_t>
	{
		std::scoped_lock lock{ aActor->BlobAuthorityMutex };
		if (aActor->BlobAuthority != std::optional{ aAuthority }) {
			return std::nullopt;
		}
		return aActor->BlobAuthorityGeneration;
	}

	void epoch_application_host::implementation::revoke_deferred(deferred_ingress& aIngress) noexcept
	{
		if (auto* const accepted = std::get_if<deferred_accept>(&aIngress);
			accepted != nullptr && accepted->Handoff
			&& accepted->Handoff->SmartGit) {
			accepted->Handoff->SmartGit->revoke();
		}
	}

	void epoch_application_host::implementation::clear_deferred_ingress() noexcept
	{
		for (auto& ingress : DeferredIngress) {
			revoke_deferred(ingress);
		}
		DeferredIngress.clear();
	}

	void epoch_application_host::implementation::defer_diagnostic(const core::epoch_reason_code aReason) noexcept
	{
		if (!DeferredDiagnostic) {
			DeferredDiagnostic = aReason;
		}
	}

	auto epoch_application_host::implementation::defer_ordinary(deferred_ingress aIngress) noexcept
		-> bool
	{
		if (DeferredShutdown || Closing) {
			revoke_deferred(aIngress);
			return false;
		}
		if (DeferredIngress.size() >= Limits.DeferredIngress) {
			revoke_deferred(aIngress);
			defer_diagnostic(core::epoch_reason_code::quota_exceeded);
			schedule_deferred_ingress();
			return false;
		}
		try {
			DeferredIngress.push_back(std::move(aIngress));
		} catch (...) {
			revoke_deferred(aIngress);
			defer_diagnostic(core::epoch_reason_code::unavailable);
			schedule_deferred_ingress();
			return false;
		}
		schedule_deferred_ingress();
		return true;
	}

	auto epoch_application_host::implementation::defer_invalidation(
		const pull_request_authority_invalidation& aInvalidation) noexcept -> bool
	{
		if (DeferredShutdown || Closing) {
			return true;
		}
		for (auto ingress = DeferredIngress.begin();
			 ingress != DeferredIngress.end();) {
			auto* const accepted = std::get_if<deferred_accept>(&*ingress);
			if (accepted == nullptr || !accepted->Handoff
				|| invalidation_identity(*accepted->Handoff) != aInvalidation) {
				++ingress;
				continue;
			}
			revoke_deferred(*ingress);
			ingress = DeferredIngress.erase(ingress);
		}

		const auto activeMatches = Active
			&& invalidation_identity(Active->Handoff) == aInvalidation;
		const auto pendingMatches = PendingAuthority == aInvalidation;
		if (!activeMatches && !pendingMatches) {
			return true;
		}
		if (std::ranges::any_of(
				DeferredInvalidations,
				[&aInvalidation](const auto& aPending) {
					return aPending == aInvalidation;
				})) {
			return true;
		}
		const auto empty = std::ranges::find(
			DeferredInvalidations,
			std::optional<pull_request_authority_invalidation>{ });
		if (empty == DeferredInvalidations.end()) {
			ProjectionDeliveryFailed = true;
			return false;
		}
		*empty = aInvalidation;
		schedule_deferred_ingress();
		return true;
	}

	auto epoch_application_host::implementation::deferred_invalidations_empty() const noexcept -> bool
	{
		return std::ranges::none_of(
			DeferredInvalidations,
			[](const auto& aPending) { return aPending.has_value(); });
	}

	auto epoch_application_host::implementation::deferred_work_pending() const noexcept -> bool
	{
		return DeferredIngressScheduled || !DeferredIngress.empty()
			|| !deferred_invalidations_empty() || DeferredDiagnostic
			|| DeferredShutdown;
	}

	void epoch_application_host::implementation::clear_deferred_invalidations() noexcept
	{
		for (auto& invalidation : DeferredInvalidations) {
			invalidation.reset();
		}
	}

	void epoch_application_host::implementation::defer_shutdown() noexcept
	{
		DeferredShutdown = true;
		schedule_deferred_ingress();
	}

	void epoch_application_host::implementation::schedule_deferred_ingress() noexcept
	{
		if (
			(DeferredIngress.empty() && !DeferredShutdown
			 && !DeferredDiagnostic && deferred_invalidations_empty())
			|| DeferredIngressScheduled) {
			return;
		}
		DeferredIngressScheduled = true;
		const auto gate          = std::weak_ptr<completion_gate>{ CompletionGate };
		try {
			DecisionLane.post(
				[gate] {
					const auto locked = gate.lock();
					if (!locked) {
						return;
					}
					const std::lock_guard barrier{ locked->Barrier };
					auto* const           host = locked->Host.load(std::memory_order_acquire);
					if (host == nullptr) {
						return;
					}
					host->DeferredIngressScheduled = false;
					host->run_turn([] { });
				});
		} catch (...) {
			DeferredIngressScheduled = false;
			ProjectionDeliveryFailed = true;
		}
	}

	void epoch_application_host::implementation::enqueue_completion(
		const std::weak_ptr<completion_gate>& aGate,
		decision_executor* const              aDecisionLane,
		std::shared_ptr<job_control>          aControl,
		worker_completion                     aCompletion) noexcept
	{
		aControl->Completion = std::move(aCompletion);
		auto task            = std::move(aControl->DecisionTask);
		if (!task) {
			if (const auto locked = aGate.lock()) {
				locked->DeliveryFailed.store(true, std::memory_order_release);
			}
			return;
		}
		try {
			aDecisionLane->post(std::move(task));
		} catch (...) {
			const auto locked = aGate.lock();
			if (!locked) {
				return;
			}
			// A conforming decision lane never rejects. Preserve the live
			// job (and therefore non-quiescence) instead of retiring it or
			// mutating core-owned state from this worker thread.
			locked->DeliveryFailed.store(true, std::memory_order_release);
		}
	}

	void epoch_application_host::implementation::enqueue_actor_work(
		const std::shared_ptr<authority_effect_actor>& aActor,
		std::function<void()>                          aWork,
		const profile_epoch_actor_priority             aPriority)
	{
		const auto workState = ActorWork;
		++workState->Pending;
		++workState->Queued;
		try {
			aActor->Shared->enqueue([aActor, workState, work = std::move(aWork)] {
				--workState->Queued;
				struct work_guard final
				{
					std::atomic_size_t& Pending;
					~work_guard() { --Pending; }
				} guard{ workState->Pending };
				work();
			},
									aPriority);
		} catch (...) {
			--workState->Queued;
			--workState->Pending;
			throw;
		}
	}

	auto epoch_application_host::implementation::submit_erased_effect(
		worker_task                                 aTask,
		worker_completion                           aExceptionCompletion,
		admission_failure                           aAdmissionFailure,
		const std::optional<core::epoch_request_id> aRequest,
		const std::optional<core::epoch_generation> aEpochGeneration,
		const std::optional<core::comparison_generation>
												aComparisonGeneration,
		const std::optional<std::uint64_t>      aOfferToken,
		std::shared_ptr<authority_effect_actor> aActor,
		std::shared_ptr<job_control>            aAdmittedControl) -> bool
	{
		auto control = std::move(aAdmittedControl);
		if (!control) {
			auto admitted = admit_job(
				aRequest, aEpochGeneration, aComparisonGeneration, aOfferToken);
			if (!admitted) {
				reject_admission(aAdmissionFailure, admitted.error());
				return false;
			}
			control = std::move(*admitted);
		}
		const auto  gate         = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const decisionLane = &DecisionLane;
		try {
			auto work = [gate,
						 decisionLane,
						 control,
						 consumer = Consumer,
						 aExceptionCompletion,
						 task = std::move(aTask)]() mutable {
				auto completion = worker_completion{ };
				if (!consumer->active() && !control->MandatoryCleanup) {
					static_cast<void>(control->Stop.request_stop());
				}
				try {
					completion = task(control->Stop.get_token());
				} catch (...) {
					completion = std::move(aExceptionCompletion);
					if (!completion) {
						completion = [](implementation& aHost) {
							aHost.publish_diagnostic(
								core::epoch_reason_code::unavailable);
						};
					}
				}
				enqueue_completion(
					gate,
					decisionLane,
					std::move(control),
					std::move(completion));
			};
			if (aActor) {
				enqueue_actor_work(std::move(aActor), std::move(work));
			} else {
				Effects.post(std::move(work));
			}
			return true;
		} catch (...) {
			finish_job(control);
			reject_admission(
				aAdmissionFailure, core::epoch_failure::unavailable);
			return false;
		}
	}

	auto epoch_application_host::implementation::admit_job(
		const std::optional<core::epoch_request_id>      aRequest,
		const std::optional<core::epoch_generation>      aEpochGeneration,
		const std::optional<core::comparison_generation> aComparisonGeneration,
		const std::optional<std::uint64_t>               aOfferToken,
		const bool                                       aUseTerminalReservation,
		const bool                                       aMandatoryCleanup,
		const bool                                       aPreparation) noexcept
		-> std::expected<std::shared_ptr<job_control>, core::epoch_failure>
	{
		try {
			std::scoped_lock lock{ JobsMutex };
			if (!aUseTerminalReservation && !aMandatoryCleanup
				&& BlobIngressClosing.load(std::memory_order_acquire)) {
				return std::unexpected{ core::epoch_failure::unavailable };
			}
			const auto usesReservation = aUseTerminalReservation && ReservedTerminalAdmissions != 0U;
			if (!usesReservation
				&& Jobs.size() + ReservedTerminalAdmissions + RetiringActorJobs
					>= Limits.OutstandingJobs) {
				return std::unexpected{ core::epoch_failure::quota_exceeded };
			}
#if defined(BPR_APPLICATION_TESTING)
			fail_admission_at(detail::testing::
								  epoch_admission_allocation_stage::job_control);
#endif
			auto control                  = std::make_shared<job_control>();
			control->Request              = aRequest;
			control->EpochGeneration      = aEpochGeneration;
			control->ComparisonGeneration = aComparisonGeneration;
			control->OfferToken           = aOfferToken;
			control->MandatoryCleanup     = aMandatoryCleanup;
#if defined(BPR_APPLICATION_TESTING)
			fail_admission_at(detail::testing::
								  epoch_admission_allocation_stage::decision_task);
#endif
			control->DecisionTask = [gate        = std::weak_ptr<completion_gate>{ CompletionGate },
									 weakControl = std::weak_ptr<job_control>{ control }]() mutable {
				const auto locked        = gate.lock();
				auto       lockedControl = weakControl.lock();
				if (!locked || !lockedControl) {
					return;
				}
				const std::lock_guard barrier{ locked->Barrier };
				auto* const           host = locked->Host.load(std::memory_order_acquire);
				if (host == nullptr) {
					return;
				}
				auto completion = std::move(lockedControl->Completion);
				if (!host->Consumer->current() && !host->Closing) {
					host->shutdown();
				}
				host->run_turn(
					[host,
					 terminalControl    = std::move(lockedControl),
					 terminalCompletion = std::move(completion)]() mutable {
						const auto reserved = host->retire_job_for_terminal(terminalControl);
						struct reservation_guard final
						{
							implementation& Host;
							bool            Reserved;

							~reservation_guard()
							{
								Host.release_terminal_reservation(Reserved);
							}
						} reservation{ *host, reserved };
						terminalCompletion(*host);
					});
			};
#if defined(BPR_APPLICATION_TESTING)
			fail_admission_at(
				detail::testing::epoch_admission_allocation_stage::job_ledger);
#endif
			control->Preparation = aPreparation;
			Jobs.push_back(control);
			if (usesReservation) {
				--ReservedTerminalAdmissions;
			}
			return control;
		} catch (...) {
			return std::unexpected{ core::epoch_failure::unavailable };
		}
	}

	void epoch_application_host::implementation::reject_admission(
		const admission_failure&  aFailure,
		const core::epoch_failure aReason) noexcept
	{
		try {
			if (aFailure) {
				aFailure(*this, aReason);
			} else {
				publish_diagnostic(core::reason_for(aReason));
			}
		} catch (...) {
			ProjectionDeliveryFailed = true;
		}
	}

	void epoch_application_host::implementation::finish_job(const std::shared_ptr<job_control>& aControl) noexcept
	{
		std::scoped_lock lock{ JobsMutex };
		const auto       match = std::ranges::find(Jobs, aControl);
		if (match != Jobs.end()) {
			Jobs.erase(match);
		}
	}

	void epoch_application_host::implementation::release_pending_cleanup(
		const std::shared_ptr<pending_context_cleanup>& aCleanup) noexcept
	{
		if (!aCleanup || !aCleanup->Control) {
			return;
		}
		finish_job(aCleanup->Control);
		aCleanup->Control.reset();
		aCleanup->Context.reset();
	}

	auto epoch_application_host::implementation::retire_job_for_terminal(
		const std::shared_ptr<job_control>& aControl) noexcept -> bool
	{
		std::scoped_lock lock{ JobsMutex };
		const auto       match = std::ranges::find(Jobs, aControl);
		if (match == Jobs.end()) {
			return false;
		}
		Jobs.erase(match);
		++ReservedTerminalAdmissions;
		return true;
	}

	void epoch_application_host::implementation::release_terminal_reservation(const bool aReserved) noexcept
	{
		if (!aReserved) {
			return;
		}
		std::scoped_lock lock{ JobsMutex };
		if (ReservedTerminalAdmissions != 0U) {
			--ReservedTerminalAdmissions;
		}
	}

	auto epoch_application_host::implementation::retire_actor_job(
		const std::shared_ptr<job_control>& aControl) noexcept -> bool
	{
		std::scoped_lock lock{ JobsMutex };
		const auto       match = std::ranges::find(Jobs, aControl);
		if (match == Jobs.end()) {
			return false;
		}
		Jobs.erase(match);
		++RetiringActorJobs;
		if (aControl->Preparation) {
			++RetiringPreparationJobs;
		}
		return true;
	}

	void epoch_application_host::implementation::release_actor_job_reservation(
		const bool aReserved,
		const bool aPreparation) noexcept
	{
		if (!aReserved) {
			return;
		}
		std::scoped_lock lock{ JobsMutex };
		if (RetiringActorJobs != 0U) {
			--RetiringActorJobs;
		}
		if (aPreparation && RetiringPreparationJobs != 0U) {
			--RetiringPreparationJobs;
		}
	}

	void epoch_application_host::implementation::cancel_all_jobs() noexcept
	{
		cancel_jobs([](const job_control& aJob) {
			return !aJob.MandatoryCleanup;
		});
	}

	void epoch_application_host::implementation::close_blob_ingress_and_cancel_jobs() noexcept
	{
		BlobIngressClosing.store(true, std::memory_order_release);
		cancel_all_jobs();
	}

	auto epoch_application_host::implementation::actors_idle() const noexcept -> bool
	{
		return ActorWork->Pending.load(std::memory_order_acquire) == 0U;
	}

	void epoch_application_host::implementation::cancel_authority_jobs() noexcept
	{
		cancel_jobs([](const job_control& aJob) {
			return !aJob.OfferToken && !aJob.MandatoryCleanup;
		});
	}

	void epoch_application_host::implementation::cancel_offer_job(const std::uint64_t aToken) noexcept
	{
		cancel_jobs([aToken](const job_control& aJob) {
			return aJob.OfferToken == aToken;
		});
	}

	void epoch_application_host::implementation::cancel_work(const core::cancel_epoch_work_request& aRequest) noexcept
	{
		cancel_jobs([&](const job_control& aJob) {
			if (aJob.Request != aRequest.Request) {
				return false;
			}
			const bool generationMatches = std::visit(
				[&aJob](const auto aGeneration) {
					using generation_type = std::remove_cvref_t<decltype(aGeneration)>;
					if constexpr (std::is_same_v<
									  generation_type,
									  core::epoch_generation>) {
						return aJob.EpochGeneration == aGeneration;
					} else {
						return aJob.ComparisonGeneration == aGeneration;
					}
				},
				aRequest.Generation);
			return generationMatches;
		});
	}

	// [[BPR-1302]]. No job, blob call, turn, deferred ingress, invalidation,
	// diagnostic, shutdown, offer or actor work is outstanding. Shutdown
	// quiescence and running idleness are this one fact under opposite
	// lifecycles.
	auto epoch_application_host::implementation::outstanding_work_settled() const noexcept -> bool
	{
		return job_count() == 0U && blob_calls_idle() && !TurnActive && DeferredIngress.empty()
			&& deferred_invalidations_empty() && !DeferredIngressScheduled && !DeferredDiagnostic
			&& !DeferredShutdown && !PendingOfferToken && actors_idle();
	}

	auto epoch_application_host::implementation::job_count() const noexcept -> std::size_t
	{
		std::scoped_lock lock{ JobsMutex };
		return Jobs.size() + ReservedTerminalAdmissions + RetiringActorJobs;
	}

	/// [[BPR-1344]]. The part of `job_count` that preparation admitted.
	auto epoch_application_host::implementation::preparation_job_count() const noexcept -> std::size_t
	{
		std::scoped_lock lock{ JobsMutex };
		return preparation_job_count_locked();
	}

	/// [[BPR-1344]]. The rest of `job_count`, read under the same lock so
	/// that it never counts a job twice or misses one moving between them.
	auto epoch_application_host::implementation::reviewer_job_count() const noexcept -> std::size_t
	{
		std::scoped_lock lock{ JobsMutex };
		return Jobs.size() + ReservedTerminalAdmissions + RetiringActorJobs
			- preparation_job_count_locked();
	}

	/// [[BPR-1344]]. The reviewer's jobs still to run or to answer: neither
	/// preparation's nor finished actor work holding only its retirement
	/// reservation, which the effect thread releases after the job's
	/// completion has already been posted.
	auto epoch_application_host::implementation::active_reviewer_job_count() const noexcept -> std::size_t
	{
		std::scoped_lock lock{ JobsMutex };
		return active_reviewer_job_count_locked();
	}

	auto epoch_application_host::implementation::active_reviewer_job_count_locked() const noexcept -> std::size_t
	{
		const auto preparing = static_cast<std::size_t>(std::ranges::count_if(
			Jobs, [](const auto& aJob) { return aJob->Preparation; }));
		return Jobs.size() - preparing + ReservedTerminalAdmissions;
	}

	auto epoch_application_host::implementation::preparation_job_count_locked() const noexcept -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(
				   Jobs, [](const auto& aJob) { return aJob->Preparation; }))
			+ RetiringPreparationJobs;
	}

	auto epoch_application_host::implementation::begin_blob_call() noexcept -> bool
	{
		std::scoped_lock lock{ JobsMutex };
		if (!Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
			return false;
		}
		++InFlightBlobCalls;
		return true;
	}

	auto epoch_application_host::implementation::begin_cleanup_call() noexcept -> bool
	{
		std::scoped_lock lock{ JobsMutex };
		++InFlightBlobCalls;
		return true;
	}

	void epoch_application_host::implementation::finish_blob_call() noexcept
	{
		std::scoped_lock lock{ JobsMutex };
		if (InFlightBlobCalls != 0U) {
			--InFlightBlobCalls;
		}
	}

	auto epoch_application_host::implementation::blob_calls_idle() const noexcept -> bool
	{
		std::scoped_lock lock{ JobsMutex };
		return InFlightBlobCalls == 0U;
	}

	auto epoch_application_host::implementation::queued_actor_work() const noexcept -> std::size_t
	{
		return ActorWork->Queued.load(std::memory_order_acquire);
	}
}
