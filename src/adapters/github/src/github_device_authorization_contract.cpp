#include "github_device_authorization_contract.hpp"

#include "bpr/application/github_host_authority.hpp"

#include "github_http_helpers.hpp"

#include <QString>
#include <QUrl>

#include <glaze/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <map>
#include <utility>

namespace bpr::adapters::github::detail {
	namespace {
		using device_failure = device_authorization_contract_failure;

		struct device_json_options : glz::opts
		{
			bool validate_skipped             = true;
			bool validate_trailing_whitespace = true;
			constexpr device_json_options()
			{
				null_terminated = false;
			}
		};

		// Views live only during parsing of the caller-owned response. Indexing the
		// flat envelope first preserves exact field inventories and typed failures;
		// token strings are decoded when constructing typed grant records.
		using device_response_json = std::map<std::string, glz::raw_json_view, std::less<>>;

		template <typename T>
		[[nodiscard]] auto field_value(const glz::raw_json_view& aValue) -> std::expected<T, device_failure>
		{
			auto parsed = T{ };
			if (glz::read<device_json_options{ }>(parsed, aValue.str)) {
				return std::unexpected(device_failure::invalid_field_value);
			}
			return parsed;
		}

		constexpr std::string_view device_code_url
			= "https://github.com/login/device/code";
		constexpr std::string_view token_url
			= "https://github.com/login/oauth/access_token";
		constexpr std::string_view device_grant_type
			= "urn:ietf:params:oauth:grant-type:device_code";
		constexpr std::string_view refresh_grant_type           = "refresh_token";
		constexpr std::string_view official_error_uri_authority = "docs.github.com";

		// The authority a returned `verification_uri` must sit under, shared
		// with the host that checks the same address again before it opens a
		// browser. Two checks, one declaration: this one refuses the address at
		// the wire, so it never reaches a host at all, and the host refuses it
		// again against the authority its composition handed it.
		[[nodiscard]] auto expected_verification_authority() -> QLatin1StringView
		{
			constexpr auto authority
				= application::default_device_verification_authority;
			return QLatin1StringView{
				authority.data(),
				static_cast<qsizetype>(authority.size()),
			};
		}

		[[nodiscard]] auto device_headers() -> std::vector<http_header>
		{
			return {
				{ .Name = "Accept", .Value = "application/json" },
				{ .Name = "User-Agent", .Value = "BetterPullReview/0.1" },
				{ .Name  = "Content-Type",
				  .Value = "application/x-www-form-urlencoded" },
			};
		}

		[[nodiscard]] auto valid_device_transient_code(
			const std::string_view aCode) noexcept -> bool
		{
			if (aCode.empty()
				|| aCode.size() > maximum_device_transient_code_bytes) {
				return false;
			}
			return std::ranges::all_of(aCode, [](const char aCharacter) {
				const auto value = static_cast<unsigned char>(aCharacter);
				return (std::isalnum(value) != 0) || aCharacter == '-'
					|| aCharacter == '_';
			});
		}

		[[nodiscard]] auto response_object(
			const wire_response& aResponse,
			const int            aExpectedStatus)
			-> std::expected<device_response_json, device_failure>
		{
			if (aResponse.Status != aExpectedStatus) {
				return std::unexpected(device_failure::unexpected_status);
			}
			if (aResponse.Body.size()
				> maximum_device_authorization_response_bytes) {
				return std::unexpected(device_failure::response_limit);
			}
			const auto contentType = std::ranges::find_if(
				aResponse.Headers,
				[](const http_header& aHeader) {
					return QString::fromStdString(aHeader.Name)
							   .compare(
								   QStringLiteral("Content-Type"),
								   Qt::CaseInsensitive)
						== 0;
				});
			if (contentType == aResponse.Headers.end()
				|| !json_media_type(contentType->Value)) {
				return std::unexpected(device_failure::unexpected_content_type);
			}
			auto object = device_response_json{ };
			if (glz::read<device_json_options{ }>(object, aResponse.Body)) {
				return std::unexpected(device_failure::malformed_response);
			}
			return object;
		}

		[[nodiscard]] auto exact_keys(const device_response_json&                   aObject,
									  const std::initializer_list<std::string_view> aKeys) -> bool
		{
			return aObject.size() == aKeys.size()
				&& std::ranges::all_of(aKeys, [&aObject](const auto aKey) { return aObject.contains(aKey); });
		}

