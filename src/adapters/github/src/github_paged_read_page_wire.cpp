#include "github_paged_read_page_wire.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <system_error>
#include <tuple>

namespace bpr::adapters::github::detail {
	auto provider_error(
		const application::github_provider_failure           aFailure,
		const std::optional<std::uint32_t>                   aRetryAfter,
		application::github_rate_limit_state                 aRateLimits,
		std::optional<application::github_rate_limit_record> aRateLimitCause) -> application::github_provider_error
	{
		return {
			.Failure           = aFailure,
			.RetryAfterSeconds = aRetryAfter,
			.RateLimits        = std::move(aRateLimits),
			.RateLimitCause    = std::move(aRateLimitCause),
		};
	}

	auto
	failed(const application::github_provider_failure           aFailure,
		   const std::optional<std::uint32_t>                   aRetryAfter,
		   application::github_rate_limit_state                 aRateLimits,
		   std::optional<application::github_rate_limit_record> aRateLimitCause) -> std::unexpected<github_paged_read_failure>
	{
		return std::unexpected{
			github_paged_read_failure{ provider_error(aFailure,
													  aRetryAfter,
													  std::move(aRateLimits),
													  std::move(aRateLimitCause)) }
		};
	}

	auto failed(const application::github_provider_error& aError)
		-> std::unexpected<github_paged_read_failure>
	{
		return std::unexpected{ github_paged_read_failure{ aError } };
	}

	auto
	rate_limit_state(const application::github_rate_limit_record& aRecord) noexcept
		-> application::github_rate_limit_state
	{
		auto state = application::github_rate_limit_state{ };
		static_cast<void>(application::observe(state, aRecord));
		return state;
	}

	auto cancelled() -> std::unexpected<github_paged_read_failure>
	{
		return std::unexpected{
			github_paged_read_failure{ github_paged_read_cancelled{ } }
		};
	}

	auto ascii_fold(const char aValue) noexcept -> char
	{
		return aValue >= 'A' && aValue <= 'Z' ? static_cast<char>(aValue - 'A' + 'a')
											  : aValue;
	}

	auto ascii_equal_fold(const std::string_view aLeft,
						  const std::string_view aRight) noexcept
		-> bool
	{
		return aLeft.size() == aRight.size()
			&& std::ranges::equal(aLeft,
								  aRight,
								  [](const char a_, const char aB) { return ascii_fold(a_) == ascii_fold(aB); });
	}

	auto trim_ows(std::string_view aValue) noexcept -> std::string_view
	{
		while (!aValue.empty() && (aValue.front() == ' ' || aValue.front() == '\t')) {
			aValue.remove_prefix(1U);
		}
		while (!aValue.empty() && (aValue.back() == ' ' || aValue.back() == '\t')) {
			aValue.remove_suffix(1U);
		}
		return aValue;
	}

	auto decimal(std::string_view aValue) noexcept
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

	json_structural_preflight::json_structural_preflight(const std::string_view aInput, const std::size_t aMaximumStringBytes) noexcept
		: Input{ aInput }
		, MaximumStringBytes{ aMaximumStringBytes }
	{
	}

