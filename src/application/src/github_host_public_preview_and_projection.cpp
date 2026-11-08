#include "github_host_implementation.hpp"

#include <algorithm>
#include <limits>
#include <span>
#include <string_view>

namespace bpr::application {
	void github_application_host::implementation::activate_public_preview_now(const core::host_profile_id aProfile)
	{
		if (Closing || aProfile.value() == 0U || (CredentialBindings.find(aProfile) != nullptr && CredentialBindings.find(aProfile)->BprProfile != BprProfile)) {
			return;
		}
		PublicPreviewRefusal.reset();
		PublicPreviewProfile = aProfile;
		if (AnonymousPullRequests == nullptr) {
			// Composition supplied no credential-free read authority, so
			// the mode cannot be entered. Refuse before the core is asked
			// anything: no identity generation is abandoned, no profile is
			// resolved, and nothing observable was attempted.
			PublicPreviewRefusal = github_public_preview_refusal{
				.Cause = github_public_preview_refusal_cause::
					public_preview_unavailable,
				.Policy = std::nullopt,
			};
			publish_public_preview();
			return;
		}
		// A signed-out activation carries no material of any kind. Any
		// enrollment or resume left staged by an earlier turn is dropped
		// here rather than consumed by a session that has no account to
		// bind it to.
		NextEnrollment.reset();
		NextResumeProfile.reset();
		NextPublicPreview = true;
		try {
			dispatch_core(core::activate_host_profile_intent{
				.Profile = aProfile,
				.Mode    = core::github_access_mode::public_preview,
			});
		} catch (...) {
			NextPublicPreview = false;
			throw;
		}
		NextPublicPreview = false;
	}

	auto github_application_host::implementation::signed_out_mode() const noexcept -> bool
	{
		return State.AccessMode
			== core::github_access_mode::public_preview;
	}

	auto github_application_host::implementation::is_public_preview() const noexcept -> bool
	{
		return AcceptedSession.has_value()
			&& AcceptedSession->Mode
			== core::github_access_mode::public_preview;
	}

	auto github_application_host::implementation::anonymous_capability_evidence(
		const github_profile_authority& aAuthority) const noexcept
		-> github_capability_cache_entry
	{
		// Synthesised, never observed and never cached. Every capability
		// is unsupported by policy rather than unknown, and the read shape
		// claims nothing, so no later reader can mistake this for evidence
		// a probe produced. The authority names no account, so this entry
		// can never be a valid capability-cache key.
		github_capability_cache_entry entry{ };
		entry.Authority = github_capability_authority_key{
			.Profile               = aAuthority.Profile,
			.ProfileRevision       = aAuthority.ProfileRevision,
			.Host                  = aAuthority.Host,
			.Account               = { },
			.ServerContract        = aAuthority.ServerContract,
			.ProbeContractRevision = aAuthority.ProbeContractRevision,
		};
		entry.Capabilities = core::anonymous_capabilities();
		entry.ReadShape    = { };
		entry.Revision     = core::capability_revision{ 1U };
		return entry;
	}

	void github_application_host::implementation::begin_public_preview(
		const core::github_session_request_context& aContext,
		const github_profile_authority&             aAuthority)
	{
		if (aAuthority.ServerContract
			!= github_server_contract::github_com_2022_11_28) {
			// Public preview is limited to the GitHub.com contract. Refuse
			// GHES before constructing an anonymous request.
			PublicPreviewRefusal = github_public_preview_refusal{
				.Cause  = github_public_preview_refusal_cause::policy_refused,
				.Policy = core::refuse(
					core::anonymous_refusal_reason::ghes_unavailable),
			};
			fail_session(aContext, core::github_failure::capability);
			return;
		}
		if (AnonymousPullRequests == nullptr) {
			PublicPreviewRefusal = github_public_preview_refusal{
				.Cause = github_public_preview_refusal_cause::
					public_preview_unavailable,
				.Policy = std::nullopt,
			};
			fail_session(aContext, core::github_failure::capability);
			return;
		}
		ViewerMode = viewer_mode::public_preview;
		// [[BPR-0822]]. Name the nested type: GCC 15 destroys a braced
		// aggregate member twice when a later member initializer throws.
		accept_session(session_candidate{
			.Context            = aContext,
			.ProfileAuthority   = aAuthority,
			.Viewer             = github_viewer_record{ },
			.CapabilityEvidence = anonymous_capability_evidence(aAuthority),
			.AccountTransport   = { },
			.RateLimits         = RateLimits,
			.CacheStage         = std::nullopt,
			.Mode               = core::github_access_mode::public_preview,
		});
	}

