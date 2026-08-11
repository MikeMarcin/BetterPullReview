#include "live_review_mutation_provider.hpp"

#include "review_mutation_provider_admission.hpp"

#include <new>
#include <utility>

namespace bpr::adapters::github {
	live_review_mutation_provider::live_review_mutation_provider(
		application::review_mutation_transport     aLease,
		detail::github_mutation_wire_transport&    aTransport,
		detail::live_review_mutation_configuration aConfiguration) noexcept
		: Session{ aTransport, std::move(aConfiguration), std::move(aLease) }
	{
	}

	auto live_review_mutation_provider::observe_target()
		-> std::expected<core::review_mutation_target, application::review_mutation_failure>
	{
		return Session.observe_target();
	}

	auto live_review_mutation_provider::submit(
		const application::review_mutation_request& aRequest)
		-> std::expected<application::review_mutation_receipt, application::review_mutation_failure>
	{
		return Session.submit(aRequest);
	}

	auto live_review_mutation_provider::capabilities() const
		-> application::review_provider_capabilities
	{
		return {
			.ResolvesThreads    = false,
			.RecordsViewedState = false,
		};
	}

	auto make_live_review_mutation_provider(
		application::review_mutation_transport       aLease,
		application::review_mutation_target_binding  aTarget,
		application::review_mutation_acting_identity aActingIdentity) noexcept
		-> std::expected<
			std::unique_ptr<live_review_mutation_provider>,
			application::review_mutation_failure>
	{
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
		};
		if (provider == nullptr) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		return std::unique_ptr<live_review_mutation_provider>{ provider };
	}
} // namespace bpr::adapters::github