	auto json_structural_preflight::run() const noexcept -> json_preflight_result
	{
		return core::json_preflight(Input, { .Depth = 32U, .ObjectMembers = 256U, .ArrayElements = 10'000U, .Values = 1'000'000U, .StringBytes = MaximumStringBytes }).Result;
	}

	auto json_tag(const page_json_value& aValue) noexcept -> char
	{
		const auto begin = aValue.Json.find_first_not_of(" \t\r\n");
		return begin == std::string_view::npos ? '\0' : aValue.Json[begin];
	}

	auto json_is_object(const page_json_value& aValue) noexcept -> bool { return json_tag(aValue) == '{'; }

	auto json_is_array(const page_json_value& aValue) noexcept -> bool { return json_tag(aValue) == '['; }

	auto json_is_string(const page_json_value& aValue) noexcept -> bool { return json_tag(aValue) == '"'; }

	auto json_is_null(const page_json_value& aValue) noexcept -> bool { return json_tag(aValue) == 'n'; }

	auto json_is_undefined(const page_json_value& aValue) noexcept -> bool { return aValue.Json.empty(); }

	auto json_is_bool(const page_json_value& aValue) noexcept -> bool { return json_tag(aValue) == 't' || json_tag(aValue) == 'f'; }

	auto parse_json_document(const std::string_view aBody) -> std::optional<page_json_value>
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

	auto json_object(const page_json_value& aValue) -> page_json_object
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

	auto json_array(const page_json_value& aValue) -> page_json_array
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

	auto json_member(const page_json_object& aObject, const std::string_view aKey) -> page_json_value
	{
		const auto found = aObject.find(aKey);
		return found == aObject.end() ? page_json_value{ } : found->second;
	}

	auto text(const page_json_value& aValue) -> std::string
	{
		std::string result;
		return json_is_string(aValue) && !glz::read<page_json_options{ }>(result, aValue.Json) ? result : std::string{ };
	}

	auto json_text(const page_json_value& aValue) -> QString
	{
		return QString::fromStdString(text(aValue));
	}

	auto json_bool(const page_json_value& aValue) -> bool
	{
		bool result = false;
		return json_is_bool(aValue) && !glz::read<page_json_options{ }>(result, aValue.Json) && result;
	}

	auto exact_integer(const page_json_value& aValue, const bool aPositive) noexcept -> bool
	{
		std::uint64_t result = 0U;
		return !glz::read<page_json_options{ }>(result, aValue.Json) && (!aPositive || result != 0U);
	}

	auto integer(const page_json_value& aValue) noexcept -> std::uint64_t
	{
		std::uint64_t result = 0U;
		return !glz::read<page_json_options{ }>(result, aValue.Json) ? result : 0U;
	}

	auto uint32(const page_json_value& aValue) noexcept
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

	auto nonempty_string(const page_json_value& aValue) -> bool
	{
		return json_is_string(aValue) && !json_text(aValue).isEmpty();
	}

	auto bounded_text(const std::string_view aValue,
					  const std::size_t      aMaximum,
					  const bool             aRequired) noexcept -> bool
	{
		return (!aRequired || !aValue.empty()) && aValue.size() <= aMaximum
			&& aValue.find('\0') == std::string_view::npos;
	}

	auto sorted_keys(const page_json_object& aObject)
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

	auto
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

	auto
	has_exact_keys(const page_json_object&                       aObject,
				   const std::initializer_list<std::string_view> aExpected) -> bool
	{
		return has_exact_keys(
			aObject,
			std::span<const std::string_view>{ aExpected.begin(), aExpected.size() });
	}

	auto
	has_required_keys(const page_json_object&                 aObject,
					  const std::span<const std::string_view> aRequired) -> bool
	{
		return std::ranges::all_of(
			aRequired,
			[&aObject](const std::string_view aKey) { return aObject.contains(aKey); });
	}

	auto
	has_required_keys(const page_json_object&                       aObject,
					  const std::initializer_list<std::string_view> aRequired)
		-> bool
	{
		return has_required_keys(
			aObject,
			std::span<const std::string_view>{ aRequired.begin(), aRequired.size() });
	}

	auto graphql_success_envelope(const page_json_object& aObject) -> bool
	{
		return has_exact_keys(aObject, { "data" })
			|| (has_exact_keys(aObject, { "data", "extensions" })
				&& json_is_object(json_member(aObject, "extensions")));
	}

	auto one_of(const page_json_value&                        aValue,
				const std::initializer_list<std::string_view> aAllowed)
		-> bool
	{
		if (!json_is_string(aValue)) {
			return false;
		}
		const auto candidate = text(aValue);
		return std::ranges::find(aAllowed, candidate) != aAllowed.end();
	}

	auto hexadecimal_oid(const page_json_value& aValue) -> bool
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

	auto valid_line(const page_json_value& aValue) noexcept -> bool
	{
		return exact_integer(aValue, true)
			&& integer(aValue)
			<= application::maximum_github_line;
	}

	auto optional_line(const page_json_value& aValue)
		-> std::optional<std::uint32_t>
	{
		return json_is_null(aValue) ? std::nullopt : uint32(aValue);
	}

	auto response_byte_count(const wire_response& aResponse) noexcept
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

	auto limits_valid(const github_paged_read_limits& aLimits) noexcept
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

	auto ascii_contains_fold(const std::string_view aText,
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

	auto
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

	auto graphql_error_rate_limited(const page_json_value& aErrors) -> bool
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

	auto status_failure(const int                       aStatus,
						const github_response_metadata& aMetadata,
						const bool                      aSecondaryRateLimit)
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

	production_read_execution::production_read_execution(github_wire_transport_port&                 aTransport,
														 const validated_github_host_profile&        aProfile,
														 github_wire_parent_context                  aParent,
														 const github_paged_read_limits&             aLimits,
														 const std::stop_token                       aStop,
														 application::github_rate_limit_state&       aRateLimits,
														 std::optional<core::application_session_id> aAuthenticationSession,
														 github_paged_response_observer* aObserver, shared_home_budget* aHomeBudget)
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

	auto
	production_read_execution::enrich(application::github_provider_error aError) const noexcept
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

	auto production_read_execution::enrich(github_paged_read_failure aFailure) const noexcept
		-> github_paged_read_failure
	{
		if (auto* error = std::get_if<application::github_provider_error>(&aFailure);
			error != nullptr) {
			*error = enrich(std::move(*error));
		}
		return aFailure;
	}

	auto
	production_read_execution::terminal_failure(const application::github_provider_failure aFailure) const
		-> std::unexpected<github_paged_read_failure>
	{
		return failed(aFailure, std::nullopt, RateLimits);
	}

	auto production_read_execution::fetch(const semantic_read_request& aSemantic)
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
				// [[BPR-1589]]. A token without `read:org` cannot ask who is
				// in a team: GitHub says so with a typed error, which is a
				// permission, not a malformed answer.
				if (aSemantic.Operation == read_operation::get_team_membership && json_is_array(errors)
					&& std::ranges::any_of(json_array(errors), [](const auto& aError) {
						   const auto type = text(json_member(json_object(aError), "type"));
						   return type == "INSUFFICIENT_SCOPES" || type == "FORBIDDEN";
					   })) {
					return metadataFailed(application::github_provider_failure::permission);
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

	auto production_read_execution::rate_limits() const noexcept
		-> const application::github_rate_limit_state&
	{
		return RateLimits;
	}

	auto
	production_read_execution::bind_repository(const core::repository_id aRepository) noexcept -> bool
	{
		if (aRepository.value() == 0U
			|| (BoundRepository.has_value() && *BoundRepository != aRepository)) {
			return false;
		}
		BoundRepository = aRepository;
		return true;
	}

	auto production_read_execution::bound_repository() const noexcept
		-> std::optional<core::repository_id>
	{
		return BoundRepository;
	}

	auto
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

	auto
	semantic_request_for(const server_contract                     aServer,
						 const core::pull_request_request_context& aContext,
						 const read_operation                      aOperation,
						 const std::uint32_t                       aPage) -> semantic_read_request
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
		case read_operation::list_reactions:
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
		case read_operation::get_team_membership:
		case read_operation::list_home_review_membership:
		case read_operation::list_home_pull_requests:
		case read_operation::list_notifications:
			break;
		}
		return result;
	}

	auto singleton_complete(const parsed_page& aPage) noexcept -> bool
	{
		return aPage.Completeness == page_completeness::complete
			&& !aPage.Metadata.Link.has_value();
	}

	auto parse_link_relations(const std::string_view aValue)
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

	auto parse_url_query(const std::string_view aQuery)
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

	auto
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
		case read_operation::get_team_membership:
		case read_operation::list_home_review_membership:
		case read_operation::list_home_pull_requests:
		case read_operation::list_reactions:
		case read_operation::list_notifications:
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

	auto query_value_matches(const read_operation   aOperation,
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

	auto
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

	auto
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
}
