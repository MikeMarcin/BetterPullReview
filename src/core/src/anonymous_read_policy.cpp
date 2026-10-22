#include "bpr/core/anonymous_read_policy.hpp"

namespace bpr::core {
	namespace {
		/// Decides why public preview refuses an intent, or that it does not.
		/// The switch names every intent, so an intent added to the
		/// enumeration stops this translation unit compiling rather than
		/// reaching a legality answer nobody chose for it.
		[[nodiscard]] auto public_preview_refusal(
			const anonymous_read_intent aIntent) noexcept
			-> std::optional<anonymous_refusal_reason>
		{
			switch (aIntent) {
			case anonymous_read_intent::read_repository:
			case anonymous_read_intent::read_pull_request:
			case anonymous_read_intent::read_changed_files:
			case anonymous_read_intent::read_commits:
			case anonymous_read_intent::read_reviews:
			case anonymous_read_intent::read_review_comments:
			case anonymous_read_intent::read_issue_comments:
			case anonymous_read_intent::read_checks:
			case anonymous_read_intent::search_pull_requests:
				return std::nullopt;
			case anonymous_read_intent::read_review_threads:
				return anonymous_refusal_reason::graphql_refused;
			case anonymous_read_intent::read_viewer_identity:
				return anonymous_refusal_reason::viewer_identity_refused;
			case anonymous_read_intent::resolve_review_thread:
			case anonymous_read_intent::unresolve_review_thread:
			case anonymous_read_intent::set_viewed_file_state:
			case anonymous_read_intent::publish_review_comment:
			case anonymous_read_intent::submit_review:
			case anonymous_read_intent::react_to_comment:
				return anonymous_refusal_reason::mutation_refused;
			case anonymous_read_intent::read_private_repository:
				return anonymous_refusal_reason::private_or_missing;
			case anonymous_read_intent::select_enterprise_host:
				return anonymous_refusal_reason::ghes_unavailable;
			}
			// A value outside the enumeration is not an intent this policy
			// knows, and the only safe answer to an unknown intent is no.
			return anonymous_refusal_reason::mutation_refused;
		}

		[[nodiscard]] auto advanced(
			const github_identity_generation aGeneration) noexcept
			-> github_identity_generation
		{
			return github_identity_generation{ aGeneration.value() + 1U };
		}
	}

	auto catalog_index(const anonymous_read_intent aIntent) noexcept
		-> std::size_t
	{
		switch (aIntent) {
		case anonymous_read_intent::read_repository:
			return 0U;
		case anonymous_read_intent::read_pull_request:
			return 1U;
		case anonymous_read_intent::read_changed_files:
			return 2U;
		case anonymous_read_intent::read_commits:
			return 3U;
		case anonymous_read_intent::read_reviews:
			return 4U;
		case anonymous_read_intent::read_review_comments:
			return 5U;
		case anonymous_read_intent::read_issue_comments:
			return 6U;
		case anonymous_read_intent::read_checks:
			return 7U;
		case anonymous_read_intent::search_pull_requests:
			return 8U;
		case anonymous_read_intent::read_review_threads:
			return 9U;
		case anonymous_read_intent::read_viewer_identity:
			return 10U;
		case anonymous_read_intent::resolve_review_thread:
			return 11U;
		case anonymous_read_intent::unresolve_review_thread:
			return 12U;
		case anonymous_read_intent::set_viewed_file_state:
			return 13U;
		case anonymous_read_intent::publish_review_comment:
			return 14U;
		case anonymous_read_intent::submit_review:
			return 15U;
		case anonymous_read_intent::read_private_repository:
			return 16U;
		case anonymous_read_intent::select_enterprise_host:
			return 17U;
		case anonymous_read_intent::react_to_comment:
			return 18U;
		}
		return anonymous_read_intent_count;
	}

	auto refuse(const anonymous_refusal_reason aReason) noexcept
		-> anonymous_refusal
	{
		return anonymous_refusal{ .Reason = aReason, .Budget = std::nullopt };
	}

	auto refuse_rate_limited(const anonymous_rate_limit_fact& aFact) noexcept
		-> anonymous_refusal
	{
		return anonymous_refusal{
			.Reason = anonymous_refusal_reason::rate_limited,
			.Budget = aFact,
		};
	}

	auto well_formed(const anonymous_refusal& aRefusal) noexcept -> bool
	{
		return aRefusal.Budget.has_value()
			== (aRefusal.Reason == anonymous_refusal_reason::rate_limited);
	}

	auto terminal(const anonymous_refusal& aRefusal) noexcept -> bool
	{
		return aRefusal.Reason == anonymous_refusal_reason::rate_limited;
	}

	auto legality(
		const github_access_mode    aMode,
		const anonymous_read_intent aIntent) noexcept
		-> anonymous_intent_legality
	{
		if (aMode == github_access_mode::authenticated) {
			return anonymous_intent_legal{ };
		}

		const auto refusal = public_preview_refusal(aIntent);
		if (!refusal.has_value()) {
			return anonymous_intent_legal{ };
		}
		return anonymous_intent_refused{ .Refusal = refuse(*refusal) };
	}

	auto legality(const anonymous_read_intent aIntent) noexcept
		-> anonymous_intent_legality
	{
		return legality(github_access_mode::public_preview, aIntent);
	}

	auto legal(const anonymous_read_intent aIntent) noexcept -> bool
	{
		return !public_preview_refusal(aIntent).has_value();
	}

