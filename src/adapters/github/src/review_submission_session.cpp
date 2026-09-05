#include "review_submission_session.hpp"

#include <algorithm>
#include <atomic>
#include <concepts>
#include <type_traits>
#include <utility>

namespace bpr::adapters::github::detail {
	namespace {
		auto wire_input(const application::review_submission_request& aRequest) -> review_submission_request
		{
			return { .Target = aRequest.LiveBinding->Target, .ActingIdentity = aRequest.LiveBinding->ActingIdentity, .Disposition = aRequest.Proposal.Disposition, .Body = aRequest.Proposal.Body };
		}
		auto result(const application::review_submission_request& aRequest, application::review_submission_publication aPublication = application::review_submission_publication::indeterminate, application::review_mutation_failure aFailure = application::review_mutation_failure::unavailable) -> application::review_submission_result { return { .Request = aRequest, .Publication = aPublication, .Failure = aFailure }; }
		auto receipt(const review_submission_observation& aReview) -> application::review_submission_receipt { return { .ReviewDatabaseId = application::review_database_id{ aReview.ReviewDatabaseId }, .ReviewNodeId = aReview.ReviewNodeId, .Target = aReview.Target.Semantic, .ActingAccount = aReview.Author.Account, .Disposition = aReview.Disposition }; }
	}
	review_submission_session::review_submission_session(github_mutation_wire_transport& aTransport, live_review_mutation_configuration aConfiguration, application::review_mutation_transport aLease, std::unique_ptr<application::review_submission_recovery_port> aRecovery)
		: Transport{ aTransport }
		, Configuration{ std::move(aConfiguration) }
		, Lease{ std::move(aLease) }
		, Recovery{ std::move(aRecovery) }
	{ }
	auto review_submission_session::perform(review_mutation_wire_request aWire, std::stop_token aStop, github_mutation_transmission_witness aWitness) -> github_mutation_wire_completion
	{
		if (aStop.stop_requested()) {
			return github_mutation_wire_cancelled{ };
		}
		if (!Lease || !Lease->active() || !valid(Configuration) || !validate_outbound_review_mutation_request(aWire)) {
			return github_mutation_wire_error{ };
		}
		return Transport.perform_review_mutation({ .Context = Configuration.CredentialContext, .Wire = std::move(aWire) }, aStop, std::move(aWitness));
	}
	auto review_submission_session::observe_exact(std::stop_token aStop) -> std::expected<void, application::review_mutation_failure>
	{
		auto viewer = compile_viewer_observation_request(Configuration.ActingIdentity, Configuration.Target.ServerContract);
		if (!viewer) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		auto  observedViewer = perform(*viewer, aStop);
		auto* viewerSuccess  = std::get_if<github_mutation_wire_success>(&observedViewer);
		if (!viewerSuccess || !parse_viewer_observation(viewerSuccess->Response, Configuration.ActingIdentity)) {
			return std::unexpected(application::review_mutation_failure::undesignated_target);
		}
		auto target = compile_target_observation_request(Configuration.Target, Configuration.Target.ServerContract);
		if (!target) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		auto  observedTarget = perform(*target, aStop);
		auto* targetSuccess  = std::get_if<github_mutation_wire_success>(&observedTarget);
		if (!targetSuccess) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		auto parsed = parse_target_observation(targetSuccess->Response, Configuration.Target);
		if (!parsed) {
			return std::unexpected(application::review_mutation_failure::undesignated_target);
		}
		if (parsed->Target.Semantic.ExpectedHead != Configuration.Target.Semantic.ExpectedHead) {
			return std::unexpected(application::review_mutation_failure::stale_head);
		}
		return { };
	}
	auto review_submission_session::observe_reviews(const application::review_submission_request& aRequest, std::stop_token aStop) -> review_mutation_parse_result<review_submission_reconciliation>
	{
		auto wire = compile_review_submission_observation_request(wire_input(aRequest), Configuration.Target.ServerContract);
		if (!wire) {
			return std::unexpected(wire.error());
		}
		auto completion = perform(*wire, aStop);
		if (auto* success = std::get_if<github_mutation_wire_success>(&completion)) {
			return parse_review_submission_observations(success->Response, wire_input(aRequest));
		}
		return std::unexpected(review_mutation_contract_failure::invalid_status);
	}
	auto review_submission_session::finish_known(const application::review_submission_recovery_record& aRecord, std::stop_token aStop, bool aDuplicate) -> application::review_submission_result
	{
		auto outcome = result(aRecord.Request);
		if (!application::valid(aRecord)) {
			return outcome;
		}
		if (aRecord.Receipt) {
			outcome.Publication = application::review_submission_publication::published;
			outcome.Receipt     = aRecord.Receipt;
			outcome.Duplicate   = true;
			return outcome;
		}
		if (aRecord.Transmission != application::review_mutation_recovery_transmission::possibly_transmitted || !aRecord.BaselineReviewIds) {
			return outcome;
		}
		auto observed = observe_reviews(aRecord.Request, aStop);
		if (!observed) {
			return outcome;
		}
		return resolve_observed(aRecord, *observed, aDuplicate);
	}
	auto review_submission_session::resolve_observed(const application::review_submission_recovery_record& aRecord, const review_submission_reconciliation& aObserved, bool aDuplicate) -> application::review_submission_result
	{
		auto                                                  outcome = result(aRecord.Request);
		std::optional<application::review_submission_receipt> candidate;
		for (const auto& review : aObserved.ExactReviews) {
			const auto current = receipt(review);
			if (std::ranges::binary_search(*aRecord.BaselineReviewIds, current.ReviewDatabaseId)) {
				continue;
			}
			if (candidate) {
				return outcome;
			}
			candidate = current;
		}
		if (!candidate) {
			return outcome;
		}
		auto resolved = Recovery->resolve(aRecord.Request.IdempotencyKey, aRecord.RequestFingerprint, *candidate);
		if (!resolved || !application::valid(*resolved) || resolved->Request != aRecord.Request || resolved->RequestFingerprint != aRecord.RequestFingerprint || resolved->AuthorityFingerprint != aRecord.AuthorityFingerprint || resolved->BaselineReviewIds != aRecord.BaselineReviewIds || resolved->Receipt != candidate) {
			return outcome;
		}
		outcome.Publication = application::review_submission_publication::published;
		outcome.Receipt     = candidate;
		outcome.Duplicate   = aDuplicate;
		return outcome;
	}
	auto review_submission_session::execute(const application::review_submission_request& aRequest, std::stop_token aStop, bool aAllowNew) -> application::review_submission_result
	{
		using publication = application::review_submission_publication;
		try {
			std::scoped_lock lock{ Mutex };
			if (!application::valid(aRequest) || !aRequest.LiveBinding || aRequest.LiveBinding->Target != Configuration.Target || aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity) {
				return result(aRequest, publication::definitely_not_published, application::review_mutation_failure::undesignated_target);
			}
			if (!Recovery || !Lease || !Lease->active() || !valid(Configuration)) {
				return result(aRequest);
			}
			auto wire = compile_review_submission_request(wire_input(aRequest), Configuration.Target.ServerContract);
			if (!wire) {
				return result(aRequest, publication::definitely_not_published);
			}
			const auto fingerprint = application::review_submission_request_fingerprint(aRequest);
			const auto authority   = application::review_submission_authority_fingerprint(*aRequest.LiveBinding);
			auto       loaded      = Recovery->load(aRequest.IdempotencyKey);
			if (!loaded) {
				return result(aRequest);
			}
			if (*loaded) {
				const auto& known = **loaded;
				if (!application::valid(known) || known.Request != aRequest || known.RequestFingerprint != fingerprint || known.AuthorityFingerprint != authority) {
					return result(aRequest);
				}
				if (known.Receipt || known.Transmission != application::review_mutation_recovery_transmission::possibly_transmitted) {
					return finish_known(known, aStop, true);
				}
				if (aStop.stop_requested() || !observe_exact(aStop)) {
					return result(aRequest);
				}
				return finish_known(known, aStop, true);
			}
			if (!aAllowNew) {
				return result(aRequest, publication::definitely_not_published);
			}
			if (aStop.stop_requested()) {
				return result(aRequest, publication::definitely_not_published, application::review_mutation_failure::cancelled);
			}
			const auto baseline = observe_reviews(aRequest, aStop);
			if (!baseline) {
				return result(aRequest, publication::definitely_not_published);
			}
			auto expected = application::review_submission_recovery_record{ .Request = aRequest, .RequestFingerprint = fingerprint, .AuthorityFingerprint = authority, .BaselineReviewIds = baseline->ReviewIds, .Transmission = application::review_mutation_recovery_transmission::prepared };
			auto admitted = Recovery->admit(expected);
			if (!admitted || !application::valid(admitted->Record) || admitted->Record.Request != aRequest || admitted->Record.RequestFingerprint != fingerprint || admitted->Record.AuthorityFingerprint != authority) {
				return result(aRequest);
			}
			if (!admitted->Inserted) {
				return finish_known(admitted->Record, aStop, true);
			}
			if (admitted->Record != expected) {
				return result(aRequest);
			}
			const auto current = observe_exact(aStop);
			if (!current) {
				return result(aRequest, publication::definitely_not_published, current.error());
			}
			if (aStop.stop_requested() || !Lease->active()) {
				return result(aRequest, publication::definitely_not_published, application::review_mutation_failure::cancelled);
			}
			auto witnessed  = std::make_shared<std::atomic_bool>(false);
			auto completion = github_mutation_wire_completion{ github_mutation_wire_error{ .Transmission = github_mutation_transmission::indeterminate } };
			try {
				completion = perform(*wire, aStop, [this, witnessed, expected] {
					if (witnessed->exchange(true)) {
						return false;
					}
					const auto marked  = Recovery->mark_possibly_transmitted(expected.Request.IdempotencyKey, expected.RequestFingerprint);
					auto       exact   = expected;
					exact.Transmission = application::review_mutation_recovery_transmission::possibly_transmitted;
					return marked && *marked == exact && application::valid(*marked);
				});
			} catch (...) { /* The durable witness and server observation decide the outcome. */
			}
			// A POST receipt alone never establishes completion: server truth must contain one new exact review.
			const auto observed         = observe_reviews(aRequest, { });
			const auto definitelyUnsent = std::visit([](const auto& aCompletion) {
				using completion_type = std::remove_cvref_t<decltype(aCompletion)>;
				if constexpr (std::same_as<completion_type, github_mutation_wire_success>) {
					return false;
				} else {
					return aCompletion.Transmission == github_mutation_transmission::definitely_not_sent;
				}
			},
													 completion);
			if (!witnessed->load() && definitelyUnsent) {
				return result(aRequest, publication::definitely_not_published);
			}
			const auto persisted = Recovery->load(aRequest.IdempotencyKey);
			if (!persisted || !*persisted || !application::valid(**persisted) || (**persisted).Request != aRequest || (**persisted).BaselineReviewIds != expected.BaselineReviewIds || (**persisted).Transmission != application::review_mutation_recovery_transmission::possibly_transmitted || !observed) {
				return result(aRequest);
			}
			return resolve_observed(**persisted, *observed, false);
		} catch (...) {
			return result(aRequest);
		}
	}
	auto review_submission_session::submit_and_reconcile(const application::review_submission_request& aRequest, std::stop_token aStop) -> application::review_submission_result { return execute(aRequest, aStop, true); }
	auto review_submission_session::reconcile(const application::review_submission_request& aRequest, std::stop_token aStop) -> application::review_submission_result { return execute(aRequest, aStop, false); }
}