	auto github_application_host::implementation::current_profile_session() const -> std::optional<github_profile_session>
	{
		if (!AcceptedSession || !AcceptedProfileAuthority || !AcceptedCapabilityEvidence) {
			return std::nullopt;
		}
		return github_profile_session{ BprProfile, State.ApplicationSession, *AcceptedProfileAuthority, *AcceptedSession,
									   State.IdentityGeneration, *AcceptedCapabilityEvidence, AccountTransport };
	}

	void github_application_host::implementation::retire_windows()
	{
		if (!PublishedSession) {
			return;
		}
		if (ReadWindow) {
			static_cast<void>(ReadWindow->retire(*PublishedSession));
		}
		for (auto& slot : AttachedWindows) {
			if (slot.Host->retire(*PublishedSession)) {
				// Only exact retirement authorizes this live attachment to bind
				// the profile owner's next accepted authentication identity.
				slot.Authority.reset();
				slot.Account = { };
			}
		}
		PublishedSession.reset();
	}

	void github_application_host::implementation::invalidate_epoch_authority() noexcept { retire_windows(); }

	void github_application_host::implementation::revoke_review_mutation_transport() noexcept { ReviewMutationTransport.reset(); }

	void github_application_host::implementation::read(const core::github_window_read_event& aEvent)
	{
		if (PrimaryReadClosed) {
			return;
		}
		const auto session = current_profile_session();
		if (!session) {
			std::visit([this](const auto& aTyped) { dispatch_core(core::github_event{ aTyped }); }, aEvent);
			return;
		}
		if (!ReadWindow && (ExplicitPrimaryAttached || AttachedWindows.size() >= 32U)) {
			return;
		}
		if (!ReadWindow) {
			ReadWindow = std::make_shared<github_window_read_host>(
				core::bpr_window_context{ BprProfile, core::bpr_window_id{ 1 }, core::bpr_window_generation{ 1 } },
				State.ApplicationSession, PullRequests, DecisionLane,
				[this](const github_window_read_update& aUpdate) {
					if (Closing) {
						return;
					}
					State                   = ReadWindow->state();
					AcceptedSnapshot        = ReadWindow->accepted_snapshot();
					ReviewMutationTransport = ReadWindow->mutation_transport();
					static_cast<void>(merge(RateLimits, ReadWindow->rate_limits()));
					if (aUpdate.PublicPreviewRefusal) {
						PublicPreviewRefusal = aUpdate.PublicPreviewRefusal;
					}
					if (aUpdate.ProviderError) {
						publish_error_advisory(*aUpdate.ProviderError);
					}
					publish_transition(aUpdate.Transition, nullptr,
									   aUpdate.Transition.AcceptedSnapshot && AcceptedSnapshot ? &*AcceptedSnapshot : nullptr);
				},
				Epochs, AnonymousPullRequests, AnonymousSmartGit);
		}
		if (!ReadWindow->adopt(*session)) {
			return;
		}
		PublishedSession = session;
		ReadWindow->advance_request_floor(State.NextRequest);
		ReadWindow->dispatch(aEvent);
	}

	bool github_application_host::implementation::attach_window(const std::shared_ptr<github_window_read_host>& aWindow)
	{
		std::erase_if(AttachedWindows, [](const auto& aSlot) { return (aSlot.Detaching && aSlot.Host->is_quiescent()); });
		const auto session = current_profile_session();
		if (Closing || !aWindow || aWindow->window().Profile != BprProfile
			|| aWindow->window().Window.value() == 0U || aWindow->window().Generation.value() == 0U || aWindow->state().ApplicationSession.value() == 0U
			|| (!session && aWindow->state().ActiveProfile.has_value())
			|| AttachedWindows.size() + (ReadWindow ? 1U : 0U) >= 32U
			|| aWindow->state().ApplicationSession == State.ApplicationSession) {
			return false;
		}
		const auto restored = aWindow->window().Generation.value() > 1U;
		if (restored) {
			if (RestorationSealed || ReadWindow || RestoredWindowCount == RestoredWindows.size()
				|| std::ranges::find(RestoredWindows, aWindow->window().Window) != RestoredWindows.end()) {
				return false;
			}
		} else if (aWindow->window().Window.value() <= LastAttachedWindow.value()) {
			return false;
		}
		for (const auto& slot : AttachedWindows) {
			if (slot.Context.Window == aWindow->window().Window || slot.Session == aWindow->state().ApplicationSession) {
				return false;
			}
		}
		if (session && !aWindow->adopt(*session)) {
			return false;
		}
		AttachedWindows.push_back({ aWindow->window(), aWindow->state().ApplicationSession,
									session ? std::optional{ session->ProfileAuthority } : std::nullopt,
									session ? session->Session.Viewer.Account : core::github_account_id{ }, aWindow });
		ExplicitPrimaryAttached = ExplicitPrimaryAttached || aWindow->window().Window == core::bpr_window_id{ 1 };
		LastAttachedWindow      = std::max(LastAttachedWindow, aWindow->window().Window);
		if (restored) {
			RestoredWindows[RestoredWindowCount++] = aWindow->window().Window;
		} else {
			RestorationSealed = true;
		}
		if (session) {
			PublishedSession = session;
		}
		return true;
	}

