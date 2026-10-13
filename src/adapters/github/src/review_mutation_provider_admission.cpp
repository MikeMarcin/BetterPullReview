#include "review_mutation_provider_admission.hpp"

#include <utility>

namespace bpr::adapters::github::detail {
	auto admit_review_mutation_provider(
		application::review_mutation_transport       aLease,
		application::review_mutation_target_binding  aTarget,
		application::review_mutation_acting_identity aActingIdentity) noexcept
		-> std::expected<
			review_mutation_provider_admission,
			application::review_mutation_failure>
	{
		if (aLease == nullptr || !aLease->active()) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		auto lease = std::dynamic_pointer_cast<github_review_mutation_transport_lease>(
			aLease);
		if (lease == nullptr) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		auto* transport = lease->mutation_transport();
		if (transport == nullptr) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		const auto& profile = lease->profile();
		const auto& context = lease->credential_context();
		if (profile.Authentication == application::github_authentication_transport::github_cli
			&& !lease->strict_cli()) {
			return std::unexpected(application::review_mutation_failure::unsupported);
		}
		if (!application::valid(profile) || !application::valid(context)
			|| context.Authority.Profile != profile.Profile
			|| context.Authority.ProfileRevision != profile.ProfileRevision
			|| context.Authority.Host != profile.Host
			|| context.Authority.Hostname != profile.Hostname
			|| context.Authority.ServerContract != profile.ServerContract
			|| context.Authority.Host != aTarget.Semantic.Host
			|| context.Authority.Hostname != aTarget.Hostname
			|| context.Authority.ServerContract != aTarget.ServerContract
			|| context.Authority.Account != aActingIdentity.Account) {
			return std::unexpected(
				application::review_mutation_failure::undesignated_target);
		}
		auto configuration = live_review_mutation_configuration{
			.CredentialContext = context,
			.Target            = std::move(aTarget),
			.ActingIdentity    = std::move(aActingIdentity),
		};
		if (!valid(configuration)) {
			return std::unexpected(
				application::review_mutation_failure::undesignated_target);
		}
		return review_mutation_provider_admission{
			.Lease         = std::move(lease),
			.Transport     = transport,
			.Configuration = std::move(configuration),
		};
	}
} // namespace bpr::adapters::github::detail
