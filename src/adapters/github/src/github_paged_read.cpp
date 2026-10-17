#include "github_paged_read.hpp"
#include "bpr/application/review_queue_facts.hpp"
#include "bpr/core/json_preflight.hpp"
#include "github_http_helpers.hpp"
#include "github_review_comment_grouping.hpp"

#include <QString>

#include <glaze/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace bpr::adapters::github::detail {
	namespace {
		constexpr std::size_t   maximum_header_name_bytes   = 256U;
		constexpr std::size_t   maximum_header_value_bytes  = 8U * 1024U;
		constexpr std::uint64_t maximum_retry_after_seconds = 86'400U;
		/// [[BPR-1439]]. Spelled with its length: a `"\r\n\0"` literal ends at
		/// the NUL, so a header carrying one used to pass.
		constexpr std::string_view header_breaks{ "\r\n\0", 3U };

		constexpr std::size_t   maximum_production_page_bytes      = 4U * 1024U * 1024U;
		constexpr std::size_t   maximum_production_aggregate_bytes = 64U * 1024U * 1024U;
		constexpr std::size_t   maximum_production_pages           = 1'024U;
		constexpr std::size_t   maximum_production_page_records    = 100U;
		constexpr std::size_t   maximum_production_cursor_bytes    = 1'024U;
		constexpr std::uint32_t maximum_graphql_sequence           = 1'024U;

		struct page_json_options : glz::opts
		{
			// Glaze discovers these option overrides by their exact library-defined names.
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool escape_control_characters = true;
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool validate_skipped = true;
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool validate_trailing_whitespace = true;
			constexpr page_json_options()
			{
				null_terminated       = false;
				error_on_missing_keys = true;
			}
		};

		[[nodiscard]] auto provider_error(
			const application::github_provider_failure           aFailure,
			const std::optional<std::uint32_t>                   aRetryAfter     = std::nullopt,
			application::github_rate_limit_state                 aRateLimits     = { },
			std::optional<application::github_rate_limit_record> aRateLimitCause = std::nullopt) -> application::github_provider_error
		{
			return {
				.Failure           = aFailure,
				.RetryAfterSeconds = aRetryAfter,
				.RateLimits        = std::move(aRateLimits),
				.RateLimitCause    = std::move(aRateLimitCause),
			};
		}

		[[nodiscard]] auto
		failed(const application::github_provider_failure           aFailure,
			   const std::optional<std::uint32_t>                   aRetryAfter     = std::nullopt,
			   application::github_rate_limit_state                 aRateLimits     = { },
			   std::optional<application::github_rate_limit_record> aRateLimitCause = std::nullopt) -> std::unexpected<github_paged_read_failure>
		{
			return std::unexpected{
				github_paged_read_failure{ provider_error(aFailure,
														  aRetryAfter,
														  std::move(aRateLimits),
														  std::move(aRateLimitCause)) }
			};
		}

		[[nodiscard]] auto failed(const application::github_provider_error& aError)
			-> std::unexpected<github_paged_read_failure>
		{
			return std::unexpected{ github_paged_read_failure{ aError } };
		}

		[[nodiscard]] auto
		rate_limit_state(const application::github_rate_limit_record& aRecord) noexcept
			-> application::github_rate_limit_state
		{
			auto state = application::github_rate_limit_state{ };
			static_cast<void>(application::observe(state, aRecord));
			return state;
		}

		[[nodiscard]] auto cancelled() -> std::unexpected<github_paged_read_failure>
		{
			return std::unexpected{
				github_paged_read_failure{ github_paged_read_cancelled{ } }
			};
		}

		[[nodiscard]] auto ascii_fold(const char aValue) noexcept -> char
		{
			return aValue >= 'A' && aValue <= 'Z' ? static_cast<char>(aValue - 'A' + 'a')
												  : aValue;
		}

		[[nodiscard]] auto ascii_equal_fold(const std::string_view aLeft,
											const std::string_view aRight) noexcept
			-> bool
		{
			return aLeft.size() == aRight.size()
				&& std::ranges::equal(aLeft,
									  aRight,
									  [](const char a_, const char aB) { return ascii_fold(a_) == ascii_fold(aB); });
		}

		[[nodiscard]] auto trim_ows(std::string_view aValue) noexcept -> std::string_view
		{
			while (!aValue.empty() && (aValue.front() == ' ' || aValue.front() == '\t')) {
				aValue.remove_prefix(1U);
			}
			while (!aValue.empty() && (aValue.back() == ' ' || aValue.back() == '\t')) {
				aValue.remove_suffix(1U);
			}
			return aValue;
		}

		[[nodiscard]] auto decimal(std::string_view aValue) noexcept
			-> std::optional<std::uint64_t>
		{
			aValue = trim_ows(aValue);
			if (aValue.empty() || aValue.size() > 32U) {
				return std::nullopt;
			}
			std::uint64_t result = 0U;
			const auto    parsed = std::from_chars(aValue.data(), aValue.data() + aValue.size(), result);
			if (parsed.ec != std::errc{ } || parsed.ptr != aValue.data() + aValue.size()) {
				return std::nullopt;
			}
			return result;
		}

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

		using json_preflight_result = core::json_preflight_result;

		/// [[BPR-1433]]. The shared core check with the paged read's ceilings.
		class json_structural_preflight
		{
		public:
			json_structural_preflight(const std::string_view aInput, const std::size_t aMaximumStringBytes) noexcept
				: Input{ aInput }
				, MaximumStringBytes{ aMaximumStringBytes }
			{
			}

			[[nodiscard]] auto run() const noexcept -> json_preflight_result
			{
				return core::json_preflight(Input, { .Depth = 32U, .ObjectMembers = 256U, .ArrayElements = 10'000U, .Values = 1'000'000U, .StringBytes = MaximumStringBytes }).Result;
			}

		private:
			std::string_view Input;
			std::size_t      MaximumStringBytes;
		};

		// A fetched page outlives its transport completion. Every indexed child
		// shares the immutable body, including copied GraphQL connection nodes.
		// Only Glaze parses JSON; the checks below retain missing versus null.
		struct page_json_value
		{
			std::shared_ptr<const std::string> Owner;
			std::string_view                   Json;
		};
		using page_json_object = std::map<std::string, page_json_value, std::less<>>;
		using page_json_array  = std::vector<page_json_value>;

		[[nodiscard]] auto json_tag(const page_json_value& aValue) noexcept -> char
		{
			const auto begin = aValue.Json.find_first_not_of(" \t\r\n");
			return begin == std::string_view::npos ? '\0' : aValue.Json[begin];
		}
		[[nodiscard]] auto json_is_object(const page_json_value& aValue) noexcept -> bool { return json_tag(aValue) == '{'; }
		[[nodiscard]] auto json_is_array(const page_json_value& aValue) noexcept -> bool { return json_tag(aValue) == '['; }
		[[nodiscard]] auto json_is_string(const page_json_value& aValue) noexcept -> bool { return json_tag(aValue) == '"'; }
		[[nodiscard]] auto json_is_null(const page_json_value& aValue) noexcept -> bool { return json_tag(aValue) == 'n'; }
		[[nodiscard]] auto json_is_undefined(const page_json_value& aValue) noexcept -> bool { return aValue.Json.empty(); }
		[[nodiscard]] auto json_is_bool(const page_json_value& aValue) noexcept -> bool { return json_tag(aValue) == 't' || json_tag(aValue) == 'f'; }
		[[nodiscard]] auto json_is_number(const page_json_value& aValue) noexcept -> bool
		{
			const auto tag = json_tag(aValue);
			return tag == '-' || (tag >= '0' && tag <= '9');
		}
		[[nodiscard]] auto parse_json_document(const std::string_view aBody) -> std::optional<page_json_value>
		{
			glz::skip validated;
			if (glz::read<page_json_options{ }>(validated, aBody)) {
				return std::nullopt;
			}
			const auto owner  = std::make_shared<const std::string>(aBody);
			auto       result = page_json_value{ owner, *owner };
			return json_is_object(result) || json_is_array(result)
				? std::optional<page_json_value>{ std::move(result) }
				: std::nullopt;
		}
		[[nodiscard]] auto json_object(const page_json_value& aValue) -> page_json_object
		{
			std::map<std::string, glz::raw_json_view, std::less<>> values;
			if (!json_is_object(aValue) || glz::read<page_json_options{ }>(values, aValue.Json)) {
				return { };
			}
			page_json_object result;
			for (const auto& [key, value] : values) {
				result.emplace(key, page_json_value{ aValue.Owner, value.str });
			}
			return result;
		}
		[[nodiscard]] auto json_array(const page_json_value& aValue) -> page_json_array
		{
			std::vector<glz::raw_json_view> values;
			if (!json_is_array(aValue) || glz::read<page_json_options{ }>(values, aValue.Json)) {
				return { };
			}
			page_json_array result;
			result.reserve(values.size());
			for (const auto& value : values) {
				result.push_back(page_json_value{ aValue.Owner, value.str });
			}
			return result;
		}
		[[nodiscard]] auto json_member(const page_json_object& aObject, const std::string_view aKey) -> page_json_value
		{
			const auto found = aObject.find(aKey);
			return found == aObject.end() ? page_json_value{ } : found->second;
		}
		[[nodiscard]] auto text(const page_json_value& aValue) -> std::string
		{
			std::string result;
			return json_is_string(aValue) && !glz::read<page_json_options{ }>(result, aValue.Json) ? result : std::string{ };
		}
		[[nodiscard]] auto json_text(const page_json_value& aValue) -> QString
		{
			return QString::fromStdString(text(aValue));
		}
		[[nodiscard]] auto json_bool(const page_json_value& aValue) -> bool
		{
			bool result = false;
			return json_is_bool(aValue) && !glz::read<page_json_options{ }>(result, aValue.Json) && result;
		}
		[[nodiscard]] auto exact_integer(const page_json_value& aValue, const bool aPositive = false) noexcept -> bool
		{
			std::uint64_t result = 0U;
			return !glz::read<page_json_options{ }>(result, aValue.Json) && (!aPositive || result != 0U);
		}
		[[nodiscard]] auto integer(const page_json_value& aValue) noexcept -> std::uint64_t
		{
			std::uint64_t result = 0U;
			return !glz::read<page_json_options{ }>(result, aValue.Json) ? result : 0U;
		}

		[[nodiscard]] auto uint32(const page_json_value& aValue) noexcept
			-> std::optional<std::uint32_t>
		{
			if (!exact_integer(aValue)) {
				return std::nullopt;
			}
			const auto result = integer(aValue);
			if (result > std::numeric_limits<std::uint32_t>::max()) {
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(result);
		}

		[[nodiscard]] auto nonempty_string(const page_json_value& aValue) -> bool
		{
			return json_is_string(aValue) && !json_text(aValue).isEmpty();
		}

		[[nodiscard]] auto bounded_text(const std::string_view aValue,
										const std::size_t      aMaximum,
										const bool             aRequired = false) noexcept -> bool
		{
			return (!aRequired || !aValue.empty()) && aValue.size() <= aMaximum
				&& aValue.find('\0') == std::string_view::npos;
		}

		[[nodiscard]] auto sorted_keys(const page_json_object& aObject)
			-> std::vector<std::string>
		{
			std::vector<std::string> keys;
			keys.reserve(static_cast<std::size_t>(aObject.size()));
			for (auto iterator = aObject.cbegin(); iterator != aObject.cend();
				 ++iterator) {
				keys.emplace_back(iterator->first);
			}
			std::ranges::sort(keys);
			return keys;
		}

		[[nodiscard]] auto
		has_exact_keys(const page_json_object&                 aObject,
					   const std::span<const std::string_view> aExpected) -> bool
		{
			std::vector<std::string> keys;
			keys.reserve(aExpected.size());
			for (const auto key : aExpected) {
				keys.emplace_back(key);
			}
			std::ranges::sort(keys);
			return sorted_keys(aObject) == keys;
		}

		[[nodiscard]] auto
		has_exact_keys(const page_json_object&                       aObject,
					   const std::initializer_list<std::string_view> aExpected) -> bool
		{
			return has_exact_keys(
				aObject,
				std::span<const std::string_view>{ aExpected.begin(), aExpected.size() });
		}

		[[nodiscard]] auto
		has_required_keys(const page_json_object&                 aObject,
						  const std::span<const std::string_view> aRequired) -> bool
		{
			return std::ranges::all_of(
				aRequired,
				[&aObject](const std::string_view aKey) { return aObject.contains(aKey); });
		}

		[[nodiscard]] auto
		has_required_keys(const page_json_object&                       aObject,
						  const std::initializer_list<std::string_view> aRequired)
			-> bool
		{
			return has_required_keys(
				aObject,
				std::span<const std::string_view>{ aRequired.begin(), aRequired.size() });
		}

		[[nodiscard]] auto graphql_success_envelope(const page_json_object& aObject) -> bool
		{
			return has_exact_keys(aObject, { "data" })
				|| (has_exact_keys(aObject, { "data", "extensions" })
					&& json_is_object(json_member(aObject, "extensions")));
		}

		[[nodiscard]] auto one_of(const page_json_value&                        aValue,
								  const std::initializer_list<std::string_view> aAllowed)
			-> bool
		{
			if (!json_is_string(aValue)) {
				return false;
			}
			const auto candidate = text(aValue);
			return std::ranges::find(aAllowed, candidate) != aAllowed.end();
		}

		[[nodiscard]] auto hexadecimal_oid(const page_json_value& aValue) -> bool
		{
			if (!json_is_string(aValue)) {
				return false;
			}
			const auto candidate = text(aValue);
			return (candidate.size() == 40U || candidate.size() == 64U)
				&& std::ranges::all_of(
					   candidate,
					   [](const char aCharacter) {
						   return (aCharacter >= '0' && aCharacter <= '9')
							   || (aCharacter >= 'a' && aCharacter <= 'f')
							   || (aCharacter >= 'A' && aCharacter <= 'F');
					   });
		}

		[[nodiscard]] auto valid_line(const page_json_value& aValue) noexcept -> bool
		{
			return exact_integer(aValue, true)
				&& integer(aValue)
				<= application::maximum_github_line;
		}

		[[nodiscard]] auto optional_line(const page_json_value& aValue)
			-> std::optional<std::uint32_t>
		{
			return json_is_null(aValue) ? std::nullopt : uint32(aValue);
		}

		[[nodiscard]] auto response_byte_count(const wire_response& aResponse) noexcept
			-> std::optional<std::size_t>
		{
			auto total = aResponse.Body.size();
			for (const auto& header : aResponse.Headers) {
				if (header.Name.size() > std::numeric_limits<std::size_t>::max() - total) {
					return std::nullopt;
				}
				total += header.Name.size();
				if (header.Value.size()
					> std::numeric_limits<std::size_t>::max() - total) {
					return std::nullopt;
				}
				total += header.Value.size();
			}
			return total;
		}

		[[nodiscard]] auto limits_valid(const github_paged_read_limits& aLimits) noexcept
			-> bool
		{
			return application::detail::within_security_ceiling(aLimits.Records)
				&& aLimits.MaximumPageBytes != 0U
				&& aLimits.MaximumPageBytes <= maximum_production_page_bytes
				&& aLimits.MaximumAggregateResponseBytes != 0U
				&& aLimits.MaximumAggregateResponseBytes
				<= maximum_production_aggregate_bytes
				&& aLimits.MaximumPageBytes <= aLimits.MaximumAggregateResponseBytes
				&& aLimits.MaximumTotalPages != 0U
				&& aLimits.MaximumTotalPages <= maximum_production_pages
				&& aLimits.MaximumPageRecords != 0U
				&& aLimits.MaximumPageRecords <= maximum_production_page_records
				&& aLimits.MaximumCursorBytes != 0U
				&& aLimits.MaximumCursorBytes <= maximum_production_cursor_bytes;
		}

		[[nodiscard]] auto ascii_contains_fold(const std::string_view aText,
											   const std::string_view aPhrase) noexcept
			-> bool
		{
			if (aPhrase.empty() || aPhrase.size() > aText.size()) {
				return false;
			}
			for (std::size_t start = 0U; start <= aText.size() - aPhrase.size(); ++start) {
				if (ascii_equal_fold(aText.substr(start, aPhrase.size()), aPhrase)) {
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] auto
		secondary_rate_limit_message(const page_json_value* const aDocument) -> bool
		{
			if (aDocument == nullptr || !json_is_object(*aDocument)) {
				return false;
			}
			const auto message = json_member(json_object(*aDocument), "message");
			if (!json_is_string(message)) {
				return false;
			}
			const auto raw = text(message);
			return bounded_text(raw, 4U * 1'024U, true)
				&& ascii_contains_fold(raw, "secondary rate limit");
		}

		[[nodiscard]] auto graphql_error_rate_limited(const page_json_value& aErrors) -> bool
		{
			if (!json_is_array(aErrors)) {
				return false;
			}
			for (const auto& value : json_array(aErrors)) {
				if (!json_is_object(value)) {
					continue;
				}
				const auto error = json_object(value);
				if (json_is_string(json_member(error, "type"))
					&& json_text(json_member(error, "type")) == QStringLiteral("RATE_LIMITED")) {
					return true;
				}
				const auto extensions = json_member(error, "extensions");
				if (!json_is_object(extensions)) {
					continue;
				}
				for (const auto key : { "type", "code" }) {
					if (json_is_string(json_member(json_object(extensions), key))
						&& json_text(json_member(json_object(extensions), key))
							== QStringLiteral("RATE_LIMITED")) {
						return true;
					}
				}
			}
			return false;
		}

		[[nodiscard]] constexpr auto
		expected_rate_limit_resource(const read_operation aOperation) noexcept
			-> application::github_rate_limit_resource
		{
			if (graphql_read(aOperation)) {
				return application::github_rate_limit_resource::graphql;
			}
			if (aOperation == read_operation::search_pull_requests
				|| aOperation == read_operation::search_personal_pull_requests) {
				return application::github_rate_limit_resource::search;
			}
			return application::github_rate_limit_resource::core;
		}

		[[nodiscard]] constexpr auto rate_limits_are_empty_or_exact(
			const application::github_rate_limit_state&   aState,
			const application::github_rate_limit_resource aExpected) noexcept -> bool
		{
			if (!application::valid(aState) || !application::has_observations(aState)) {
				return application::valid(aState);
			}
			const auto count = static_cast<std::size_t>(aState.Core.has_value())
				+ static_cast<std::size_t>(aState.Search.has_value())
				+ static_cast<std::size_t>(aState.Graphql.has_value());
			return count == 1U && application::find(aState, aExpected) != nullptr;
		}

		[[nodiscard]] auto status_failure(const int                       aStatus,
										  const github_response_metadata& aMetadata,
										  const bool                      aSecondaryRateLimit = false)
			-> application::github_provider_error
		{
			const auto ordinary =
				[&aMetadata](const application::github_provider_failure aFailure,
							 const std::optional<std::uint32_t>         aRetryAfter = std::nullopt) {
					return provider_error(aFailure,
										  aRetryAfter,
										  aMetadata.RateLimit.has_value()
											  ? rate_limit_state(*aMetadata.RateLimit)
											  : application::github_rate_limit_state{ });
				};
			const auto limited = [&aMetadata, aStatus, aSecondaryRateLimit]() {
				if (!aMetadata.RateLimit.has_value()) {
					return provider_error(
						application::github_provider_failure::malformed_response);
				}
				auto       cause       = *aMetadata.RateLimit;
				const auto primary     = cause.Remaining == 0U;
				cause.SecondaryLimited = !primary
					&& (aStatus == 429 || aSecondaryRateLimit
						|| (aStatus == 403 && aMetadata.RetryAfterSeconds.has_value()));
				const auto retryAfter = aMetadata.RetryAfterSeconds.has_value()
					? aMetadata.RetryAfterSeconds
					: (!primary && (aStatus == 429 || aSecondaryRateLimit)
						   ? std::optional<std::uint32_t>{ 60U }
						   : std::nullopt);
				return provider_error(application::github_provider_failure::rate_limit,
									  retryAfter,
									  rate_limit_state(cause),
									  cause);
			};
			if (aStatus == 401) {
				return ordinary(application::github_provider_failure::authentication);
			}
			if (aStatus == 403) {
				if (aMetadata.RetryAfterSeconds.has_value()
					|| (aMetadata.RateLimit.has_value()
						&& aMetadata.RateLimit->Remaining == 0U)
					|| aSecondaryRateLimit) {
					return limited();
				}
				return ordinary(application::github_provider_failure::permission);
			}
			if (aStatus == 429) {
				return limited();
			}
			if (aStatus == 404) {
				return ordinary(application::github_provider_failure::permission);
			}
			if (aStatus == 408 || aStatus == 504) {
				return ordinary(application::github_provider_failure::timeout);
			}
			if (aStatus >= 500 && aStatus <= 599) {
				return ordinary(application::github_provider_failure::unavailable,
								aMetadata.RetryAfterSeconds);
			}
			if (aStatus >= 400 && aStatus <= 499) {
				return ordinary(application::github_provider_failure::api_version);
			}
			return ordinary(application::github_provider_failure::malformed_response);
		}

		struct parsed_page
		{
			wire_request             Request;
			github_response_metadata Metadata;
			page_completeness        Completeness = page_completeness::unknown;
			page_json_value          Document;
		};

		struct shared_home_budget
		{
			std::size_t Pages = 0, Bytes = 0;
		};
		class production_read_execution
		{
		public:
			/// [[BPR-0988]]. A discovery read's parent is a repository context
			/// or, for one pull request's requested reviewers, a pull request
			/// context; every other read's is a pull request context.
			production_read_execution(github_wire_transport_port&                 aTransport,
									  const validated_github_host_profile&        aProfile,
									  github_wire_parent_context                  aParent,
									  const github_paged_read_limits&             aLimits,
									  const std::stop_token                       aStop,
									  application::github_rate_limit_state&       aRateLimits,
									  std::optional<core::application_session_id> aAuthenticationSession,
									  github_paged_response_observer* aObserver, shared_home_budget* aHomeBudget = nullptr)
				: Transport{ aTransport }
				, Profile{ aProfile }
				, Parent{ std::move(aParent) }
				, Limits{ aLimits }
				, Stop{ aStop }
				, RateLimits{ aRateLimits }
				, AuthenticationSession{ aAuthenticationSession }
				, Observer{ aObserver }
				, HomeBudget{ aHomeBudget }
			{
			}

			[[nodiscard]] auto
			enrich(application::github_provider_error aError) const noexcept
				-> application::github_provider_error
			{
				auto accumulated = RateLimits;
				if (!application::merge(accumulated, aError.RateLimits)) {
					return provider_error(
						application::github_provider_failure::malformed_response,
						std::nullopt,
						RateLimits);
				}
				aError.RateLimits = std::move(accumulated);
				return application::valid(aError)
					? aError
					: provider_error(application::github_provider_failure::
										 malformed_response,
									 std::nullopt,
									 RateLimits);
			}

			[[nodiscard]] auto enrich(github_paged_read_failure aFailure) const noexcept
				-> github_paged_read_failure
			{
				if (auto* error = std::get_if<application::github_provider_error>(&aFailure);
					error != nullptr) {
					*error = enrich(std::move(*error));
				}
				return aFailure;
			}

			[[nodiscard]] auto
			terminal_failure(const application::github_provider_failure aFailure) const
				-> std::unexpected<github_paged_read_failure>
			{
				return failed(aFailure, std::nullopt, RateLimits);
			}

			[[nodiscard]] auto fetch(const semantic_read_request& aSemantic)
				-> std::expected<parsed_page, github_paged_read_failure>
			{
				if (Stop.stop_requested()) {
					return cancelled();
				}
				if (Pages == Limits.MaximumTotalPages || (HomeBudget && HomeBudget->Pages >= 32)) {
					return terminal_failure(
						application::github_provider_failure::response_limit);
				}

				const auto compiled = compile_read_request(aSemantic);
				if (!compiled.has_value()) {
					return terminal_failure(
						application::github_provider_failure::malformed_response);
				}
				const github_wire_correlation correlation{
					.Parent                = Parent,
					.ProfileRevision       = Profile.revision(),
					.Host                  = Profile.host(),
					.Operation             = aSemantic.Operation,
					.Page                  = aSemantic.Page,
					.AuthenticationSession = AuthenticationSession,
				};
				if (Observer != nullptr) {
					Observer->request_started(aSemantic.Operation);
				}
				if (HomeBudget) {
					++HomeBudget->Pages;
				}
				const auto completion = Transport.perform(
					github_wire_request{
						.Correlation = correlation,
						.Wire        = *compiled,
					},
					Stop);
				if (completion.valueless_by_exception()) {
					return terminal_failure(
						application::github_provider_failure::malformed_response);
				}
				if (const auto* success = std::get_if<github_wire_success>(&completion);
					success != nullptr) {
					if (success->Correlation != correlation) {
						return terminal_failure(
							application::github_provider_failure::malformed_response);
					}
					if (Observer != nullptr) {
						Observer->observe(aSemantic.Operation, success->Response.Status);
					}
					const auto bytes = response_byte_count(success->Response);
					if (!bytes.has_value() || *bytes > Limits.MaximumPageBytes
						|| *bytes
							> Limits.MaximumAggregateResponseBytes - AggregateBytes
						|| (HomeBudget && *bytes > 8U * 1024U * 1024U - HomeBudget->Bytes)) {
						return terminal_failure(
							application::github_provider_failure::response_limit);
					}
					++Pages;
					if (HomeBudget) {
						HomeBudget->Bytes += *bytes;
					}
					AggregateBytes += *bytes;

					const auto metadata = parse_github_response_metadata(success->Response);
					if (!metadata.has_value()) {
						return failed(enrich(metadata.error()));
					}
					const auto expectedResource = expected_rate_limit_resource(aSemantic.Operation);
					if ((!metadata->RateLimit.has_value()
						 && aSemantic.Server == server_contract::github_com)
						|| (metadata->RateLimit.has_value()
							&& metadata->RateLimit->Resource != expectedResource)) {
						return terminal_failure(
							application::github_provider_failure::malformed_response);
					}
					if (metadata->RateLimit.has_value()
						&& !application::observe(RateLimits, *metadata->RateLimit)) {
						return terminal_failure(
							application::github_provider_failure::malformed_response);
					}
					const auto metadataFailed =
						[this,
						 &metadata](const application::github_provider_failure aFailure,
									const std::optional<std::uint32_t>         aRetryAfter = std::nullopt,
									const bool                                 aSecondary  = false) {
							auto rateLimits = RateLimits;
							auto cause      = metadata->RateLimit;
							if (cause.has_value()
								&& aFailure
									== application::github_provider_failure::rate_limit) {
								cause->SecondaryLimited = cause->Remaining != 0U && aSecondary;
								static_cast<void>(
									application::observe(rateLimits, *cause));
							} else {
								cause = std::nullopt;
							}
							return failed(aFailure,
										  aRetryAfter,
										  std::move(rateLimits),
										  std::move(cause));
						};
					if (success->Response.Status != 200) {
						std::optional<page_json_value> errorDocument;
						if (!success->Response.Body.empty()) {
							const auto errorPreflight = json_structural_preflight{ success->Response.Body,
																				   Limits.MaximumPageBytes }
															.run();
							if (errorPreflight
								== json_preflight_result::structural_limit) {
								return metadataFailed(
									application::github_provider_failure::
										response_limit);
							}
							if (errorPreflight == json_preflight_result::valid) {
								errorDocument = parse_json_document(success->Response.Body);
							}
						}
						return failed(enrich(status_failure(
							success->Response.Status,
							*metadata,
							errorDocument.has_value()
								&& secondary_rate_limit_message(&*errorDocument))));
					}
					if (success->Response.Body.empty()) {
						return metadataFailed(
							application::github_provider_failure::malformed_response);
					}
					const auto preflight = json_structural_preflight{ success->Response.Body,
																	  Limits.MaximumPageBytes }
											   .run();
					if (preflight == json_preflight_result::structural_limit) {
						return metadataFailed(
							application::github_provider_failure::response_limit);
					}
					if (preflight != json_preflight_result::valid) {
						return metadataFailed(
							application::github_provider_failure::malformed_response);
					}
					auto parsed = parse_json_document(success->Response.Body);
					if (!parsed.has_value()) {
						return metadataFailed(application::github_provider_failure::malformed_response);
					}
					auto document = std::move(*parsed);
					if (graphql_read(aSemantic.Operation)
						&& json_is_object(document) && json_object(document).contains("errors")) {
						const auto structuredRateLimit = graphql_error_rate_limited(
							json_member(json_object(document), "errors"));
						if ((metadata->RateLimit.has_value()
							 && metadata->RateLimit->Remaining == 0U)
							|| metadata->RetryAfterSeconds.has_value()
							|| structuredRateLimit) {
							if (!metadata->RateLimit.has_value()) {
								return metadataFailed(
									application::github_provider_failure::
										malformed_response);
							}
							const auto secondary = metadata->RateLimit->Remaining != 0U
								&& (structuredRateLimit
									|| metadata->RetryAfterSeconds.has_value());
							return metadataFailed(
								application::github_provider_failure::rate_limit,
								metadata->RetryAfterSeconds.has_value()
									? metadata->RetryAfterSeconds
									: (structuredRateLimit
											   && metadata->RateLimit->Remaining != 0U
										   ? std::optional<std::uint32_t>{ 60U }
										   : std::nullopt),
								secondary);
						}
						const auto errors = json_member(json_object(document), "errors");
						if (json_is_array(errors) && std::ranges::any_of(json_array(errors), [](const auto& aError) { return text(json_member(json_object(aError), "type")) == "undefinedField"; })) {
							return metadataFailed(application::github_provider_failure::unsupported_capability);
						}
						return metadataFailed(application::github_provider_failure::malformed_response);
					}
					if (metadata->RetryAfterSeconds.has_value()) {
						return metadataFailed(
							application::github_provider_failure::malformed_response);
					}
					return parsed_page{
						.Request      = std::move(*compiled),
						.Metadata     = *metadata,
						.Completeness = success->Response.Completeness,
						.Document     = std::move(document),
					};
				}
				if (const auto* error = std::get_if<github_wire_error>(&completion);
					error != nullptr) {
					if (error->Correlation != correlation
						|| !application::valid(error->Error)
						|| !rate_limits_are_empty_or_exact(
							error->Error.RateLimits,
							expected_rate_limit_resource(aSemantic.Operation))) {
						return terminal_failure(
							application::github_provider_failure::malformed_response);
					}
					return failed(enrich(error->Error));
				}
				const auto* cancellation = std::get_if<github_wire_cancelled>(&completion);
				if (cancellation == nullptr || cancellation->Correlation != correlation) {
					return terminal_failure(
						application::github_provider_failure::malformed_response);
				}
				return cancelled();
			}

			[[nodiscard]] auto rate_limits() const noexcept
				-> const application::github_rate_limit_state&
			{
				return RateLimits;
			}

			[[nodiscard]] auto
			bind_repository(const core::repository_id aRepository) noexcept -> bool
			{
				if (aRepository.value() == 0U
					|| (BoundRepository.has_value() && *BoundRepository != aRepository)) {
					return false;
				}
				BoundRepository = aRepository;
				return true;
			}

			[[nodiscard]] auto bound_repository() const noexcept
				-> std::optional<core::repository_id>
			{
				return BoundRepository;
			}

		private:
			github_wire_transport_port&                 Transport;
			const validated_github_host_profile&        Profile;
			github_wire_parent_context                  Parent;
			const github_paged_read_limits&             Limits;
			std::stop_token                             Stop;
			std::size_t                                 Pages          = 0U;
			std::size_t                                 AggregateBytes = 0U;
			application::github_rate_limit_state&       RateLimits;
			std::optional<core::repository_id>          BoundRepository;
			std::optional<core::application_session_id> AuthenticationSession;
			github_paged_response_observer*             Observer;
			shared_home_budget*                         HomeBudget;
		};

		[[nodiscard]] auto
		server_contract_for(const validated_github_host_profile& aProfile)
			-> std::optional<server_contract>
		{
			switch (aProfile.server_contract()) {
			case application::github_server_contract::github_com_2022_11_28:
				return server_contract::github_com;
			case application::github_server_contract::ghes_3_18_2022_11_28:
				return server_contract::ghes_3_18;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto
		semantic_request_for(const server_contract                     aServer,
							 const core::pull_request_request_context& aContext,
							 const read_operation                      aOperation,
							 const std::uint32_t                       aPage = 1U) -> semantic_read_request
		{
			semantic_read_request result{
				.Server            = aServer,
				.Operation         = aOperation,
				.Owner             = aContext.Repository.Owner,
				.Repository        = aContext.Repository.Name,
				.PullRequestNumber = 0U,
				.HeadOid           = { },
				.SearchTerms       = { },
				.Page              = aPage,
				.GraphqlPage       = std::nullopt,
			};
			switch (aOperation) {
			case read_operation::get_pull_request:
			case read_operation::list_pull_request_files:
			case read_operation::list_pull_request_commits:
			case read_operation::list_pull_request_timeline:
			case read_operation::list_reviews:
			case read_operation::list_review_threads:
			case read_operation::list_issue_comments:
			case read_operation::list_review_comments:
			case read_operation::list_review_requests:
			case read_operation::get_merge_assessment:
			case read_operation::resolve_body_images:
			case read_operation::list_requested_reviewers:
				result.PullRequestNumber = aContext.Number.value();
				break;
			case read_operation::get_viewer_identity:
			case read_operation::get_repository:
			case read_operation::list_commit_statuses:
			case read_operation::list_checks:
			case read_operation::search_pull_requests:
			case read_operation::list_branch_rules:
			case read_operation::get_branch_protection:
			case read_operation::search_personal_pull_requests:
			case read_operation::get_merge_base:
				break;
			}
			return result;
		}

		[[nodiscard]] auto singleton_complete(const parsed_page& aPage) noexcept -> bool
		{
			return aPage.Completeness == page_completeness::complete
				&& !aPage.Metadata.Link.has_value();
		}

		struct link_relation
		{
			std::string Url;
			std::string Relation;
		};

		/// The strict pagination form GitHub writes: at most four links, each
		/// with exactly one parameter, a quoted `rel` naming one pagination
		/// relation, and no relation twice.
		[[nodiscard]] auto parse_link_relations(const std::string_view aValue)
			-> std::optional<std::vector<link_relation>>
		{
			if (aValue.size() > maximum_header_value_bytes) {
				return std::nullopt;
			}
			const auto links = parse_link_header(aValue);
			if (!links || links->size() > 4U) {
				return std::nullopt;
			}
			std::vector<link_relation> relations;
			for (const auto& link : *links) {
				const auto names = link_relations(link);
				if (link.Parameters.size() != 1U || !link.Parameters.front().Quoted || !names || names->size() != 1U
					|| !pagination_relation(names->front())
					|| std::ranges::any_of(relations, [&names](const link_relation& aExisting) { return aExisting.Relation == names->front(); })) {
					return std::nullopt;
				}
				relations.push_back(link_relation{
					.Url      = std::string{ link.Target },
					.Relation = std::string{ names->front() },
				});
			}
			return relations;
		}

		struct url_query_parameter
		{
			std::string_view Name;
			std::string_view Value;
		};

		[[nodiscard]] auto parse_url_query(const std::string_view aQuery)
			-> std::optional<std::vector<url_query_parameter>>
		{
			if (aQuery.empty() || aQuery.size() > maximum_header_value_bytes) {
				return std::nullopt;
			}
			std::vector<url_query_parameter> result;
			std::size_t                      position = 0U;
			while (position < aQuery.size()) {
				if (result.size() == 4U) {
					return std::nullopt;
				}
				auto end = aQuery.find('&', position);
				if (end == std::string_view::npos) {
					end = aQuery.size();
				}
				const auto parameter = aQuery.substr(position, end - position);
				const auto equals    = parameter.find('=');
				if (parameter.empty() || equals == std::string_view::npos || equals == 0U
					|| equals + 1U == parameter.size()
					|| parameter.find('=', equals + 1U) != std::string_view::npos) {
					return std::nullopt;
				}
				const auto name = parameter.substr(0U, equals);
				if (std::ranges::any_of(result,
										[name](const url_query_parameter& aExisting) { return aExisting.Name == name; })) {
					return std::nullopt;
				}
				result.push_back({
					.Name  = name,
					.Value = parameter.substr(equals + 1U),
				});
				if (end == aQuery.size()) {
					break;
				}
				position = end + 1U;
			}
			return result;
		}

		[[nodiscard]] auto
		numeric_repository_alias(const wire_request&       aRequest,
								 const core::repository_id aRepository)
			-> std::optional<std::string>
		{
			switch (aRequest.Operation) {
			case read_operation::list_pull_request_files:
			case read_operation::list_pull_request_commits:
			case read_operation::list_pull_request_timeline:
			case read_operation::list_reviews:
			case read_operation::list_issue_comments:
			case read_operation::list_review_comments:
			case read_operation::list_commit_statuses:
			case read_operation::list_checks:
				break;
			case read_operation::get_viewer_identity:
			case read_operation::get_repository:
			case read_operation::get_pull_request:
			case read_operation::list_review_threads:
			case read_operation::search_pull_requests:
			case read_operation::list_branch_rules:
			case read_operation::get_branch_protection:
			case read_operation::search_personal_pull_requests:
			case read_operation::list_review_requests:
			case read_operation::get_merge_assessment:
			case read_operation::resolve_body_images:
			case read_operation::list_requested_reviewers:
			case read_operation::get_merge_base:
				return std::nullopt;
			}
			const auto namedRepository = std::string{ "/repos/" }
				+ aRequest.Semantic.Owner + "/"
				+ aRequest.Semantic.Repository;
			const auto position = aRequest.Path.find(namedRepository);
			if (position == std::string::npos
				|| aRequest.Path.find(namedRepository, position + 1U)
					!= std::string::npos) {
				return std::nullopt;
			}
			return aRequest.Path.substr(0U, position) + "/repositories/"
				+ std::to_string(aRepository.value())
				+ aRequest.Path.substr(position + namedRepository.size());
		}

		[[nodiscard]] auto query_value_matches(const read_operation   aOperation,
											   const std::string_view aName,
											   const std::string_view aActual,
											   const std::string_view aExpected) noexcept
			-> bool
		{
			if (aActual == aExpected) {
				return true;
			}
			if ((aOperation != read_operation::search_pull_requests
				 && aOperation != read_operation::search_personal_pull_requests)
				|| aName != "q") {
				return false;
			}

			std::size_t actualPosition   = 0U;
			std::size_t expectedPosition = 0U;
			while (expectedPosition < aExpected.size()) {
				if (aExpected.substr(expectedPosition).starts_with("%20")) {
					if (actualPosition == aActual.size()
						|| aActual[actualPosition] != '+') {
						return false;
					}
					++actualPosition;
					expectedPosition += 3U;
					continue;
				}
				if (actualPosition == aActual.size()
					|| aActual[actualPosition] != aExpected[expectedPosition]) {
					return false;
				}
				++actualPosition;
				++expectedPosition;
			}
			return actualPosition == aActual.size();
		}

		[[nodiscard]] auto
		page_number_from_url(const validated_github_host_profile& aProfile,
							 const parsed_page&                   aPage,
							 const core::repository_id            aRepository,
							 const std::string_view               aUrl) -> std::optional<std::uint32_t>
		{
			if (aUrl.size() > maximum_header_value_bytes || !aUrl.starts_with("https://")
				|| std::ranges::any_of(
					aUrl,
					[](const unsigned char aCharacter) { return aCharacter <= 0x20U || aCharacter >= 0x7FU; })) {
				return std::nullopt;
			}
			constexpr std::size_t schemeSize = std::string_view{ "https://" }.size();
			const auto            pathStart  = aUrl.find('/', schemeSize);
			const auto            queryStart = pathStart == std::string_view::npos
				? std::string_view::npos
				: aUrl.find('?', pathStart);
			if (pathStart == std::string_view::npos
				|| queryStart == std::string_view::npos
				|| aUrl.find('?', queryStart + 1U) != std::string_view::npos
				|| aUrl.find('#') != std::string_view::npos) {
				return std::nullopt;
			}
			const auto authority = aUrl.substr(schemeSize, pathStart - schemeSize);
			if (authority != aProfile.rest_authority()
				|| authority.find('@') != std::string_view::npos
				|| authority.find(':') != std::string_view::npos) {
				return std::nullopt;
			}
			const auto path       = aUrl.substr(pathStart, queryStart - pathStart);
			const auto query      = aUrl.substr(queryStart + 1U);
			const auto parameters = parse_url_query(query);
			if (!parameters.has_value()) {
				return std::nullopt;
			}
			const auto pageParameter = std::ranges::find(
				*parameters, std::string_view{ "page" }, &url_query_parameter::Name);
			const auto perPageParameter = std::ranges::find(
				*parameters, std::string_view{ "per_page" }, &url_query_parameter::Name);
			if (pageParameter == parameters->end()
				|| perPageParameter == parameters->end()
				|| perPageParameter->Value != "100") {
				return std::nullopt;
			}
			const auto number = decimal(pageParameter->Value);
			if (!number.has_value() || *number == 0U
				|| *number > std::numeric_limits<std::uint32_t>::max()) {
				return std::nullopt;
			}
			const auto relationPage = static_cast<std::uint32_t>(*number);
			if (std::to_string(relationPage) != pageParameter->Value) {
				return std::nullopt;
			}

			auto semantic        = aPage.Request.Semantic;
			semantic.Page        = relationPage;
			const auto canonical = compile_read_request(semantic);
			if (!canonical.has_value()) {
				return std::nullopt;
			}
			const auto alias = numeric_repository_alias(*canonical, aRepository);
			if (path != canonical->Path && (!alias.has_value() || path != *alias)) {
				return std::nullopt;
			}
			const auto canonicalParameters = parse_url_query(canonical->Query);
			if (!canonicalParameters.has_value()
				|| parameters->size() != canonicalParameters->size()) {
				return std::nullopt;
			}
			for (const auto& expected : *canonicalParameters) {
				const auto actual = std::ranges::find(
					*parameters, expected.Name, &url_query_parameter::Name);
				if (actual == parameters->end()
					|| !query_value_matches(canonical->Operation,
											expected.Name,
											actual->Value,
											expected.Value)) {
					return std::nullopt;
				}
			}
			return relationPage;
		}

		[[nodiscard]] auto
		rest_has_next_page(const validated_github_host_profile& aProfile,
						   const core::repository_id            aRepository,
						   const parsed_page&                   aPage,
						   const std::size_t                    aRecordCount)
			-> std::expected<bool, github_paged_read_failure>
		{
			if (aPage.Completeness == page_completeness::unknown
				|| aPage.Request.Method != http_method::get || aPage.Request.Query.empty()) {
				return failed(application::github_provider_failure::malformed_response);
			}
			if (!aPage.Metadata.Link.has_value()) {
				if (aPage.Completeness != page_completeness::complete) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				return false;
			}

			const auto relations = parse_link_relations(*aPage.Metadata.Link);
			if (!relations.has_value()) {
				return failed(application::github_provider_failure::malformed_response);
			}
			bool                         hasNext = false;
			std::optional<std::uint32_t> lastPage;
			for (const auto& relation : *relations) {
				const auto relationPage = page_number_from_url(aProfile, aPage, aRepository, relation.Url);
				if (!relationPage.has_value()) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto current = aPage.Request.Semantic.Page;
				if ((relation.Relation == "next"
					 && (current == std::numeric_limits<std::uint32_t>::max()
						 || *relationPage != current + 1U))
					|| (relation.Relation == "prev"
						&& (current <= 1U || *relationPage != current - 1U))
					|| (relation.Relation == "first"
						&& (current <= 1U || *relationPage != 1U))
					|| (relation.Relation == "last" && *relationPage < current)) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				hasNext = hasNext || relation.Relation == "next";
				if (relation.Relation == "last") {
					lastPage = relationPage;
				}
			}

			if ((hasNext && aPage.Completeness != page_completeness::has_next_page)
				|| (!hasNext && aPage.Completeness != page_completeness::complete)
				|| (hasNext && aRecordCount != 100U)
				|| (lastPage.has_value()
					&& ((hasNext && *lastPage <= aPage.Request.Semantic.Page)
						|| (!hasNext && *lastPage != aPage.Request.Semantic.Page)))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			return hasNext;
		}

		/// \brief [[BPR-1321]] Maps a provider-stated account type; nothing is inferred from logins or IDs.
		[[nodiscard]] auto actor_kind(const std::string_view aType) noexcept
			-> application::github_actor_kind
		{
			if (aType == "Bot") {
				return application::github_actor_kind::bot;
			}
			return aType == "User" ? application::github_actor_kind::user
								   : application::github_actor_kind::other;
		}

		[[nodiscard]] auto parse_rest_actor(const page_json_value& aValue)
			-> std::optional<application::github_actor_record>
		{
			if (json_is_null(aValue)) {
				return std::optional<application::github_actor_record>{ };
			}
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue), { "login", "node_id" })
				|| !nonempty_string(json_member(json_object(aValue), "login"))
				|| !nonempty_string(json_member(json_object(aValue), "node_id"))) {
				return std::nullopt;
			}
			const auto object = json_object(aValue);
			// REST already holds a bot in the canonical `<name>[bot]` form, so
			// only the stated type is recorded. An absent or non-string `type`
			// is no type evidence.
			const auto type = json_member(object, "type");
			return application::github_actor_record{
				.Actor = application::github_actor_id{ text(json_member(object, "node_id")) },
				.Login = text(json_member(object, "login")),
				.Kind  = json_is_string(type) ? actor_kind(text(type)) : application::github_actor_kind::unobserved,
			};
		}

		struct optional_actor_parse
		{
			bool                                            Valid = false;
			std::optional<application::github_actor_record> Actor;
		};

		[[nodiscard]] auto optional_rest_actor(const page_json_value& aValue)
			-> optional_actor_parse
		{
			if (json_is_null(aValue)) {
				return { .Valid = true, .Actor = std::nullopt };
			}
			const auto actor = parse_rest_actor(aValue);
			return actor.has_value()
				? optional_actor_parse{ .Valid = true, .Actor = actor }
				: optional_actor_parse{ };
		}

		[[nodiscard]] auto optional_graphql_actor(const page_json_value& aValue)
			-> optional_actor_parse
		{
			if (json_is_null(aValue)) {
				return { .Valid = true, .Actor = std::nullopt };
			}
			if (!json_is_object(aValue)
				|| (!has_exact_keys(json_object(aValue), { "id", "login" })
					&& !(has_exact_keys(json_object(aValue), { "__typename", "id", "login" })
						 && nonempty_string(json_member(json_object(aValue), "__typename"))))
				|| !nonempty_string(json_member(json_object(aValue), "id"))
				|| !nonempty_string(json_member(json_object(aValue), "login"))) {
				return { };
			}
			const auto object   = json_object(aValue);
			const auto typeName = json_member(object, "__typename");
			const auto kind     = json_is_string(typeName) ? actor_kind(text(typeName)) : application::github_actor_kind::unobserved;
			auto       login    = text(json_member(object, "login"));
			// [[BPR-1321]] GitHub's GraphQL `Bot.login` omits the `[bot]` suffix
			// that REST carries for the same node. Only a typed `Bot` takes the
			// REST form; snapshot validation still compares the result exactly
			// and refuses it against any observation not typed as a bot.
			constexpr std::string_view botSuffix = "[bot]";
			if (kind == application::github_actor_kind::bot && !login.ends_with(botSuffix)) {
				login += botSuffix;
			}
			return {
				.Valid = true,
				.Actor = application::github_actor_record{
					.Actor = application::github_actor_id{ text(json_member(object, "id")) },
					.Login = std::move(login),
					.Kind  = kind,
				},
			};
		}

		[[nodiscard]] auto pull_request_state(const QString& aState,
											  const bool     aMerged) noexcept
			-> application::github_pull_request_state
		{
			if (aMerged) {
				return application::github_pull_request_state::merged;
			}
			return aState == QStringLiteral("open")
				? application::github_pull_request_state::open
				: application::github_pull_request_state::closed;
		}

		[[nodiscard]] auto file_change(const QString& aValue) noexcept
			-> application::github_file_change
		{
			if (aValue == QStringLiteral("added")) {
				return application::github_file_change::added;
			}
			if (aValue == QStringLiteral("modified")) {
				return application::github_file_change::modified;
			}
			if (aValue == QStringLiteral("removed")) {
				return application::github_file_change::removed;
			}
			if (aValue == QStringLiteral("renamed")) {
				return application::github_file_change::renamed;
			}
			if (aValue == QStringLiteral("copied")) {
				return application::github_file_change::copied;
			}
			if (aValue == QStringLiteral("unchanged")) {
				return application::github_file_change::unchanged;
			}
			return application::github_file_change::type_changed;
		}

		[[nodiscard]] auto review_state(const QString& aValue) noexcept
			-> application::github_review_state
		{
			if (aValue == QStringLiteral("PENDING")) {
				return application::github_review_state::pending;
			}
			if (aValue == QStringLiteral("COMMENTED")) {
				return application::github_review_state::commented;
			}
			if (aValue == QStringLiteral("APPROVED")) {
				return application::github_review_state::approved;
			}
			if (aValue == QStringLiteral("CHANGES_REQUESTED")) {
				return application::github_review_state::changes_requested;
			}
			return application::github_review_state::dismissed;
		}

		[[nodiscard]] auto check_status(const QString& aValue) noexcept
			-> application::github_check_status
		{
			if (aValue == QStringLiteral("queued")) {
				return application::github_check_status::queued;
			}
			if (aValue == QStringLiteral("in_progress")) {
				return application::github_check_status::in_progress;
			}
			if (aValue == QStringLiteral("completed")) {
				return application::github_check_status::completed;
			}
			if (aValue == QStringLiteral("waiting")) {
				return application::github_check_status::waiting;
			}
			if (aValue == QStringLiteral("requested")) {
				return application::github_check_status::requested;
			}
			return application::github_check_status::pending;
		}

		[[nodiscard]] auto check_conclusion(const page_json_value& aValue) noexcept
			-> application::github_check_conclusion
		{
			if (json_is_null(aValue)) {
				return application::github_check_conclusion::none;
			}
			const auto conclusion = json_text(aValue);
			if (conclusion == QStringLiteral("success")) {
				return application::github_check_conclusion::success;
			}
			if (conclusion == QStringLiteral("failure")) {
				return application::github_check_conclusion::failure;
			}
			if (conclusion == QStringLiteral("neutral")) {
				return application::github_check_conclusion::neutral;
			}
			if (conclusion == QStringLiteral("cancelled")) {
				return application::github_check_conclusion::cancelled;
			}
			if (conclusion == QStringLiteral("skipped")) {
				return application::github_check_conclusion::skipped;
			}
			if (conclusion == QStringLiteral("timed_out")) {
				return application::github_check_conclusion::timed_out;
			}
			if (conclusion == QStringLiteral("action_required")) {
				return application::github_check_conclusion::action_required;
			}
			return application::github_check_conclusion::stale;
		}

		[[nodiscard]] auto viewed_state(const QString& aValue) noexcept
			-> std::optional<application::github_file_viewed_state>
		{
			if (aValue == QStringLiteral("DISMISSED")) {
				return application::github_file_viewed_state::dismissed;
			}
			if (aValue == QStringLiteral("UNVIEWED")) {
				return application::github_file_viewed_state::unviewed;
			}
			if (aValue == QStringLiteral("VIEWED")) {
				return application::github_file_viewed_state::viewed;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto diff_side(const page_json_value& aValue) noexcept
			-> std::optional<application::github_diff_side>
		{
			if (!json_is_string(aValue)) {
				return std::nullopt;
			}
			if (json_text(aValue) == QStringLiteral("LEFT")) {
				return application::github_diff_side::left;
			}
			if (json_text(aValue) == QStringLiteral("RIGHT")) {
				return application::github_diff_side::right;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto bounded_graphql_token(const std::string_view aValue,
												 const std::size_t      aMaximum) noexcept
			-> bool
		{
			return !aValue.empty() && aValue.size() <= aMaximum
				&& std::ranges::all_of(aValue,
									   [](const char aCharacter) {
										   const auto byte = static_cast<unsigned char>(aCharacter);
										   return byte >= 0x21U && byte <= 0x7eU;
									   });
		}

		struct graphql_connection_page
		{
			std::uint64_t              TotalCount = 0U;
			page_json_array            Nodes;
			std::optional<std::string> NextCursor;
		};

		[[nodiscard]] auto
		parse_graphql_connection(const page_json_value&          aValue,
								 const github_paged_read_limits& aLimits)
			-> std::expected<graphql_connection_page, github_paged_read_failure>
		{
			if (!json_is_object(aValue)
				|| !has_exact_keys(json_object(aValue),
								   { "nodes", "pageInfo", "totalCount" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto connection = json_object(aValue);
			if (!exact_integer(json_member(connection, "totalCount"))
				|| !json_is_array(json_member(connection, "nodes"))
				|| !json_is_object(json_member(connection, "pageInfo"))
				|| !has_exact_keys(json_object(json_member(connection, "pageInfo")),
								   { "endCursor", "hasNextPage" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto nodes    = json_array(json_member(connection, "nodes"));
			const auto pageInfo = json_object(json_member(connection, "pageInfo"));
			if (static_cast<std::size_t>(nodes.size()) > aLimits.MaximumPageRecords
				|| !json_is_bool(json_member(pageInfo, "hasNextPage"))) {
				return failed(application::github_provider_failure::response_limit);
			}
			const auto                 total       = integer(json_member(connection, "totalCount"));
			const auto                 hasNext     = json_bool(json_member(pageInfo, "hasNextPage"));
			const auto                 cursorValue = json_member(pageInfo, "endCursor");
			std::optional<std::string> nextCursor;
			if (hasNext) {
				if (static_cast<std::size_t>(nodes.size()) != 100U
					|| !json_is_string(cursorValue)) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				auto cursor = text(cursorValue);
				if (!bounded_graphql_token(cursor, aLimits.MaximumCursorBytes)
					|| total <= static_cast<std::uint64_t>(nodes.size())) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				nextCursor = std::move(cursor);
			} else if ((nodes.empty() && !json_is_null(cursorValue))
					   || (!nodes.empty()
						   && (!json_is_string(cursorValue)
							   || !bounded_graphql_token(text(cursorValue),
														 aLimits.MaximumCursorBytes)))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			if (total < static_cast<std::uint64_t>(nodes.size())) {
				return failed(application::github_provider_failure::malformed_response);
			}
			return graphql_connection_page{
				.TotalCount = total,
				.Nodes      = nodes,
				.NextCursor = std::move(nextCursor),
			};
		}

		// These are optional only for old recordings. A remote body without the new
		// observations remains readable but cannot be claimed as a raw tour baseline.
		[[nodiscard]] auto parse_comment_observation(const page_json_value& aValue, bool aGraphql)
			-> std::expected<application::github_comment_observation, github_paged_read_failure>
		{
			const auto                              object = json_object(aValue);
			application::github_comment_observation result;
			const auto                              scalar = [&](std::string_view aKey, std::string& aOut) {
				const auto value = json_member(object, aKey);
				if (json_is_undefined(value) || json_is_null(value)) {
					return true;
				}
				if (!json_is_string(value)) {
					return false;
				}
				aOut = text(value);
				return true;
			};
			if (!scalar(aGraphql ? "url" : "html_url", result.WebUrl) || !scalar(aGraphql ? "updatedAt" : "updated_at", result.UpdatedAt)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto id = json_member(object, aGraphql ? "fullDatabaseId" : "id");
			if (!json_is_undefined(id) && !json_is_null(id)) {
				if (exact_integer(id, true)) {
					result.DatabaseId = integer(id);
				} else if (json_is_string(id)) {
					const auto digits = text(id);
					const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), result.DatabaseId);
					if (digits.empty() || digits.front() == '0' || parsed.ec != std::errc{ } || parsed.ptr != digits.data() + digits.size() || !result.DatabaseId) {
						return failed(application::github_provider_failure::malformed_response);
					}
				} else {
					return failed(application::github_provider_failure::malformed_response);
				}
			}
			const auto body = json_member(object, "body");
			if (!json_is_undefined(body)) {
				if (!json_is_string(body)) {
					return failed(application::github_provider_failure::malformed_response);
				}
				result.RawBody = text(body);
			}
			if (aGraphql) {
				for (const auto& entry : { std::pair{ "commit", &result.Commit }, std::pair{ "originalCommit", &result.OriginalCommit }, std::pair{ "pullRequestReview", &result.ReviewNode } }) {
					const auto value = json_member(object, entry.first);
					if (json_is_undefined(value) || json_is_null(value)) {
						continue;
					}
					const auto key = std::string_view{ entry.first } == "pullRequestReview" ? "id" : "oid";
					if (!json_is_object(value) || !has_exact_keys(json_object(value), { key }) || !nonempty_string(json_member(json_object(value), key))) {
						return failed(application::github_provider_failure::malformed_response);
					}
					*entry.second = text(json_member(json_object(value), key));
				}
			} else if (!scalar("commit_id", result.Commit) || !scalar("original_commit_id", result.OriginalCommit)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			for (const auto& entry : { std::pair{ aGraphql ? "startLine" : "start_line", &result.StartLine }, std::pair{ aGraphql ? "originalStartLine" : "original_start_line", &result.OriginalStartLine } }) {
				const auto value = json_member(object, entry.first);
				if (json_is_undefined(value) || json_is_null(value)) {
					continue;
				}
				if (!valid_line(value)) {
					return failed(application::github_provider_failure::malformed_response);
				}
				*entry.second = optional_line(value);
			}
			return result;
		}

		[[nodiscard]] auto parse_graphql_comment(const page_json_value& aValue)
			-> std::expected<application::github_review_comment_record,
							 github_paged_read_failure>
		{
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue),
									  {
										  "author",
										  "bodyText",
										  "id",
										  "line",
										  "originalLine",
										  "outdated",
										  "path",
										  "replyTo",
									  })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto keys = std::vector<std::string_view>{ "author", "bodyText", "id", "line", "originalLine", "outdated", "path", "replyTo" };
			for (const auto key : { "body", "url", "updatedAt", "fullDatabaseId", "commit", "originalCommit", "startLine", "originalStartLine", "pullRequestReview" }) {
				if (!json_is_undefined(json_member(json_object(aValue), key))) {
					keys.emplace_back(key);
				}
			}
			if (!has_exact_keys(json_object(aValue), keys)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto observation = parse_comment_observation(aValue, true);
			if (!observation) {
				return std::unexpected(observation.error());
			}

			const auto comment      = json_object(aValue);
			const auto author       = optional_graphql_actor(json_member(comment, "author"));
			const auto line         = json_member(comment, "line");
			const auto originalLine = json_member(comment, "originalLine");
			const auto reply        = json_member(comment, "replyTo");
			if (!nonempty_string(json_member(comment, "id"))
				|| !json_is_string(json_member(comment, "bodyText"))
				|| !nonempty_string(json_member(comment, "path"))
				|| !json_is_bool(json_member(comment, "outdated")) || !author.Valid
				|| (!json_is_null(line) && !valid_line(line))
				|| (!json_is_null(originalLine) && !valid_line(originalLine))
				|| (!json_is_null(reply)
					&& (!json_is_object(reply) || !has_exact_keys(json_object(reply), { "id" })
						|| !nonempty_string(json_member(json_object(reply), "id"))))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			std::optional<application::github_comment_id> replyTo;
			if (json_is_object(reply)) {
				replyTo = application::github_comment_id{ text(json_member(json_object(reply), "id")) };
			}
			return application::github_review_comment_record{
				.Comment      = application::github_comment_id{ text(json_member(comment, "id")) },
				.ReplyTo      = std::move(replyTo),
				.Author       = author.Actor,
				.Body         = text(json_member(comment, "bodyText")),
				.Path         = text(json_member(comment, "path")),
				.Line         = optional_line(line),
				.OriginalLine = optional_line(originalLine),
				.Outdated     = json_bool(json_member(comment, "outdated")),
				.Deleted      = false,
				.Observation  = *observation,
			};
		}

		struct parsed_graphql_thread
		{
			application::github_review_thread_record Record;
			std::uint64_t                            TotalComments = 0U;
			std::optional<std::string>               NextCommentsCursor;
		};

		[[nodiscard]] auto
		parse_graphql_thread(const page_json_value&                aValue,
							 const application::github_read_shape& aShape,
							 const github_paged_read_limits&       aLimits)
			-> std::expected<parsed_graphql_thread, github_paged_read_failure>
		{
			if (!json_is_object(aValue)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			std::vector<std::string_view> keys{
				"comments",
				"diffSide",
				"id",
				"isOutdated",
				"isResolved",
				"line",
				"originalLine",
				"path",
				"subjectType",
			};
			if (aShape.ResolvePermission) {
				keys.emplace_back("viewerCanResolve");
			}
			if (aShape.UnresolvePermission) {
				keys.emplace_back("viewerCanUnresolve");
			}
			const auto thread = json_object(aValue);
			if (!has_exact_keys(thread, keys)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto comments = parse_graphql_connection(json_member(thread, "comments"), aLimits);
			if (!comments.has_value()) {
				return std::unexpected{ comments.error() };
			}
			if (comments->TotalCount > aLimits.Records.MaximumCommentsPerThread
				|| comments->TotalCount > aLimits.Records.MaximumTotalComments) {
				return failed(application::github_provider_failure::response_limit);
			}
			const auto line         = json_member(thread, "line");
			const auto originalLine = json_member(thread, "originalLine");
			const auto side         = diff_side(json_member(thread, "diffSide"));
			const auto fileSubject  = json_text(json_member(thread, "subjectType")) == QStringLiteral("FILE");
			const auto lineSubject  = json_text(json_member(thread, "subjectType")) == QStringLiteral("LINE");
			if (!nonempty_string(json_member(thread, "id"))
				|| !json_is_bool(json_member(thread, "isOutdated"))
				|| !json_is_bool(json_member(thread, "isResolved"))
				|| !nonempty_string(json_member(thread, "path"))
				|| (!fileSubject && !lineSubject)
				|| !side.has_value()
				|| (!json_is_null(line) && !valid_line(line))
				|| (!json_is_null(originalLine) && !valid_line(originalLine))
				|| (fileSubject
					&& !(json_is_null(line) && json_is_null(originalLine))
					&& !(optional_line(line) == 1U && optional_line(originalLine) == 1U
						 && side == application::github_diff_side::right))
				|| (lineSubject && !json_bool(json_member(thread, "isOutdated"))
					&& json_is_null(line) && !json_is_null(originalLine))
				|| (aShape.ResolvePermission
					&& !json_is_bool(json_member(thread, "viewerCanResolve")))
				|| (aShape.UnresolvePermission
					&& !json_is_bool(json_member(thread, "viewerCanUnresolve")))) {
				return failed(application::github_provider_failure::malformed_response);
			}

			application::github_review_thread_record record{
				.Thread                    = application::github_thread_id{ text(json_member(thread, "id")) },
				.Subject                   = fileSubject
					? application::github_review_thread_subject::file
					: application::github_review_thread_subject::line,
				.Path                      = text(json_member(thread, "path")),
				.Line                      = fileSubject ? std::nullopt : optional_line(line),
				.OriginalLine              = fileSubject ? std::nullopt : optional_line(originalLine),
				.DiffSide                  = *side,
				.Resolved                  = json_bool(json_member(thread, "isResolved")),
				.Outdated                  = json_bool(json_member(thread, "isOutdated")),
				.ViewerResolvePermission   = aShape.ResolvePermission
					? (json_bool(json_member(thread, "viewerCanResolve"))
						   ? application::github_permission_state::allowed
						   : application::github_permission_state::denied)
					: application::github_permission_state::unavailable,
				.ViewerUnresolvePermission = aShape.UnresolvePermission
					? (json_bool(json_member(thread, "viewerCanUnresolve"))
						   ? application::github_permission_state::allowed
						   : application::github_permission_state::denied)
					: application::github_permission_state::unavailable,
				.Comments                  = { },
			};
			record.Comments.reserve(static_cast<std::size_t>(comments->TotalCount));
			for (const auto& commentValue : comments->Nodes) {
				auto comment = parse_graphql_comment(commentValue);
				if (!comment.has_value()) {
					return std::unexpected{ comment.error() };
				}
				if (comment->Path != record.Path
					|| (record.Subject == application::github_review_thread_subject::file
						&& (comment->Line.has_value()
							|| comment->OriginalLine.has_value()))) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				record.Comments.push_back(std::move(*comment));
			}
			return parsed_graphql_thread{
				.Record             = std::move(record),
				.TotalComments      = comments->TotalCount,
				.NextCommentsCursor = std::move(comments->NextCursor),
			};
		}

		[[nodiscard]] auto graphql_repository_and_pull_request(
			const parsed_page&                             aPage,
			const application::github_repository_record&   aRepository,
			const application::github_pull_request_record& aPullRequest,
			page_json_object*                              aDataOutput,
			page_json_object*                              aRepositoryOutput,
			page_json_object*                              aPullRequestOutput)
			-> std::expected<void, github_paged_read_failure>
		{
			if (!singleton_complete(aPage) || !json_is_object(aPage.Document)
				|| !graphql_success_envelope(json_object(aPage.Document))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto dataValue = json_member(json_object(aPage.Document), "data");
			if (!json_is_object(dataValue)
				|| !has_exact_keys(json_object(dataValue), { "repository" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto repositoryValue = json_member(json_object(dataValue), "repository");
			if (!json_is_object(repositoryValue)
				|| !has_exact_keys(json_object(repositoryValue), { "id", "pullRequest" })
				|| text(json_member(json_object(repositoryValue), "id"))
					!= aRepository.Node.value()) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto pullRequestValue = json_member(json_object(repositoryValue), "pullRequest");
			if (!json_is_object(pullRequestValue)
				|| !has_required_keys(
					json_object(pullRequestValue), { "headRefOid", "id" })
				|| text(json_member(json_object(pullRequestValue), "id"))
					!= aPullRequest.Node.value()
				|| text(json_member(json_object(pullRequestValue), "headRefOid"))
					!= aPullRequest.HeadOid.value()) {
				return failed(application::github_provider_failure::malformed_response);
			}
			*aDataOutput        = json_object(dataValue);
			*aRepositoryOutput  = json_object(repositoryValue);
			*aPullRequestOutput = json_object(pullRequestValue);
			return { };
		}

		struct thread_pagination_state
		{
			std::uint64_t                   TotalComments = 0U;
			std::optional<std::string>      NextCursor;
			std::unordered_set<std::string> SeenCursors;
		};

		[[nodiscard]] auto provider_request_valid(
			const validated_github_host_profile&                     aProfile,
			const application::github_pull_request_provider_request& aRequest,
			const core::provider_snapshot_id                         aSnapshot,
			const github_paged_read_limits&                          aLimits,
			const github_provider_read_authority                     aReadAuthority) noexcept -> bool
		{
			const auto& context     = aRequest.Context;
			const auto& authority   = aRequest.ProfileAuthority;
			const auto& evidence    = aRequest.CapabilityEvidence;
			const auto  observation = application::github_capability_probe_observation{
				.Authority   = evidence.Authority,
				.ObservedAt  = evidence.ObservedAt,
				.Facts       = evidence.Facts,
				.SchemaFacts = evidence.SchemaFacts,
			};
			// RFC-BPR-0017. The signed-out reader, decided once. Two groups of
			// terms below describe an account and a probe, and a signed-out
			// read has neither; every other term still applies to it exactly
			// as written, and this one is false unless the whole synthesised
			// shape holds under the authority that is allowed to present it.
			const auto signedOut = aReadAuthority
					== github_provider_read_authority::anonymous_public
				&& signed_out_read_identity(aRequest);
			return limits_valid(aLimits) && aSnapshot.value() != 0U
				&& (!aRequest.AuthenticationSession || (aRequest.AuthenticationSession->value() != 0U && aReadAuthority != github_provider_read_authority::anonymous_public))
				&& application::detail::valid_context(context)
				&& application::valid(authority) && aRequest.Attempt.valid()
				// An anonymous read holds no account, so it holds no account
				// transport lease to present. Every other term below still
				// applies to it; only the lease is authority-specific, and the
				// authenticated authority keeps requiring it.
				&& (aReadAuthority
						== github_provider_read_authority::anonymous_public
					|| aRequest.AccountTransport != nullptr)
				// The reader. Named exactly, or named not at all: a
				// signed-out read carries an empty viewer, which is the
				// identity the accepted snapshot is required to carry back.
				&& (signedOut
					|| (aRequest.Viewer.Account.value() != 0U
						&& bounded_text(aRequest.Viewer.Node.value(),
										aLimits.Records.MaximumIdentityBytes,
										true)
						&& bounded_text(aRequest.Viewer.Login,
										aLimits.Records.MaximumLoginBytes,
										true)))
				&& aRequest.Viewer.Account == context.Account
				&& bounded_text(aRequest.Viewer.DisplayName,
								aLimits.Records.MaximumDisplayNameBytes)
				&& authority.Profile == context.Profile
				&& authority.Profile == aProfile.profile()
				&& authority.ProfileRevision == aProfile.revision()
				&& authority.Host == context.Host && authority.Host == aProfile.host()
				&& authority.Hostname.value() == aProfile.hostname()
				&& authority.ServerContract == aProfile.server_contract()
				&& authority.Authentication == aProfile.authentication_transport()
				&& (signedOut || application::valid(observation))
				&& evidence.Authority.Profile == authority.Profile
				&& evidence.Authority.ProfileRevision == authority.ProfileRevision
				&& evidence.Authority.Host == authority.Host
				&& evidence.Authority.Account == context.Account
				&& evidence.Authority.ServerContract == authority.ServerContract
				&& evidence.Authority.ProbeContractRevision
				== authority.ProbeContractRevision
				&& evidence.Revision == context.Capabilities
				// The probe. Observed within the freshness window and
				// projected from what it observed, or -- signed out -- never
				// observed at all, which `signed_out_read_identity` has
				// already checked term by term.
				&& (signedOut
					|| (evidence.ExpiresBefore > evidence.ObservedAt
						&& evidence.ExpiresBefore.value()
								- evidence.ObservedAt.value()
							<= application::
								maximum_github_capability_freshness_window
						&& evidence.Capabilities
							== application::projected_capabilities(observation)
						&& evidence.ReadShape
							== application::projected_read_shape(observation)));
		}

		[[nodiscard]] auto
		parse_repository(const parsed_page&                        aPage,
						 const core::pull_request_request_context& aContext)
			-> std::expected<application::github_repository_record,
							 github_paged_read_failure>
		{
			if (!singleton_complete(aPage) || !json_is_object(aPage.Document)
				|| !has_required_keys(json_object(aPage.Document),
									  {
										  "default_branch",
										  "full_name",
										  "id",
										  "name",
										  "node_id",
										  "owner",
										  "private",
									  })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aPage.Document);
			const auto owner  = json_member(object, "owner");
			if (!exact_integer(json_member(object, "id"), true)
				|| !nonempty_string(json_member(object, "name"))
				|| !nonempty_string(json_member(object, "full_name"))
				|| !nonempty_string(json_member(object, "node_id"))
				|| !json_is_bool(json_member(object, "private"))
				|| !nonempty_string(json_member(object, "default_branch")) || !json_is_object(owner)
				|| !has_required_keys(json_object(owner), { "login" })
				|| !nonempty_string(json_member(json_object(owner), "login"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto rawOwner    = text(json_member(json_object(owner), "login"));
			const auto rawName     = text(json_member(object, "name"));
			const auto rawFullName = text(json_member(object, "full_name"));
			if (!ascii_equal_fold(rawOwner, aContext.Repository.Owner)
				|| !ascii_equal_fold(rawName, aContext.Repository.Name)
				|| !ascii_equal_fold(rawFullName, rawOwner + "/" + rawName)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto id = core::repository_id{ integer(json_member(object, "id")) };
			if (aContext.ExpectedRepository.has_value()
				&& *aContext.ExpectedRepository != id) {
				return failed(application::github_provider_failure::malformed_response);
			}
			return application::github_repository_record{
				.Repository    = id,
				.Node          = application::github_node_id{ text(json_member(object, "node_id")) },
				.Owner         = rawOwner,
				.Name          = rawName,
				.DefaultBranch = text(json_member(object, "default_branch")),
				.Private       = json_bool(json_member(object, "private")),
			};
		}

		[[nodiscard]] auto parse_timeline_timestamp(const std::string_view aValue) -> std::optional<std::int64_t>
		{
			// REST's UTC second-resolution event timestamp; no locale or clock.
			if (aValue.size() != 20U || aValue[4] != '-' || aValue[7] != '-'
				|| aValue[10] != 'T' || aValue[13] != ':' || aValue[16] != ':' || aValue[19] != 'Z') {
				return std::nullopt;
			}
			const auto number = [aValue](const std::size_t aStart, const std::size_t aSize) -> std::optional<unsigned> {
				const auto part = aValue.substr(aStart, aSize);
				if (!std::ranges::all_of(part, [](const char aChar) { return aChar >= '0' && aChar <= '9'; })) {
					return std::nullopt;
				}
				unsigned   result = 0U;
				const auto parsed = std::from_chars(part.data(), part.data() + part.size(), result);
				return parsed.ec == std::errc{ } ? std::optional{ result } : std::nullopt;
			};
			const auto year = number(0U, 4U), month = number(5U, 2U), day = number(8U, 2U), hour = number(11U, 2U), minute = number(14U, 2U), second = number(17U, 2U);
			if (!year || !month || !day || !hour || !minute || !second || *year < 1970U || *hour > 23U || *minute > 59U || *second > 59U) {
				return std::nullopt;
			}
			const auto date = std::chrono::year{ static_cast<int>(*year) } / std::chrono::month{ *month } / std::chrono::day{ *day };
			if (!date.ok()) {
				return std::nullopt;
			}
			return std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::sys_days{ date } + std::chrono::hours{ *hour } + std::chrono::minutes{ *minute } + std::chrono::seconds{ *second }).time_since_epoch()).count();
		}

		struct parsed_pull_ref
		{
			std::string                        Ref;
			std::string                        Oid;
			std::optional<core::repository_id> Repository;
			std::optional<std::string>         FullName;
		};

		[[nodiscard]] auto parse_pull_ref(const page_json_value& aValue,
										  const bool             aRepositoryRequired)
			-> std::optional<parsed_pull_ref>
		{
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue), { "ref", "repo", "sha" })) {
				return std::nullopt;
			}
			const auto object = json_object(aValue);
			if (!nonempty_string(json_member(object, "ref"))
				|| !hexadecimal_oid(json_member(object, "sha"))) {
				return std::nullopt;
			}
			parsed_pull_ref result{
				.Ref        = text(json_member(object, "ref")),
				.Oid        = text(json_member(object, "sha")),
				.Repository = std::nullopt,
				.FullName   = std::nullopt,
			};
			const auto repository = json_member(object, "repo");
			if (json_is_null(repository)) {
				return aRepositoryRequired ? std::nullopt
										   : std::optional{ std::move(result) };
			}
			if (!json_is_object(repository)
				|| !has_required_keys(json_object(repository), { "full_name", "id" })
				|| !exact_integer(json_member(json_object(repository), "id"), true)
				|| !nonempty_string(json_member(json_object(repository), "full_name"))) {
				return std::nullopt;
			}
			result.Repository = core::repository_id{ integer(json_member(json_object(repository), "id")) };
			result.FullName   = text(json_member(json_object(repository), "full_name"));
			return result;
		}

		struct parsed_pull_request
		{
			application::github_pull_request_record Record;
			std::size_t                             ChangedFiles = 0U;
			std::size_t                             Commits      = 0U;
		};

		[[nodiscard]] auto
		parse_pull_request(const parsed_page&                           aPage,
						   const core::pull_request_request_context&    aContext,
						   const application::github_repository_record& aRepository,
						   const application::github_read_limits&       aLimits)
			-> std::expected<parsed_pull_request, github_paged_read_failure>
		{
			if (!singleton_complete(aPage) || !json_is_object(aPage.Document)
				|| !has_required_keys(json_object(aPage.Document),
									  {
										  "base",
										  "body",
										  "changed_files",
										  "commits",
										  "draft",
										  "head",
										  "id",
										  "merged",
										  "node_id",
										  "number",
										  "state",
										  "title",
										  "user",
									  })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aPage.Document);
			const auto base   = parse_pull_ref(json_member(object, "base"), true);
			const auto head   = parse_pull_ref(json_member(object, "head"), false);
			const auto author = optional_rest_actor(json_member(object, "user"));
			if (!exact_integer(json_member(object, "id"), true)
				|| !exact_integer(json_member(object, "number"), true)
				|| integer(json_member(object, "number")) > core::maximum_pull_request_number
				|| !exact_integer(json_member(object, "changed_files"))
				|| !exact_integer(json_member(object, "commits"), true)
				|| !nonempty_string(json_member(object, "node_id"))
				|| !nonempty_string(json_member(object, "title"))
				|| (!json_is_null(json_member(object, "body")) && !json_is_string(json_member(object, "body")))
				|| !one_of(json_member(object, "state"), { "open", "closed" })
				|| !json_is_bool(json_member(object, "draft")) || !json_is_bool(json_member(object, "merged"))
				|| !base.has_value() || !head.has_value() || !author.Valid
				|| (json_bool(json_member(object, "merged"))
					&& json_text(json_member(object, "state")) != QStringLiteral("closed"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto changedFiles = integer(json_member(object, "changed_files"));
			const auto commits      = integer(json_member(object, "commits"));
			if (changedFiles > aLimits.MaximumFiles
				|| commits > std::numeric_limits<std::size_t>::max()) {
				return failed(application::github_provider_failure::response_limit);
			}
			const auto id     = core::pull_request_id{ integer(json_member(object, "id")) };
			const auto number = core::pull_request_number{
				static_cast<std::uint32_t>(integer(json_member(object, "number")))
			};
			const auto repositoryFullName = aRepository.Owner + "/" + aRepository.Name;
			if (number != aContext.Number || !base->Repository.has_value()
				|| *base->Repository != aRepository.Repository
				|| !base->FullName.has_value()
				|| !ascii_equal_fold(*base->FullName, repositoryFullName)
				|| (aContext.ExpectedPullRequest.has_value()
					&& *aContext.ExpectedPullRequest != id)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			return parsed_pull_request{
				.Record = application::github_pull_request_record{
					.PullRequest         = id,
					.Repository          = aRepository.Repository,
					.Number              = number,
					.Node                = application::github_node_id{ text(json_member(object, "node_id")) },
					.Title               = text(json_member(object, "title")),
					.Body                = json_is_null(json_member(object, "body"))
						? std::string{ }
						: text(json_member(object, "body")),
					.State               = pull_request_state(json_text(json_member(object, "state")),
															  json_bool(json_member(object, "merged"))),
					.Draft               = json_bool(json_member(object, "draft")),
					.Author              = author.Actor,
					.BaseOid             = application::commit_oid{ base->Oid },
					.HeadOid             = application::commit_oid{ head->Oid },
					.BaseRef             = base->Ref,
					.HeadRef             = head->Ref,
					.ReportedCommitCount = static_cast<std::size_t>(commits),
				},
				.ChangedFiles = static_cast<std::size_t>(changedFiles),
				.Commits      = static_cast<std::size_t>(commits),
			};
		}

		[[nodiscard]] auto parse_file(const page_json_value& aValue)
			-> std::expected<application::github_file_record, github_paged_read_failure>
		{
			if (!json_is_object(aValue)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object      = json_object(aValue);
			const auto hasPrevious = object.contains("previous_filename");
			if (!has_required_keys(object,
								   { "additions", "deletions", "filename", "status" })
				|| !nonempty_string(json_member(object, "filename"))
				|| !one_of(json_member(object, "status"),
						   {
							   "added",
							   "modified",
							   "removed",
							   "renamed",
							   "copied",
							   "changed",
							   "unchanged",
						   })
				|| !exact_integer(json_member(object, "additions"))
				|| !exact_integer(json_member(object, "deletions"))
				|| (hasPrevious
					&& (!nonempty_string(json_member(object, "previous_filename"))
						|| (json_text(json_member(object, "status"))
								!= QStringLiteral("renamed")
							&& json_text(json_member(object, "status"))
								!= QStringLiteral("copied"))))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			std::optional<std::string> previousPath;
			if (hasPrevious) {
				previousPath = text(json_member(object, "previous_filename"));
			}
			return application::github_file_record{
				.Path         = text(json_member(object, "filename")),
				.PreviousPath = std::move(previousPath),
				.Change       = file_change(json_text(json_member(object, "status"))),
				.Additions    = integer(json_member(object, "additions")),
				.Deletions    = integer(json_member(object, "deletions")),
				.ContentKind  = application::github_file_content_kind::unknown,
				.ViewedState  = application::github_file_viewed_state::unavailable,
			};
		}

		[[nodiscard]] auto parse_commit(const page_json_value& aValue)
			-> std::expected<application::github_commit_record,
							 github_paged_read_failure>
		{
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue),
									  { "author", "commit", "node_id", "sha" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aValue);
			const auto author = optional_rest_actor(json_member(object, "author"));
			const auto commit = json_member(object, "commit");
			if (!hexadecimal_oid(json_member(object, "sha"))
				|| !nonempty_string(json_member(object, "node_id")) || !author.Valid
				|| !json_is_object(commit)
				|| !has_required_keys(json_object(commit), { "message" })
				|| !json_is_string(json_member(json_object(commit), "message"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto headline = text(json_member(json_object(commit), "message"));
			if (const auto newline = headline.find('\n'); newline != std::string::npos) {
				headline.resize(newline);
			}
			return application::github_commit_record{
				.Oid      = application::commit_oid{ text(json_member(object, "sha")) },
				.Node     = application::github_node_id{ text(json_member(object, "node_id")) },
				.Author   = author.Actor,
				.Headline = std::move(headline),
			};
		}

		[[nodiscard]] auto parse_review(const page_json_value& aValue)
			-> std::expected<application::github_review_record,
							 github_paged_read_failure>
		{
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue),
									  { "body",
										"commit_id",
										"id",
										"node_id",
										"state",
										"user" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aValue);
			const auto author = optional_rest_actor(json_member(object, "user"));
			if (!exact_integer(json_member(object, "id"), true)
				|| !nonempty_string(json_member(object, "node_id"))
				|| !json_is_string(json_member(object, "body")) || !author.Valid
				|| !one_of(json_member(object, "state"),
						   {
							   "PENDING",
							   "COMMENTED",
							   "APPROVED",
							   "CHANGES_REQUESTED",
							   "DISMISSED",
						   })
				|| (!json_is_null(json_member(object, "commit_id"))
					&& !hexadecimal_oid(json_member(object, "commit_id")))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			std::optional<application::commit_oid> commit;
			if (!json_is_null(json_member(object, "commit_id"))) {
				commit = application::commit_oid{ text(json_member(object, "commit_id")) };
			}
			const auto observation = parse_comment_observation(aValue, false);
			if (!observation) {
				return std::unexpected(observation.error());
			}

			return application::github_review_record{
				.DatabaseId  = integer(json_member(object, "id")),
				.Review      = application::github_review_id{ text(json_member(object, "node_id")) },
				.Author      = author.Actor,
				.State       = review_state(json_text(json_member(object, "state"))),
				.Commit      = std::move(commit),
				.Body        = text(json_member(object, "body")),
				.Observation = *observation,
			};
		}

		[[nodiscard]] auto parse_issue_comment(const page_json_value& aValue)
			-> std::expected<application::github_issue_comment_record,
							 github_paged_read_failure>
		{
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue), { "body", "node_id", "user" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aValue);
			const auto author = optional_rest_actor(json_member(object, "user"));
			if (!nonempty_string(json_member(object, "node_id"))
				|| !json_is_string(json_member(object, "body")) || !author.Valid) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto observation = parse_comment_observation(aValue, false);
			if (!observation) {
				return std::unexpected(observation.error());
			}

			return application::github_issue_comment_record{
				.Comment     = application::github_comment_id{ text(json_member(object, "node_id")) },
				.Author      = author.Actor,
				.Body        = text(json_member(object, "body")),
				.Deleted     = false,
				.Observation = *observation,
			};
		}

		/// Parses one entry of the REST inline review-comment listing.
		///
		/// The identity used here is the REST numeric id rendered as text
		/// rather than the node id, because `in_reply_to_id` names the numeric
		/// id and reply linkage is only checkable when both ends live in one
		/// identifier space. `position` is required and carries the anchor
		/// fact: the service returns null once the comment's line has left the
		/// diff, which is what outdated means on this surface. Nothing here
		/// reads resolution, viewer, viewed, or actionability, because the
		/// response carries none of them and inferring one is prohibited.
		[[nodiscard]] auto parse_review_comment(const page_json_value& aValue)
			-> std::expected<application::github_review_comment_record,
							 github_paged_read_failure>
		{
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue),
									  {
										  "body",
										  "id",
										  "path",
										  "position",
										  "user",
									  })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aValue);
			const auto author = optional_rest_actor(json_member(object, "user"));
			const auto reply  = json_member(object, "in_reply_to_id");
			if (!exact_integer(json_member(object, "id"), true) || !author.Valid
				|| !json_is_string(json_member(object, "body"))
				|| !nonempty_string(json_member(object, "path"))
				|| (!json_is_null(json_member(object, "position"))
					&& !exact_integer(json_member(object, "position"), true))
				|| (!json_is_undefined(json_member(object, "line"))
					&& !json_is_null(json_member(object, "line"))
					&& !exact_integer(json_member(object, "line"), true))
				|| (!json_is_undefined(json_member(object, "original_line"))
					&& !json_is_null(json_member(object, "original_line"))
					&& !exact_integer(json_member(object, "original_line"), true))
				|| (!json_is_undefined(reply) && !json_is_null(reply)
					&& !exact_integer(reply, true))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto replyTo = std::optional<application::github_comment_id>{ };
			if (json_is_number(reply)) {
				replyTo = application::github_comment_id{
					std::to_string(integer(reply))
				};
			}
			const auto observation = parse_comment_observation(aValue, false);
			if (!observation) {
				return std::unexpected(observation.error());
			}

			return application::github_review_comment_record{
				.Comment = application::github_comment_id{
					std::to_string(integer(json_member(object, "id"))) },
				.ReplyTo      = std::move(replyTo),
				.Author       = author.Actor,
				.Body         = text(json_member(object, "body")),
				.Path         = text(json_member(object, "path")),
				.Line         = optional_line(json_member(object, "line")),
				.OriginalLine = optional_line(json_member(object, "original_line")),
				.Outdated     = json_is_null(json_member(object, "position")),
				.Deleted      = false,
				.Observation  = *observation,
			};
		}

		[[nodiscard]] auto parse_check(const page_json_value& aValue,
									   const std::string_view aHeadOid)
			-> std::expected<application::github_check_record,
							 github_paged_read_failure>
		{
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue),
									  {
										  "conclusion",
										  "head_sha",
										  "name",
										  "node_id",
										  "status",
									  })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object    = json_object(aValue);
			const auto completed = json_text(json_member(object, "status")) == QStringLiteral("completed");
			if (!nonempty_string(json_member(object, "node_id"))
				|| !nonempty_string(json_member(object, "name"))
				|| !hexadecimal_oid(json_member(object, "head_sha"))
				|| text(json_member(object, "head_sha")) != aHeadOid
				|| !one_of(json_member(object, "status"),
						   {
							   "queued",
							   "in_progress",
							   "completed",
							   "waiting",
							   "requested",
							   "pending",
						   })
				|| (!json_is_null(json_member(object, "conclusion"))
					&& !one_of(json_member(object, "conclusion"),
							   {
								   "success",
								   "failure",
								   "neutral",
								   "cancelled",
								   "skipped",
								   "timed_out",
								   "action_required",
								   "stale",
							   }))
				|| (completed != !json_is_null(json_member(object, "conclusion")))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto result             = application::github_check_record{ };
			result.Check            = application::github_check_id{ text(json_member(object, "node_id")) };
			result.Name             = text(json_member(object, "name"));
			result.Status           = check_status(json_text(json_member(object, "status")));
			result.Conclusion       = check_conclusion(json_member(object, "conclusion"));
			result.EvaluatedOid     = application::commit_oid{ std::string{ aHeadOid } };
			const auto optionalText = [](const page_json_value& aField) { return json_is_string(aField) ? text(aField) : std::string{ }; };
			result.DetailsUrl       = optionalText(json_member(object, "details_url"));
			if (result.DetailsUrl.empty()) {
				result.DetailsUrl = optionalText(json_member(object, "html_url"));
			}
			if (const auto output = json_member(object, "output"); json_is_object(output)) {
				result.Description = optionalText(json_member(json_object(output), "title"));
				const auto summary = optionalText(json_member(json_object(output), "summary"));
				if (!summary.empty()) {
					result.Description += (result.Description.empty() ? "" : "\n") + summary;
				}
			}
			if (const auto app = json_member(object, "app"); json_is_object(app)) {
				result.Producer = optionalText(json_member(json_object(app), "slug"));
				if (exact_integer(json_member(json_object(app), "id"), true)) {
					result.AppId = integer(json_member(json_object(app), "id"));
				}
			}
			return result;
		}

		[[nodiscard]] auto parse_commit_status(const page_json_value& aValue, const std::string_view aHeadOid)
			-> std::expected<application::github_check_record, github_paged_read_failure>
		{
			if (!json_is_object(aValue)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aValue);
			if (!nonempty_string(json_member(object, "node_id")) || !nonempty_string(json_member(object, "context"))
				|| !one_of(json_member(object, "state"), { "pending", "success", "failure", "error" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto result            = application::github_check_record{ };
			result.Check           = application::github_check_id{ "status:" + text(json_member(object, "node_id")) };
			result.Name            = text(json_member(object, "context"));
			const auto state       = text(json_member(object, "state"));
			result.Status          = state == "pending" ? application::github_check_status::pending : application::github_check_status::completed;
			result.Conclusion      = state == "pending" ? application::github_check_conclusion::none : state == "success" ? application::github_check_conclusion::success
																														  : application::github_check_conclusion::failure;
			result.Source          = application::github_check_source::commit_status;
			result.StatusError     = state == "error";
			result.EvaluatedOid    = application::commit_oid{ std::string{ aHeadOid } };
			const auto description = json_member(object, "description");
			const auto target      = json_member(object, "target_url");
			if ((!json_is_null(description) && !json_is_string(description)) || (!json_is_null(target) && !json_is_string(target))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			if (json_is_string(description)) {
				result.Description = text(description);
			}
			if (json_is_string(target)) {
				result.DetailsUrl = text(target);
			}
			if (const auto creator = json_member(object, "creator"); json_is_object(creator)) {
				const auto login = json_member(json_object(creator), "login");
				if (json_is_string(login)) {
					result.Producer = text(login);
				}
			}
			return result;
		}

		[[nodiscard]] auto
		parse_search_result(const page_json_value&                       aValue,
							const application::github_repository_record& aRepository,
							const validated_github_host_profile&         aProfile)
			-> std::expected<application::github_search_result_record,
							 github_paged_read_failure>
		{
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue),
									  { "draft",
										"html_url",
										"node_id",
										"number",
										"pull_request",
										"repository_url",
										"state",
										"title" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object      = json_object(aValue);
			const auto number      = uint32(json_member(object, "number"));
			const auto pullRequest = json_member(object, "pull_request");
			if (!number.has_value() || *number == 0U
				|| *number > core::maximum_pull_request_number
				|| !nonempty_string(json_member(object, "node_id"))
				|| !nonempty_string(json_member(object, "title"))
				|| !nonempty_string(json_member(object, "repository_url"))
				|| !nonempty_string(json_member(object, "html_url")) || !json_is_object(pullRequest)
				|| !has_required_keys(json_object(pullRequest), { "url" })
				|| !nonempty_string(json_member(json_object(pullRequest), "url"))
				|| !one_of(json_member(object, "state"), { "open" })
				|| !json_is_bool(json_member(object, "draft"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto expectedRepositoryUrl = std::string{ aProfile.rest_root() }
				+ "/repos/" + aRepository.Owner + "/"
				+ aRepository.Name;
			const auto expectedHtmlUrl = "https://" + std::string{ aProfile.hostname() } + "/" + aRepository.Owner
				+ "/" + aRepository.Name + "/pull/" + std::to_string(*number);
			const auto expectedPullRequestUrl = expectedRepositoryUrl + "/pulls/" + std::to_string(*number);
			if (text(json_member(object, "repository_url")) != expectedRepositoryUrl
				|| text(json_member(object, "html_url")) != expectedHtmlUrl
				|| text(json_member(json_object(pullRequest), "url"))
					!= expectedPullRequestUrl) {
				return failed(application::github_provider_failure::malformed_response);
			}
			return application::github_search_result_record{
				.Repository      = aRepository.Repository,
				.PullRequestNode = application::github_node_id{ text(json_member(object, "node_id")) },
				.Number          = core::pull_request_number{ *number },
				.Title           = text(json_member(object, "title")),
				.State           = application::github_pull_request_state::open,
				.Draft           = json_bool(json_member(object, "draft")),
			};
		}

		template <typename Record, typename Parser>
		[[nodiscard]] auto
		read_rest_array_collection(production_read_execution&           aExecution,
								   const validated_github_host_profile& aProfile,
								   semantic_read_request                aSemantic,
								   std::vector<Record>&                 aOutput,
								   const std::size_t                    aMaximumRecords,
								   const std::size_t                    aMaximumPageRecords,
								   Parser&&                             aParser)
			-> std::expected<void, github_paged_read_failure>
		{
			while (true) {
				auto page = aExecution.fetch(aSemantic);
				if (!page.has_value()) {
					return std::unexpected{ page.error() };
				}
				if (!json_is_array(page->Document)) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto records = json_array(page->Document);
				const auto count   = static_cast<std::size_t>(records.size());
				if (count > aMaximumPageRecords || count > aMaximumRecords
					|| aOutput.size() > aMaximumRecords - count) {
					return failed(application::github_provider_failure::response_limit);
				}
				aOutput.reserve(aOutput.size() + count);
				for (const auto& value : records) {
					auto record = aParser(value);
					if (!record.has_value()) {
						return std::unexpected{ record.error() };
					}
					aOutput.push_back(std::move(*record));
				}
				const auto repository = aExecution.bound_repository();
				if (!repository.has_value()) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto hasNext = rest_has_next_page(aProfile, *repository, *page, count);
				if (!hasNext.has_value()) {
					return std::unexpected{ hasNext.error() };
				}
				if (!*hasNext) {
					return { };
				}
				if (aOutput.size() == aMaximumRecords) {
					return failed(application::github_provider_failure::response_limit);
				}
				if (aSemantic.Page == std::numeric_limits<std::uint32_t>::max()) {
					return failed(application::github_provider_failure::response_limit);
				}
				++aSemantic.Page;
			}
		}

		[[nodiscard]] auto
		perform_graphql_projection(production_read_execution&                 aExecution,
								   const server_contract                      aServer,
								   const core::pull_request_request_context&  aContext,
								   const application::github_read_shape&      aShape,
								   application::github_pull_request_snapshot& aSnapshot,
								   const github_paged_read_limits&            aLimits)
			-> std::expected<void, github_paged_read_failure>
		{
			if ((aShape.ResolvePermission || aShape.UnresolvePermission)
				&& !aShape.ReviewThreads) {
				return failed(application::github_provider_failure::malformed_response);
			}
			if (!aShape.ViewedFileState && !aShape.ReviewThreads) {
				aSnapshot.ReviewThreadsAvailability = application::github_collection_availability::unavailable;
				return { };
			}

			std::uint32_t sequence        = 1U;
			auto          initialSemantic = semantic_request_for(
				aServer, aContext, read_operation::list_review_threads, sequence);
			initialSemantic.GraphqlPage = github_graphql_page_request{
				.Kind        = github_graphql_page_kind::initial,
				.Shape       = aShape,
				.AfterCursor = { },
				.ThreadId    = { },
			};
			auto initial = aExecution.fetch(initialSemantic);
			if (!initial.has_value()) {
				return std::unexpected{ initial.error() };
			}
			page_json_object initialData;
			page_json_object initialRepository;
			page_json_object initialPullRequest;
			if (const auto identity = graphql_repository_and_pull_request(*initial,
																		  aSnapshot.Repository,
																		  aSnapshot.PullRequest,
																		  &initialData,
																		  &initialRepository,
																		  &initialPullRequest);
				!identity.has_value()) {
				return std::unexpected{ identity.error() };
			}
			std::vector<std::string_view> initialPullKeys{ "headRefOid", "id" };
			if (aShape.ViewedFileState) {
				initialPullKeys.emplace_back("files");
			}
			if (aShape.ReviewThreads) {
				initialPullKeys.emplace_back("reviewThreads");
			}
			if (!has_exact_keys(initialPullRequest, initialPullKeys)) {
				return failed(application::github_provider_failure::malformed_response);
			}

			if (aShape.ViewedFileState) {
				auto files = parse_graphql_connection(
					json_member(initialPullRequest, "files"), aLimits);
				if (!files.has_value()) {
					return std::unexpected{ files.error() };
				}
				if (files->TotalCount > aLimits.Records.MaximumFiles
					|| files->TotalCount != aSnapshot.Files.size()) {
					return failed(
						files->TotalCount > aLimits.Records.MaximumFiles
							? application::github_provider_failure::response_limit
							: application::github_provider_failure::malformed_response);
				}
				std::unordered_map<std::string, application::github_file_viewed_state>
					viewed;
				viewed.reserve(static_cast<std::size_t>(files->TotalCount));
				const auto appendFileNodes = [&viewed](const page_json_array& aNodes)
					-> std::expected<void, github_paged_read_failure> {
					for (const auto& value : aNodes) {
						if (!json_is_object(value)
							|| !has_exact_keys(json_object(value),
											   { "path", "viewerViewedState" })
							|| !nonempty_string(json_member(json_object(value), "path"))) {
							return failed(application::github_provider_failure::
											  malformed_response);
						}
						const auto object = json_object(value);
						const auto state  = viewed_state(json_text(json_member(object, "viewerViewedState")));
						if (!state.has_value()
							|| !viewed.emplace(text(json_member(object, "path")), *state)
									.second) {
							return failed(application::github_provider_failure::
											  malformed_response);
						}
					}
					return { };
				};
				if (const auto append = appendFileNodes(files->Nodes);
					!append.has_value()) {
					return std::unexpected{ append.error() };
				}
				std::unordered_set<std::string> seenCursors;
				auto                            nextCursor = std::move(files->NextCursor);
				while (nextCursor.has_value()) {
					if (!seenCursors.insert(*nextCursor).second) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					if (sequence == maximum_graphql_sequence) {
						return failed(
							application::github_provider_failure::response_limit);
					}
					++sequence;
					auto semantic = semantic_request_for(
						aServer, aContext, read_operation::list_review_threads, sequence);
					semantic.GraphqlPage = github_graphql_page_request{
						.Kind        = github_graphql_page_kind::files,
						.Shape       = aShape,
						.AfterCursor = *nextCursor,
						.ThreadId    = { },
					};
					auto page = aExecution.fetch(semantic);
					if (!page.has_value()) {
						return std::unexpected{ page.error() };
					}
					page_json_object data;
					page_json_object repository;
					page_json_object pullRequest;
					if (const auto identity = graphql_repository_and_pull_request(*page,
																				  aSnapshot.Repository,
																				  aSnapshot.PullRequest,
																				  &data,
																				  &repository,
																				  &pullRequest);
						!identity.has_value()) {
						return std::unexpected{ identity.error() };
					}
					if (!has_exact_keys(
							pullRequest, { "files", "headRefOid", "id" })) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					auto connection = parse_graphql_connection(json_member(pullRequest, "files"), aLimits);
					if (!connection.has_value()) {
						return std::unexpected{ connection.error() };
					}
					if (connection->TotalCount != files->TotalCount) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					if (const auto append = appendFileNodes(connection->Nodes);
						!append.has_value()) {
						return std::unexpected{ append.error() };
					}
					if (viewed.size() > files->TotalCount) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					nextCursor = std::move(connection->NextCursor);
				}
				if (viewed.size() != files->TotalCount) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				for (auto& file : aSnapshot.Files) {
					const auto found = viewed.find(file.Path);
					if (found == viewed.end()) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					file.ViewedState = found->second;
					viewed.erase(found);
				}
				if (!viewed.empty()) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
			}

			if (!aShape.ReviewThreads) {
				aSnapshot.ReviewThreadsAvailability = application::github_collection_availability::unavailable;
				return { };
			}

			auto threads = parse_graphql_connection(
				json_member(initialPullRequest, "reviewThreads"), aLimits);
			if (!threads.has_value()) {
				return std::unexpected{ threads.error() };
			}
			if (threads->TotalCount > aLimits.Records.MaximumReviewThreads) {
				return failed(application::github_provider_failure::response_limit);
			}
			aSnapshot.ReviewThreads.reserve(
				static_cast<std::size_t>(threads->TotalCount));
			std::vector<thread_pagination_state> pagination;
			pagination.reserve(static_cast<std::size_t>(threads->TotalCount));
			std::unordered_set<std::string> threadIds;
			threadIds.reserve(static_cast<std::size_t>(threads->TotalCount));
			std::size_t totalComments     = 0U;
			const auto  appendThreadNodes = [&](const page_json_array& aNodes)
				-> std::expected<void, github_paged_read_failure> {
				for (const auto& value : aNodes) {
					auto parsed = parse_graphql_thread(value, aShape, aLimits);
					if (!parsed.has_value()) {
						return std::unexpected{ parsed.error() };
					}
					const auto id = std::string{ parsed->Record.Thread.value() };
					if (!threadIds.insert(id).second) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					if (parsed->TotalComments > aLimits.Records.MaximumTotalComments
						|| totalComments
							> aLimits.Records.MaximumTotalComments
								- static_cast<std::size_t>(parsed->TotalComments)) {
						return failed(
							application::github_provider_failure::response_limit);
					}
					totalComments += static_cast<std::size_t>(parsed->TotalComments);
					thread_pagination_state state{
						.TotalComments = parsed->TotalComments,
						.NextCursor    = std::move(parsed->NextCommentsCursor),
						.SeenCursors   = { },
					};
					aSnapshot.ReviewThreads.push_back(std::move(parsed->Record));
					pagination.push_back(std::move(state));
				}
				return { };
			};
			if (const auto append = appendThreadNodes(threads->Nodes);
				!append.has_value()) {
				return std::unexpected{ append.error() };
			}

			std::unordered_set<std::string> threadCursors;
			auto                            nextThreadCursor = std::move(threads->NextCursor);
			while (nextThreadCursor.has_value()) {
				if (!threadCursors.insert(*nextThreadCursor).second) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				if (sequence == maximum_graphql_sequence) {
					return failed(application::github_provider_failure::response_limit);
				}
				++sequence;
				auto semantic = semantic_request_for(
					aServer, aContext, read_operation::list_review_threads, sequence);
				semantic.GraphqlPage = github_graphql_page_request{
					.Kind        = github_graphql_page_kind::threads,
					.Shape       = aShape,
					.AfterCursor = *nextThreadCursor,
					.ThreadId    = { },
				};
				auto page = aExecution.fetch(semantic);
				if (!page.has_value()) {
					return std::unexpected{ page.error() };
				}
				page_json_object data;
				page_json_object repository;
				page_json_object pullRequest;
				if (const auto identity = graphql_repository_and_pull_request(*page,
																			  aSnapshot.Repository,
																			  aSnapshot.PullRequest,
																			  &data,
																			  &repository,
																			  &pullRequest);
					!identity.has_value()) {
					return std::unexpected{ identity.error() };
				}
				if (!has_exact_keys(
						pullRequest, { "headRefOid", "id", "reviewThreads" })) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				auto connection = parse_graphql_connection(
					json_member(pullRequest, "reviewThreads"), aLimits);
				if (!connection.has_value()) {
					return std::unexpected{ connection.error() };
				}
				if (connection->TotalCount != threads->TotalCount) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				if (const auto append = appendThreadNodes(connection->Nodes);
					!append.has_value()) {
					return std::unexpected{ append.error() };
				}
				if (aSnapshot.ReviewThreads.size() > threads->TotalCount) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				nextThreadCursor = std::move(connection->NextCursor);
			}
			if (aSnapshot.ReviewThreads.size() != threads->TotalCount) {
				return failed(application::github_provider_failure::malformed_response);
			}

			for (std::size_t index = 0U; index < pagination.size(); ++index) {
				auto& state  = pagination[index];
				auto& thread = aSnapshot.ReviewThreads[index];
				while (state.NextCursor.has_value()) {
					if (!state.SeenCursors.insert(*state.NextCursor).second) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					if (sequence == maximum_graphql_sequence) {
						return failed(
							application::github_provider_failure::response_limit);
					}
					++sequence;
					auto semantic = semantic_request_for(
						aServer, aContext, read_operation::list_review_threads, sequence);
					semantic.GraphqlPage = github_graphql_page_request{
						.Kind        = github_graphql_page_kind::comments,
						.Shape       = aShape,
						.AfterCursor = *state.NextCursor,
						.ThreadId    = std::string{ thread.Thread.value() },
					};
					auto page = aExecution.fetch(semantic);
					if (!page.has_value()) {
						return std::unexpected{ page.error() };
					}
					if (!singleton_complete(*page) || !json_is_object(page->Document)
						|| !graphql_success_envelope(json_object(page->Document))) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					const auto dataValue = json_member(json_object(page->Document), "data");
					if (!json_is_object(dataValue)
						|| !has_exact_keys(json_object(dataValue),
										   { "node", "repository" })) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					const auto data            = json_object(dataValue);
					const auto repositoryValue = json_member(data, "repository");
					if (!json_is_object(repositoryValue)
						|| !has_exact_keys(json_object(repositoryValue),
										   { "id", "pullRequest" })
						|| text(json_member(json_object(repositoryValue), "id"))
							!= aSnapshot.Repository.Node.value()) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					const auto pullRequestValue = json_member(json_object(repositoryValue), "pullRequest");
					if (!json_is_object(pullRequestValue)
						|| !has_exact_keys(
							json_object(pullRequestValue), { "headRefOid", "id" })
						|| text(json_member(json_object(pullRequestValue), "id"))
							!= aSnapshot.PullRequest.Node.value()
						|| text(json_member(json_object(pullRequestValue), "headRefOid"))
							!= aSnapshot.PullRequest.HeadOid.value()) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					const auto nodeValue = json_member(data, "node");
					if (!json_is_object(nodeValue)
						|| !has_exact_keys(json_object(nodeValue), { "comments", "id" })
						|| text(json_member(json_object(nodeValue), "id"))
							!= thread.Thread.value()) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					auto connection = parse_graphql_connection(
						json_member(json_object(nodeValue), "comments"), aLimits);
					if (!connection.has_value()) {
						return std::unexpected{ connection.error() };
					}
					if (connection->TotalCount != state.TotalComments) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					for (const auto& value : connection->Nodes) {
						auto comment = parse_graphql_comment(value);
						if (!comment.has_value()) {
							return std::unexpected{ comment.error() };
						}
						if (comment->Path != thread.Path
							|| (thread.Subject
									== application::github_review_thread_subject::file
								&& (comment->Line.has_value()
									|| comment->OriginalLine.has_value()))) {
							return failed(application::github_provider_failure::
											  malformed_response);
						}
						thread.Comments.push_back(std::move(*comment));
					}
					if (thread.Comments.size() > state.TotalComments) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					state.NextCursor = std::move(connection->NextCursor);
				}
				if (thread.Comments.size() != state.TotalComments) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
			}
			aSnapshot.ReviewThreadsAvailability = application::github_collection_availability::available;
			return { };
		}

		using observation = application::github_observation_availability;

		[[nodiscard]] auto fatal_optional_read_failure(const github_paged_read_failure& aFailure) -> bool
		{
			if (std::holds_alternative<github_paged_read_cancelled>(aFailure)) {
				return true;
			}
			const auto* error = std::get_if<application::github_provider_error>(&aFailure);
			// Lost authentication or account binding invalidates the entire read,
			// even when only an optional explanation discovered it.
			return error && error->Failure == application::github_provider_failure::authentication;
		}

		[[nodiscard]] auto observation_failure(const github_paged_read_failure& aFailure) -> observation
		{
			const auto* failure = std::get_if<application::github_provider_error>(&aFailure);
			if (!failure) {
				return observation::failed;
			}
			switch (failure->Failure) {
			case application::github_provider_failure::permission:
			case application::github_provider_failure::authentication:         return observation::permission;
			case application::github_provider_failure::api_version:
			case application::github_provider_failure::unsupported_capability: return observation::unsupported;
			case application::github_provider_failure::response_limit:
			case application::github_provider_failure::malformed_response:     return observation::incomplete;
			default:                                                           return observation::failed;
			}
		}

		[[nodiscard]] auto merge_pull_object(const parsed_page& aPage, const application::github_pull_request_snapshot& aSnapshot)
			-> std::expected<page_json_object, github_paged_read_failure>
		{
			auto data = page_json_object{ }, repository = page_json_object{ }, pull = page_json_object{ };
			if (const auto identity = graphql_repository_and_pull_request(aPage, aSnapshot.Repository, aSnapshot.PullRequest, &data, &repository, &pull); !identity) {
				return std::unexpected{ identity.error() };
			}
			if (text(json_member(pull, "baseRefOid")) != aSnapshot.PullRequest.BaseOid.value() || text(json_member(pull, "baseRefName")) != aSnapshot.PullRequest.BaseRef) {
				return failed(application::github_provider_failure::malformed_response);
			}
			return pull;
		}

		[[nodiscard]] auto read_merge_assessment(production_read_execution& aExecution, server_contract aServer, const core::pull_request_request_context& aContext, application::github_pull_request_snapshot& aSnapshot)
			-> std::expected<void, github_paged_read_failure>
		{
			const auto page = aExecution.fetch(semantic_request_for(aServer, aContext, read_operation::get_merge_assessment));
			if (!page) {
				return std::unexpected{ page.error() };
			}
			const auto pull = merge_pull_object(*page, aSnapshot);
			if (!pull) {
				return std::unexpected{ pull.error() };
			}
			const auto state = json_member(*pull, "mergeStateStatus"), review = json_member(*pull, "reviewDecision"), automatic = json_member(*pull, "autoMergeRequest");
			if (!one_of(state, { "UNKNOWN", "BEHIND", "BLOCKED", "CLEAN", "DIRTY", "DRAFT", "HAS_HOOKS", "UNSTABLE" }) || (!json_is_null(review) && !one_of(review, { "APPROVED", "CHANGES_REQUESTED", "REVIEW_REQUIRED" })) || (!json_is_null(automatic) && (!json_is_object(automatic) || !has_exact_keys(json_object(automatic), { "enabledAt" })))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto value             = text(state);
			using assessment             = application::github_merge_assessment;
			aSnapshot.Merge.Assessment   = value == "BEHIND" ? assessment::behind : value == "BLOCKED" ? assessment::blocked
				: value == "CLEAN"                                                                     ? assessment::clean
				: value == "DIRTY"                                                                     ? assessment::conflicts
				: value == "DRAFT"                                                                     ? assessment::draft
				: value == "HAS_HOOKS"                                                                 ? assessment::hooks
				: value == "UNSTABLE"                                                                  ? assessment::unstable
																									   : assessment::unknown;
			using decision               = application::github_review_decision;
			aSnapshot.Merge.Review       = text(review) == "APPROVED" ? decision::approved : text(review) == "CHANGES_REQUESTED" ? decision::changes_requested
				: text(review) == "REVIEW_REQUIRED"                                                                              ? decision::required
																																 : decision::unknown;
			aSnapshot.Merge.AutoMerge    = !json_is_null(automatic);
			aSnapshot.Merge.Availability = observation::available;
			return { };
		}

		[[nodiscard]] auto parse_pending_request(const page_json_value& aValue) -> std::expected<application::github_review_request_record, github_paged_read_failure>
		{
			if (!json_is_object(aValue)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aValue);
			if (!has_exact_keys(object, { "id", "asCodeOwner", "requestedReviewer" }) || !nonempty_string(json_member(object, "id")) || !json_is_bool(json_member(object, "asCodeOwner"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto result        = application::github_review_request_record{ };
			result.Request     = text(json_member(object, "id"));
			result.AsCodeOwner = json_bool(json_member(object, "asCodeOwner"));
			const auto actor   = json_member(object, "requestedReviewer");
			if (json_is_null(actor)) {
				return result;
			}
			if (!json_is_object(actor) || !nonempty_string(json_member(json_object(actor), "__typename"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto fields = json_object(actor);
			const auto kind   = text(json_member(fields, "__typename"));
			if (kind != "Team" && kind != "User") {
				return result;
			}
			if (!nonempty_string(json_member(fields, "id"))) {
				return result;
			}
			result.Actor = text(json_member(fields, "id"));
			result.Login = text(json_member(fields, kind == "Team" ? "slug" : "login"));
			if (result.Login.empty()) {
				return failed(application::github_provider_failure::malformed_response);
			}
			if (kind == "Team") {
				const auto org = json_member(fields, "organization");
				if (!json_is_object(org) || !nonempty_string(json_member(json_object(org), "login"))) {
					result.Actor.clear();
					result.Login.clear();
					return result;
				}
				result.Kind         = application::github_requested_actor_kind::team;
				result.Organization = text(json_member(json_object(org), "login"));
				result.Name         = text(json_member(fields, "name"));
			} else {
				result.Kind = application::github_requested_actor_kind::user;
			}
			return result;
		}

		[[nodiscard]] auto read_pending_requests(production_read_execution& aExecution, server_contract aServer, const core::pull_request_request_context& aContext, application::github_pull_request_snapshot& aSnapshot, const github_paged_read_limits& aLimits)
			-> std::expected<void, github_paged_read_failure>
		{
			auto                         output  = std::vector<application::github_review_request_record>{ };
			auto                         cursors = std::unordered_set<std::string>{ }, requests = std::unordered_set<std::string>{ }, actors = std::unordered_set<std::string>{ };
			auto                         semantic = semantic_request_for(aServer, aContext, read_operation::list_review_requests);
			std::optional<std::uint64_t> expected;
			while (true) {
				const auto page = aExecution.fetch(semantic);
				if (!page) {
					return std::unexpected{ page.error() };
				}
				const auto pull = merge_pull_object(*page, aSnapshot);
				if (!pull) {
					return std::unexpected{ pull.error() };
				}
				const auto connection = parse_graphql_connection(json_member(*pull, "reviewRequests"), aLimits);
				if (!connection) {
					return std::unexpected{ connection.error() };
				}
				if ((expected && *expected != connection->TotalCount) || connection->TotalCount > 1000U) {
					return failed(application::github_provider_failure::response_limit);
				}
				expected = connection->TotalCount;
				for (const auto& node : connection->Nodes) {
					const auto record = parse_pending_request(node);
					if (!record) {
						return std::unexpected{ record.error() };
					}
					if (!requests.insert(record->Request).second || (!record->Actor.empty() && !actors.insert(record->Actor).second)) {
						return failed(application::github_provider_failure::malformed_response);
					}
					output.push_back(*record);
				}
				if (!connection->NextCursor) {
					if (output.size() != *expected) {
						return failed(application::github_provider_failure::malformed_response);
					}
					aSnapshot.ReviewRequests             = std::move(output);
					aSnapshot.ReviewRequestsAvailability = observation::available;
					return { };
				}
				if (semantic.Page == 10U || !cursors.insert(*connection->NextCursor).second || output.size() >= *expected) {
					return failed(application::github_provider_failure::response_limit);
				}
				semantic.ReviewCursor = *connection->NextCursor;
				++semantic.Page;
			}
		}

		[[nodiscard]] auto read_rest_pending_requests(production_read_execution& aExecution, server_contract aServer, const core::pull_request_request_context& aContext, application::github_pull_request_snapshot& aSnapshot) -> std::expected<void, github_paged_read_failure>
		{
			const auto page = aExecution.fetch(semantic_request_for(aServer, aContext, read_operation::list_requested_reviewers));
			if (!page) {
				return std::unexpected{ page.error() };
			}
			if (!singleton_complete(*page) || !json_is_object(page->Document)) {
				return failed(application::github_provider_failure::response_limit);
			}
			const auto object = json_object(page->Document);
			if (!json_is_array(json_member(object, "users")) || !json_is_array(json_member(object, "teams"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto records = std::vector<application::github_review_request_record>{ };
			auto seen    = std::unordered_set<std::string>{ };
			for (const auto& key : { "users", "teams" }) {
				for (const auto& actor : json_array(json_member(object, key))) {
					if (!json_is_object(actor)) {
						return failed(application::github_provider_failure::malformed_response);
					}
					const auto fields = json_object(actor);
					auto       result = application::github_review_request_record{ };
					result.Actor      = text(json_member(fields, "node_id"));
					result.Login      = text(json_member(fields, std::string_view{ key } == "users" ? "login" : "slug"));
					if (result.Actor.empty() || result.Login.empty() || !seen.insert(result.Actor).second) {
						return failed(application::github_provider_failure::malformed_response);
					}
					result.Request = "rest:" + result.Actor;
					result.Kind    = std::string_view{ key } == "users" ? application::github_requested_actor_kind::user : application::github_requested_actor_kind::team;
					if (result.Kind == application::github_requested_actor_kind::team) {
						result.Organization = aSnapshot.Repository.Owner;
						result.Name         = text(json_member(fields, "name"));
					}
					records.push_back(std::move(result));
				}
			}
			aSnapshot.ReviewRequests              = std::move(records);
			aSnapshot.ReviewRequestActorsObserved = true;
			// REST establishes actor identities, never code-owner origin. Keep the
			// richer connection's actual availability beside this usable roster.
			return { };
		}

		[[nodiscard]] auto parse_required_checks(const page_json_value& aValue, const std::string_view aAppKey) -> std::expected<std::vector<application::github_required_check>, github_paged_read_failure>
		{
			if (!json_is_array(aValue)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto checks = std::vector<application::github_required_check>{ };
			for (const auto& value : json_array(aValue)) {
				if (!json_is_object(value) || !nonempty_string(json_member(json_object(value), "context"))) {
					return failed(application::github_provider_failure::malformed_response);
				}
				auto check     = application::github_required_check{ };
				check.Context  = text(json_member(json_object(value), "context"));
				const auto app = json_member(json_object(value), aAppKey);
				if (!json_is_null(app) && !json_is_undefined(app)) {
					// Classic -1 permits any app, as does a null integration.
					std::int64_t appNumber = 0;
					if (glz::read<page_json_options{ }>(appNumber, app.Json) || appNumber != -1) {
						if (!exact_integer(app, true)) {
							return failed(application::github_provider_failure::malformed_response);
						}
						check.AppId = integer(app);
					}
				}
				checks.push_back(std::move(check));
			}
			return checks;
		}

		[[nodiscard]] auto parse_branch_rule(const page_json_value& aValue) -> std::expected<application::github_branch_requirement, github_paged_read_failure>
		{
			if (!json_is_object(aValue)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aValue);
			if (!nonempty_string(json_member(object, "type")) || !one_of(json_member(object, "ruleset_source_type"), { "Repository", "Organization", "Enterprise" }) || !nonempty_string(json_member(object, "ruleset_source")) || !exact_integer(json_member(object, "ruleset_id"), true)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto result       = application::github_branch_requirement{ };
			result.Type       = text(json_member(object, "type"));
			result.Source     = text(json_member(object, "ruleset_source"));
			result.SourceKind = text(json_member(object, "ruleset_source_type"));
			result.RulesetId  = integer(json_member(object, "ruleset_id"));
			const auto params = json_object(json_member(object, "parameters"));
			if (result.Type == "pull_request") {
				const auto minimum = json_member(params, "required_approving_review_count");
				if (!exact_integer(minimum) || integer(minimum) > 100U || !json_is_bool(json_member(params, "require_code_owner_review")) || !json_is_bool(json_member(params, "require_last_push_approval")) || !json_is_bool(json_member(params, "dismiss_stale_reviews_on_push"))) {
					return failed(application::github_provider_failure::malformed_response);
				}
				result.ApprovalMinimum     = static_cast<std::uint32_t>(integer(minimum));
				result.CodeOwners          = json_bool(json_member(params, "require_code_owner_review"));
				result.LastPushApproval    = json_bool(json_member(params, "require_last_push_approval"));
				result.DismissStaleReviews = json_bool(json_member(params, "dismiss_stale_reviews_on_push"));
				if (json_is_bool(json_member(params, "required_review_thread_resolution")) && json_bool(json_member(params, "required_review_thread_resolution"))) {
					result.Type = "pull_request_and_conversation_resolution";
				}
				const auto teams = json_member(params, "required_reviewers");
				if (!json_is_undefined(teams)) {
					if (!json_is_array(teams)) {
						return failed(application::github_provider_failure::malformed_response);
					}
					for (const auto& team : json_array(teams)) {
						const auto fields = json_object(team), reviewer = json_object(json_member(fields, "reviewer"));
						const auto count = json_member(fields, "minimum_approvals");
						if (text(json_member(reviewer, "type")) != "Team" || !exact_integer(json_member(reviewer, "id"), true) || !exact_integer(count) || integer(count) > 100U) {
							return failed(application::github_provider_failure::malformed_response);
						}
						result.RequiredTeams.emplace_back(integer(json_member(reviewer, "id")), static_cast<std::uint32_t>(integer(count)));
					}
				}
			} else if (result.Type == "required_status_checks") {
				const auto checks = parse_required_checks(json_member(params, "required_status_checks"), "integration_id");
				if (!checks) {
					return std::unexpected{ checks.error() };
				}
				if (!json_is_bool(json_member(params, "strict_required_status_checks_policy"))) {
					return failed(application::github_provider_failure::malformed_response);
				}
				result.Checks       = *checks;
				result.StrictChecks = json_bool(json_member(params, "strict_required_status_checks_policy"));
			}
			return result;
		}

		[[nodiscard]] auto read_classic_protection(production_read_execution& aExecution, server_contract aServer, const core::pull_request_request_context& aContext, application::github_pull_request_snapshot& aSnapshot) -> std::expected<void, github_paged_read_failure>
		{
			auto semantic      = semantic_request_for(aServer, aContext, read_operation::get_branch_protection);
			semantic.BranchRef = aSnapshot.PullRequest.BaseRef;
			const auto page    = aExecution.fetch(semantic);
			if (!page) {
				return std::unexpected{ page.error() };
			}
			if (!singleton_complete(*page) || !json_is_object(page->Document)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto fields = json_object(page->Document);
			if (!nonempty_string(json_member(fields, "url"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto       records = std::vector<application::github_branch_requirement>{ };
			const auto reviews = json_member(fields, "required_pull_request_reviews");
			if (!json_is_null(reviews) && !json_is_undefined(reviews)) {
				const auto params  = json_object(reviews);
				const auto minimum = json_member(params, "required_approving_review_count");
				if (!json_is_object(reviews) || !exact_integer(minimum) || integer(minimum) > 100U || !json_is_bool(json_member(params, "require_code_owner_reviews")) || !json_is_bool(json_member(params, "dismiss_stale_reviews"))) {
					return failed(application::github_provider_failure::malformed_response);
				}
				auto rule                = application::github_branch_requirement{ };
				rule.Type                = "pull_request";
				rule.ApprovalMinimum     = static_cast<std::uint32_t>(integer(minimum));
				rule.CodeOwners          = json_bool(json_member(params, "require_code_owner_reviews"));
				rule.DismissStaleReviews = json_bool(json_member(params, "dismiss_stale_reviews"));
				rule.LastPushApproval    = json_bool(json_member(params, "require_last_push_approval"));
				records.push_back(std::move(rule));
			}
			const auto status = json_member(fields, "required_status_checks");
			if (!json_is_null(status) && !json_is_undefined(status)) {
				const auto params = json_object(status);
				const auto checks = parse_required_checks(json_member(params, "checks"), "app_id");
				if (!checks || !json_is_bool(json_member(params, "strict"))) {
					return failed(application::github_provider_failure::malformed_response);
				}
				auto rule         = application::github_branch_requirement{ };
				rule.Type         = "required_status_checks";
				rule.Checks       = *checks;
				rule.StrictChecks = json_bool(json_member(params, "strict"));
				records.push_back(std::move(rule));
			}
			for (const auto& type : { "required_conversation_resolution", "required_signatures", "required_linear_history" }) {
				const auto value = json_member(fields, type);
				if (json_is_object(value) && json_bool(json_member(json_object(value), "enabled"))) {
					auto rule = application::github_branch_requirement{ };
					rule.Type = type;
					records.push_back(std::move(rule));
				}
			}
			for (auto& record : records) {
				record.Source     = aSnapshot.PullRequest.BaseRef;
				record.SourceKind = "Classic branch protection";
			}
			aSnapshot.BranchPolicy.Protection             = std::move(records);
			aSnapshot.BranchPolicy.ProtectionAvailability = observation::available;
			return { };
		}

		[[nodiscard]] auto read_merge_requirements(production_read_execution& aExecution, const validated_github_host_profile& aProfile, server_contract aServer, const core::pull_request_request_context& aContext, application::github_pull_request_snapshot& aSnapshot, const github_paged_read_limits& aLimits) -> std::expected<void, github_paged_read_failure>
		{
			const auto assessment = read_merge_assessment(aExecution, aServer, aContext, aSnapshot);
			if (!assessment) {
				if (fatal_optional_read_failure(assessment.error())) {
					return std::unexpected{ assessment.error() };
				}
				aSnapshot.Merge.Availability = observation_failure(assessment.error());
			}
			const auto requests = read_pending_requests(aExecution, aServer, aContext, aSnapshot, aLimits);
			if (!requests) {
				if (fatal_optional_read_failure(requests.error())) {
					return std::unexpected{ requests.error() };
				}
				aSnapshot.ReviewRequestsAvailability = observation_failure(requests.error());
				const auto fallback                  = read_rest_pending_requests(aExecution, aServer, aContext, aSnapshot);
				if (!fallback && fatal_optional_read_failure(fallback.error())) {
					return std::unexpected{ fallback.error() };
				}
			}
			auto semantic      = semantic_request_for(aServer, aContext, read_operation::list_branch_rules);
			semantic.BranchRef = aSnapshot.PullRequest.BaseRef;
			auto       rules   = std::vector<application::github_branch_requirement>{ };
			const auto read    = read_rest_array_collection(aExecution, aProfile, semantic, rules, 1000U, aLimits.MaximumPageRecords, parse_branch_rule);
			if (read) {
				aSnapshot.BranchPolicy.Rules             = std::move(rules);
				aSnapshot.BranchPolicy.RulesAvailability = observation::available;
			} else {
				if (fatal_optional_read_failure(read.error())) {
					return std::unexpected{ read.error() };
				}
				aSnapshot.BranchPolicy.RulesAvailability = observation_failure(read.error());
			}
			const auto protection = read_classic_protection(aExecution, aServer, aContext, aSnapshot);
			if (!protection) {
				if (fatal_optional_read_failure(protection.error())) {
					return std::unexpected{ protection.error() };
				}
				aSnapshot.BranchPolicy.ProtectionAvailability = observation_failure(protection.error());
			}
			const auto validCollection = [&](const application::github_pull_request_snapshot& aRecords) {
				auto budget = application::detail::github_text_budget{ aLimits.Records };
				return application::detail::validate_merge_observations(aRecords, budget, aLimits.Records) == application::github_read_validation_error::none;
			};
			auto records                                = application::github_pull_request_snapshot{ };
			records.ReviewRequestsAvailability          = observation::available;
			records.BranchPolicy.RulesAvailability      = observation::available;
			records.BranchPolicy.ProtectionAvailability = observation::available;

			records.Viewer             = aSnapshot.Viewer;
			records.PullRequest.Author = aSnapshot.PullRequest.Author;
			records.Reviews            = aSnapshot.Reviews;
			records.ReviewRequests     = aSnapshot.ReviewRequests;
			if (!validCollection(records)) {
				aSnapshot.ReviewRequests.clear();
				aSnapshot.ReviewRequestsAvailability  = observation::incomplete;
				aSnapshot.ReviewRequestActorsObserved = false;
			}
			records.ReviewRequests.clear();
			records.BranchPolicy.Rules = aSnapshot.BranchPolicy.Rules;
			if (!validCollection(records)) {
				aSnapshot.BranchPolicy.Rules.clear();
				aSnapshot.BranchPolicy.RulesAvailability = observation::incomplete;
			}
			records.BranchPolicy.Rules.clear();
			records.BranchPolicy.Protection = aSnapshot.BranchPolicy.Protection;
			if (!validCollection(records)) {
				aSnapshot.BranchPolicy.Protection.clear();
				aSnapshot.BranchPolicy.ProtectionAvailability = observation::incomplete;
			}
			return { };
		}

		[[nodiscard]] auto
		read_checks(production_read_execution&                     aExecution,
					const validated_github_host_profile&           aProfile,
					semantic_read_request                          aSemantic,
					const std::string_view                         aHeadOid,
					std::vector<application::github_check_record>& aOutput,
					const github_paged_read_limits&                aLimits)
			-> std::expected<void, github_paged_read_failure>
		{
			std::optional<std::uint64_t> total;
			while (true) {
				auto page = aExecution.fetch(aSemantic);
				if (!page.has_value()) {
					return std::unexpected{ page.error() };
				}
				if (!json_is_object(page->Document)
					|| !has_required_keys(json_object(page->Document),
										  { "check_runs", "total_count" })) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto object = json_object(page->Document);
				if (!exact_integer(json_member(object, "total_count"))
					|| !json_is_array(json_member(object, "check_runs"))) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto observedTotal = integer(json_member(object, "total_count"));
				if (observedTotal > aLimits.Records.MaximumChecks) {
					return failed(application::github_provider_failure::response_limit);
				}
				if (total.has_value() && *total != observedTotal) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				total              = observedTotal;
				const auto records = json_array(json_member(object, "check_runs"));
				const auto count   = static_cast<std::size_t>(records.size());
				if (count > aLimits.MaximumPageRecords
					|| count > aLimits.Records.MaximumChecks
					|| aOutput.size() > aLimits.Records.MaximumChecks - count) {
					return failed(application::github_provider_failure::response_limit);
				}
				aOutput.reserve(aOutput.size() + count);
				for (const auto& value : records) {
					auto check = parse_check(value, aHeadOid);
					if (!check.has_value()) {
						return std::unexpected{ check.error() };
					}
					auto budget = application::detail::github_text_budget{ aLimits.Records };
					if (application::detail::validate_check(*check, budget, aLimits.Records) != application::github_read_validation_error::none) {
						return failed(application::github_provider_failure::response_limit);
					}
					aOutput.push_back(std::move(*check));
				}
				if (aOutput.size() > observedTotal) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto repository = aExecution.bound_repository();
				if (!repository.has_value()) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto hasNext = rest_has_next_page(aProfile, *repository, *page, count);
				if (!hasNext.has_value()) {
					return std::unexpected{ hasNext.error() };
				}
				if (!*hasNext) {
					if (aOutput.size() != observedTotal) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					return { };
				}
				if (aSemantic.Page == std::numeric_limits<std::uint32_t>::max()) {
					return failed(application::github_provider_failure::response_limit);
				}
				++aSemantic.Page;
			}
		}

		[[nodiscard]] auto read_search_results(
			production_read_execution&                             aExecution,
			const validated_github_host_profile&                   aProfile,
			semantic_read_request                                  aSemantic,
			const application::github_repository_record&           aRepository,
			std::vector<application::github_search_result_record>& aOutput,
			const github_paged_read_limits&                        aLimits)
			-> std::expected<application::github_search_availability, github_paged_read_failure>
		{
			std::optional<std::uint64_t> total;
			while (true) {
				auto page = aExecution.fetch(aSemantic);
				if (!page.has_value()) {
					return std::unexpected{ page.error() };
				}
				if (!json_is_object(page->Document)
					|| !has_required_keys(
						json_object(page->Document),
						{ "incomplete_results", "items", "total_count" })) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto object = json_object(page->Document);
				if (!exact_integer(json_member(object, "total_count"))
					|| !json_is_array(json_member(object, "items"))
					|| !json_is_bool(json_member(object, "incomplete_results"))
					|| json_bool(json_member(object, "incomplete_results"))) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto observedTotal = integer(json_member(object, "total_count"));
				if (total.has_value() && *total != observedTotal) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto resultLimited = observedTotal > aLimits.Records.MaximumSearchResults;
				total                    = observedTotal;
				const auto records       = json_array(json_member(object, "items"));
				const auto count         = static_cast<std::size_t>(records.size());
				if (count > aLimits.MaximumPageRecords
					|| count > aLimits.Records.MaximumSearchResults
					|| aOutput.size() > aLimits.Records.MaximumSearchResults - count) {
					return failed(application::github_provider_failure::response_limit);
				}
				aOutput.reserve(aOutput.size() + count);
				for (const auto& value : records) {
					auto result = parse_search_result(value, aRepository, aProfile);
					if (!result.has_value()) {
						return std::unexpected{ result.error() };
					}
					aOutput.push_back(std::move(*result));
				}
				if (aOutput.size() > observedTotal) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto boundRepository = aExecution.bound_repository();
				if (!boundRepository.has_value()
					|| *boundRepository != aRepository.Repository) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto hasNext = rest_has_next_page(aProfile, *boundRepository, *page, count);
				if (!hasNext.has_value()) {
					return std::unexpected{ hasNext.error() };
				}
				if (!*hasNext) {
					if (aOutput.size() != observedTotal) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					return application::github_search_availability::available;
				}
				if (resultLimited) {
					// Keep this bounded page until the existing snapshot validator
					// has checked its fields and cross-collection identities.
					return application::github_search_availability::result_limit;
				}
				if (aSemantic.Page == std::numeric_limits<std::uint32_t>::max()) {
					return failed(application::github_provider_failure::response_limit);
				}
				++aSemantic.Page;
			}
		}

		[[nodiscard]] auto validation_failure(
			const application::github_read_validation_error aError) noexcept
			-> application::github_provider_failure
		{
			switch (aError) {
			case application::github_read_validation_error::text_too_long:
			case application::github_read_validation_error::collection_too_large:
			case application::github_read_validation_error::allocation_limit:
			case application::github_read_validation_error::total_text_too_large:
				return application::github_provider_failure::response_limit;
			case application::github_read_validation_error::none:
			case application::github_read_validation_error::invalid_context:
			case application::github_read_validation_error::invalid_limits:
			case application::github_read_validation_error::missing_identity:
			case application::github_read_validation_error::identity_mismatch:
			case application::github_read_validation_error::missing_required_text:
			case application::github_read_validation_error::invalid_text:
			case application::github_read_validation_error::invalid_repository:
			case application::github_read_validation_error::invalid_pull_request_number:
			case application::github_read_validation_error::invalid_commit_oid:
			case application::github_read_validation_error::invalid_line:
			case application::github_read_validation_error::invalid_enum:
			case application::github_read_validation_error::invalid_capability_revision:
			case application::github_read_validation_error::invalid_rate_limit:
			case application::github_read_validation_error::duplicate_identity:
			case application::github_read_validation_error::invalid_relationship:
			case application::github_read_validation_error::relationship_file_previous_path:
			case application::github_read_validation_error::relationship_thread_comment_path:
			case application::github_read_validation_error::relationship_review_group_root:
			case application::github_read_validation_error::relationship_review_group_reply:
			case application::github_read_validation_error::relationship_actor_login:
			case application::github_read_validation_error::relationship_thread_reply:
			case application::github_read_validation_error::relationship_search_repository:
			case application::github_read_validation_error::relationship_search_target_identity:
			case application::github_read_validation_error::relationship_search_target_metadata:
			case application::github_read_validation_error::relationship_timeline_availability:
			case application::github_read_validation_error::relationship_timeline_event:
			case application::github_read_validation_error::relationship_review_comments_availability:
			case application::github_read_validation_error::relationship_review_threads_capability:
			case application::github_read_validation_error::relationship_file_viewed_capability:
			case application::github_read_validation_error::relationship_thread_permission_capability:
			case application::github_read_validation_error::relationship_search_availability:
				return application::github_provider_failure::malformed_response;
			}
			return application::github_provider_failure::malformed_response;
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

	auto perform_github_merge_base_read(
		github_wire_transport_port&                              aTransport,
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		const core::repository_id                                aRepository,
		const github_merge_base_basis&                           aBasis,
		const std::string_view                                   aHead,
		const std::stop_token                                    aStop,
		const github_paged_read_limits&                          aLimits,
		const github_provider_read_authority                     aReadAuthority,
		github_paged_response_observer*                          aObserver) noexcept -> github_merge_base_read_result
	{
		auto rates = application::github_rate_limit_state{ };
		try {
			if (aReadAuthority == github_provider_read_authority::anonymous_public) {
				const auto admission = admit_anonymous_read(read_operation::get_merge_base, aProfile.server_contract());
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
			limits.MaximumTotalPages = 1U;
			production_read_execution execution{ aTransport, aProfile, aRequest.Context, limits, aStop, rates, aRequest.AuthenticationSession, aObserver };
			if (!execution.bind_repository(aRepository)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto semantic      = semantic_request_for(*server_contract_for(aProfile), aRequest.Context, read_operation::get_merge_base);
			semantic.BaseOid   = aBasis.BaseRef.empty() ? aBasis.BaseOid : std::string{ };
			semantic.BranchRef = aBasis.BaseRef;
			semantic.HeadOid   = std::string{ aHead };
			auto page          = execution.fetch(semantic);
			if (!page) {
				return std::unexpected{ page.error() };
			}
			if (!json_is_object(page->Document)) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto document  = json_object(page->Document);
			const auto baseValue = json_member(document, "base_commit");
			const auto mergeBase = json_member(document, "merge_base_commit");
			if (!json_is_object(baseValue) || !json_is_object(mergeBase)) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto baseSha  = json_member(json_object(baseValue), "sha");
			const auto mergeSha = json_member(json_object(mergeBase), "sha");
			const auto url      = json_member(document, "url");
			// The answer is bound to what was asked: GitHub echoes the compared
			// pair in `url`, whose head must be exactly the head asked about; a
			// reply about another base commit, or in another object format, is
			// not this pull request's comparison base.
			const auto echoedHead = json_is_string(url) && text(url).ends_with("..." + std::string{ aHead });
			if (!echoedHead || !hexadecimal_oid(baseSha) || !hexadecimal_oid(mergeSha)
				|| (aBasis.BaseRef.empty() && text(baseSha) != aBasis.BaseOid)
				|| text(mergeSha).size() != aHead.size() || text(baseSha).size() != aHead.size()) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			return github_merge_base_read{ .MergeBaseOid = text(mergeSha), .BaseCommitOid = text(baseSha), .RateLimits = rates };
		} catch (...) {
			return failed(application::github_provider_failure::response_limit, std::nullopt, rates);
		}
	}

	auto perform_github_pull_request_confirmation(
		github_wire_transport_port&                              aTransport,
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		const application::github_pull_request_snapshot&         aSnapshot,
		const std::stop_token                                    aStop,
		const github_paged_read_limits&                          aLimits,
		const github_provider_read_authority                     aReadAuthority,
		github_paged_response_observer*                          aObserver) noexcept -> github_pull_request_confirmation_result
	{
		auto rates = application::github_rate_limit_state{ };
		try {
			if (!provider_request_valid(aProfile, aRequest, core::provider_snapshot_id{ 1U }, aLimits, aReadAuthority)
				|| aSnapshot.Repository.Repository.value() == 0U
				|| (aRequest.Context.ExpectedRepository && *aRequest.Context.ExpectedRepository != aSnapshot.Repository.Repository)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto limits              = aLimits;
			limits.MaximumTotalPages = 1U;
			production_read_execution execution{ aTransport, aProfile, aRequest.Context, limits, aStop, rates, aRequest.AuthenticationSession, aObserver };
			if (!execution.bind_repository(aSnapshot.Repository.Repository)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto page = execution.fetch(semantic_request_for(*server_contract_for(aProfile), aRequest.Context, read_operation::get_pull_request));
			if (!page) {
				return std::unexpected{ page.error() };
			}
			auto pullRequest = parse_pull_request(*page, aRequest.Context, aSnapshot.Repository, aLimits.Records);
			if (!pullRequest) {
				return std::unexpected{ execution.enrich(pullRequest.error()) };
			}
			const auto& now = pullRequest->Record;
			const auto& was = aSnapshot.PullRequest;
			return github_pull_request_confirmation{
				.Unchanged = now.PullRequest == was.PullRequest && now.HeadOid == was.HeadOid && now.BaseOid == was.BaseOid
					&& now.BaseRef == was.BaseRef && now.State == was.State && pullRequest->ChangedFiles == aSnapshot.Files.size(),
				.RateLimits = rates,
			};
		} catch (...) {
			return failed(application::github_provider_failure::response_limit, std::nullopt, rates);
		}
	}

	namespace {
		/// [[BPR-0988]]. The authenticated half of `provider_request_valid`,
		/// for a read scoped to a repository rather than a pull request.
		[[nodiscard]] auto discovery_request_valid(
			const validated_github_host_profile&           aProfile,
			const application::personal_discovery_request& aRequest,
			const github_paged_read_limits&                aLimits) noexcept -> bool
		{
			const auto& context     = aRequest.Context;
			const auto& authority   = aRequest.ProfileAuthority;
			const auto& evidence    = aRequest.CapabilityEvidence;
			const auto  observation = application::github_capability_probe_observation{
				.Authority   = evidence.Authority,
				.ObservedAt  = evidence.ObservedAt,
				.Facts       = evidence.Facts,
				.SchemaFacts = evidence.SchemaFacts,
			};
			const auto reason = aRequest.Reason == application::personal_discovery_reason::retained
				? aRequest.Number && aRequest.Number->value() != 0U && aRequest.Number->value() <= core::maximum_pull_request_number
				: (aRequest.Reason == application::personal_discovery_reason::authored
				   || aRequest.Reason == application::personal_discovery_reason::requested)
					&& !aRequest.Number;
			return reason && limits_valid(aLimits) && application::detail::valid_context(context)
				&& (!aRequest.AuthenticationSession || aRequest.AuthenticationSession->value() != 0U)
				&& application::valid(authority) && aRequest.AccountTransport != nullptr
				&& aRequest.Viewer.Account.value() != 0U && aRequest.Viewer.Account == context.Account
				&& bounded_text(aRequest.Viewer.Login, aLimits.Records.MaximumLoginBytes, true)
				&& authority.Profile == context.Profile && authority.Profile == aProfile.profile()
				&& authority.ProfileRevision == aProfile.revision()
				&& authority.Host == context.Host && authority.Host == aProfile.host()
				&& authority.Hostname.value() == aProfile.hostname()
				&& authority.ServerContract == aProfile.server_contract()
				&& authority.Authentication == aProfile.authentication_transport()
				&& application::valid(observation)
				&& evidence.Authority.Profile == authority.Profile
				&& evidence.Authority.ProfileRevision == authority.ProfileRevision
				&& evidence.Authority.Host == authority.Host
				&& evidence.Authority.Account == context.Account
				&& evidence.Authority.ServerContract == authority.ServerContract
				&& evidence.Authority.ProbeContractRevision == authority.ProbeContractRevision
				&& evidence.Revision == context.Capabilities
				&& evidence.ExpiresBefore > evidence.ObservedAt
				&& evidence.ExpiresBefore.value() - evidence.ObservedAt.value()
				<= application::maximum_github_capability_freshness_window
				&& evidence.Capabilities == application::projected_capabilities(observation)
				&& evidence.ReadShape == application::projected_read_shape(observation);
		}

		/// [[BPR-0988]]. One search item, checked against the exact repository
		/// the read is scoped to. The name compares without case: a saved
		/// repository may be written in another case than GitHub's.
		[[nodiscard]] auto parse_discovery_item(const page_json_value&                         aValue,
												const application::personal_discovery_request& aRequest,
												const validated_github_host_profile&           aProfile,
												const github_paged_read_limits&                aLimits)
			-> std::expected<application::personal_discovery_row, github_paged_read_failure>
		{
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue), { "number", "pull_request", "repository_url", "state", "title", "user" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object      = json_object(aValue);
			const auto number      = uint32(json_member(object, "number"));
			const auto pullRequest = json_member(object, "pull_request");
			const auto user        = json_member(object, "user");
			const auto title       = text(json_member(object, "title"));
			const auto draft       = json_member(object, "draft");
			if (!number.has_value() || *number == 0U || *number > core::maximum_pull_request_number
				|| !json_is_string(json_member(object, "title")) || !bounded_text(title, aLimits.Records.MaximumTitleBytes)
				|| !json_is_object(pullRequest) || !one_of(json_member(object, "state"), { "open" })
				|| (has_required_keys(object, { "draft" }) && !json_is_bool(draft))
				|| !nonempty_string(json_member(object, "repository_url"))
				|| (!json_is_null(user) && (!json_is_object(user) || !nonempty_string(json_member(json_object(user), "login"))))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto expected = std::string{ aProfile.rest_root() } + "/repos/" + aRequest.Context.Repository.Owner + "/"
				+ aRequest.Context.Repository.Name;
			if (!application::detail::ascii_equal_fold(text(json_member(object, "repository_url")), expected)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto author = json_is_null(user) ? std::string{ } : text(json_member(json_object(user), "login"));
			if (!bounded_text(author, aLimits.Records.MaximumLoginBytes)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			return application::personal_discovery_row{
				.Number = core::pull_request_number{ *number },
				.Title  = title,
				.Author = author,
				.Draft  = json_is_bool(draft) ? std::optional<bool>{ json_bool(draft) } : std::nullopt,
			};
		}

		struct requested_reviewers
		{
			bool          NamesViewer = false;
			std::uint32_t Awaited     = 0U;
		};

		/// [[BPR-0988]]. The users and teams a pull request still awaits, and
		/// whether the viewer is one of the users.
		[[nodiscard]] auto parse_requested_reviewers(const parsed_page& aPage, const std::string_view aLogin, const github_paged_read_limits& aLimits)
			-> std::expected<requested_reviewers, github_paged_read_failure>
		{
			if (!singleton_complete(aPage) || !json_is_object(aPage.Document)
				|| !has_required_keys(json_object(aPage.Document), { "teams", "users" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aPage.Document);
			if (!json_is_array(json_member(object, "users")) || !json_is_array(json_member(object, "teams"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto users = json_array(json_member(object, "users"));
			const auto teams = json_array(json_member(object, "teams"));
			if (users.size() + teams.size() > aLimits.MaximumPageRecords) {
				return failed(application::github_provider_failure::response_limit);
			}
			auto result = requested_reviewers{ .Awaited = static_cast<std::uint32_t>(users.size() + teams.size()) };
			for (const auto& value : users) {
				if (!json_is_object(value) || !nonempty_string(json_member(json_object(value), "login"))) {
					return failed(application::github_provider_failure::malformed_response);
				}
				result.NamesViewer = result.NamesViewer || application::detail::ascii_equal_fold(text(json_member(json_object(value), "login")), aLogin);
			}
			if (!std::ranges::all_of(teams, [](const page_json_value& aValue) { return json_is_object(aValue); })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			return result;
		}

		[[nodiscard]] auto requested_reviewers_parent(const core::repository_request_context& aContext, const core::pull_request_number aNumber)
			-> core::pull_request_request_context
		{
			return {
				.Request             = aContext.Request,
				.ApplicationSession  = aContext.ApplicationSession,
				.Profile             = aContext.Profile,
				.Host                = aContext.Host,
				.Account             = aContext.Account,
				.Repository          = aContext.Repository,
				.Number              = aNumber,
				.ExpectedRepository  = std::nullopt,
				.ExpectedPullRequest = std::nullopt,
				.Capabilities        = aContext.Capabilities,
				.IdentityGeneration  = aContext.IdentityGeneration,
				.Generation          = core::request_generation{ 1U },
				.Mode                = core::github_access_mode::authenticated,
			};
		}

		/// [[BPR-1407]]. A pull request's age, size and comment count. A field
		/// GitHub leaves out is unknown; one it sends in another shape is refused.
		[[nodiscard]] auto parse_change_facts(const page_json_object& aObject) -> std::optional<core::queue_change_facts>
		{
			auto result = core::queue_change_facts{ };
			if (aObject.contains("created_at")) {
				const auto created = json_member(aObject, "created_at");
				const auto opened  = json_is_string(created) ? parse_timeline_timestamp(text(created)) : std::nullopt;
				if (!opened) {
					return std::nullopt;
				}
				result.OpenedAt = *opened / 1000;
			}
			const auto count = [&aObject](const std::string_view aKey, std::optional<std::uint64_t>& aValue) {
				if (!aObject.contains(aKey)) {
					return true;
				}
				const auto value = uint32(json_member(aObject, aKey));
				aValue           = value;
				return value.has_value();
			};
			auto conversation = std::optional<std::uint64_t>{ };
			auto review       = std::optional<std::uint64_t>{ };
			if (!count("additions", result.Additions) || !count("deletions", result.Deletions)
				|| !count("comments", conversation) || !count("review_comments", review)) {
				return std::nullopt;
			}
			// A total needs both; either alone would undercount.
			if (conversation && review) {
				result.Comments = *conversation + *review;
			}
			return result;
		}

		[[nodiscard]] auto read_home_summary(production_read_execution& aExecution, const validated_github_host_profile& aProfile, const application::personal_discovery_request& aRequest, server_contract aServer, const core::pull_request_request_context& aContext, const parsed_page& aPage, const github_paged_read_limits& aLimits, bool* aLimited = nullptr) -> std::expected<core::queue_waiting_facts, github_paged_read_failure>
		{
			if (!json_is_object(aPage.Document)) {
				return core::queue_waiting_facts{ };
			}
			const auto object = json_object(aPage.Document);
			if (!json_is_object(json_member(object, "base")) || !json_is_object(json_member(object, "head")) || !json_is_object(json_member(json_object(json_member(object, "base")), "repo"))) {
				return core::queue_waiting_facts{ };
			}
			const auto base = json_object(json_member(object, "base"));
			const auto head = json_object(json_member(object, "head"));
			const auto repo = json_object(json_member(base, "repo"));
			if (integer(json_member(object, "number")) != aContext.Number.value() || !application::detail::ascii_equal_fold(text(json_member(repo, "full_name")), aContext.Repository.Owner + "/" + aContext.Repository.Name)) {
				return aExecution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			auto snapshot                  = application::github_pull_request_snapshot{ };
			snapshot.Viewer.Actor          = aRequest.Viewer.Node;
			snapshot.Viewer.Login          = aRequest.Viewer.Login;
			snapshot.Repository.Repository = core::repository_id{ integer(json_member(repo, "id")) };
			snapshot.Repository.Node       = application::github_node_id{ text(json_member(repo, "node_id")) };
			snapshot.Repository.Owner      = aContext.Repository.Owner;
			snapshot.Repository.Name       = aContext.Repository.Name;
			snapshot.PullRequest.Node      = application::github_node_id{ text(json_member(object, "node_id")) };
			snapshot.PullRequest.Number    = aContext.Number;
			snapshot.PullRequest.HeadOid   = application::commit_oid{ text(json_member(head, "sha")) };
			snapshot.PullRequest.BaseOid   = application::commit_oid{ text(json_member(base, "sha")) };
			snapshot.PullRequest.BaseRef   = text(json_member(base, "ref"));
			if (snapshot.Repository.Node.value().empty() || snapshot.PullRequest.Node.value().empty() || snapshot.PullRequest.BaseRef.empty() || !hexadecimal_oid(json_member(base, "sha")) || !hexadecimal_oid(json_member(head, "sha"))) {
				return core::queue_waiting_facts{ };
			}
			if (!aExecution.bind_repository(snapshot.Repository.Repository)) {
				return aExecution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto noteLimit = [aLimited](const github_paged_read_failure& aFailure) {
				const auto* error = std::get_if<application::github_provider_error>(&aFailure);
				if (aLimited && error && error->Failure == application::github_provider_failure::response_limit) {
					*aLimited = true;
				}
			};
			const auto requests = read_pending_requests(aExecution, aServer, aContext, snapshot, aLimits);
			if (!requests) {
				noteLimit(requests.error());
				if (fatal_optional_read_failure(requests.error())) {
					return std::unexpected{ requests.error() };
				}
				snapshot.ReviewRequestsAvailability = observation_failure(requests.error());
			}
			const auto assessment = read_merge_assessment(aExecution, aServer, aContext, snapshot);
			if (!assessment) {
				noteLimit(assessment.error());
			}
			if (!assessment && fatal_optional_read_failure(assessment.error())) {
				return std::unexpected{ assessment.error() };
			}
			auto semantic               = semantic_request_for(aServer, aContext, read_operation::list_checks);
			semantic.HeadOid            = snapshot.PullRequest.HeadOid.value();
			auto checks                 = read_checks(aExecution, aProfile, semantic, snapshot.PullRequest.HeadOid.value(), snapshot.Checks, aLimits);
			snapshot.ChecksAvailability = checks ? application::github_collection_availability::available : application::github_collection_availability::unavailable;
			if (!checks) {
				noteLimit(checks.error());
			}
			if (!checks && fatal_optional_read_failure(checks.error())) {
				return std::unexpected{ checks.error() };
			}
			semantic                      = semantic_request_for(aServer, aContext, read_operation::list_commit_statuses);
			semantic.HeadOid              = snapshot.PullRequest.HeadOid.value();
			auto statuses                 = std::vector<application::github_check_record>{ };
			auto statusRead               = read_rest_array_collection(aExecution, aProfile, semantic, statuses, aLimits.Records.MaximumChecks, aLimits.MaximumPageRecords, [&snapshot](const page_json_value& aValue) { return parse_commit_status(aValue, snapshot.PullRequest.HeadOid.value()); });
			snapshot.StatusesAvailability = statusRead ? application::github_collection_availability::available : application::github_collection_availability::unavailable;
			if (!statusRead) {
				noteLimit(statusRead.error());
			}
			if (!statusRead && fatal_optional_read_failure(statusRead.error())) {
				return std::unexpected{ statusRead.error() };
			}
			auto contexts = std::unordered_set<std::string>{ };
			for (auto& status : statuses) {
				if (contexts.insert(status.Name).second) {
					snapshot.Checks.push_back(std::move(status));
				}
			}
			auto budget = application::detail::github_text_budget{ aLimits.Records };
			if (application::detail::validate_merge_observations(snapshot, budget, aLimits.Records) != application::github_read_validation_error::none) {
				snapshot.ReviewRequests.clear();
				snapshot.ReviewRequestsAvailability = observation::incomplete;
			}
			return application::queue_waiting_for(snapshot);
		}

		/// [[BPR-1379]]. Retained rows need the current title, head and complete
		/// submitted-review evidence, without opening the snapshot read path.
		[[nodiscard]] auto read_retained_metadata(github_wire_transport_port&          aTransport,
												  const validated_github_host_profile& aProfile, const application::personal_discovery_request& aRequest,
												  const server_contract aServer, const std::stop_token aStop, const github_paged_read_limits& aLimits,
												  application::github_rate_limit_state& aRates) -> github_personal_discovery_read_result
		{
			auto limits                          = aLimits;
			limits.MaximumTotalPages             = std::min<std::size_t>(limits.MaximumTotalPages, 32U);
			limits.MaximumAggregateResponseBytes = std::min(limits.MaximumAggregateResponseBytes, std::size_t{ 8U * 1024U * 1024U });
			limits.Records.MaximumReviews        = std::min(limits.Records.MaximumReviews, application::maximum_personal_discovery_reviews);
			const auto                context    = requested_reviewers_parent(aRequest.Context, *aRequest.Number);
			auto                      homeBudget = shared_home_budget{ };
			production_read_execution execution{ aTransport, aProfile, context, limits, aStop, aRates, aRequest.AuthenticationSession, nullptr, &homeBudget };
			const auto                page = execution.fetch(semantic_request_for(aServer, context, read_operation::get_pull_request));
			if (!page) {
				return std::unexpected{ page.error() };
			}
			if (!singleton_complete(*page) || !json_is_object(page->Document)
				|| !has_required_keys(json_object(page->Document), { "base", "head", "number", "requested_reviewers", "requested_teams", "state", "title", "url", "user" })) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto object   = json_object(page->Document);
			const auto number   = uint32(json_member(object, "number"));
			const auto title    = json_member(object, "title");
			const auto draft    = json_member(object, "draft");
			const auto head     = json_member(object, "head");
			const auto users    = json_member(object, "requested_reviewers");
			const auto teams    = json_member(object, "requested_teams");
			const auto author   = optional_rest_actor(json_member(object, "user"));
			const auto expected = std::string{ aProfile.rest_root() } + "/repos/" + context.Repository.Owner + "/"
				+ context.Repository.Name + "/pulls/" + std::to_string(context.Number.value());
			if (!number || *number != context.Number.value() || !json_is_string(title)
				|| !bounded_text(text(title), limits.Records.MaximumTitleBytes, true)
				|| (has_required_keys(object, { "draft" }) && !json_is_bool(draft))
				|| !one_of(json_member(object, "state"), { "open", "closed" })
				|| !json_is_string(json_member(object, "url"))
				|| !application::detail::ascii_equal_fold(text(json_member(object, "url")), expected)
				|| !json_is_object(head) || !hexadecimal_oid(json_member(json_object(head), "sha"))
				|| !author.Valid || !json_is_array(users) || !json_is_array(teams)) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			if (json_array(users).size() + json_array(teams).size() > limits.MaximumPageRecords) {
				return execution.terminal_failure(application::github_provider_failure::response_limit);
			}
			const auto base = json_member(object, "base");
			if (!json_is_object(base)) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto repository = json_member(json_object(base), "repo");
			if (!json_is_object(repository) || !exact_integer(json_member(json_object(repository), "id"), true)
				|| !nonempty_string(json_member(json_object(repository), "full_name"))
				|| !application::detail::ascii_equal_fold(text(json_member(json_object(repository), "full_name")), context.Repository.Owner + "/" + context.Repository.Name)
				|| !execution.bind_repository(core::repository_id{ integer(json_member(json_object(repository), "id")) })) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			auto budget = application::detail::github_text_budget{ limits.Records };
			if (application::detail::validate_author(author.Actor, budget, limits.Records) != application::github_read_validation_error::none) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			for (const auto& user : json_array(users)) {
				const auto reviewer = optional_rest_actor(user);
				if (!reviewer.Valid || !reviewer.Actor
					|| application::detail::validate_author(reviewer.Actor, budget, limits.Records) != application::github_read_validation_error::none) {
					return execution.terminal_failure(application::github_provider_failure::malformed_response);
				}
			}
			if (!std::ranges::all_of(json_array(teams), [](const page_json_value& aValue) { return json_is_object(aValue); })) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			auto row = application::personal_discovery_row{
				.Number           = context.Number,
				.Title            = text(title),
				.Author           = author.Actor ? author.Actor->Login : std::string{ },
				.Detailed         = true,
				.ReviewersAwaited = static_cast<std::uint32_t>(json_array(users).size() + json_array(teams).size()),
				.HeadOid          = application::commit_oid{ text(json_member(json_object(head), "sha")) },
				.Closed           = text(json_member(object, "state")) == "closed",
				.Draft            = json_is_bool(draft) ? std::optional<bool>{ json_bool(draft) } : std::nullopt,
			};
			row.AuthorIdentity = author.Actor ? std::optional{ author.Actor->Actor } : std::nullopt;
			const auto change  = parse_change_facts(object);
			if (!change) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			row.Change = *change;
			if (row.Closed) {
				return github_personal_discovery_read{ .Rows = { std::move(row) }, .RateLimits = aRates };
			}
			auto       seen    = std::unordered_set<std::uint64_t>{ };
			const auto reviews = read_rest_array_collection(execution, aProfile,
															semantic_request_for(aServer, context, read_operation::list_reviews), row.Reviews,
															limits.Records.MaximumReviews, limits.MaximumPageRecords,
															[&budget, &seen, &limits](const page_json_value& aValue)
																-> std::expected<application::personal_discovery_review, github_paged_read_failure> {
																const auto review = parse_review(aValue);
																if (!review) {
																	return std::unexpected{ review.error() };
																}
																if (!seen.insert(review->DatabaseId).second
																	|| application::detail::validate_review(*review, budget, limits.Records) != application::github_read_validation_error::none) {
																	return failed(application::github_provider_failure::malformed_response);
																}
																return application::personal_discovery_review{
																	.DatabaseId = review->DatabaseId,
																	.Author     = review->Author ? std::optional{ review->Author->Actor } : std::nullopt,
																	.State      = review->State,
																	.Commit     = review->Commit,
																};
															});
			if (!reviews) {
				return std::unexpected{ execution.enrich(reviews.error()) };
			}
			const auto summary = read_home_summary(execution, aProfile, aRequest, aServer, context, *page, limits, &row.WaitingLimited);
			if (!summary) {
				return std::unexpected{ summary.error() };
			}
			row.Waiting = *summary;
			// The validated singleton carries the complete requested arrays. It
			// can settle the count when the richer code-owner query is unavailable;
			// a nonempty count preserves old actor names as last known.
			if (!row.Waiting.Requests) {
				row.Waiting.Requests = row.ReviewersAwaited;
				row.Waiting.People   = static_cast<std::uint32_t>(json_array(users).size());
				row.Waiting.Teams    = static_cast<std::uint32_t>(json_array(teams).size());
			}
			row.Waiting.Head = row.HeadOid->value();
			return github_personal_discovery_read{ .Rows = { std::move(row) }, .RateLimits = aRates };
		}
	} // namespace

	auto perform_markdown_image_resolution(github_wire_transport_port& aTransport, const validated_github_host_profile& aProfile,
										   const application::markdown_image_resolution_request& aRequest, const std::stop_token aStop,
										   const github_paged_read_limits& aLimits) noexcept -> github_markdown_image_resolution_result
	{
		try {
			const auto server        = aProfile.server_contract() == application::github_server_contract::github_com_2022_11_28 ? server_contract::github_com : server_contract::ghes_3_18;
			auto       rates         = application::github_rate_limit_state{ };
			auto       limits        = aLimits;
			limits.MaximumTotalPages = 1U;
			production_read_execution execution{ aTransport, aProfile, aRequest.Context, limits, aStop, rates, aRequest.AuthenticationSession, nullptr };
			auto                      semantic = semantic_request_for(server, aRequest.Context, read_operation::resolve_body_images);
			semantic.Nodes                     = aRequest.Nodes;
			const auto page                    = execution.fetch(semantic);
			if (!page) {
				return std::unexpected{ page.error() };
			}
			const auto malformed = [&execution] { return execution.terminal_failure(application::github_provider_failure::malformed_response); };
			if (!json_is_object(page->Document)) {
				return malformed();
			}
			const auto data = json_member(json_object(page->Document), "data");
			if (!json_is_object(data)) {
				return malformed();
			}
			const auto fields     = json_object(data);
			const auto repository = json_member(fields, "repository");
			const auto nodes      = json_member(fields, "nodes");
			if (!json_is_object(repository) || !json_is_array(nodes) || text(json_member(json_object(repository), "id")) != aRequest.RepositoryNode) {
				return malformed();
			}
			const auto pull = json_member(json_object(repository), "pullRequest");
			if (!json_is_object(pull) || text(json_member(json_object(pull), "id")) != aRequest.PullRequestNode) {
				return malformed();
			}
			const auto answered = json_array(nodes);
			if (answered.size() != aRequest.Nodes.size()) {
				return malformed();
			}
			auto result = github_markdown_image_resolution{ };
			for (std::size_t index = 0; index < answered.size(); ++index) {
				auto body = application::markdown_image_resolved_body{ .Node = aRequest.Nodes[index], .Served = { } };
				if (!json_is_null(answered[index])) {
					if (!json_is_object(answered[index])) {
						return malformed();
					}
					const auto node = json_object(answered[index]);
					const auto html = json_member(node, "bodyHTML");
					if (!one_of(json_member(node, "__typename"), { "PullRequest", "IssueComment", "PullRequestReview", "PullRequestReviewComment" })
						|| text(json_member(node, "id")) != aRequest.Nodes[index] || !json_is_string(html)) {
						return malformed();
					}
					if (auto served = core::served_markdown_images(text(html))) {
						body.Served = std::move(*served);
					}
				}
				result.Bodies.push_back(std::move(body));
			}
			result.RateLimits = std::move(rates);
			return result;
		} catch (...) {
			return std::unexpected{ github_paged_read_failure{ provider_error(application::github_provider_failure::response_limit) } };
		}
	}

	auto perform_personal_discovery_read(
		github_wire_transport_port&                    aTransport,
		const validated_github_host_profile&           aProfile,
		const application::personal_discovery_request& aRequest,
		const std::stop_token                          aStop,
		const github_paged_read_limits&                aLimits) noexcept -> github_personal_discovery_read_result
	{
		auto rates = application::github_rate_limit_state{ };
		try {
			const auto server = server_contract_for(aProfile);
			if (!server.has_value() || !discovery_request_valid(aProfile, aRequest, aLimits)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			if (aStop.stop_requested()) {
				return cancelled();
			}
			if (aRequest.Reason == application::personal_discovery_reason::retained) {
				return read_retained_metadata(aTransport, aProfile, aRequest, *server, aStop, aLimits, rates);
			}
			const auto requested     = aRequest.Reason == application::personal_discovery_reason::requested;
			auto       limits        = aLimits;
			limits.MaximumTotalPages = 1U;
			production_read_execution search{ aTransport, aProfile, aRequest.Context, limits, aStop, rates, aRequest.AuthenticationSession, nullptr };
			const auto                semantic = semantic_read_request{
				.Server            = *server,
				.Operation         = read_operation::search_personal_pull_requests,
				.Owner             = aRequest.Context.Repository.Owner,
				.Repository        = aRequest.Context.Repository.Name,
				.PullRequestNumber = 0U,
				.HeadOid           = { },
				.SearchTerms       = (requested ? std::string{ "review-requested:" } : std::string{ "author:" }) + aRequest.Viewer.Login,
				.Page              = 1U,
				.GraphqlPage       = std::nullopt,
			};
			auto page = search.fetch(semantic);
			if (!page.has_value()) {
				auto failure = page.error();
				// GitHub refuses a search whose repository qualifier names
				// nothing this account can read (422): removed, renamed or no
				// longer visible. That is lost access, not an API mismatch.
				if (auto* error = std::get_if<application::github_provider_error>(&failure);
					error != nullptr && error->Failure == application::github_provider_failure::api_version) {
					error->Failure = application::github_provider_failure::permission;
				}
				return std::unexpected{ std::move(failure) };
			}
			if (!json_is_object(page->Document) || !has_required_keys(json_object(page->Document), { "incomplete_results", "items", "total_count" })) {
				return search.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(page->Document);
			if (!exact_integer(json_member(object, "total_count")) || !json_is_array(json_member(object, "items"))
				|| !json_is_bool(json_member(object, "incomplete_results"))) {
				return search.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto items = json_array(json_member(object, "items"));
			if (items.size() > application::maximum_personal_discovery_rows || items.size() > limits.MaximumPageRecords) {
				return search.terminal_failure(application::github_provider_failure::response_limit);
			}
			// One page is all a discovery read takes, so a continuation is never
			// followed: it only makes the answer partial, as a total larger
			// than the page does.
			if (page->Completeness == page_completeness::unknown
				|| (page->Completeness == page_completeness::has_next_page && items.size() != limits.MaximumPageRecords)) {
				return search.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto more   = page->Completeness == page_completeness::has_next_page || integer(json_member(object, "total_count")) > items.size();
			auto       result = github_personal_discovery_read{ };
			if (json_bool(json_member(object, "incomplete_results"))) {
				result.Completeness = application::personal_discovery_completeness::incomplete_results;
			} else if (more) {
				result.Completeness = application::personal_discovery_completeness::result_limit;
			}
			auto seen = std::unordered_set<std::uint32_t>{ };
			for (const auto& value : items) {
				auto row = parse_discovery_item(value, aRequest, aProfile, aLimits);
				if (!row.has_value()) {
					return std::unexpected{ search.enrich(row.error()) };
				}
				if (!seen.insert(row->Number.value()).second) {
					return search.terminal_failure(application::github_provider_failure::malformed_response);
				}
				// A search expansion may name another actor; only the viewer's
				// own pull requests are authored ones.
				if (!requested && !application::detail::ascii_equal_fold(row->Author, aRequest.Viewer.Login)) {
					continue;
				}
				result.Rows.push_back(std::move(*row));
			}
			auto homeBudget = shared_home_budget{ };
			auto details    = std::size_t{ 0U };
			auto kept       = std::vector<application::personal_discovery_row>{ };
			auto overflow   = false;
			for (auto& row : result.Rows) {
				if (details == application::maximum_personal_discovery_details) {
					overflow = true;
					if (!requested) {
						kept.push_back(std::move(row));
					}
					continue;
				}
				++details;
				production_read_execution detail{ aTransport, aProfile, requested_reviewers_parent(aRequest.Context, row.Number), limits, aStop, rates, aRequest.AuthenticationSession, nullptr };
				auto                      reviewers = detail.fetch(semantic_read_request{
					.Server            = *server,
					.Operation         = read_operation::list_requested_reviewers,
					.Owner             = aRequest.Context.Repository.Owner,
					.Repository        = aRequest.Context.Repository.Name,
					.PullRequestNumber = row.Number.value(),
					.HeadOid           = { },
					.SearchTerms       = { },
					.Page              = 1U,
					.GraphqlPage       = std::nullopt,
				});
				if (!reviewers.has_value()) {
					return std::unexpected{ reviewers.error() };
				}
				const auto parsed = parse_requested_reviewers(*reviewers, aRequest.Viewer.Login, aLimits);
				if (!parsed.has_value()) {
					return std::unexpected{ detail.enrich(parsed.error()) };
				}
				// Asked of a team the viewer belongs to is not asked of the
				// viewer: automatic team-request discovery is a non-goal.
				if (requested && !parsed->NamesViewer) {
					continue;
				}
				row.Detailed         = true;
				row.ReviewersAwaited = parsed->Awaited;
				row.Waiting.Requests = parsed->Awaited;
				if (homeBudget.Pages < 32) {
					auto summaryLimits                          = aLimits;
					summaryLimits.MaximumTotalPages             = std::min(summaryLimits.MaximumTotalPages, std::size_t{ 32 });
					summaryLimits.MaximumAggregateResponseBytes = std::min(summaryLimits.MaximumAggregateResponseBytes, std::size_t{ 8U * 1024U * 1024U });
					const auto                context           = requested_reviewers_parent(aRequest.Context, row.Number);
					production_read_execution summaryExecution{ aTransport, aProfile, context, summaryLimits, aStop, rates, aRequest.AuthenticationSession, nullptr, &homeBudget };
					const auto                metadata = summaryExecution.fetch(semantic_request_for(*server, context, read_operation::get_pull_request));
					if (metadata && json_is_object(metadata->Document)) {
						// The summary is optional, so a malformed change fact
						// leaves the row's change unknown rather than failing it.
						row.Change         = parse_change_facts(json_object(metadata->Document)).value_or(core::queue_change_facts{ });
						auto       limited = false;
						const auto summary = read_home_summary(summaryExecution, aProfile, aRequest, *server, context, *metadata, summaryLimits, &limited);
						if (limited && result.Completeness == application::personal_discovery_completeness::complete) {
							result.Completeness = application::personal_discovery_completeness::detail_limit;
						}
						if (summary) {
							application::merge_queue_waiting(row.Waiting, *summary);
							if (!summary->Requests) {
								// Optional names do not revoke the complete REST count.
								row.Waiting.RequestsStale = false;
							}
						} else if (fatal_optional_read_failure(summary.error())) {
							return std::unexpected{ summary.error() };
						}
					} else if (!metadata) {
						if (fatal_optional_read_failure(metadata.error())) {
							return std::unexpected{ metadata.error() };
						}
						const auto* error = std::get_if<application::github_provider_error>(&metadata.error());
						if (error && error->Failure == application::github_provider_failure::response_limit && result.Completeness == application::personal_discovery_completeness::complete) {
							result.Completeness = application::personal_discovery_completeness::detail_limit;
						}
					}
				} else if (result.Completeness == application::personal_discovery_completeness::complete) {
					result.Completeness = application::personal_discovery_completeness::detail_limit;
				}
				kept.push_back(std::move(row));
			}
			if (requested && overflow && result.Completeness == application::personal_discovery_completeness::complete) {
				result.Completeness = application::personal_discovery_completeness::detail_limit;
			}
			if (homeBudget.Pages >= 32 && result.Completeness == application::personal_discovery_completeness::complete) {
				result.Completeness = application::personal_discovery_completeness::detail_limit;
			}
			result.Rows       = std::move(kept);
			result.RateLimits = rates;
			return result;
		} catch (...) {
			return failed(application::github_provider_failure::response_limit, std::nullopt, rates);
		}
	}

	auto perform_paged_github_read(
		github_wire_transport_port&                              aTransport,
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		const core::provider_snapshot_id                         aSnapshotId,
		const std::stop_token                                    aStop,
		const github_paged_read_limits&                          aLimits,
		const github_provider_read_authority                     aReadAuthority,
		github_paged_response_observer*                          aObserver) noexcept -> github_paged_read_result
	{
		auto observedRateLimits = application::github_rate_limit_state{ };
		try {
			if (!provider_request_valid(
					aProfile, aRequest, aSnapshotId, aLimits, aReadAuthority)) {
				return failed(
					application::github_provider_failure::malformed_response);
			}
			if (aStop.stop_requested()) {
				return cancelled();
			}
			const auto server = server_contract_for(aProfile);
			if (!server.has_value()) {
				return failed(
					application::github_provider_failure::malformed_response);
			}

			production_read_execution execution{ aTransport,
												 aProfile,
												 aRequest.Context,
												 aLimits,
												 aStop,
												 observedRateLimits, aRequest.AuthenticationSession, aObserver };

			auto repositoryPage = execution.fetch(semantic_request_for(
				*server, aRequest.Context, read_operation::get_repository));
			if (!repositoryPage.has_value()) {
				return std::unexpected{ repositoryPage.error() };
			}
			auto repository = parse_repository(*repositoryPage, aRequest.Context);
			if (!repository.has_value()) {
				return std::unexpected{ execution.enrich(repository.error()) };
			}
			if (!execution.bind_repository(repository->Repository)) {
				return execution.terminal_failure(
					application::github_provider_failure::malformed_response);
			}

			auto pullRequestPage = execution.fetch(semantic_request_for(
				*server, aRequest.Context, read_operation::get_pull_request));
			if (!pullRequestPage.has_value()) {
				return std::unexpected{ pullRequestPage.error() };
			}
			auto pullRequest = parse_pull_request(
				*pullRequestPage, aRequest.Context, *repository, aLimits.Records);
			if (!pullRequest.has_value()) {
				return std::unexpected{ execution.enrich(pullRequest.error()) };
			}

			const auto expectedFiles   = pullRequest->ChangedFiles;
			const auto expectedCommits = std::min(pullRequest->Commits, application::maximum_github_pull_request_endpoint_commits);
			// An observation is independently bounded and validated. It grants no
			// snapshot authority; the same complete read and final audit still run.
			auto overviewBudget = application::detail::github_text_budget{ aLimits.Records };
			if (aObserver != nullptr && !aStop.stop_requested()
				&& application::detail::validate_repository(*repository, overviewBudget, aLimits.Records) == application::github_read_validation_error::none
				&& application::detail::validate_pull_request(pullRequest->Record, overviewBudget, aLimits.Records) == application::github_read_validation_error::none) {
				aObserver->overview_observed({ aRequest.Context, *repository, pullRequest->Record, expectedFiles });
			}
			application::github_pull_request_snapshot result{
				.Profile = aRequest.ProfileAuthority.Profile,
				.Host    = aRequest.ProfileAuthority.Host,
				.Account = aRequest.Viewer.Account,
				.Viewer  = application::github_actor_record{
					.Actor = aRequest.Viewer.Node,
					.Login = aRequest.Viewer.Login,
				},
				.Snapshot                   = aSnapshotId,
				.Repository                 = std::move(*repository),
				.PullRequest                = std::move(pullRequest->Record),
				.Files                      = { },
				.Commits                    = { },
				.Reviews                    = { },
				.ReviewThreadsAvailability  = application::github_collection_availability::unavailable,
				.ReviewThreads              = { },
				.ReviewCommentsAvailability = application::github_collection_availability::unavailable,
				.ReviewCommentGroups        = { },
				.IssueComments              = { },
				.Checks                     = { },
				.SearchResults              = { },
				.CapabilityEvidence         = application::github_capability_evidence{
					.Capabilities    = aRequest.CapabilityEvidence.Capabilities,
					.Revision        = aRequest.CapabilityEvidence.Revision,
					.ReadShape       = aRequest.CapabilityEvidence.ReadShape,
					.ContractVersion = 1U,
				},
				.RateLimits = { },
			};

			if (const auto files = read_rest_array_collection(
					execution,
					aProfile,
					semantic_request_for(*server,
										 aRequest.Context,
										 read_operation::list_pull_request_files),
					result.Files,
					aLimits.Records.MaximumFiles,
					aLimits.MaximumPageRecords,
					[](const page_json_value& aValue) { return parse_file(aValue); });
				!files.has_value()) {
				return std::unexpected{ execution.enrich(files.error()) };
			}
			if (result.Files.size() != expectedFiles) {
				return execution.terminal_failure(
					application::github_provider_failure::malformed_response);
			}

			if (const auto commits = read_rest_array_collection(
					execution,
					aProfile,
					semantic_request_for(*server,
										 aRequest.Context,
										 read_operation::list_pull_request_commits),
					result.Commits,
					aLimits.Records.MaximumCommits,
					aLimits.MaximumPageRecords,
					[](const page_json_value& aValue) { return parse_commit(aValue); });
				!commits.has_value()) {
				return std::unexpected{ execution.enrich(commits.error()) };
			}
			if (result.Commits.size() != expectedCommits || result.Commits.empty()
				|| result.Commits.back().Oid != result.PullRequest.HeadOid) {
				return execution.terminal_failure(
					application::github_provider_failure::malformed_response);
			}

			if (const auto reviews = read_rest_array_collection(
					execution,
					aProfile,
					semantic_request_for(
						*server, aRequest.Context, read_operation::list_reviews),
					result.Reviews,
					aLimits.Records.MaximumReviews,
					aLimits.MaximumPageRecords,
					[](const page_json_value& aValue) { return parse_review(aValue); });
				!reviews.has_value()) {
				return std::unexpected{ execution.enrich(reviews.error()) };
			}

			if (const auto graphql = perform_graphql_projection(execution,
																*server,
																aRequest.Context,
																aRequest.CapabilityEvidence.ReadShape,
																result,
																aLimits);
				!graphql.has_value()) {
				return std::unexpected{ execution.enrich(graphql.error()) };
			}

			// The inline review conversation is read over REST only under the
			// signed-out authority. An authenticated read already has the
			// GraphQL threads, which carry resolution the REST listing cannot;
			// issuing both would put the same conversation in two places and
			// would change the recorded authenticated wire shape. The
			// availability field stays `unavailable` on that path, which is
			// the truthful answer for a collection that was never asked for.
			if (aReadAuthority
				== github_provider_read_authority::anonymous_public) {
				std::vector<application::github_review_comment_record> flat;
				if (const auto reviewComments = read_rest_array_collection(
						execution,
						aProfile,
						semantic_request_for(*server,
											 aRequest.Context,
											 read_operation::list_review_comments),
						flat,
						aLimits.Records.MaximumTotalComments,
						aLimits.MaximumPageRecords,
						[](const page_json_value& aValue) {
							return parse_review_comment(aValue);
						});
					!reviewComments.has_value()) {
					return std::unexpected{ execution.enrich(reviewComments.error()) };
				}
				if (!group_review_comments(std::move(flat),
										   result.ReviewCommentGroups)) {
					return execution.terminal_failure(
						application::github_provider_failure::malformed_response);
				}
				result.ReviewCommentsAvailability = application::github_collection_availability::available;
			}

			if (const auto comments = read_rest_array_collection(
					execution,
					aProfile,
					semantic_request_for(*server,
										 aRequest.Context,
										 read_operation::list_issue_comments),
					result.IssueComments,
					aLimits.Records.MaximumIssueComments,
					aLimits.MaximumPageRecords,
					[](const page_json_value& aValue) { return parse_issue_comment(aValue); });
				!comments.has_value()) {
				return std::unexpected{ execution.enrich(comments.error()) };
			}

			// Finish mandatory content before optional results can spend the
			// remaining page/byte budget. Exhaustion then qualifies only details.
			auto searchSemantic = semantic_request_for(
				*server, aRequest.Context, read_operation::search_pull_requests);
			searchSemantic.SearchTerms = "is:open";
			const auto search          = read_search_results(execution,
															 aProfile,
															 std::move(searchSemantic),
															 result.Repository,
															 result.SearchResults,
															 aLimits);
			if (!search.has_value()) {
				return std::unexpected{ execution.enrich(search.error()) };
			}
			auto checksSemantic = semantic_request_for(
				*server, aRequest.Context, read_operation::list_checks);
			checksSemantic.HeadOid = std::string{ result.PullRequest.HeadOid.value() };
			if (const auto checks = read_checks(execution,
												aProfile,
												std::move(checksSemantic),
												result.PullRequest.HeadOid.value(),
												result.Checks,
												aLimits);
				!checks.has_value()) {
				if (fatal_optional_read_failure(checks.error())) {
					return std::unexpected{ checks.error() };
				}
				result.Checks.clear();
				result.ChecksAvailability = application::github_collection_availability::unavailable;
			}

			auto statusesSemantic    = semantic_request_for(*server, aRequest.Context, read_operation::list_commit_statuses);
			statusesSemantic.HeadOid = std::string{ result.PullRequest.HeadOid.value() };
			auto       statuses      = std::vector<application::github_check_record>{ };
			const auto statusRead    = read_rest_array_collection(execution, aProfile, std::move(statusesSemantic), statuses,
																  aLimits.Records.MaximumChecks, aLimits.MaximumPageRecords, [&](const page_json_value& aValue) { return parse_commit_status(aValue, result.PullRequest.HeadOid.value()); });
			if (statusRead) {
				// The statuses endpoint is newest first. One context has one current
				// value; old attempts never enter current totals, including across pages.
				auto contexts = std::unordered_set<std::string>{ };
				for (auto& status : statuses) {
					if (contexts.insert(status.Name).second) {
						result.Checks.push_back(std::move(status));
					}
				}
				result.StatusesAvailability = application::github_collection_availability::available;
			} else if (fatal_optional_read_failure(statusRead.error())) {
				return std::unexpected{ statusRead.error() };
			}

			if (aReadAuthority != github_provider_read_authority::anonymous_public) {
				// Optional explanation has a separate small aggregate budget. It
				// cannot exhaust essential PR reads or change their diagnostic origin.
				auto requirementLimits                          = aLimits;
				requirementLimits.MaximumTotalPages             = std::min(requirementLimits.MaximumTotalPages, std::size_t{ 32 });
				requirementLimits.MaximumAggregateResponseBytes = std::min(requirementLimits.MaximumAggregateResponseBytes, std::size_t{ 8U * 1024U * 1024U });
				production_read_execution requirementExecution{ aTransport, aProfile, aRequest.Context, requirementLimits, aStop, observedRateLimits, aRequest.AuthenticationSession, nullptr };
				if (!requirementExecution.bind_repository(result.Repository.Repository)) {
					return execution.terminal_failure(application::github_provider_failure::malformed_response);
				}
				const auto requirements = read_merge_requirements(requirementExecution, aProfile, *server, aRequest.Context, result, requirementLimits);
				if (!requirements) {
					return std::unexpected{ requirements.error() };
				}
			}

			// Received rows participate in ordinary validation even when their
			// search will be refused. No staged snapshot is published here.
			result.SearchAvailability = application::github_search_availability::available;

			if (aStop.stop_requested()) {
				return cancelled();
			}
			// The authority the read ran under, carried on the snapshot it
			// produced. The application layer validates the reader against
			// this field rather than against the request, so a signed-out read
			// that forgot to say so would be judged by the authenticated rule
			// and refused for holding no account.
			result.Mode       = aReadAuthority
					== github_provider_read_authority::anonymous_public
				? core::github_access_mode::public_preview
				: core::github_access_mode::authenticated;
			result.RateLimits = execution.rate_limits();
			auto validation   = application::validate(result, aRequest.Context, aLimits.Records);
			if (!validation.valid() && (validation.Error == application::github_read_validation_error::total_text_too_large || validation.Error == application::github_read_validation_error::collection_too_large)) {
				// Prefer accessible requests when optional policy details exhaust
				// the combined snapshot bound. Essential PR content stays usable.
				result.BranchPolicy.Rules.clear();
				result.BranchPolicy.Protection.clear();
				result.BranchPolicy.RulesAvailability      = observation::incomplete;
				result.BranchPolicy.ProtectionAvailability = observation::incomplete;
				validation                                 = application::validate(result, aRequest.Context, aLimits.Records);
				if (!validation.valid()) {
					result.ReviewRequests.clear();
					result.ReviewRequestsAvailability  = observation::incomplete;
					result.ReviewRequestActorsObserved = false;
					validation                         = application::validate(result, aRequest.Context, aLimits.Records);
				}
			}
			if (!validation.valid() && !result.Checks.empty()) {
				// Optional result detail cannot spend away otherwise readable PR facts.
				result.Checks.clear();
				result.ChecksAvailability   = application::github_collection_availability::unavailable;
				result.StatusesAvailability = application::github_collection_availability::unavailable;
				validation                  = application::validate(result, aRequest.Context, aLimits.Records);
			}
			if (!validation.valid()) {
				if (aObserver != nullptr) {
					aObserver->validation_failed(validation);
				}
				return execution.terminal_failure(
					validation_failure(validation.Error));
			}
			if (*search == application::github_search_availability::result_limit) {
				result.SearchResults.clear();
			}
			result.SearchAvailability = *search;
			return result;
		} catch (const std::bad_alloc&) {
			return failed(application::github_provider_failure::response_limit,
						  std::nullopt,
						  observedRateLimits);
		} catch (...) {
			return failed(application::github_provider_failure::malformed_response,
						  std::nullopt,
						  observedRateLimits);
		}
	}
}