	void github_application_host::implementation::observe_attached_read(const std::shared_ptr<github_window_read_host>& aWindow, const bool aFreshSnapshot)
	{
		if (Closing || !aWindow) {
			return;
		}
		const auto attached = std::ranges::find_if(AttachedWindows, [&aWindow](const auto& aSlot) { return aSlot.Host == aWindow && !aSlot.Detaching; });
		if (attached == AttachedWindows.end()) {
			return;
		}
		AcceptedSnapshot = aWindow->accepted_snapshot();
		static_cast<void>(merge(RateLimits, aWindow->rate_limits()));
		publish_public_preview();
		if (!aFreshSnapshot || Closing || SharedSnapshotSequence == std::numeric_limits<std::uint64_t>::max()) {
			return;
		}
		// Copy the bounded attachment set before publishing to any sibling:
		// its callback may detach itself or another window.
		std::vector<std::shared_ptr<github_window_read_host>> windows;
		windows.reserve(AttachedWindows.size());
		for (const auto& slot : AttachedWindows) {
			if (!slot.Detaching) {
				windows.push_back(slot.Host);
			}
		}
		aWindow->share_snapshot(windows, ++SharedSnapshotSequence);
	}

	void github_application_host::implementation::detach_window(const core::bpr_window_context aContext)
	{
		const auto attached = std::ranges::find_if(AttachedWindows, [aContext](const auto& aSlot) { return aSlot.Context == aContext; });
		if (attached != AttachedWindows.end()) {
			attached->Host->shutdown();
			attached->Detaching = true;
			std::erase_if(AttachedWindows, [](const auto& aSlot) { return aSlot.Detaching && aSlot.Host->is_quiescent(); });
			return;
		}
		if (!ExplicitPrimaryAttached && aContext == core::bpr_window_context{ BprProfile, core::bpr_window_id{ 1 }, core::bpr_window_generation{ 1 } }) {
			if (ReadWindow) {
				ReadWindow->shutdown();
			}
			PrimaryReadClosed   = true;
			PublicPreview       = nullptr;
			PublicPreviewSignIn = { };
			Epochs              = nullptr;
			AcceptedSnapshot.reset();
			ReviewMutationTransport.reset();
		}
	}

	void github_application_host::implementation::publish_profile_session()
	{
		const auto session = current_profile_session();
		if (!session) {
			return;
		}
		PublishedSession = session;
		if (ReadWindow && !PrimaryReadClosed) {
			static_cast<void>(ReadWindow->adopt(*session));
		}
		for (auto& slot : AttachedWindows) {
			if (slot.Detaching || (slot.Authority && (*slot.Authority != session->ProfileAuthority || slot.Account != session->Session.Viewer.Account))) {
				continue;
			}
			if (slot.Host->adopt(*session) && !slot.Authority) {
				slot.Authority = session->ProfileAuthority;
				slot.Account   = session->Session.Viewer.Account;
			}
		}
	}

	/// [[BPR-1829]]. One record each time the accepted account changes.
	void github_application_host::implementation::observe_account() noexcept
	{
		if (State.Account == RecordedAccount) {
			return;
		}
		RecordedAccount = State.Account;
		record_operation(DiagnosticLog.get(), { .Action = core::operation_action::account_change, .Count = State.Account ? 1U : 0U });
	}

	void github_application_host::implementation::dispatch_core(const core::github_event& aEvent, std::optional<github_host_diagnostic_projection> aFailureDiagnostic)
	{
		auto output = core::transition(State, aEvent);
		for (const auto& request : output.Requests) {
			const auto* fetch = std::get_if<core::fetch_pull_request_request>(&request);
			if (fetch != nullptr
				&& !fetch->Context.ExpectedRepository.has_value()
				&& !fetch->Context.ExpectedPullRequest.has_value()) {
				invalidate_epoch_authority();
				StagedSnapshot.reset();
				revoke_review_mutation_transport();
				AcceptedSnapshot.reset();
			}
		}
		publish_and_execute(output, std::move(aFailureDiagnostic));
	}