	auto unobservable_inline_review_comments() noexcept
		-> inline_review_comment_observation
	{
		return inline_review_comment_observation{
			.Availability = anonymous_observation_availability::unavailable,
			.Count        = 0U,
		};
	}

	auto observed_inline_review_comments(const std::uint32_t aCount) noexcept
		-> inline_review_comment_observation
	{
		return inline_review_comment_observation{
			.Availability = anonymous_observation_availability::observed,
			.Count        = aCount,
		};
	}

	auto observable(
		const inline_review_comment_observation& aObservation) noexcept -> bool
	{
		return aObservation.Availability
			== anonymous_observation_availability::observed;
	}

	auto observed_none(
		const inline_review_comment_observation& aObservation) noexcept -> bool
	{
		return observable(aObservation) && aObservation.Count == 0U;
	}

	auto well_formed(
		const inline_review_comment_observation& aObservation) noexcept -> bool
	{
		return observable(aObservation) || aObservation.Count == 0U;
	}

	auto observes_viewer_state(const github_access_mode aMode) noexcept -> bool
	{
		return aMode == github_access_mode::authenticated;
	}

	auto anonymous_observations() noexcept -> anonymous_observation_state
	{
		return anonymous_observation_state{
			.Viewer               = anonymous_viewer_identity::unavailable,
			.ThreadResolution     = anonymous_thread_resolution::unavailable,
			.ViewedFile           = anonymous_viewed_file_state::unavailable,
			.Actionability        = anonymous_actionability::unavailable,
			.InlineReviewComments = unobservable_inline_review_comments(),
		};
	}

	auto anonymous_capabilities() noexcept -> github_capabilities
	{
		return github_capabilities{
			.ReviewThreads       = github_capability_state::unsupported,
			.ViewedFileMutations = github_capability_state::unsupported,
			.ResolveThread       = github_capability_state::unsupported,
			.UnresolveThread     = github_capability_state::unsupported,
		};
	}

	auto gated_intent(const github_capability aCapability) noexcept
		-> anonymous_read_intent
	{
		switch (aCapability) {
		case github_capability::review_threads:
			return anonymous_read_intent::read_review_threads;
		case github_capability::viewed_file_mutations:
			return anonymous_read_intent::set_viewed_file_state;
		case github_capability::resolve_thread:
			return anonymous_read_intent::resolve_review_thread;
		case github_capability::unresolve_thread:
			return anonymous_read_intent::unresolve_review_thread;
		}
		return anonymous_read_intent::read_review_threads;
	}

	auto crosses_modes(const github_access_mode_change& aChange) noexcept
		-> bool
	{
		return aChange.Previous != aChange.Next;
	}

	auto mode_change_invalidation(
		const github_access_mode_change& aChange,
		const application_session_id     aApplicationSession,
		const github_identity_generation aCurrentGeneration,
		const github_identity_scope&     aPrevious,
		request_id                       aRequest)
		-> std::optional<invalidate_github_identity_request>
	{
		if (!crosses_modes(aChange)) {
			return std::nullopt;
		}

		return invalidate_github_identity_request{
			.Request            = aRequest,
			.ApplicationSession = aApplicationSession,
			.NewGeneration      = advanced(aCurrentGeneration),
			.Previous           = aPrevious,
		};
	}

	auto open_read(
		anonymous_read_policy_state&        aState,
		const anonymous_read_intent         aIntent,
		const pull_request_request_context& aContext) -> anonymous_open_outcome
	{
		if (aState.Terminal.has_value()) {
			return *aState.Terminal;
		}

		const auto decision = legality(aState.Mode, aIntent);
		if (const auto* refused
			= std::get_if<anonymous_intent_refused>(&decision)) {
			return refused->Refusal;
		}

		auto identity = anonymous_request_identity{
			.Mode    = aState.Mode,
			.Context = aContext,
		};
		aState.Pending = identity;
		return identity;
	}

	auto admit(
		anonymous_read_policy_state&      aState,
		const anonymous_request_identity& aObserved)
		-> anonymous_completion_admission
	{
		if (
			!aState.Pending.has_value() || *aState.Pending != aObserved
			|| aObserved.Mode != aState.Mode
			|| aObserved.Context.ApplicationSession != aState.ApplicationSession
			|| aObserved.Context.IdentityGeneration
				!= aState.IdentityGeneration) {
			return anonymous_completion_admission::stale_completion;
		}

		aState.Pending.reset();
		return anonymous_completion_admission::accepted;
	}

	auto exhaust(
		anonymous_read_policy_state&     aState,
		const anonymous_rate_limit_fact& aFact) -> anonymous_refusal
	{
		const auto refusal = refuse_rate_limited(aFact);
		aState.Terminal    = refusal;
		aState.Pending.reset();
		return refusal;
	}

	auto change_mode(
		anonymous_read_policy_state& aState,
		const github_access_mode     aNext,
		const request_id             aRequest)
		-> std::optional<invalidate_github_identity_request>
	{
		const auto change = github_access_mode_change{
			.Previous = aState.Mode,
			.Next     = aNext,
		};
		auto request = mode_change_invalidation(
			change,
			aState.ApplicationSession,
			aState.IdentityGeneration,
			aState.Scope,
			aRequest);
		if (!request.has_value()) {
			return std::nullopt;
		}

		aState.Mode               = aNext;
		aState.IdentityGeneration = request->NewGeneration;
		aState.Scope              = github_identity_scope{ };
		aState.Pending.reset();
		aState.Terminal.reset();
		return request;
	}
}