		[[nodiscard]] auto bounded_seconds(const glz::raw_json_view& aValue)
			-> std::expected<std::uint32_t, device_failure>
		{
			const auto integral = field_value<std::uint32_t>(aValue);
			if (!integral || *integral == 0U || *integral > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
				return std::unexpected(device_failure::invalid_field_value);
			}
			return *integral;
		}

		[[nodiscard]] auto non_empty_text(const glz::raw_json_view& aValue)
			-> std::expected<std::string, device_failure>
		{
			auto text = field_value<std::string>(aValue);
			if (!text || text->empty() || text->size() > maximum_device_transient_code_bytes) {
				return std::unexpected(device_failure::invalid_field_value);
			}
			return text;
		}

		[[nodiscard]] auto official_error_uri(const glz::raw_json_view& aValue) noexcept -> bool
		{
			const auto text = field_value<std::string>(aValue);
			if (!text) {
				return false;
			}
			const auto url = QUrl{ QString::fromStdString(*text) };
			return url.isValid() && url.scheme() == QStringLiteral("https")
				&& url.host() == QLatin1StringView{ official_error_uri_authority.data(), static_cast<qsizetype>(official_error_uri_authority.size()) };
		}

		[[nodiscard]] auto error_object_code(const device_response_json& aObject)
			-> std::expected<std::string, device_failure>
		{
			const auto plain = exact_keys(
				aObject,
				{ "error",
				  "error_description",
				  "error_uri" });
			const auto withInterval = exact_keys(
				aObject,
				{ "error",
				  "error_description",
				  "error_uri",
				  "interval" });
			if (!plain && !withInterval) {
				return std::unexpected(device_failure::unexpected_field);
			}
			const auto code = field_value<std::string>(aObject.at("error"));
			if (!code || code->empty()) {
				return std::unexpected(device_failure::invalid_field_value);
			}
			if (!field_value<std::string>(aObject.at("error_description"))) {
				return std::unexpected(device_failure::invalid_field_value);
			}
			if (!official_error_uri(
					aObject.at("error_uri"))) {
				return std::unexpected(device_failure::unofficial_error_uri);
			}
			auto text = *code;
			if (withInterval && text != "slow_down") {
				return std::unexpected(device_failure::unexpected_field);
			}
			if (plain && text == "slow_down") {
				return std::unexpected(device_failure::missing_field);
			}
			return text;
		}

		// The authorization success body and the renewal success body are the
		// same six fields, so they are the same code. Splitting them would
		// let one drift into accepting a shape the other refuses, which is
		// the difference this exchange can least afford.
		[[nodiscard]] auto token_grant_body(const device_response_json& aObject)
			-> std::expected<device_token_grant, device_failure>
		{
			if (!exact_keys(
					aObject,
					{ "access_token",
					  "expires_in",
					  "refresh_token",
					  "refresh_token_expires_in",
					  "scope",
					  "token_type" })) {
				return std::unexpected(device_failure::unexpected_field);
			}

			const auto tokenType = field_value<std::string>(aObject.at("token_type"));
			if (!tokenType || *tokenType != "bearer") {
				return std::unexpected(device_failure::invalid_field_value);
			}
			const auto scope = field_value<std::string>(aObject.at("scope"));
			if (!scope || !scope->empty()) {
				return std::unexpected(device_failure::invalid_field_value);
			}

			auto grant = device_token_grant{ };

			const auto accessToken = non_empty_text(
				aObject.at("access_token"));
			if (!accessToken.has_value()) {
				return std::unexpected(accessToken.error());
			}
			grant.AccessToken = *accessToken;

			const auto refreshToken = non_empty_text(
				aObject.at("refresh_token"));
			if (!refreshToken.has_value()) {
				return std::unexpected(refreshToken.error());
			}
			grant.RefreshToken = *refreshToken;

			const auto expires = bounded_seconds(
				aObject.at("expires_in"));
			if (!expires.has_value()) {
				return std::unexpected(expires.error());
			}
			grant.ExpiresInSeconds = *expires;

			const auto refreshExpires = bounded_seconds(
				aObject.at("refresh_token_expires_in"));
			if (!refreshExpires.has_value()) {
				return std::unexpected(refreshExpires.error());
			}
			grant.RefreshTokenExpiresInSeconds = *refreshExpires;

			return grant;
		}
	}

