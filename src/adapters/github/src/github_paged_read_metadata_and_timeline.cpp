#include "bpr/core/home_discovery_plan.hpp"
#include "bpr/core/json_preflight.hpp"
#include "github_http_helpers.hpp"
#include "github_paged_read.hpp"
#include "github_paged_read_page_wire.hpp"
#include "github_paged_read_record_parsing.hpp"
#include "github_review_comment_grouping.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <new>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail {
	namespace {
		constexpr std::size_t maximum_header_name_bytes = 256U;

		constexpr std::uint64_t maximum_retry_after_seconds = 86'400U;

		/// [[BPR-1439]]. Spelled with its length: a `"\r\n\0"` literal ends at
		/// the NUL, so a header carrying one used to pass.
		constexpr std::string_view header_breaks{ "\r\n\0", 3U };

		enum class metadata_header : std::uint8_t {
			content_type,
			link,
			limit,
			remaining,
			reset,
			resource,
			retry_after,
			count,
		};

		[[nodiscard]] auto metadata_kind(const std::string_view aName) noexcept
			-> std::optional<metadata_header>
		{
			if (ascii_equal_fold(aName, "Content-Type")) {
				return metadata_header::content_type;
			}
			if (ascii_equal_fold(aName, "Link")) {
				return metadata_header::link;
			}
			if (ascii_equal_fold(aName, "X-RateLimit-Limit")) {
				return metadata_header::limit;
			}
			if (ascii_equal_fold(aName, "X-RateLimit-Remaining")) {
				return metadata_header::remaining;
			}
			if (ascii_equal_fold(aName, "X-RateLimit-Reset")) {
				return metadata_header::reset;
			}
			if (ascii_equal_fold(aName, "X-RateLimit-Resource")) {
				return metadata_header::resource;
			}
			if (ascii_equal_fold(aName, "Retry-After")) {
				return metadata_header::retry_after;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto parse_metadata_impl(const wire_response& aResponse)
			-> github_response_metadata_result
		{
			std::array<bool, static_cast<std::size_t>(metadata_header::count)> seen{ };
			std::optional<std::string>                                         contentType;
			std::optional<std::uint64_t>                                       limit;
			std::optional<std::uint64_t>                                       remaining;
			std::optional<std::uint64_t>                                       reset;
			std::optional<application::github_rate_limit_resource>             resource;
			std::optional<std::uint64_t>                                       retryAfter;
			std::optional<std::string>                                         link;

			for (const auto& header : aResponse.Headers) {
				if (header.Name.empty() || header.Name.size() > maximum_header_name_bytes
					|| header.Value.size() > maximum_header_value_bytes
					|| header.Name.find_first_of(header_breaks) != std::string::npos
					|| header.Value.find_first_of(header_breaks) != std::string::npos) {
					return std::unexpected{ provider_error(
						application::github_provider_failure::malformed_response) };
				}
				const auto kind = metadata_kind(header.Name);
				if (!kind.has_value()) {
					return std::unexpected{ provider_error(
						application::github_provider_failure::malformed_response) };
				}
				const auto index = static_cast<std::size_t>(*kind);
				if (seen[index]) {
					return std::unexpected{ provider_error(
						application::github_provider_failure::malformed_response) };
				}
				seen[index] = true;

				switch (*kind) {
				case metadata_header::content_type:
					contentType = std::string{ trim_ows(header.Value) };
					break;
				case metadata_header::link:
					link = std::string{ trim_ows(header.Value) };
					break;
				case metadata_header::limit:
					limit = decimal(header.Value);
					break;
				case metadata_header::remaining:
					remaining = decimal(header.Value);
					break;
				case metadata_header::reset:
					reset = decimal(header.Value);
					break;
				case metadata_header::resource: {
					const auto value = trim_ows(header.Value);
					if (value == "core") {
						resource = application::github_rate_limit_resource::core;
					} else if (value == "search") {
						resource = application::github_rate_limit_resource::search;
					} else if (value == "graphql") {
						resource = application::github_rate_limit_resource::graphql;
					}
					break;
				}
				case metadata_header::retry_after:
					retryAfter = decimal(header.Value);
					break;
				case metadata_header::count:
					return std::unexpected{ provider_error(
						application::github_provider_failure::malformed_response) };
				}
			}

			const auto rateHeaderCount = static_cast<std::size_t>(
											 seen[static_cast<std::size_t>(metadata_header::limit)])
				+ static_cast<std::size_t>(
											 seen[static_cast<std::size_t>(metadata_header::remaining)])
				+ static_cast<std::size_t>(
											 seen[static_cast<std::size_t>(metadata_header::reset)])
				+ static_cast<std::size_t>(
											 seen[static_cast<std::size_t>(metadata_header::resource)]);
			if (!contentType.has_value() || !json_media_type(*contentType)
				|| (rateHeaderCount != 0U && rateHeaderCount != 4U)
				|| (rateHeaderCount == 4U
					&& (!limit.has_value() || !remaining.has_value()
						|| !reset.has_value() || !resource.has_value()
						|| *remaining > *limit))
				|| (seen[static_cast<std::size_t>(metadata_header::retry_after)]
					&& !retryAfter.has_value())
				|| (retryAfter.has_value()
					&& *retryAfter > maximum_retry_after_seconds)
				|| (link.has_value() && link->empty())) {
				return std::unexpected{ provider_error(
					application::github_provider_failure::malformed_response) };
			}

			return github_response_metadata{
				.Link              = std::move(link),
				.RateLimit         = rateHeaderCount == 4U
					? std::optional<
						  application::
							  github_rate_limit_record>{ application::
															 github_rate_limit_record{
																 .Resource         = *resource,
																 .Limit            = *limit,
																 .Remaining        = *remaining,
																 .ResetUnixSeconds = *reset,
																 .SecondaryLimited = false,
															 } }
					: std::nullopt,
				.RetryAfterSeconds = retryAfter.has_value()
					? std::optional{ static_cast<std::uint32_t>(*retryAfter) }
					: std::nullopt,
			};
		}
	}

	auto parse_github_response_metadata(const wire_response& aResponse) noexcept
		-> github_response_metadata_result
	{
		try {
			return parse_metadata_impl(aResponse);
		} catch (const std::bad_alloc&) {
			return std::unexpected{ provider_error(
				application::github_provider_failure::response_limit) };
		} catch (...) {
			return std::unexpected{ provider_error(
				application::github_provider_failure::malformed_response) };
		}
	}

	auto github_error_body_is_secondary_rate_limit(
		const std::string_view aBody, const std::size_t aMaximumBytes) noexcept
		-> bool
	{
		if (aBody.empty() || aBody.size() > aMaximumBytes) {
			return false;
		}
		try {
			const auto preflight = json_structural_preflight{ aBody, aMaximumBytes }.run();
			if (preflight != json_preflight_result::valid) {
				return false;
			}
			const auto document = parse_json_document(aBody);
			return document.has_value() && secondary_rate_limit_message(&*document);
		} catch (...) {
			return false;
		}
	}

	auto github_graphql_error_body_is_rate_limited(
		const std::string_view aBody, const std::size_t aMaximumBytes) noexcept
		-> bool
	{
		if (aBody.empty() || aBody.size() > aMaximumBytes) {
			return false;
		}
		try {
			const auto preflight = json_structural_preflight{ aBody, aMaximumBytes }.run();
			if (preflight != json_preflight_result::valid) {
				return false;
			}
			const auto document = parse_json_document(aBody);
			return document.has_value() && json_is_object(*document)
				&& graphql_error_rate_limited(json_member(json_object(*document), "errors"));
		} catch (...) {
			return false;
		}
	}

	auto normalize_observed_github_viewer(
		const wire_response&            aResponse,
		const server_contract           aServer,
		const github_paged_read_limits& aLimits) noexcept
		-> observed_github_viewer_result
	{
		try {
			if (!limits_valid(aLimits)) {
				return std::unexpected{ provider_error(
					application::github_provider_failure::response_limit) };
			}
			const auto bytes = response_byte_count(aResponse);
			if (!bytes.has_value() || *bytes > aLimits.MaximumPageBytes
				|| *bytes > aLimits.MaximumAggregateResponseBytes) {
				return std::unexpected{ provider_error(
					application::github_provider_failure::response_limit) };
			}
			const auto metadata = parse_github_response_metadata(aResponse);
			if (!metadata.has_value()) {
				return std::unexpected{ metadata.error() };
			}
			if ((!metadata->RateLimit.has_value()
				 && aServer == server_contract::github_com)
				|| (metadata->RateLimit.has_value()
					&& metadata->RateLimit->Resource
						!= application::github_rate_limit_resource::core)) {
				return std::unexpected{ provider_error(
					application::github_provider_failure::malformed_response) };
			}
			auto rateLimits = application::github_rate_limit_state{ };
			if (metadata->RateLimit.has_value()) {
				static_cast<void>(
					application::observe(rateLimits, *metadata->RateLimit));
			}
			const auto metadataError =
				[&rateLimits](const application::github_provider_failure aFailure) { return provider_error(aFailure, std::nullopt, rateLimits); };
			if (aResponse.Status != 200) {
				return std::unexpected{ status_failure(
					aResponse.Status,
					*metadata,
					(aResponse.Status == 403 || aResponse.Status == 429)
						&& github_error_body_is_secondary_rate_limit(
							aResponse.Body, aLimits.MaximumPageBytes)) };
			}
			if (metadata->RetryAfterSeconds.has_value() || metadata->Link.has_value()
				|| aResponse.Completeness != page_completeness::complete
				|| aResponse.Body.empty()) {
				return std::unexpected{ metadataError(
					application::github_provider_failure::malformed_response) };
			}
			const auto preflight = json_structural_preflight{ aResponse.Body, aLimits.MaximumPageBytes }
									   .run();
			if (preflight == json_preflight_result::structural_limit) {
				return std::unexpected{ metadataError(
					application::github_provider_failure::response_limit) };
			}
			if (preflight != json_preflight_result::valid) {
				return std::unexpected{ metadataError(
					application::github_provider_failure::malformed_response) };
			}
			const auto parsed = parse_json_document(aResponse.Body);
			if (!parsed.has_value()) {
				return std::unexpected{ metadataError(application::github_provider_failure::malformed_response) };
			}
			const auto& document = *parsed;
			if (!json_is_object(document)
				|| !has_required_keys(json_object(document),
									  { "id", "login", "name", "node_id" })) {
				return std::unexpected{ metadataError(
					application::github_provider_failure::malformed_response) };
			}
			const auto object = json_object(document);
			if (!exact_integer(json_member(object, "id"), true)
				|| !nonempty_string(json_member(object, "login"))
				|| !nonempty_string(json_member(object, "node_id"))
				|| (!json_is_null(json_member(object, "name"))
					&& !json_is_string(json_member(object, "name")))) {
				return std::unexpected{ metadataError(
					application::github_provider_failure::malformed_response) };
			}
			const auto node  = text(json_member(object, "node_id"));
			const auto login = text(json_member(object, "login"));
			const auto name  = json_is_null(json_member(object, "name"))
				? std::string{ }
				: text(json_member(object, "name"));
			if (!bounded_text(node, aLimits.Records.MaximumIdentityBytes, true)
				|| !bounded_text(login, aLimits.Records.MaximumLoginBytes, true)
				|| !bounded_text(name, aLimits.Records.MaximumDisplayNameBytes)) {
				return std::unexpected{ metadataError(
					application::github_provider_failure::response_limit) };
			}
			return observed_github_viewer{
				.Viewer = application::github_viewer_record{
					.Account     = core::github_account_id{ integer(json_member(object, "id")) },
					.Node        = application::github_actor_id{ node },
					.Login       = login,
					.DisplayName = name,
				},
				.RateLimits = std::move(rateLimits),
			};
		} catch (const std::bad_alloc&) {
			return std::unexpected{ provider_error(
				application::github_provider_failure::response_limit) };
		} catch (...) {
			return std::unexpected{ provider_error(
				application::github_provider_failure::malformed_response) };
		}
	}

	auto perform_paged_github_timeline_read(
		github_wire_transport_port&                              aTransport,
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		const core::repository_id                                aRepository,
		const std::stop_token                                    aStop,
		const github_paged_read_limits&                          aLimits,
		const github_provider_read_authority                     aReadAuthority,
		github_paged_response_observer*                          aObserver) noexcept -> github_timeline_read_result
	{
		auto rates = application::github_rate_limit_state{ };
		try {
			if (aReadAuthority == github_provider_read_authority::anonymous_public) {
				const auto admission = admit_anonymous_read(read_operation::list_pull_request_timeline, aProfile.server_contract());
				if (!admission) {
					return failed(provider_failure_for(admission.error()));
				}
			}
			if (!provider_request_valid(aProfile, aRequest, core::provider_snapshot_id{ 1U }, aLimits, aReadAuthority)
				|| aRepository.value() == 0U
				|| (aRequest.Context.ExpectedRepository && *aRequest.Context.ExpectedRepository != aRepository)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto limits              = aLimits;
			limits.MaximumTotalPages = std::min(limits.MaximumTotalPages, std::size_t{ 10U });
			production_read_execution execution{ aTransport, aProfile, aRequest.Context, limits, aStop, rates, aRequest.AuthenticationSession, aObserver };
			if (!execution.bind_repository(aRepository)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto                              semantic = semantic_request_for(*server_contract_for(aProfile), aRequest.Context, read_operation::list_pull_request_timeline);
			github_timeline_read              result;
			std::unordered_set<std::uint64_t> seen;
			while (true) {
				auto page = execution.fetch(semantic);
				if (!page) {
					return std::unexpected{ page.error() };
				}
				if (!json_is_array(page->Document)) {
					return execution.terminal_failure(application::github_provider_failure::malformed_response);
				}
				const auto records = json_array(page->Document);
				if (records.size() > limits.MaximumPageRecords) {
					return execution.terminal_failure(application::github_provider_failure::response_limit);
				}
				for (const auto& value : records) {
					if (!json_is_object(value)) {
						return execution.terminal_failure(application::github_provider_failure::malformed_response);
					}
					const auto object = json_object(value);
					if (!nonempty_string(json_member(object, "event"))) {
						return execution.terminal_failure(application::github_provider_failure::malformed_response);
					}
					if (text(json_member(object, "event")) != "head_ref_force_pushed") {
						continue;
					}
					const auto id      = json_member(object, "id");
					const auto oid     = json_member(object, "commit_id");
					const auto created = json_member(object, "created_at");
					if (!exact_integer(id, true) || !hexadecimal_oid(oid) || !json_is_string(created)) {
						return execution.terminal_failure(application::github_provider_failure::malformed_response);
					}
					const auto timestamp = parse_timeline_timestamp(text(created));
					if (!timestamp || !seen.insert(integer(id)).second) {
						return execution.terminal_failure(application::github_provider_failure::malformed_response);
					}
					result.ForcePushEvents.push_back({ .EventId = integer(id), .AfterHeadOid = text(oid), .CreatedAtUnixMilliseconds = *timestamp });
				}
				const auto next = rest_has_next_page(aProfile, aRepository, *page, records.size());
				if (!next) {
					return std::unexpected{ execution.enrich(next.error()) };
				}
				if (!*next) {
					result.RateLimits = rates;
					return result;
				}
				++semantic.Page;
			}
		} catch (...) {
			return failed(application::github_provider_failure::response_limit, std::nullopt, rates);
		}
	}
}
