#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_DESIGNATED_LIVE_PUBLIC_OBSERVER_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_DESIGNATED_LIVE_PUBLIC_OBSERVER_HPP

#include "bpr/application/review_mutation_port.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <stop_token>

namespace bpr::adapters::github::detail {
	enum class designated_live_public_observation_failure : std::uint8_t {
		unavailable,
		network,
		timeout,
		response_limit,
		redirect,
		http,
		malformed,
		identity_mismatch,
		cancelled,
	};

	using designated_live_public_observation = std::expected<
		application::review_mutation_target_binding,
		designated_live_public_observation_failure>;

	[[nodiscard]] auto observe_designated_live_public_target(
		std::stop_token aStop = { })
		-> designated_live_public_observation;

#if defined(BPR_DESIGNATED_LIVE_PUBLIC_OBSERVER_TESTING)
	struct designated_live_public_observer_test_configuration
	{
		std::uint16_t         Port = 0U;
		std::filesystem::path CertificateAuthority;
	};

	[[nodiscard]] auto observe_designated_live_public_target_for_test(
		const designated_live_public_observer_test_configuration& aConfiguration,
		std::stop_token                                           aStop = { })
		-> designated_live_public_observation;
#endif
} // namespace bpr::adapters::github::detail

#endif
