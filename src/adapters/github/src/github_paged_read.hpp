#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PAGED_READ_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PAGED_READ_HPP

#include "github_host_profile.hpp"
#include "github_wire_transport.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <variant>

namespace bpr::adapters::github::detail {
	struct github_paged_read_limits
	{
		application::github_read_limits Records;
		std::size_t                     MaximumPageBytes              = 4U * 1024U * 1024U;
		std::size_t                     MaximumAggregateResponseBytes = 64U * 1024U * 1024U;
		std::size_t                     MaximumTotalPages             = 1'024U;
		std::size_t                     MaximumPageRecords            = 100U;
		std::size_t                     MaximumCursorBytes            = 1'024U;

		auto operator<=>(const github_paged_read_limits&) const = default;
	};

	struct github_response_metadata
	{
		std::optional<std::string>                           Link;
		std::optional<application::github_rate_limit_record> RateLimit;
		std::optional<std::uint32_t>                         RetryAfterSeconds;

		auto operator<=>(const github_response_metadata&) const = default;
	};

	using github_response_metadata_result = std::expected<github_response_metadata, application::github_provider_error>;

	[[nodiscard]] auto
	parse_github_response_metadata(const wire_response& aResponse) noexcept
		-> github_response_metadata_result;

	[[nodiscard]] auto github_error_body_is_secondary_rate_limit(
		std::string_view aBody, std::size_t aMaximumBytes) noexcept -> bool;

	[[nodiscard]] auto github_graphql_error_body_is_rate_limited(
		std::string_view aBody, std::size_t aMaximumBytes) noexcept -> bool;

	struct observed_github_viewer
	{
		application::github_viewer_record    Viewer;
		application::github_rate_limit_state RateLimits;

		auto operator<=>(const observed_github_viewer&) const = default;
	};

	using observed_github_viewer_result = std::expected<observed_github_viewer, application::github_provider_error>;

	[[nodiscard]] auto normalize_observed_github_viewer(
		const wire_response&            aResponse,
		server_contract                 aServer = server_contract::github_com,
		const github_paged_read_limits& aLimits = { }) noexcept
		-> observed_github_viewer_result;

	struct github_paged_read_cancelled
	{
		auto operator<=>(const github_paged_read_cancelled&) const = default;
	};

	using github_paged_read_failure = std::variant<application::github_provider_error,
												   github_paged_read_cancelled>;

	using github_paged_read_result = std::expected<application::github_pull_request_snapshot,
												   github_paged_read_failure>;

	[[nodiscard]] auto perform_paged_github_read(
		github_wire_transport_port&                              aTransport,
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		core::provider_snapshot_id                               aSnapshot,
		std::stop_token                                          aStop,
		const github_paged_read_limits&                          aLimits = { }) noexcept
		-> github_paged_read_result;
}

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PAGED_READ_HPP
