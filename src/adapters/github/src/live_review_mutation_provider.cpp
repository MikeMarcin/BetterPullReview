#include "live_review_mutation_provider.hpp"

#include "review_mutation_provider_admission.hpp"

#include <new>
#include <utility>

namespace bpr::adapters::github {
	live_review_mutation_provider::live_review_mutation_provider(
		application::review_mutation_transport                      aLease,
		detail::github_mutation_wire_transport&                     aTransport,
		detail::live_review_mutation_configuration                  aConfiguration,
		std::unique_ptr<application::review_mutation_recovery_port> aRecovery, application::review_publication_lineage_port* aLineage, std::shared_ptr<application::diagnostic_log_port> aLog) noexcept
		: Session{ aTransport,
				   std::move(aConfiguration),
				   std::move(aLease),
				   std::move(aRecovery), std::nullopt, aLineage, std::move(aLog) }
	{
	}

	auto live_review_mutation_provider::observe_target()
		-> std::expected<core::review_mutation_target, application::review_mutation_failure>
	{
		return observe_target(std::stop_token{ });
	}

	auto live_review_mutation_provider::observe_target(const std::stop_token aStop)
		-> std::expected<core::review_mutation_target, application::review_mutation_failure>
	{
		return Session.observe_target(aStop);
	}

	auto live_review_mutation_provider::submit(
		const application::review_mutation_request& aRequest)
		-> std::expected<application::review_mutation_receipt, application::review_mutation_failure>
	{
		return submit(aRequest, std::stop_token{ });
	}

	auto live_review_mutation_provider::submit(
		const application::review_mutation_request& aRequest,
		const std::stop_token                       aStop)
		-> std::expected<application::review_mutation_receipt, application::review_mutation_failure>
	{
		return Session.submit(aRequest, aStop);
	}

	auto live_review_mutation_provider::submit_and_reconcile(
		const application::review_mutation_request& aRequest,
		const std::stop_token                       aStop)
		-> application::review_mutation_submission
	{
		return Session.submit_and_reconcile(aRequest, aStop);
	}

	auto live_review_mutation_provider::resolve_thread(
		const application::review_thread_resolution_request& aRequest,
		const std::stop_token                                aStop)
		-> std::expected<application::review_thread_resolution_receipt, application::review_mutation_failure>
	{
		return Session.resolve_thread(aRequest, aStop);
	}

	auto live_review_mutation_provider::set_reaction(
		const application::review_reaction_request& aRequest,
		const std::stop_token                       aStop)
		-> std::expected<application::review_reaction_receipt, application::review_mutation_failure>
	{
		return Session.set_reaction(aRequest, aStop);
	}

	auto live_review_mutation_provider::reconcile_publication(const application::review_mutation_request& aRequest, std::stop_token aStop) -> application::review_mutation_submission
	{
		return Session.reconcile_publication(aRequest, aStop);
	}
	void live_review_mutation_provider::repair_pending_publications(std::stop_token aStop)
	{
		Session.repair_pending_publications(aStop);
	}
	auto live_review_mutation_provider::capabilities() const
		-> application::review_provider_capabilities
	{
		// [[BPR-0657]]. Threads resolve through the leased GraphQL mutation;
		// viewed state is still not recorded.
		// [[BPR-1741]]. Reactions are added and removed the same way.
		return {
			.ResolvesThreads    = true,
			.RecordsViewedState = false,
			.Reacts             = true,
		};
	}

	auto make_live_review_mutation_provider(
		application::review_mutation_transport                      aLease,
		application::review_mutation_target_binding                 aTarget,
		application::review_mutation_acting_identity                aActingIdentity,
		std::unique_ptr<application::review_mutation_recovery_port> aRecovery, application::review_publication_lineage_port* aLineage, std::shared_ptr<application::diagnostic_log_port> aLog) noexcept
		-> std::expected<
			std::unique_ptr<live_review_mutation_provider>,
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
		auto* provider = new (std::nothrow) live_review_mutation_provider{
			std::move(admission->Lease),
			*admission->Transport,
			std::move(admission->Configuration),
			std::move(aRecovery),
			aLineage,
			std::move(aLog),
		};
		if (provider == nullptr) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		return std::unique_ptr<live_review_mutation_provider>{ provider };
	}
} // namespace bpr::adapters::github
