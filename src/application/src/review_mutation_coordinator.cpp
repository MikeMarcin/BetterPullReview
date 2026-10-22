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
		// [[BPR-0879]]. Resolution is a write, so it answers to the rule
		// publication answers to: the server must still be describing the pull
		// request this coordinator was designated for. One observation, not
		// two: publication observes again immediately before submitting
		// because a proposal is prepared against a revision, and this is not —
		// a thread belongs to a pull request, not to a commit. For the same
		// reason a moved head is acknowledged here rather than refused; it
		// cannot change which pull request a thread is in, and refusing would
		// make an ordinary push block resolving a thread.
		if (auto confirmed = confirm_target(true, aStop); !confirmed) {
			++Statistics.RefusedBeforeSubmit;
			return std::unexpected(confirmed.error());
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
		// A resolution POST is a submission. Counting only the attempt let
		// diagnostics report zero submissions after a real write, which is
		// exactly the wrong property for reconstructing an accidental one.
		++Statistics.Submissions;
		return Provider->resolve_thread(request, aStop);
	}

	auto review_mutation_coordinator::set_reaction(
		const std::string&           aSubject,
		const core::reaction_content aContent,
		const bool                   aAdd,
		const std::stop_token        aStop)
		-> std::expected<review_reaction_receipt, review_mutation_failure>
	{
		++Statistics.Attempts;
		if (aStop.stop_requested()) {
			++Statistics.RefusedBeforeSubmit;
			return std::unexpected(review_mutation_failure::cancelled);
		}
		if (Provider == nullptr || aSubject.empty()) {
			++Statistics.RefusedBeforeSubmit;
			return std::unexpected(review_mutation_failure::undesignated_target);
		}
		// [[BPR-1741]]. A reaction is a write, so it answers to the
		// rule a resolution does: the server must still describe the
		// designated pull request, and a moved head is acknowledged because a
		// comment belongs to a pull request, not to a commit.
		if (auto confirmed = confirm_target(true, aStop); !confirmed) {
			++Statistics.RefusedBeforeSubmit;
			return std::unexpected(confirmed.error());
		}
		auto request = review_reaction_request{
			.Target         = Designated,
			.ActingIdentity = ActingIdentity,
			.Subject        = aSubject,
			.Content        = aContent,
			.Add            = aAdd,
		};
		if (TargetBinding.has_value() && ActingIdentityBinding.has_value()) {
			request.LiveBinding = review_mutation_live_binding{
				.Target         = *TargetBinding,
				.ActingIdentity = *ActingIdentityBinding,
			};
		}
		++Statistics.Submissions;
		return Provider->set_reaction(request, aStop);
	}

	auto review_mutation_coordinator::publish_attempt(
		const core::review_publication_proposal&      aProposal,
		review_mutation_anchor_binding                aAnchor,
		const bool                                    aAcknowledged,
		const std::stop_token                         aStop,
		std::optional<review_mutation_source_binding> aSource)
		-> review_mutation_submission
	{
		return publish_bound(aProposal, std::move(aAnchor), aAcknowledged, aStop, std::move(aSource));
	}

	auto review_mutation_coordinator::publish_bound(
		const core::review_publication_proposal&      aProposal,
		std::optional<review_mutation_anchor_binding> aAnchor,
		const bool                                    aAcknowledged,
		const std::stop_token                         aStop,
		std::optional<review_mutation_source_binding> aSource)
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
				|| !admitted_anchor_commit(*aAnchor, TargetBinding->Semantic.ExpectedHead))) {
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
															 .Source         = std::move(aSource),
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
