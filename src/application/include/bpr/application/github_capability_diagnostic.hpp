#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_CAPABILITY_DIAGNOSTIC_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_CAPABILITY_DIAGNOSTIC_HPP

#include <cstdint>

namespace bpr::application {
	/// Fixed source-defined refusal categories; no remote values or text.
	enum class github_capability_diagnostic : std::uint8_t {
		none,
		unavailable_detail,
		transport_content_encoding,
		transport_duplicate_metadata,
		transport_invalid_metadata,
		transport_missing_rate_tuple,
		transport_partial_rate_tuple,
		transport_wrong_rate_resource,
		transport_invalid_content_type,
		transport_header_limit,
		transport_body_limit,
		transport_body_length,
		normalizer_invalid_limits,
		normalizer_transport_unavailable,
		normalizer_rate_metadata,
		normalizer_body_limit,
		json_limits,
		json_shape,
		graphql_errors,
		graphql_data,
		graphql_aliases,
		schema_limits,
		schema_shape,
		normalization_allocation,
		normalization_unexpected,
		provider_request_compilation,
		provider_correlation,
		provider_metadata,
		provider_rate_tuple,
		provider_retry_metadata,
		provider_observation,
		host_epoch,
		host_cache_lookup,
		host_correlation,
		host_provider_error,
		host_rate_state,
		host_cache_stage,
	};

	[[nodiscard]] constexpr auto bounded_capability_diagnostic(
		const github_capability_diagnostic aValue) noexcept -> github_capability_diagnostic
	{
		return aValue > github_capability_diagnostic::none
				&& aValue <= github_capability_diagnostic::host_cache_stage
			? aValue
			: github_capability_diagnostic::unavailable_detail;
	}
}

#endif