	void github_application_host::implementation::publish_and_execute(const core::github_transition_output& aOutput, std::optional<github_host_diagnostic_projection> aFailureDiagnostic)
	{
		if (Closing) {
			return;
		}
		for (const auto& request : aOutput.Requests) {
			if (std::holds_alternative<core::invalidate_github_identity_request>(request)) {
				retire_windows();
			}
		}
		publish_transition(aOutput, nullptr, nullptr, std::move(aFailureDiagnostic));
		for (const auto& request : aOutput.Requests) {
			execute(request);
		}
	}

	void github_application_host::implementation::publish_transition(
		const core::github_transition_output&            aOutput,
		const github_session_read_model* const           aSession,
		const github_pull_request_snapshot* const        aPullRequest,
		std::optional<github_host_diagnostic_projection> aFailureDiagnostic) noexcept
	{
		observe_account();
		try {
			std::optional<github_session_projection> sessionProjection;
			if (aSession != nullptr) {
				sessionProjection = project_github_session(*aSession, grant_projection());
			}
			std::optional<github_pull_request_projection>
				pullRequestProjection;
			if (aPullRequest != nullptr) {
				pullRequestProjection = project_github_pull_request(*aPullRequest);
			}
			std::vector<github_host_diagnostic_projection> diagnostics;
			// Only a core-accepted failure receives the call site's stage. A stale
			// terminal cannot publish context onto a replacement session.
			if (aFailureDiagnostic
				&& aOutput.Status == core::github_status_delta{ .Status = core::github_status::session_failed }
				&& !aOutput.Diagnostics.empty()) {
				aFailureDiagnostic->Reason = aOutput.Diagnostics.front().Code;
				diagnostics.push_back(std::move(*aFailureDiagnostic));
			}
			const github_presentation_update update{
				.Transition      = project_github_transition(aOutput),
				.Session         = std::move(sessionProjection),
				.PullRequest     = std::move(pullRequestProjection),
				.HostDiagnostics = std::move(diagnostics),
			};
			if (!PrimaryReadClosed) {
				Projections.publish(update);
			}
		} catch (...) {
			ProjectionDeliveryFailed = true;
		}
		publish_public_preview();
	}

	void github_application_host::implementation::publish_diagnostic(const github_host_diagnostic& aDiagnostic, const github_bootstrap_failure_stage aStage) noexcept
	{
		try {
			auto diagnostic           = project_github_host_diagnostic(aDiagnostic);
			diagnostic.BootstrapStage = aStage;
			const github_presentation_update update{
				.Transition      = std::nullopt,
				.Session         = std::nullopt,
				.PullRequest     = std::nullopt,
				.HostDiagnostics = { std::move(diagnostic) },
			};
			if (!PrimaryReadClosed) {
				Projections.publish(update);
			}
		} catch (...) {
			ProjectionDeliveryFailed = true;
		}
		publish_public_preview();
	}

	auto github_application_host::implementation::projected_refusal_reason(
		const github_public_preview_refusal& aRefusal) noexcept
		-> public_preview_refusal_reason
	{
		if (aRefusal.Cause
				!= github_public_preview_refusal_cause::policy_refused
			|| !aRefusal.Policy.has_value()) {
			return public_preview_refusal_reason::mode_unavailable;
		}
		switch (aRefusal.Policy->Reason) {
		case core::anonymous_refusal_reason::rate_limited:
			return public_preview_refusal_reason::budget_exhausted;
		case core::anonymous_refusal_reason::private_or_missing:
			return public_preview_refusal_reason::private_or_missing;
		case core::anonymous_refusal_reason::ghes_unavailable:
			return public_preview_refusal_reason::enterprise_host_unavailable;
		case core::anonymous_refusal_reason::graphql_refused:
			return public_preview_refusal_reason::review_threads_unavailable;
		case core::anonymous_refusal_reason::viewer_identity_refused:
			return public_preview_refusal_reason::viewer_identity_unavailable;
		case core::anonymous_refusal_reason::mutation_refused:
			break;
		}
		return public_preview_refusal_reason::triage_or_publishing_refused;
	}

