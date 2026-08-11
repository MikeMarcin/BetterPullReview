#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_REVIEW_MUTATION_PROVIDER_ADMISSION_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_REVIEW_MUTATION_PROVIDER_ADMISSION_HPP

#include "github_mutation_contract.hpp"
#include "github_provider.hpp"

#include <expected>
#include <memory>

namespace bpr::adapters::github::detail {
	struct review_mutation_provider_admission
	{
		std::shared_ptr<github_review_mutation_transport_lease> Lease;
		github_mutation_wire_transport*                         Transport = nullptr;
		live_review_mutation_configuration                      Configuration;
	};

	/// \brief Admits one exact active lease, target, and acting identity.
	[[nodiscard]] auto admit_review_mutation_provider(
		application::review_mutation_transport       aLease,
		application::review_mutation_target_binding  aTarget,
		application::review_mutation_acting_identity aActingIdentity) noexcept
		-> std::expected<
			review_mutation_provider_admission,
			application::review_mutation_failure>;
} // namespace bpr::adapters::github::detail

#endif
