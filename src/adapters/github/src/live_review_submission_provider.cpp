#include "live_review_submission_provider.hpp"

#include "review_mutation_provider_admission.hpp"

#include <new>
#include <utility>

namespace bpr::adapters::github {
	live_review_submission_provider::live_review_submission_provider(
		application::review_mutation_transport                        aLease,
		detail::github_mutation_wire_transport&                       aTransport,
		detail::live_review_mutation_configuration                    aConfiguration,
		std::unique_ptr<application::review_submission_recovery_port> aRecovery,
		std::shared_ptr<application::diagnostic_log_port>             aLog) noexcept
		: Session{ aTransport,
				   std::move(aConfiguration),
				   std::move(aLease),
				   std::move(aRecovery),
				   std::move(aLog) }
	{
	}

	auto live_review_submission_provider::pending(std::stop_token aStop) -> std::expected<std::vector<application::review_submission_request>, application::review_mutation_failure> { return Session.pending(aStop); }
	auto live_review_submission_provider::submit_and_reconcile(const application::review_submission_request& aRequest, std::stop_token aStop) -> application::review_submission_result { return Session.submit_and_reconcile(aRequest, aStop); }
	auto live_review_submission_provider::reconcile(const application::review_submission_request& aRequest, std::stop_token aStop) -> application::review_submission_result { return Session.reconcile(aRequest, aStop); }
	auto live_review_submission_provider::last_unknown(const std::string& aKey, std::stop_token aStop) -> std::optional<core::review_submission_unknown_diagnosis> { return Session.last_unknown(aKey, aStop); }
	auto live_review_submission_provider::admitted(const std::string& aKey, std::stop_token aStop) -> std::expected<std::optional<application::review_submission_request>, application::review_mutation_failure> { return Session.admitted(aKey, aStop); }
	auto live_review_submission_provider::pending_tours(std::stop_token aStop) -> std::expected<std::vector<application::staged_review_plan>, application::review_mutation_failure> { return Session.pending_tours(aStop); }
	/// [[BPR-1222]]. Owner amendment 2026-10-09 ("yeah switch it on, let's test
	/// it out for real"): live tour publication runs through the same session
	/// the fixtures and the BPR-Test course exercised. Each write still needs the
	/// plan's one-use child authority.
	auto live_review_submission_provider::publish_tour(const application::staged_review_plan& aPlan, std::stop_token aStop, application::staged_child_authorizer aAuthorize) -> application::staged_publication_result { return Session.publish_tour(aPlan, aStop, std::move(aAuthorize)); }
	auto live_review_submission_provider::reconcile_tour(const application::staged_review_plan& aPlan, std::stop_token aStop) -> application::staged_publication_result { return Session.reconcile_tour(aPlan, aStop); }

	auto make_live_review_submission_provider(
		application::review_mutation_transport                        aLease,
		application::review_mutation_target_binding                   aTarget,
		application::review_mutation_acting_identity                  aActingIdentity,
		std::unique_ptr<application::review_submission_recovery_port> aRecovery,
		std::shared_ptr<application::diagnostic_log_port>             aLog) noexcept
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
			std::move(aLog),
		};
		if (provider == nullptr) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		return std::unique_ptr<live_review_submission_provider>{ provider };
	}
} // namespace bpr::adapters::github
