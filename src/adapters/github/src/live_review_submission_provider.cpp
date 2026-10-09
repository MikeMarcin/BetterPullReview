#include "live_review_submission_provider.hpp"

#include "review_mutation_provider_admission.hpp"

#include <new>
#include <utility>

namespace bpr::adapters::github {
	live_review_submission_provider::live_review_submission_provider(
		application::review_mutation_transport                        aLease,
		detail::github_mutation_wire_transport&                       aTransport,
		detail::live_review_mutation_configuration                    aConfiguration,
		std::unique_ptr<application::review_submission_recovery_port> aRecovery) noexcept
		: Session{ aTransport,
				   std::move(aConfiguration),
				   std::move(aLease),
				   std::move(aRecovery) }
	{
	}

	auto live_review_submission_provider::pending(std::stop_token aStop) -> std::expected<std::vector<application::review_submission_request>, application::review_mutation_failure> { return Session.pending(aStop); }
	auto live_review_submission_provider::submit_and_reconcile(const application::review_submission_request& aRequest, std::stop_token aStop) -> application::review_submission_result { return Session.submit_and_reconcile(aRequest, aStop); }
	auto live_review_submission_provider::reconcile(const application::review_submission_request& aRequest, std::stop_token aStop) -> application::review_submission_result { return Session.reconcile(aRequest, aStop); }
	auto live_review_submission_provider::admitted(const std::string& aKey, std::stop_token aStop) -> std::expected<std::optional<application::review_submission_request>, application::review_mutation_failure> { return Session.admitted(aKey, aStop); }
	auto live_review_submission_provider::pending_tours(std::stop_token aStop) -> std::expected<std::vector<application::staged_review_plan>, application::review_mutation_failure> { return Session.pending_tours(aStop); }
	auto live_review_submission_provider::publish_tour(const application::staged_review_plan& aPlan, std::stop_token aStop, application::staged_child_authorizer aAuthorize) -> application::staged_publication_result
	{
		static_cast<void>(aAuthorize);
		// Pending mutation endpoints require separate host/transport qualification.
		static_cast<void>(aPlan);
		static_cast<void>(aStop);
		return { };
	}
	auto live_review_submission_provider::reconcile_tour(const application::staged_review_plan& aPlan, std::stop_token aStop) -> application::staged_publication_result { return Session.reconcile_tour(aPlan, aStop); }

	auto make_live_review_submission_provider(
		application::review_mutation_transport                        aLease,
		application::review_mutation_target_binding                   aTarget,
		application::review_mutation_acting_identity                  aActingIdentity,
		std::unique_ptr<application::review_submission_recovery_port> aRecovery) noexcept
		-> std::expected<
			std::unique_ptr<live_review_submission_provider>,
			application::review_mutation_failure>
	{
		if (aRecovery == nullptr) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		auto admission = detail::admit_review_mutation_provider(
			std::move(aLease),
			std::move(aTarget),
			std::move(aActingIdentity));
		if (!admission.has_value()) {
			return std::unexpected(admission.error());
		}
		auto* provider = new (std::nothrow) live_review_submission_provider{
			std::move(admission->Lease),
			*admission->Transport,
			std::move(admission->Configuration),
			std::move(aRecovery),
		};
		if (provider == nullptr) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		return std::unique_ptr<live_review_submission_provider>{ provider };
	}
} // namespace bpr::adapters::github
