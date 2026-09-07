#include "bpr/application/review_mutation_coordinator.hpp"

#include <utility>

namespace bpr::application {
	review_mutation_coordinator::review_mutation_coordinator(
		review_mutation_port&               aProvider,
		const core::review_mutation_target& aDesignated,
		const core::github_account_id       aActingIdentity) noexcept
		: Provider{ &aProvider }
		, Designated{ aDesignated }
		, ActingIdentity{ aActingIdentity }
	{
	}

	review_mutation_coordinator::review_mutation_coordinator(
		review_mutation_port&           aProvider,
		review_mutation_target_binding  aDesignated,
		review_mutation_acting_identity aActingIdentity) noexcept
		: Provider{ &aProvider }
		, Designated{ aDesignated.Semantic }
		, ActingIdentity{ aActingIdentity.Account }
		, TargetBinding{ std::move(aDesignated) }
		, ActingIdentityBinding{ std::move(aActingIdentity) }
	{
	}

	auto review_mutation_coordinator::designated() const noexcept
		-> const core::review_mutation_target&
	{
		return Designated;
	}

	auto review_mutation_coordinator::capabilities() const
		-> review_provider_capabilities
	{
		return Provider->capabilities();
	}

	auto review_mutation_coordinator::statistics() const noexcept
		-> review_mutation_statistics
	{
		return Statistics;
	}

	auto review_mutation_coordinator::confirm_target(
		const bool            aAcknowledged,
		const std::stop_token aStop)
		-> std::expected<void, review_mutation_failure>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(review_mutation_failure::cancelled);
		}
		++Statistics.TargetObservations;
		const auto observed = Provider->observe_target(aStop);
		if (!observed) {
			return std::unexpected(observed.error());
		}

		switch (core::revalidate(Designated, *observed)) {
		case core::target_revalidation::accepted:
			return { };
		case core::target_revalidation::head_moved:
			// A moved head is the reviewer's decision to make, and only theirs.
			return aAcknowledged
				? std::expected<void, review_mutation_failure>{ }
				: std::unexpected(review_mutation_failure::stale_head);
		case core::target_revalidation::target_changed:
			// Not overridable at any level. The coordinator was designated for
			// one pull request and the server is now describing another.
			return std::unexpected(review_mutation_failure::undesignated_target);
		case core::target_revalidation::head_unknown:
			break;
		}
		return std::unexpected(review_mutation_failure::target_moved);
	}

	auto review_mutation_coordinator::publish(
		const core::review_publication_proposal& aProposal,
		const bool                               aAcknowledged,
		const std::stop_token                    aStop)
		-> std::expected<review_mutation_receipt, review_mutation_failure>
	{
		return publish_bound(aProposal, std::nullopt, aAcknowledged, aStop).Result;
	}

	auto review_mutation_coordinator::publish(
		const core::review_publication_proposal& aProposal,
		review_mutation_anchor_binding           aAnchor,
		const bool                               aAcknowledged,
		const std::stop_token                    aStop)
		-> std::expected<review_mutation_receipt, review_mutation_failure>
	{
		return publish_bound(
				   aProposal, std::move(aAnchor), aAcknowledged, aStop)
			.Result;
	}

	auto review_mutation_coordinator::resolve_thread(
		const std::string&    aThread,
		const bool            aResolve,
		const std::stop_token aStop)
		-> std::expected<review_thread_resolution_receipt, review_mutation_failure>
	{
		++Statistics.Attempts;
		if (aStop.stop_requested()) {
			++Statistics.RefusedBeforeSubmit;
			return std::unexpected(review_mutation_failure::cancelled);
		}
		if (Provider == nullptr || aThread.empty()) {
			++Statistics.RefusedBeforeSubmit;
			return std::unexpected(review_mutation_failure::undesignated_target);
		}
		auto request = review_thread_resolution_request{
			.Target         = Designated,
			.ActingIdentity = ActingIdentity,
			.Thread         = aThread,
			.Resolve        = aResolve,
		};
		if (TargetBinding.has_value() && ActingIdentityBinding.has_value()) {
			request.LiveBinding = review_mutation_live_binding{
				.Target         = *TargetBinding,
				.ActingIdentity = *ActingIdentityBinding,
			};
		}
		return Provider->resolve_thread(request, aStop);
	}

	auto review_mutation_coordinator::publish_attempt(
		const core::review_publication_proposal& aProposal,
		review_mutation_anchor_binding           aAnchor,
		const bool                               aAcknowledged,
		const std::stop_token                    aStop)
		-> review_mutation_submission
	{
		return publish_bound(aProposal, std::move(aAnchor), aAcknowledged, aStop);
	}

	auto review_mutation_coordinator::publish_bound(
		const core::review_publication_proposal&      aProposal,
		std::optional<review_mutation_anchor_binding> aAnchor,
		const bool                                    aAcknowledged,
		const std::stop_token                         aStop)
		-> review_mutation_submission
	{
		const auto failed = [](const review_mutation_failure aFailure) {
			return review_mutation_submission{
				.Result = std::unexpected(aFailure),
			};
		};
		++Statistics.Attempts;
		if (aStop.stop_requested()) {
			++Statistics.RefusedBeforeSubmit;
			return failed(review_mutation_failure::cancelled);
		}
		const auto completeBinding = TargetBinding.has_value()
			&& ActingIdentityBinding.has_value() && aAnchor.has_value();
		if (completeBinding
			&& (!valid(*TargetBinding) || !valid(*ActingIdentityBinding)
				|| !valid(*aAnchor) || aAnchor->Semantic != aProposal.Anchor
				|| aAnchor->Commit != TargetBinding->Semantic.ExpectedHead)) {
			++Statistics.RefusedBeforeSubmit;
			return failed(review_mutation_failure::undesignated_target);
		}
		if (!completeBinding
			&& (TargetBinding.has_value() || ActingIdentityBinding.has_value()
				|| aAnchor.has_value())) {
			++Statistics.RefusedBeforeSubmit;
			return failed(review_mutation_failure::undesignated_target);
		}

		if (auto prepared = confirm_target(aAcknowledged, aStop); !prepared) {
			++Statistics.RefusedBeforeSubmit;
			return failed(prepared.error());
		}

		// Observed again immediately before submitting. The first check decided
		// whether publication was permitted; this one decides whether it is
		// still the same pull request. Between the two there is a window, and
		// this is what closes it.
		if (auto current = confirm_target(aAcknowledged, aStop); !current) {
			++Statistics.RefusedBeforeSubmit;
			return failed(current.error());
		}

		++Statistics.Submissions;
		auto liveBinding = std::optional<review_mutation_live_binding>{ };
		if (completeBinding) {
			liveBinding = review_mutation_live_binding{
				.Target         = *TargetBinding,
				.ActingIdentity = *ActingIdentityBinding,
				.Anchor         = std::move(*aAnchor),
			};
		}

		if (aStop.stop_requested()) {
			++Statistics.RefusedBeforeSubmit;
			return failed(review_mutation_failure::cancelled);
		}
		auto submission = Provider->submit_and_reconcile(review_mutation_request{
															 .Proposal       = aProposal,
															 .Target         = Designated,
															 .ActingIdentity = ActingIdentity,
															 .IdempotencyKey = idempotency_key_for(aProposal, Designated),
															 .LiveBinding    = std::move(liveBinding),
														 },
														 aStop);
		if (!submission.Result) {
			return submission;
		}
		if (submission.Result->Duplicate) {
			++Statistics.Duplicates;
		}
		return submission;
	}
} // namespace bpr::application
