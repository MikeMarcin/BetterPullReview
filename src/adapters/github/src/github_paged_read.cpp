#include "github_paged_read.hpp"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
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
		constexpr std::size_t   maximum_header_name_bytes          = 256U;
		constexpr std::size_t   maximum_header_value_bytes         = 8U * 1024U;
		constexpr std::uint64_t maximum_retry_after_seconds        = 86'400U;
		constexpr double        maximum_exact_json_integer         = 9'007'199'254'740'991.0;
		constexpr std::size_t   maximum_production_page_bytes      = 4U * 1024U * 1024U;
		constexpr std::size_t   maximum_production_aggregate_bytes = 64U * 1024U * 1024U;
		constexpr std::size_t   maximum_production_pages           = 1'024U;
		constexpr std::size_t   maximum_production_page_records    = 100U;
		constexpr std::size_t   maximum_production_cursor_bytes    = 1'024U;
		constexpr std::uint32_t maximum_graphql_sequence           = 1'024U;

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

		[[nodiscard]] auto json_content_type(std::string_view aValue) noexcept -> bool
		{
			if (aValue.size() > 256U) {
				return false;
			}
			const auto separator = aValue.find(';');
			const auto media     = trim_ows(aValue.substr(0U, separator));
			if (!ascii_equal_fold(media, "application/json")) {
				return false;
			}
			if (separator == std::string_view::npos) {
				return true;
			}
			return ascii_equal_fold(trim_ows(aValue.substr(separator + 1U)),
									"charset=utf-8");
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
					|| header.Name.find_first_of("\r\n\0") != std::string::npos
					|| header.Value.find_first_of("\r\n\0") != std::string::npos) {
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
			if (!contentType.has_value() || !json_content_type(*contentType)
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

		enum class json_preflight_result : std::uint8_t {
			valid,
			malformed,
			duplicate_key,
			structural_limit,
		};

		class json_structural_preflight
		{
		public:
			json_structural_preflight(const std::string_view aInput,
									  const std::size_t      aMaximumStringBytes)
				: Input{ aInput }
				, MaximumStringBytes{ aMaximumStringBytes }
			{
			}

			[[nodiscard]] auto run() noexcept -> json_preflight_result
			{
				whitespace();
				const auto result = value(0U);
				if (result != json_preflight_result::valid) {
					return result;
				}
				whitespace();
				return Position == Input.size() ? json_preflight_result::valid
												: json_preflight_result::malformed;
			}

		private:
			static constexpr std::size_t maximum_depth              = 32U;
			static constexpr std::size_t maximum_raw_object_members = 256U;
			static constexpr std::size_t maximum_raw_array_elements = 10'000U;
			static constexpr std::size_t maximum_total_values       = 1'000'000U;

			[[nodiscard]] auto value(const std::size_t aDepth) noexcept
				-> json_preflight_result
			{
				if (Values == maximum_total_values) {
					return json_preflight_result::structural_limit;
				}
				++Values;
				whitespace();
				if (Position >= Input.size()) {
					return json_preflight_result::malformed;
				}
				switch (Input[Position]) {
				case '{':
					return object(aDepth);
				case '[':
					return array(aDepth);
				case '"':
					return string(nullptr, false) ? json_preflight_result::valid
												  : json_preflight_result::malformed;
				case 't':
					return literal("true");
				case 'f':
					return literal("false");
				case 'n':
					return literal("null");
				default:
					return number();
				}
			}

			[[nodiscard]] auto object(const std::size_t aDepth) noexcept
				-> json_preflight_result
			{
				if (aDepth >= maximum_depth) {
					return json_preflight_result::structural_limit;
				}
				++Position;
				whitespace();
				if (consume('}')) {
					return json_preflight_result::valid;
				}
				std::array<std::string_view, maximum_raw_object_members> keys;
				std::size_t                                              count = 0U;
				while (true) {
					if (count == keys.size()) {
						return json_preflight_result::structural_limit;
					}
					std::string_view key;
					if (!string(&key, true)) {
						return json_preflight_result::malformed;
					}
					if (std::ranges::find(keys.begin(),
										  keys.begin()
											  + static_cast<std::ptrdiff_t>(count),
										  key)
						!= keys.begin() + static_cast<std::ptrdiff_t>(count)) {
						return json_preflight_result::duplicate_key;
					}
					keys[count++] = key;
					whitespace();
					if (!consume(':')) {
						return json_preflight_result::malformed;
					}
					const auto child = value(aDepth + 1U);
					if (child != json_preflight_result::valid) {
						return child;
					}
					whitespace();
					if (consume('}')) {
						return json_preflight_result::valid;
					}
					if (!consume(',')) {
						return json_preflight_result::malformed;
					}
					whitespace();
				}
			}

			[[nodiscard]] auto array(const std::size_t aDepth) noexcept
				-> json_preflight_result
			{
				if (aDepth >= maximum_depth) {
					return json_preflight_result::structural_limit;
				}
				++Position;
				whitespace();
				if (consume(']')) {
					return json_preflight_result::valid;
				}
				std::size_t count = 0U;
				while (true) {
					if (count == maximum_raw_array_elements) {
						return json_preflight_result::structural_limit;
					}
					++count;
					const auto child = value(aDepth + 1U);
					if (child != json_preflight_result::valid) {
						return child;
					}
					whitespace();
					if (consume(']')) {
						return json_preflight_result::valid;
					}
					if (!consume(',')) {
						return json_preflight_result::malformed;
					}
					whitespace();
				}
			}

			[[nodiscard]] auto string(std::string_view* aOutput,
									  const bool        aObjectKey) noexcept -> bool
			{
				if (!consume('"')) {
					return false;
				}
				const auto start = Position;
				while (Position < Input.size()) {
					if (Position - start > MaximumStringBytes) {
						return false;
					}
					const auto character = static_cast<unsigned char>(Input[Position++]);
					if (character == '"') {
						if (aOutput != nullptr) {
							*aOutput = Input.substr(start, Position - start - 1U);
						}
						return true;
					}
					if (character < 0x20U) {
						return false;
					}
					if (character != '\\') {
						continue;
					}
					if (aObjectKey || Position >= Input.size()) {
						return false;
					}
					const auto escaped = Input[Position++];
					if (escaped == '"' || escaped == '\\' || escaped == '/'
						|| escaped == 'b' || escaped == 'f' || escaped == 'n'
						|| escaped == 'r' || escaped == 't') {
						continue;
					}
					if (escaped != 'u' || Input.size() - Position < 4U) {
						return false;
					}
					for (std::size_t index = 0U; index < 4U; ++index) {
						const auto digit = static_cast<unsigned char>(Input[Position++]);
						if (std::isxdigit(digit) == 0) {
							return false;
						}
					}
				}
				return false;
			}

			[[nodiscard]] auto number() noexcept -> json_preflight_result
			{
				const auto start = Position;
				static_cast<void>(consume('-'));
				if (consume('0')) {
					if (Position < Input.size() && Input[Position] >= '0'
						&& Input[Position] <= '9') {
						return json_preflight_result::malformed;
					}
				} else {
					if (Position >= Input.size() || Input[Position] < '1'
						|| Input[Position] > '9') {
						return json_preflight_result::malformed;
					}
					while (Position < Input.size() && Input[Position] >= '0'
						   && Input[Position] <= '9') {
						++Position;
					}
				}
				if (consume('.')) {
					const auto digits = Position;
					while (Position < Input.size() && Input[Position] >= '0'
						   && Input[Position] <= '9') {
						++Position;
					}
					if (Position == digits) {
						return json_preflight_result::malformed;
					}
				}
				if (Position < Input.size()
					&& (Input[Position] == 'e' || Input[Position] == 'E')) {
					++Position;
					if (Position < Input.size()
						&& (Input[Position] == '+' || Input[Position] == '-')) {
						++Position;
					}
					const auto digits = Position;
					while (Position < Input.size() && Input[Position] >= '0'
						   && Input[Position] <= '9') {
						++Position;
					}
					if (Position == digits) {
						return json_preflight_result::malformed;
					}
				}
				return Position != start ? json_preflight_result::valid
										 : json_preflight_result::malformed;
			}

			[[nodiscard]] auto literal(const std::string_view aExpected) noexcept
				-> json_preflight_result
			{
				if (Input.substr(Position, aExpected.size()) != aExpected) {
					return json_preflight_result::malformed;
				}
				Position += aExpected.size();
				return json_preflight_result::valid;
			}

			void whitespace() noexcept
			{
				while (Position < Input.size()
					   && (Input[Position] == ' ' || Input[Position] == '\t'
						   || Input[Position] == '\r' || Input[Position] == '\n')) {
					++Position;
				}
			}

			[[nodiscard]] auto consume(const char aExpected) noexcept -> bool
			{
				if (Position >= Input.size() || Input[Position] != aExpected) {
					return false;
				}
				++Position;
				return true;
			}

			std::string_view Input;
			std::size_t      MaximumStringBytes = 0U;
			std::size_t      Position           = 0U;
			std::size_t      Values             = 0U;
		};

		[[nodiscard]] auto exact_integer(const QJsonValue& aValue,
										 const bool        aPositive = false) noexcept -> bool
		{
			if (!aValue.isDouble()) {
				return false;
			}
			const auto number = aValue.toDouble();
			return std::isfinite(number) && std::trunc(number) == number
				&& number >= (aPositive ? 1.0 : 0.0)
				&& number <= maximum_exact_json_integer;
		}

		[[nodiscard]] auto integer(const QJsonValue& aValue) noexcept -> std::uint64_t
		{
			return static_cast<std::uint64_t>(aValue.toDouble());
		}

		[[nodiscard]] auto uint32(const QJsonValue& aValue) noexcept
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

		[[nodiscard]] auto text(const QJsonValue& aValue) -> std::string
		{
			const auto bytes = aValue.toString().toUtf8();
			return {
				bytes.constData(),
				static_cast<std::size_t>(bytes.size()),
			};
		}

		[[nodiscard]] auto nonempty_string(const QJsonValue& aValue) -> bool
		{
			return aValue.isString() && !aValue.toString().isEmpty();
		}

		[[nodiscard]] auto bounded_text(const std::string_view aValue,
										const std::size_t      aMaximum,
										const bool             aRequired = false) noexcept -> bool
		{
			return (!aRequired || !aValue.empty()) && aValue.size() <= aMaximum
				&& aValue.find('\0') == std::string_view::npos;
		}

		[[nodiscard]] auto sorted_keys(const QJsonObject& aObject)
			-> std::vector<std::string>
		{
			std::vector<std::string> keys;
			keys.reserve(static_cast<std::size_t>(aObject.size()));
			for (auto iterator = aObject.constBegin(); iterator != aObject.constEnd();
				 ++iterator) {
				keys.emplace_back(iterator.key().toStdString());
			}
			std::ranges::sort(keys);
			return keys;
		}

		[[nodiscard]] auto
		has_exact_keys(const QJsonObject&                      aObject,
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
		has_exact_keys(const QJsonObject&                            aObject,
					   const std::initializer_list<std::string_view> aExpected) -> bool
		{
			return has_exact_keys(
				aObject,
				std::span<const std::string_view>{ aExpected.begin(), aExpected.size() });
		}

		[[nodiscard]] auto
		has_required_keys(const QJsonObject&                      aObject,
						  const std::span<const std::string_view> aRequired) -> bool
		{
			return std::ranges::all_of(
				aRequired,
				[&aObject](const std::string_view aKey) { return aObject.contains(QString::fromUtf8(aKey)); });
		}

		[[nodiscard]] auto
		has_required_keys(const QJsonObject&                            aObject,
						  const std::initializer_list<std::string_view> aRequired)
			-> bool
		{
			return has_required_keys(
				aObject,
				std::span<const std::string_view>{ aRequired.begin(), aRequired.size() });
		}

		[[nodiscard]] auto graphql_success_envelope(const QJsonObject& aObject) -> bool
		{
			return has_exact_keys(aObject, { "data" })
				|| (has_exact_keys(aObject, { "data", "extensions" })
					&& aObject.value("extensions").isObject());
		}

		[[nodiscard]] auto one_of(const QJsonValue&                             aValue,
								  const std::initializer_list<std::string_view> aAllowed)
			-> bool
		{
			if (!aValue.isString()) {
				return false;
			}
			const auto candidate = text(aValue);
			return std::ranges::find(aAllowed, candidate) != aAllowed.end();
		}

		[[nodiscard]] auto hexadecimal_oid(const QJsonValue& aValue) -> bool
		{
			if (!aValue.isString()) {
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

		[[nodiscard]] auto valid_line(const QJsonValue& aValue) noexcept -> bool
		{
			return exact_integer(aValue, true)
				&& aValue.toDouble()
				<= static_cast<double>(application::maximum_github_line);
		}

		[[nodiscard]] auto optional_line(const QJsonValue& aPreferred,
										 const QJsonValue& aFallback)
			-> std::optional<std::uint32_t>
		{
			const auto selected = aPreferred.isNull() ? aFallback : aPreferred;
			return selected.isNull() ? std::nullopt : uint32(selected);
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
		secondary_rate_limit_message(const QJsonDocument* const aDocument) -> bool
		{
			if (aDocument == nullptr || !aDocument->isObject()) {
				return false;
			}
			const auto message = aDocument->object().value("message");
			if (!message.isString()) {
				return false;
			}
			const auto raw = text(message);
			return bounded_text(raw, 4U * 1'024U, true)
				&& ascii_contains_fold(raw, "secondary rate limit");
		}

		[[nodiscard]] auto graphql_error_rate_limited(const QJsonValue& aErrors) -> bool
		{
			if (!aErrors.isArray()) {
				return false;
			}
			for (const auto& value : aErrors.toArray()) {
				if (!value.isObject()) {
					continue;
				}
				const auto error = value.toObject();
				if (error.value("type").isString()
					&& error.value("type").toString() == QStringLiteral("RATE_LIMITED")) {
					return true;
				}
				const auto extensions = error.value("extensions");
				if (!extensions.isObject()) {
					continue;
				}
				for (const auto key : { "type", "code" }) {
					if (extensions.toObject().value(key).isString()
						&& extensions.toObject().value(key).toString()
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
			if (aOperation == read_operation::list_review_threads) {
				return application::github_rate_limit_resource::graphql;
			}
			if (aOperation == read_operation::search_pull_requests) {
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
			QJsonDocument            Document;
		};

		class production_read_execution
		{
		public:
			production_read_execution(github_wire_transport_port&               aTransport,
									  const validated_github_host_profile&      aProfile,
									  const core::pull_request_request_context& aParent,
									  const github_paged_read_limits&           aLimits,
									  const std::stop_token                     aStop,
									  application::github_rate_limit_state&     aRateLimits)
				: Transport{ aTransport }
				, Profile{ aProfile }
				, Parent{ aParent }
				, Limits{ aLimits }
				, Stop{ aStop }
				, RateLimits{ aRateLimits }
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
				if (Pages == Limits.MaximumTotalPages) {
					return terminal_failure(
						application::github_provider_failure::response_limit);
				}

				const auto compiled = compile_read_request(aSemantic);
				if (!compiled.has_value()) {
					return terminal_failure(
						application::github_provider_failure::malformed_response);
				}
				const github_wire_correlation correlation{
					.Parent          = Parent,
					.ProfileRevision = Profile.revision(),
					.Host            = Profile.host(),
					.Operation       = aSemantic.Operation,
					.Page            = aSemantic.Page,
				};
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
					const auto bytes = response_byte_count(success->Response);
					if (!bytes.has_value() || *bytes > Limits.MaximumPageBytes
						|| *bytes
							> Limits.MaximumAggregateResponseBytes - AggregateBytes) {
						return terminal_failure(
							application::github_provider_failure::response_limit);
					}
					++Pages;
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
						std::optional<QJsonDocument> errorDocument;
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
								QJsonParseError error;
								auto            candidate = QJsonDocument::fromJson(
									QByteArray{
										success->Response.Body.data(),
										static_cast<qsizetype>(
											success->Response.Body.size()),
									},
									&error);
								if (error.error == QJsonParseError::NoError) {
									errorDocument = std::move(candidate);
								}
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
					QJsonParseError parseError;
					auto            document = QJsonDocument::fromJson(
						QByteArray{
							success->Response.Body.data(),
							static_cast<qsizetype>(success->Response.Body.size()),
						},
						&parseError);
					if (parseError.error != QJsonParseError::NoError) {
						return metadataFailed(
							application::github_provider_failure::malformed_response);
					}
					if (aSemantic.Operation == read_operation::list_review_threads
						&& document.isObject() && document.object().contains("errors")) {
						const auto structuredRateLimit = graphql_error_rate_limited(
							document.object().value("errors"));
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
						return metadataFailed(
							application::github_provider_failure::malformed_response);
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
			github_wire_transport_port&               Transport;
			const validated_github_host_profile&      Profile;
			const core::pull_request_request_context& Parent;
			const github_paged_read_limits&           Limits;
			std::stop_token                           Stop;
			std::size_t                               Pages          = 0U;
			std::size_t                               AggregateBytes = 0U;
			application::github_rate_limit_state&     RateLimits;
			std::optional<core::repository_id>        BoundRepository;
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
			case read_operation::list_reviews:
			case read_operation::list_review_threads:
			case read_operation::list_issue_comments:
				result.PullRequestNumber = aContext.Number.value();
				break;
			case read_operation::get_viewer_identity:
			case read_operation::get_repository:
			case read_operation::list_checks:
			case read_operation::search_pull_requests:
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

		[[nodiscard]] auto parse_link_relations(const std::string_view aValue)
			-> std::optional<std::vector<link_relation>>
		{
			if (aValue.empty() || aValue.size() > maximum_header_value_bytes) {
				return std::nullopt;
			}
			std::vector<link_relation> relations;
			std::size_t                position = 0U;
			while (position < aValue.size()) {
				if (relations.size() == 4U) {
					return std::nullopt;
				}
				auto segmentEnd = aValue.find(',', position);
				if (segmentEnd == std::string_view::npos) {
					segmentEnd = aValue.size();
				}
				const auto segment = trim_ows(aValue.substr(position, segmentEnd - position));
				const auto close   = segment.find('>');
				if (segment.size() < 10U || segment.front() != '<'
					|| close == std::string_view::npos || close == 1U) {
					return std::nullopt;
				}
				const auto                 parameter = trim_ows(segment.substr(close + 1U));
				constexpr std::string_view prefix{ "; rel=\"" };
				if (!parameter.starts_with(prefix)
					|| parameter.size() <= prefix.size() + 1U || parameter.back() != '"'
					|| parameter.find('"', prefix.size()) != parameter.size() - 1U) {
					return std::nullopt;
				}
				const auto relation = parameter.substr(
					prefix.size(), parameter.size() - prefix.size() - 1U);
				if (relation != "next" && relation != "prev" && relation != "first"
					&& relation != "last") {
					return std::nullopt;
				}
				if (std::ranges::any_of(relations,
										[relation](const link_relation& aExisting) { return aExisting.Relation == relation; })) {
					return std::nullopt;
				}
				relations.push_back(link_relation{
					.Url      = std::string{ segment.substr(1U, close - 1U) },
					.Relation = std::string{ relation },
				});
				position = segmentEnd;
				if (position < aValue.size()) {
					if (position + 1U == aValue.size()) {
						return std::nullopt;
					}
					++position;
				}
			}
			return relations.empty() ? std::nullopt
									 : std::optional{ std::move(relations) };
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
			case read_operation::list_reviews:
			case read_operation::list_issue_comments:
			case read_operation::list_checks:
				break;
			case read_operation::get_viewer_identity:
			case read_operation::get_repository:
			case read_operation::get_pull_request:
			case read_operation::list_review_threads:
			case read_operation::search_pull_requests:
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
			if (aOperation != read_operation::search_pull_requests || aName != "q") {
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

		[[nodiscard]] auto parse_rest_actor(const QJsonValue& aValue)
			-> std::optional<application::github_actor_record>
		{
			if (aValue.isNull()) {
				return std::optional<application::github_actor_record>{ };
			}
			if (!aValue.isObject()
				|| !has_required_keys(aValue.toObject(), { "login", "node_id" })
				|| !nonempty_string(aValue.toObject().value("login"))
				|| !nonempty_string(aValue.toObject().value("node_id"))) {
				return std::nullopt;
			}
			const auto object = aValue.toObject();
			return application::github_actor_record{
				.Actor = application::github_actor_id{ text(object.value("node_id")) },
				.Login = text(object.value("login")),
			};
		}

		struct optional_actor_parse
		{
			bool                                            Valid = false;
			std::optional<application::github_actor_record> Actor;
		};

		[[nodiscard]] auto optional_rest_actor(const QJsonValue& aValue)
			-> optional_actor_parse
		{
			if (aValue.isNull()) {
				return { .Valid = true, .Actor = std::nullopt };
			}
			const auto actor = parse_rest_actor(aValue);
			return actor.has_value()
				? optional_actor_parse{ .Valid = true, .Actor = actor }
				: optional_actor_parse{ };
		}

		[[nodiscard]] auto optional_graphql_actor(const QJsonValue& aValue)
			-> optional_actor_parse
		{
			if (aValue.isNull()) {
				return { .Valid = true, .Actor = std::nullopt };
			}
			if (!aValue.isObject() || !has_exact_keys(aValue.toObject(), { "id", "login" })
				|| !nonempty_string(aValue.toObject().value("id"))
				|| !nonempty_string(aValue.toObject().value("login"))) {
				return { };
			}
			const auto object = aValue.toObject();
			return {
				.Valid = true,
				.Actor = application::github_actor_record{
					.Actor = application::github_actor_id{ text(object.value("id")) },
					.Login = text(object.value("login")),
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

		[[nodiscard]] auto check_conclusion(const QJsonValue& aValue) noexcept
			-> application::github_check_conclusion
		{
			if (aValue.isNull()) {
				return application::github_check_conclusion::none;
			}
			const auto conclusion = aValue.toString();
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
			QJsonArray                 Nodes;
			std::optional<std::string> NextCursor;
		};

		[[nodiscard]] auto
		parse_graphql_connection(const QJsonValue&               aValue,
								 const github_paged_read_limits& aLimits)
			-> std::expected<graphql_connection_page, github_paged_read_failure>
		{
			if (!aValue.isObject()
				|| !has_exact_keys(aValue.toObject(),
								   { "nodes", "pageInfo", "totalCount" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto connection = aValue.toObject();
			if (!exact_integer(connection.value("totalCount"))
				|| !connection.value("nodes").isArray()
				|| !connection.value("pageInfo").isObject()
				|| !has_exact_keys(connection.value("pageInfo").toObject(),
								   { "endCursor", "hasNextPage" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto nodes    = connection.value("nodes").toArray();
			const auto pageInfo = connection.value("pageInfo").toObject();
			if (static_cast<std::size_t>(nodes.size()) > aLimits.MaximumPageRecords
				|| !pageInfo.value("hasNextPage").isBool()) {
				return failed(application::github_provider_failure::response_limit);
			}
			const auto                 total       = integer(connection.value("totalCount"));
			const auto                 hasNext     = pageInfo.value("hasNextPage").toBool();
			const auto                 cursorValue = pageInfo.value("endCursor");
			std::optional<std::string> nextCursor;
			if (hasNext) {
				if (static_cast<std::size_t>(nodes.size()) != 100U
					|| !cursorValue.isString()) {
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
			} else if ((nodes.isEmpty() && !cursorValue.isNull())
					   || (!nodes.isEmpty()
						   && (!cursorValue.isString()
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

		[[nodiscard]] auto parse_graphql_comment(const QJsonValue& aValue)
			-> std::expected<application::github_review_comment_record,
							 github_paged_read_failure>
		{
			if (!aValue.isObject()
				|| !has_exact_keys(aValue.toObject(),
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
			const auto comment      = aValue.toObject();
			const auto author       = optional_graphql_actor(comment.value("author"));
			const auto line         = comment.value("line");
			const auto originalLine = comment.value("originalLine");
			const auto reply        = comment.value("replyTo");
			if (!nonempty_string(comment.value("id"))
				|| !comment.value("bodyText").isString()
				|| !nonempty_string(comment.value("path"))
				|| !comment.value("outdated").isBool() || !author.Valid
				|| (!line.isNull() && !valid_line(line))
				|| (!originalLine.isNull() && !valid_line(originalLine))
				|| (!reply.isNull()
					&& (!reply.isObject() || !has_exact_keys(reply.toObject(), { "id" })
						|| !nonempty_string(reply.toObject().value("id"))))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			std::optional<application::github_comment_id> replyTo;
			if (reply.isObject()) {
				replyTo = application::github_comment_id{ text(reply.toObject().value("id")) };
			}
			return application::github_review_comment_record{
				.Comment  = application::github_comment_id{ text(comment.value("id")) },
				.ReplyTo  = std::move(replyTo),
				.Author   = author.Actor,
				.Body     = text(comment.value("bodyText")),
				.Path     = text(comment.value("path")),
				.Line     = optional_line(line, originalLine),
				.Outdated = comment.value("outdated").toBool(),
				.Deleted  = false,
			};
		}

		struct parsed_graphql_thread
		{
			application::github_review_thread_record Record;
			std::uint64_t                            TotalComments = 0U;
			std::optional<std::string>               NextCommentsCursor;
		};

		[[nodiscard]] auto
		parse_graphql_thread(const QJsonValue&                     aValue,
							 const application::github_read_shape& aShape,
							 const github_paged_read_limits&       aLimits)
			-> std::expected<parsed_graphql_thread, github_paged_read_failure>
		{
			if (!aValue.isObject()) {
				return failed(application::github_provider_failure::malformed_response);
			}
			std::vector<std::string_view> keys{
				"comments",
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
			const auto thread = aValue.toObject();
			if (!has_exact_keys(thread, keys)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto comments = parse_graphql_connection(thread.value("comments"), aLimits);
			if (!comments.has_value()) {
				return std::unexpected{ comments.error() };
			}
			if (comments->TotalCount > aLimits.Records.MaximumCommentsPerThread
				|| comments->TotalCount > aLimits.Records.MaximumTotalComments) {
				return failed(application::github_provider_failure::response_limit);
			}
			const auto line         = thread.value("line");
			const auto originalLine = thread.value("originalLine");
			const auto fileSubject  = thread.value("subjectType").toString() == QStringLiteral("FILE");
			const auto lineSubject  = thread.value("subjectType").toString() == QStringLiteral("LINE");
			if (!nonempty_string(thread.value("id"))
				|| !thread.value("isOutdated").isBool()
				|| !thread.value("isResolved").isBool()
				|| !nonempty_string(thread.value("path"))
				|| (!fileSubject && !lineSubject)
				|| (!line.isNull() && !valid_line(line))
				|| (!originalLine.isNull() && !valid_line(originalLine))
				|| (fileSubject && (!line.isNull() || !originalLine.isNull()))
				|| (lineSubject && line.isNull() && originalLine.isNull())
				|| (aShape.ResolvePermission
					&& !thread.value("viewerCanResolve").isBool())
				|| (aShape.UnresolvePermission
					&& !thread.value("viewerCanUnresolve").isBool())) {
				return failed(application::github_provider_failure::malformed_response);
			}

			application::github_review_thread_record record{
				.Thread                    = application::github_thread_id{ text(thread.value("id")) },
				.Subject                   = fileSubject
					? application::github_review_thread_subject::file
					: application::github_review_thread_subject::line,
				.Path                      = text(thread.value("path")),
				.Line                      = optional_line(line, originalLine),
				.Resolved                  = thread.value("isResolved").toBool(),
				.Outdated                  = thread.value("isOutdated").toBool(),
				.ViewerResolvePermission   = aShape.ResolvePermission
					? (thread.value("viewerCanResolve").toBool()
						   ? application::github_permission_state::allowed
						   : application::github_permission_state::denied)
					: application::github_permission_state::unavailable,
				.ViewerUnresolvePermission = aShape.UnresolvePermission
					? (thread.value("viewerCanUnresolve").toBool()
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
						&& comment->Line.has_value())
					|| (record.Subject == application::github_review_thread_subject::line
						&& !comment->Line.has_value())) {
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
			QJsonObject*                                   aDataOutput,
			QJsonObject*                                   aRepositoryOutput,
			QJsonObject*                                   aPullRequestOutput)
			-> std::expected<void, github_paged_read_failure>
		{
			if (!singleton_complete(aPage) || !aPage.Document.isObject()
				|| !graphql_success_envelope(aPage.Document.object())) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto dataValue = aPage.Document.object().value("data");
			if (!dataValue.isObject()
				|| !has_exact_keys(dataValue.toObject(), { "repository" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto repositoryValue = dataValue.toObject().value("repository");
			if (!repositoryValue.isObject()
				|| !has_exact_keys(repositoryValue.toObject(), { "id", "pullRequest" })
				|| text(repositoryValue.toObject().value("id"))
					!= aRepository.Node.value()) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto pullRequestValue = repositoryValue.toObject().value("pullRequest");
			if (!pullRequestValue.isObject()
				|| text(pullRequestValue.toObject().value("id"))
					!= aPullRequest.Node.value()) {
				return failed(application::github_provider_failure::malformed_response);
			}
			*aDataOutput        = dataValue.toObject();
			*aRepositoryOutput  = repositoryValue.toObject();
			*aPullRequestOutput = pullRequestValue.toObject();
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
			const github_paged_read_limits&                          aLimits) noexcept -> bool
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
			return limits_valid(aLimits) && aSnapshot.value() != 0U
				&& application::detail::valid_context(context)
				&& application::valid(authority) && aRequest.Attempt.valid()
				&& aRequest.AccountTransport != nullptr
				&& aRequest.Viewer.Account.value() != 0U
				&& aRequest.Viewer.Account == context.Account
				&& bounded_text(aRequest.Viewer.Node.value(),
								aLimits.Records.MaximumIdentityBytes,
								true)
				&& bounded_text(
					   aRequest.Viewer.Login, aLimits.Records.MaximumLoginBytes, true)
				&& bounded_text(aRequest.Viewer.DisplayName,
								aLimits.Records.MaximumDisplayNameBytes)
				&& authority.Profile == context.Profile
				&& authority.Profile == aProfile.profile()
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
				&& evidence.Authority.ProbeContractRevision
				== authority.ProbeContractRevision
				&& evidence.Revision == context.Capabilities
				&& evidence.ExpiresBefore > evidence.ObservedAt
				&& evidence.ExpiresBefore.value() - evidence.ObservedAt.value()
				<= application::maximum_github_capability_freshness_window
				&& evidence.Capabilities
				== application::projected_capabilities(observation)
				&& evidence.ReadShape
				== application::projected_read_shape(observation);
		}

		[[nodiscard]] auto
		parse_repository(const parsed_page&                        aPage,
						 const core::pull_request_request_context& aContext)
			-> std::expected<application::github_repository_record,
							 github_paged_read_failure>
		{
			if (!singleton_complete(aPage) || !aPage.Document.isObject()
				|| !has_required_keys(aPage.Document.object(),
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
			const auto object = aPage.Document.object();
			const auto owner  = object.value("owner");
			if (!exact_integer(object.value("id"), true)
				|| !nonempty_string(object.value("name"))
				|| !nonempty_string(object.value("full_name"))
				|| !nonempty_string(object.value("node_id"))
				|| !object.value("private").isBool()
				|| !nonempty_string(object.value("default_branch")) || !owner.isObject()
				|| !has_required_keys(owner.toObject(), { "login" })
				|| !nonempty_string(owner.toObject().value("login"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto rawOwner    = text(owner.toObject().value("login"));
			const auto rawName     = text(object.value("name"));
			const auto rawFullName = text(object.value("full_name"));
			if (!ascii_equal_fold(rawOwner, aContext.Repository.Owner)
				|| !ascii_equal_fold(rawName, aContext.Repository.Name)
				|| !ascii_equal_fold(rawFullName, rawOwner + "/" + rawName)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto id = core::repository_id{ integer(object.value("id")) };
			if (aContext.ExpectedRepository.has_value()
				&& *aContext.ExpectedRepository != id) {
				return failed(application::github_provider_failure::malformed_response);
			}
			return application::github_repository_record{
				.Repository    = id,
				.Node          = application::github_node_id{ text(object.value("node_id")) },
				.Owner         = rawOwner,
				.Name          = rawName,
				.DefaultBranch = text(object.value("default_branch")),
				.Private       = object.value("private").toBool(),
			};
		}

		struct parsed_pull_ref
		{
			std::string                        Ref;
			std::string                        Oid;
			std::optional<core::repository_id> Repository;
			std::optional<std::string>         FullName;
		};

		[[nodiscard]] auto parse_pull_ref(const QJsonValue& aValue,
										  const bool        aRepositoryRequired)
			-> std::optional<parsed_pull_ref>
		{
			if (!aValue.isObject()
				|| !has_required_keys(aValue.toObject(), { "ref", "repo", "sha" })) {
				return std::nullopt;
			}
			const auto object = aValue.toObject();
			if (!nonempty_string(object.value("ref"))
				|| !hexadecimal_oid(object.value("sha"))) {
				return std::nullopt;
			}
			parsed_pull_ref result{
				.Ref        = text(object.value("ref")),
				.Oid        = text(object.value("sha")),
				.Repository = std::nullopt,
				.FullName   = std::nullopt,
			};
			const auto repository = object.value("repo");
			if (repository.isNull()) {
				return aRepositoryRequired ? std::nullopt
										   : std::optional{ std::move(result) };
			}
			if (!repository.isObject()
				|| !has_required_keys(repository.toObject(), { "full_name", "id" })
				|| !exact_integer(repository.toObject().value("id"), true)
				|| !nonempty_string(repository.toObject().value("full_name"))) {
				return std::nullopt;
			}
			result.Repository = core::repository_id{ integer(repository.toObject().value("id")) };
			result.FullName   = text(repository.toObject().value("full_name"));
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
			if (!singleton_complete(aPage) || !aPage.Document.isObject()
				|| !has_required_keys(aPage.Document.object(),
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
			const auto object = aPage.Document.object();
			const auto base   = parse_pull_ref(object.value("base"), true);
			const auto head   = parse_pull_ref(object.value("head"), false);
			const auto author = optional_rest_actor(object.value("user"));
			if (!exact_integer(object.value("id"), true)
				|| !exact_integer(object.value("number"), true)
				|| integer(object.value("number")) > core::maximum_pull_request_number
				|| !exact_integer(object.value("changed_files"))
				|| !exact_integer(object.value("commits"), true)
				|| !nonempty_string(object.value("node_id"))
				|| !nonempty_string(object.value("title"))
				|| (!object.value("body").isNull() && !object.value("body").isString())
				|| !one_of(object.value("state"), { "open", "closed" })
				|| !object.value("draft").isBool() || !object.value("merged").isBool()
				|| !base.has_value() || !head.has_value() || !author.Valid
				|| (object.value("merged").toBool()
					&& object.value("state").toString() != QStringLiteral("closed"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto changedFiles = integer(object.value("changed_files"));
			const auto commits      = integer(object.value("commits"));
			if (changedFiles > aLimits.MaximumFiles || commits > aLimits.MaximumCommits) {
				return failed(application::github_provider_failure::response_limit);
			}
			const auto id     = core::pull_request_id{ integer(object.value("id")) };
			const auto number = core::pull_request_number{
				static_cast<std::uint32_t>(integer(object.value("number")))
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
					.PullRequest = id,
					.Repository  = aRepository.Repository,
					.Number      = number,
					.Node        = application::github_node_id{ text(object.value("node_id")) },
					.Title       = text(object.value("title")),
					.Body        = object.value("body").isNull()
						? std::string{ }
						: text(object.value("body")),
					.State       = pull_request_state(object.value("state").toString(),
													  object.value("merged").toBool()),
					.Draft       = object.value("draft").toBool(),
					.Author      = author.Actor,
					.BaseOid     = application::commit_oid{ base->Oid },
					.HeadOid     = application::commit_oid{ head->Oid },
					.BaseRef     = base->Ref,
					.HeadRef     = head->Ref,
				},
				.ChangedFiles = static_cast<std::size_t>(changedFiles),
				.Commits      = static_cast<std::size_t>(commits),
			};
		}

		[[nodiscard]] auto parse_file(const QJsonValue& aValue)
			-> std::expected<application::github_file_record, github_paged_read_failure>
		{
			if (!aValue.isObject()) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object      = aValue.toObject();
			const auto hasPrevious = object.contains("previous_filename");
			if (!has_required_keys(object,
								   { "additions", "deletions", "filename", "status" })
				|| !nonempty_string(object.value("filename"))
				|| !one_of(object.value("status"),
						   {
							   "added",
							   "modified",
							   "removed",
							   "renamed",
							   "copied",
							   "changed",
							   "unchanged",
						   })
				|| !exact_integer(object.value("additions"))
				|| !exact_integer(object.value("deletions"))
				|| (hasPrevious
					&& (!nonempty_string(object.value("previous_filename"))
						|| (object.value("status").toString()
								!= QStringLiteral("renamed")
							&& object.value("status").toString()
								!= QStringLiteral("copied"))))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			std::optional<std::string> previousPath;
			if (hasPrevious) {
				previousPath = text(object.value("previous_filename"));
			}
			return application::github_file_record{
				.Path         = text(object.value("filename")),
				.PreviousPath = std::move(previousPath),
				.Change       = file_change(object.value("status").toString()),
				.Additions    = integer(object.value("additions")),
				.Deletions    = integer(object.value("deletions")),
				.ContentKind  = application::github_file_content_kind::unknown,
				.ViewedState  = application::github_file_viewed_state::unavailable,
			};
		}

		[[nodiscard]] auto parse_commit(const QJsonValue& aValue)
			-> std::expected<application::github_commit_record,
							 github_paged_read_failure>
		{
			if (!aValue.isObject()
				|| !has_required_keys(aValue.toObject(),
									  { "author", "commit", "node_id", "sha" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = aValue.toObject();
			const auto author = optional_rest_actor(object.value("author"));
			const auto commit = object.value("commit");
			if (!hexadecimal_oid(object.value("sha"))
				|| !nonempty_string(object.value("node_id")) || !author.Valid
				|| !commit.isObject()
				|| !has_required_keys(commit.toObject(), { "message" })
				|| !commit.toObject().value("message").isString()) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto headline = text(commit.toObject().value("message"));
			if (const auto newline = headline.find('\n'); newline != std::string::npos) {
				headline.resize(newline);
			}
			return application::github_commit_record{
				.Oid      = application::commit_oid{ text(object.value("sha")) },
				.Node     = application::github_node_id{ text(object.value("node_id")) },
				.Author   = author.Actor,
				.Headline = std::move(headline),
			};
		}

		[[nodiscard]] auto parse_review(const QJsonValue& aValue)
			-> std::expected<application::github_review_record,
							 github_paged_read_failure>
		{
			if (!aValue.isObject()
				|| !has_required_keys(aValue.toObject(),
									  { "body",
										"commit_id",
										"id",
										"node_id",
										"state",
										"user" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = aValue.toObject();
			const auto author = optional_rest_actor(object.value("user"));
			if (!exact_integer(object.value("id"), true)
				|| !nonempty_string(object.value("node_id"))
				|| !object.value("body").isString() || !author.Valid
				|| !one_of(object.value("state"),
						   {
							   "PENDING",
							   "COMMENTED",
							   "APPROVED",
							   "CHANGES_REQUESTED",
							   "DISMISSED",
						   })
				|| (!object.value("commit_id").isNull()
					&& !hexadecimal_oid(object.value("commit_id")))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			std::optional<application::commit_oid> commit;
			if (!object.value("commit_id").isNull()) {
				commit = application::commit_oid{ text(object.value("commit_id")) };
			}
			return application::github_review_record{
				.DatabaseId = integer(object.value("id")),
				.Review     = application::github_review_id{ text(object.value("node_id")) },
				.Author     = author.Actor,
				.State      = review_state(object.value("state").toString()),
				.Commit     = std::move(commit),
				.Body       = text(object.value("body")),
			};
		}

		[[nodiscard]] auto parse_issue_comment(const QJsonValue& aValue)
			-> std::expected<application::github_issue_comment_record,
							 github_paged_read_failure>
		{
			if (!aValue.isObject()
				|| !has_required_keys(aValue.toObject(), { "body", "node_id", "user" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = aValue.toObject();
			const auto author = optional_rest_actor(object.value("user"));
			if (!nonempty_string(object.value("node_id"))
				|| !object.value("body").isString() || !author.Valid) {
				return failed(application::github_provider_failure::malformed_response);
			}
			return application::github_issue_comment_record{
				.Comment = application::github_comment_id{ text(object.value("node_id")) },
				.Author  = author.Actor,
				.Body    = text(object.value("body")),
				.Deleted = false,
			};
		}

		[[nodiscard]] auto parse_check(const QJsonValue&      aValue,
									   const std::string_view aHeadOid)
			-> std::expected<application::github_check_record,
							 github_paged_read_failure>
		{
			if (!aValue.isObject()
				|| !has_required_keys(aValue.toObject(),
									  {
										  "conclusion",
										  "head_sha",
										  "name",
										  "node_id",
										  "status",
									  })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object    = aValue.toObject();
			const auto completed = object.value("status").toString() == QStringLiteral("completed");
			if (!nonempty_string(object.value("node_id"))
				|| !nonempty_string(object.value("name"))
				|| !hexadecimal_oid(object.value("head_sha"))
				|| text(object.value("head_sha")) != aHeadOid
				|| !one_of(object.value("status"),
						   {
							   "queued",
							   "in_progress",
							   "completed",
							   "waiting",
							   "requested",
							   "pending",
						   })
				|| (!object.value("conclusion").isNull()
					&& !one_of(object.value("conclusion"),
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
				|| (completed != !object.value("conclusion").isNull())) {
				return failed(application::github_provider_failure::malformed_response);
			}
			return application::github_check_record{
				.Check      = application::github_check_id{ text(object.value("node_id")) },
				.Name       = text(object.value("name")),
				.Status     = check_status(object.value("status").toString()),
				.Conclusion = check_conclusion(object.value("conclusion")),
			};
		}

		[[nodiscard]] auto
		parse_search_result(const QJsonValue&                            aValue,
							const application::github_repository_record& aRepository,
							const validated_github_host_profile&         aProfile)
			-> std::expected<application::github_search_result_record,
							 github_paged_read_failure>
		{
			if (!aValue.isObject()
				|| !has_required_keys(aValue.toObject(),
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
			const auto object      = aValue.toObject();
			const auto number      = uint32(object.value("number"));
			const auto pullRequest = object.value("pull_request");
			if (!number.has_value() || *number == 0U
				|| *number > core::maximum_pull_request_number
				|| !nonempty_string(object.value("node_id"))
				|| !nonempty_string(object.value("title"))
				|| !nonempty_string(object.value("repository_url"))
				|| !nonempty_string(object.value("html_url")) || !pullRequest.isObject()
				|| !has_required_keys(pullRequest.toObject(), { "url" })
				|| !nonempty_string(pullRequest.toObject().value("url"))
				|| !one_of(object.value("state"), { "open" })
				|| !object.value("draft").isBool()) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto expectedRepositoryUrl = std::string{ aProfile.rest_root() }
				+ "/repos/" + aRepository.Owner + "/"
				+ aRepository.Name;
			const auto expectedHtmlUrl = "https://" + std::string{ aProfile.hostname() } + "/" + aRepository.Owner
				+ "/" + aRepository.Name + "/pull/" + std::to_string(*number);
			const auto expectedPullRequestUrl = expectedRepositoryUrl + "/pulls/" + std::to_string(*number);
			if (text(object.value("repository_url")) != expectedRepositoryUrl
				|| text(object.value("html_url")) != expectedHtmlUrl
				|| text(pullRequest.toObject().value("url"))
					!= expectedPullRequestUrl) {
				return failed(application::github_provider_failure::malformed_response);
			}
			return application::github_search_result_record{
				.Repository      = aRepository.Repository,
				.PullRequestNode = application::github_node_id{ text(object.value("node_id")) },
				.Number          = core::pull_request_number{ *number },
				.Title           = text(object.value("title")),
				.State           = application::github_pull_request_state::open,
				.Draft           = object.value("draft").toBool(),
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
				if (!page->Document.isArray()) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto records = page->Document.array();
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
			QJsonObject initialData;
			QJsonObject initialRepository;
			QJsonObject initialPullRequest;
			if (const auto identity = graphql_repository_and_pull_request(*initial,
																		  aSnapshot.Repository,
																		  aSnapshot.PullRequest,
																		  &initialData,
																		  &initialRepository,
																		  &initialPullRequest);
				!identity.has_value()) {
				return std::unexpected{ identity.error() };
			}
			std::vector<std::string_view> initialPullKeys{ "id" };
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
					initialPullRequest.value("files"), aLimits);
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
				const auto appendFileNodes = [&viewed](const QJsonArray& aNodes)
					-> std::expected<void, github_paged_read_failure> {
					for (const auto& value : aNodes) {
						if (!value.isObject()
							|| !has_exact_keys(value.toObject(),
											   { "path", "viewerViewedState" })
							|| !nonempty_string(value.toObject().value("path"))) {
							return failed(application::github_provider_failure::
											  malformed_response);
						}
						const auto object = value.toObject();
						const auto state  = viewed_state(object.value("viewerViewedState").toString());
						if (!state.has_value()
							|| !viewed.emplace(text(object.value("path")), *state)
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
					QJsonObject data;
					QJsonObject repository;
					QJsonObject pullRequest;
					if (const auto identity = graphql_repository_and_pull_request(*page,
																				  aSnapshot.Repository,
																				  aSnapshot.PullRequest,
																				  &data,
																				  &repository,
																				  &pullRequest);
						!identity.has_value()) {
						return std::unexpected{ identity.error() };
					}
					if (!has_exact_keys(pullRequest, { "files", "id" })) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					auto connection = parse_graphql_connection(pullRequest.value("files"), aLimits);
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
				initialPullRequest.value("reviewThreads"), aLimits);
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
			const auto  appendThreadNodes = [&](const QJsonArray& aNodes)
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
				QJsonObject data;
				QJsonObject repository;
				QJsonObject pullRequest;
				if (const auto identity = graphql_repository_and_pull_request(*page,
																			  aSnapshot.Repository,
																			  aSnapshot.PullRequest,
																			  &data,
																			  &repository,
																			  &pullRequest);
					!identity.has_value()) {
					return std::unexpected{ identity.error() };
				}
				if (!has_exact_keys(pullRequest, { "id", "reviewThreads" })) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				auto connection = parse_graphql_connection(
					pullRequest.value("reviewThreads"), aLimits);
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
					if (!singleton_complete(*page) || !page->Document.isObject()
						|| !graphql_success_envelope(page->Document.object())) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					const auto dataValue = page->Document.object().value("data");
					if (!dataValue.isObject()
						|| !has_exact_keys(dataValue.toObject(),
										   { "node", "repository" })) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					const auto data            = dataValue.toObject();
					const auto repositoryValue = data.value("repository");
					if (!repositoryValue.isObject()
						|| !has_exact_keys(repositoryValue.toObject(),
										   { "id", "pullRequest" })
						|| text(repositoryValue.toObject().value("id"))
							!= aSnapshot.Repository.Node.value()) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					const auto pullRequestValue = repositoryValue.toObject().value("pullRequest");
					if (!pullRequestValue.isObject()
						|| !has_exact_keys(pullRequestValue.toObject(), { "id" })
						|| text(pullRequestValue.toObject().value("id"))
							!= aSnapshot.PullRequest.Node.value()) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					const auto nodeValue = data.value("node");
					if (!nodeValue.isObject()
						|| !has_exact_keys(nodeValue.toObject(), { "comments", "id" })
						|| text(nodeValue.toObject().value("id"))
							!= thread.Thread.value()) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					auto connection = parse_graphql_connection(
						nodeValue.toObject().value("comments"), aLimits);
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
								&& comment->Line.has_value())
							|| (thread.Subject
									== application::github_review_thread_subject::line
								&& !comment->Line.has_value())) {
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
				if (!page->Document.isObject()
					|| !has_required_keys(page->Document.object(),
										  { "check_runs", "total_count" })) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto object = page->Document.object();
				if (!exact_integer(object.value("total_count"))
					|| !object.value("check_runs").isArray()) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto observedTotal = integer(object.value("total_count"));
				if (observedTotal > aLimits.Records.MaximumChecks) {
					return failed(application::github_provider_failure::response_limit);
				}
				if (total.has_value() && *total != observedTotal) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				total              = observedTotal;
				const auto records = object.value("check_runs").toArray();
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
			-> std::expected<void, github_paged_read_failure>
		{
			std::optional<std::uint64_t> total;
			while (true) {
				auto page = aExecution.fetch(aSemantic);
				if (!page.has_value()) {
					return std::unexpected{ page.error() };
				}
				if (!page->Document.isObject()
					|| !has_required_keys(
						page->Document.object(),
						{ "incomplete_results", "items", "total_count" })) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto object = page->Document.object();
				if (!exact_integer(object.value("total_count"))
					|| !object.value("items").isArray()
					|| !object.value("incomplete_results").isBool()
					|| object.value("incomplete_results").toBool()) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto observedTotal = integer(object.value("total_count"));
				if (observedTotal > aLimits.Records.MaximumSearchResults) {
					return failed(application::github_provider_failure::response_limit);
				}
				if (total.has_value() && *total != observedTotal) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				total              = observedTotal;
				const auto records = object.value("items").toArray();
				const auto count   = static_cast<std::size_t>(records.size());
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
					return { };
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
			QJsonParseError error;
			const auto      document = QJsonDocument::fromJson(
				QByteArray{
					aBody.data(),
					static_cast<qsizetype>(aBody.size()),
				},
				&error);
			return error.error == QJsonParseError::NoError
				&& secondary_rate_limit_message(&document);
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
			QJsonParseError error;
			const auto      document = QJsonDocument::fromJson(
				QByteArray{
					aBody.data(),
					static_cast<qsizetype>(aBody.size()),
				},
				&error);
			return error.error == QJsonParseError::NoError && document.isObject()
				&& graphql_error_rate_limited(
					   document.object().value(QStringLiteral("errors")));
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
			QJsonParseError parseError;
			const auto      document = QJsonDocument::fromJson(
				QByteArray{
					aResponse.Body.data(),
					static_cast<qsizetype>(aResponse.Body.size()),
				},
				&parseError);
			if (parseError.error != QJsonParseError::NoError || !document.isObject()
				|| !has_required_keys(document.object(),
									  { "id", "login", "name", "node_id" })) {
				return std::unexpected{ metadataError(
					application::github_provider_failure::malformed_response) };
			}
			const auto object = document.object();
			if (!exact_integer(object.value("id"), true)
				|| !nonempty_string(object.value("login"))
				|| !nonempty_string(object.value("node_id"))
				|| (!object.value("name").isNull()
					&& !object.value("name").isString())) {
				return std::unexpected{ metadataError(
					application::github_provider_failure::malformed_response) };
			}
			const auto node  = text(object.value("node_id"));
			const auto login = text(object.value("login"));
			const auto name  = object.value("name").isNull()
				? std::string{ }
				: text(object.value("name"));
			if (!bounded_text(node, aLimits.Records.MaximumIdentityBytes, true)
				|| !bounded_text(login, aLimits.Records.MaximumLoginBytes, true)
				|| !bounded_text(name, aLimits.Records.MaximumDisplayNameBytes)) {
				return std::unexpected{ metadataError(
					application::github_provider_failure::response_limit) };
			}
			return observed_github_viewer{
				.Viewer = application::github_viewer_record{
					.Account     = core::github_account_id{ integer(object.value("id")) },
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

	auto perform_paged_github_read(
		github_wire_transport_port&                              aTransport,
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		const core::provider_snapshot_id                         aSnapshotId,
		const std::stop_token                                    aStop,
		const github_paged_read_limits&                          aLimits) noexcept -> github_paged_read_result
	{
		auto observedRateLimits = application::github_rate_limit_state{ };
		try {
			if (!provider_request_valid(aProfile, aRequest, aSnapshotId, aLimits)) {
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
												 observedRateLimits };

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

			const auto                                expectedFiles   = pullRequest->ChangedFiles;
			const auto                                expectedCommits = pullRequest->Commits;
			application::github_pull_request_snapshot result{
				.Profile = aRequest.ProfileAuthority.Profile,
				.Host    = aRequest.ProfileAuthority.Host,
				.Account = aRequest.Viewer.Account,
				.Viewer  = application::github_actor_record{
					.Actor = aRequest.Viewer.Node,
					.Login = aRequest.Viewer.Login,
				},
				.Snapshot                  = aSnapshotId,
				.Repository                = std::move(*repository),
				.PullRequest               = std::move(pullRequest->Record),
				.Files                     = { },
				.Commits                   = { },
				.Reviews                   = { },
				.ReviewThreadsAvailability = application::github_collection_availability::unavailable,
				.ReviewThreads             = { },
				.IssueComments             = { },
				.Checks                    = { },
				.SearchResults             = { },
				.CapabilityEvidence        = application::github_capability_evidence{
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
					[](const QJsonValue& aValue) { return parse_file(aValue); });
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
					[](const QJsonValue& aValue) { return parse_commit(aValue); });
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
					[](const QJsonValue& aValue) { return parse_review(aValue); });
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

			if (const auto comments = read_rest_array_collection(
					execution,
					aProfile,
					semantic_request_for(*server,
										 aRequest.Context,
										 read_operation::list_issue_comments),
					result.IssueComments,
					aLimits.Records.MaximumIssueComments,
					aLimits.MaximumPageRecords,
					[](const QJsonValue& aValue) { return parse_issue_comment(aValue); });
				!comments.has_value()) {
				return std::unexpected{ execution.enrich(comments.error()) };
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
				return std::unexpected{ execution.enrich(checks.error()) };
			}

			auto searchSemantic = semantic_request_for(
				*server, aRequest.Context, read_operation::search_pull_requests);
			searchSemantic.SearchTerms = "is:open";
			if (const auto search = read_search_results(execution,
														aProfile,
														std::move(searchSemantic),
														result.Repository,
														result.SearchResults,
														aLimits);
				!search.has_value()) {
				return std::unexpected{ execution.enrich(search.error()) };
			}

			if (aStop.stop_requested()) {
				return cancelled();
			}
			result.RateLimits     = execution.rate_limits();
			const auto validation = application::validate(result, aRequest.Context, aLimits.Records);
			if (!validation.valid()) {
				return execution.terminal_failure(
					validation_failure(validation.Error));
			}
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