	auto valid_device_client_id(const std::string_view aClientId) noexcept
		-> bool
	{
		if (aClientId.empty()
			|| aClientId.size() > maximum_device_client_id_length) {
			return false;
		}
		return std::ranges::all_of(aClientId, [](const char aCharacter) {
			const auto value = static_cast<unsigned char>(aCharacter);
			return (std::isalnum(value) != 0) || aCharacter == '.'
				|| aCharacter == '-' || aCharacter == '_';
		});
	}

	auto compile_device_code_request(
		const server_contract  aServer,
		const std::string_view aClientId)
		-> std::expected<
			device_authorization_wire_request,
			device_authorization_contract_failure>
	{
		if (aServer != server_contract::github_com) {
			return std::unexpected(
				device_failure::unsupported_server_contract);
		}
		if (!valid_device_client_id(aClientId)) {
			return std::unexpected(device_failure::invalid_client_id);
		}
		return device_authorization_wire_request{
			.Endpoint = device_authorization_endpoint::device_code,
			.Method   = http_method::post,
			.Url      = std::string{ device_code_url },
			.Headers  = device_headers(),
			.Body     = "client_id=" + percent_encode(aClientId),
		};
	}

	auto compile_device_token_request(
		const server_contract  aServer,
		const std::string_view aClientId,
		const std::string_view aDeviceCode)
		-> std::expected<
			device_authorization_wire_request,
			device_authorization_contract_failure>
	{
		if (aServer != server_contract::github_com) {
			return std::unexpected(
				device_failure::unsupported_server_contract);
		}
		if (!valid_device_client_id(aClientId)) {
			return std::unexpected(device_failure::invalid_client_id);
		}
		if (!valid_device_transient_code(aDeviceCode)) {
			return std::unexpected(device_failure::invalid_device_code);
		}
		return device_authorization_wire_request{
			.Endpoint = device_authorization_endpoint::token,
			.Method   = http_method::post,
			.Url      = std::string{ token_url },
			.Headers  = device_headers(),
			.Body     = "client_id=" + percent_encode(aClientId) + "&device_code="
				+ percent_encode(aDeviceCode) + "&grant_type="
				+ percent_encode(device_grant_type),
		};
	}

	auto parse_device_code_response(const wire_response& aResponse)
		-> std::expected<
			device_code_outcome,
			device_authorization_contract_failure>
	{
		if (aResponse.Status == 400) {
			const auto object = response_object(aResponse, 400);
			if (!object.has_value()) {
				return std::unexpected(object.error());
			}
			const auto code = error_object_code(*object);
			if (!code.has_value()) {
				return std::unexpected(code.error());
			}
			if (*code != "device_flow_disabled") {
				return std::unexpected(device_failure::unknown_error_code);
			}
			return device_code_outcome{ device_code_refusal{
				.Kind = device_code_refusal_kind::device_flow_disabled } };
		}

		const auto object = response_object(aResponse, 200);
		if (!object.has_value()) {
			return std::unexpected(object.error());
		}
		if (!exact_keys(
				*object,
				{ "device_code",
				  "expires_in",
				  "interval",
				  "user_code",
				  "verification_uri" })) {
			return std::unexpected(device_failure::unexpected_field);
		}

		auto grant = device_code_grant{ };

		const auto deviceCode
			= non_empty_text(object->at("device_code"));
		if (!deviceCode.has_value()) {
			return std::unexpected(deviceCode.error());
		}
		grant.DeviceCode = *deviceCode;

		const auto userCode
			= non_empty_text(object->at("user_code"));
		if (!userCode.has_value()) {
			return std::unexpected(userCode.error());
		}
		grant.UserCode = *userCode;

		const auto verification = field_value<std::string>(object->at("verification_uri"));
		if (!verification) {
			return std::unexpected(device_failure::invalid_field_value);
		}
		const auto verificationUrl = QUrl{ QString::fromStdString(*verification) };
		// [[BPR-1439]]. The exact authority: no userinfo (even an empty one),
		// no explicit port and no fragment.
		if (!verificationUrl.isValid()
			|| verificationUrl.scheme() != QStringLiteral("https")
			|| verificationUrl.host() != expected_verification_authority()
			|| verificationUrl.authority(QUrl::FullyEncoded) != expected_verification_authority()
			|| verificationUrl.port() != -1 || verificationUrl.hasFragment()) {
			return std::unexpected(
				device_failure::off_authority_verification_uri);
		}
		grant.VerificationUri = *verification;

		const auto expires
			= bounded_seconds(object->at("expires_in"));
		if (!expires.has_value()) {
			return std::unexpected(expires.error());
		}
		grant.ExpiresInSeconds = *expires;

		const auto interval
			= bounded_seconds(object->at("interval"));
		if (!interval.has_value()) {
			return std::unexpected(interval.error());
		}
		grant.IntervalSeconds = *interval;

		return device_code_outcome{ std::move(grant) };
	}