	auto github_application_host::implementation::project_review_comment(
		const github_review_comment_record& aComment)
		-> public_preview_review_comment
	{
		return public_preview_review_comment{
			.Id          = std::string{ aComment.Comment.value() },
			.AuthorLogin = aComment.Author.has_value()
				? aComment.Author->Login
				: std::string{ },
			.Body        = aComment.Body,
			.Path        = aComment.Path,
			.Line        = aComment.Line,
		};
	}

	auto github_application_host::implementation::project_review_comment_group(
		const github_review_comment_group_record& aGroup)
		-> public_preview_review_comment_group
	{
		public_preview_review_comment_group group{
			.Root    = project_review_comment(aGroup.Root),
			.Replies = { },
		};
		group.Replies.reserve(aGroup.Replies.size());
		for (const auto& reply : aGroup.Replies) {
			group.Replies.push_back(project_review_comment(reply));
		}
		return group;
	}

	auto github_application_host::implementation::project_public_preview() const
		-> public_preview_presentation
	{
		public_preview_presentation presentation{ };
		presentation.Mode = signed_out_mode()
			? public_preview_mode::signed_out
			: public_preview_mode::signed_in;
		if (RateLimits.Core.has_value()) {
			presentation.Budget = public_preview_budget{
				.Limit            = RateLimits.Core->Limit,
				.Remaining        = RateLimits.Core->Remaining,
				.ResetUnixSeconds = RateLimits.Core->ResetUnixSeconds,
			};
		}
		if (PublicPreviewRefusal.has_value()) {
			auto refusal = public_preview_refusal_presentation{
				.Reason = projected_refusal_reason(*PublicPreviewRefusal),
				.Budget = std::nullopt,
			};
			if (PublicPreviewRefusal->Policy.has_value()
				&& PublicPreviewRefusal->Policy->Budget.has_value()) {
				const auto& fact = *PublicPreviewRefusal->Policy->Budget;
				refusal.Budget   = public_preview_budget{
					.Limit            = fact.Limit,
					.Remaining        = 0U,
					.ResetUnixSeconds = fact.Reset,
				};
			}
			presentation.Refusal = refusal;
		}
		if (AcceptedSnapshot.has_value()
			&& AcceptedSnapshot->ReviewCommentsAvailability
				== github_collection_availability::available) {
			presentation.ReviewComments
				= public_preview_observation::observed;
			presentation.ReviewCommentGroups.reserve(
				AcceptedSnapshot->ReviewCommentGroups.size());
			for (const auto& group : AcceptedSnapshot->ReviewCommentGroups) {
				presentation.ReviewCommentGroups.push_back(
					project_review_comment_group(group));
			}
		}
		return presentation;
	}

	// Publication is by change on the four occasions the port promises:
	// activation, every budget observation, every refusal, and
	// invalidation. Each of those runs through a transition, a diagnostic,
	// an observed provider error, or the explicit call in `invalidate`.
	void github_application_host::implementation::publish_public_preview() noexcept
	{
		if (PublicPreview == nullptr) {
			return;
		}
		try {
			auto presentation = project_public_preview();
			if (PublishedPublicPreview.has_value()
				&& *PublishedPublicPreview == presentation) {
				return;
			}
			PublishedPublicPreview = presentation;
			PublicPreview->publish(presentation);
		} catch (...) {
			ProjectionDeliveryFailed = true;
		}
	}

	void github_application_host::implementation::observe_provider_error(const github_provider_error& aError)
	{
		if (valid(aError)) {
			static_cast<void>(merge(RateLimits, aError.RateLimits));
			publish_public_preview();
		}
	}

	void github_application_host::implementation::publish_error_advisory(const github_provider_error& aError)
	{
		if (valid(aError)
			&& (aError.RetryAfterSeconds.has_value()
				|| has_observations(aError.RateLimits))) {
			publish_diagnostic(github_host_diagnostic{
				.Reason            = core::reason_for(mapped_failure(aError.Failure)),
				.RetryAfterSeconds = aError.RetryAfterSeconds,
				.RateLimits        = RateLimits,
				.RateLimitCause    = aError.RateLimitCause,
			});
		}
	}

	auto github_application_host::implementation::requires_shutdown_credential_cleanup() const noexcept
		-> bool
	{
		if (!CredentialAuthority.has_value()
			|| !CredentialSessionContext.has_value()) {
			return false;
		}
		return CredentialLifecycle == credential_lifecycle::provisional
			|| CredentialShutdownPolicy
			== github_credential_shutdown_policy::
				delete_retained_on_shutdown
			|| (CredentialShutdownPolicy == github_credential_shutdown_policy::retain_accepted_device_grants && !DeviceBindingDurable);
	}
}
