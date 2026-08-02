#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_WIRE_REQUEST_VALIDATION_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_WIRE_REQUEST_VALIDATION_HPP

#include "github_host_profile.hpp"
#include "github_wire_transport.hpp"

#include <cstdint>
#include <expected>

namespace bpr::adapters::github::detail {
	enum class github_wire_request_validation_failure : std::uint8_t {
		profile_correlation_mismatch,
		server_contract_mismatch,
		invalid_wire_request,
	};

	[[nodiscard]] auto validate_github_wire_request(
		const validated_github_host_profile& aProfile,
		const github_wire_request&           aRequest)
		-> std::expected<void, github_wire_request_validation_failure>;

	[[nodiscard]] auto validate_github_capability_probe_wire_request(
		const validated_github_host_profile&        aProfile,
		const github_capability_probe_wire_request& aRequest)
		-> std::expected<void, github_wire_request_validation_failure>;
}

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_WIRE_REQUEST_VALIDATION_HPP