	auto parse_device_token_response(const wire_response& aResponse)
		-> std::expected<
			device_token_outcome,
			device_authorization_contract_failure>
	{
		const auto object = response_object(aResponse, 200);
		if (!object.has_value()) {
			return std::unexpected(object.error());
		}

		if (object->contains("error")) {
			const auto code = error_object_code(*object);
			if (!code.has_value()) {
				return std::unexpected(code.error());
			}
			if (*code == "authorization_pending") {
				return device_token_outcome{ device_token_delay{
					.Kind = device_token_delay_kind::authorization_pending } };
			}
			if (*code == "slow_down") {
				const auto interval = bounded_seconds(
					object->at("interval"));
				if (!interval.has_value()) {
					return std::unexpected(interval.error());
				}
				return device_token_outcome{ device_token_delay{
					.Kind                    = device_token_delay_kind::slow_down,
					.SlowDownIntervalSeconds = *interval } };
			}
			if (*code == "access_denied") {
				return device_token_outcome{ device_token_refusal{
					.Kind = device_token_refusal_kind::access_denied } };
			}
			if (*code == "expired_token") {
				return device_token_outcome{ device_token_refusal{
					.Kind = device_token_refusal_kind::expired_token } };
			}
			return std::unexpected(device_failure::unknown_error_code);
		}

		const auto grant = token_grant_body(*object);
		if (!grant.has_value()) {
			return std::unexpected(grant.error());
		}
		return device_token_outcome{ *grant };
	}

	auto compile_device_refresh_request(
		const server_contract  aServer,
		const std::string_view aClientId,
		const std::string_view aRefreshToken)
		-> std::expected<
			device_authorization_wire_request,
			device_authorization_contract_failure>
	{
		if (aServer != server_contract::github_com) {
			return std::unexpected(
				device_failure::unsupported_server_contract);
		}
		if (!valid_device_client_id(aClientId)) {
			return std::unexpected(device_failure::invalid_client_id);
		}
		if (!valid_device_transient_code(aRefreshToken)) {
			return std::unexpected(device_failure::invalid_refresh_token);
		}
		return device_authorization_wire_request{
			.Endpoint = device_authorization_endpoint::token,
			.Method   = http_method::post,
			.Url      = std::string{ token_url },
			.Headers  = device_headers(),
			.Body     = "client_id=" + percent_encode(aClientId) + "&grant_type="
				+ percent_encode(refresh_grant_type) + "&refresh_token="
				+ percent_encode(aRefreshToken),
		};
	}

	auto parse_device_refresh_response(const wire_response& aResponse)
		-> std::expected<
			device_refresh_outcome,
			device_authorization_contract_failure>
	{
		const auto object = response_object(aResponse, 200);
		if (!object.has_value()) {
			return std::unexpected(object.error());
		}

		if (object->contains("error")) {
			const auto code = error_object_code(*object);
			if (!code.has_value()) {
				return std::unexpected(code.error());
			}
			// One admitted code. A renewal has nothing to wait for, so a
			// pending or slow-down answer here would be a contract this
			// exchange does not have, and it fails closed with everything else.
			if (*code == "bad_refresh_token") {
				return device_refresh_outcome{ device_refresh_refusal{
					.Kind = device_refresh_refusal_kind::bad_refresh_token } };
			}
			return std::unexpected(device_failure::unknown_error_code);
		}

		const auto grant = token_grant_body(*object);
		if (!grant.has_value()) {
			return std::unexpected(grant.error());
		}
		return device_refresh_outcome{ *grant };
	}
}
