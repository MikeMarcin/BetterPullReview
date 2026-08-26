#include "github_device_authorization_contract.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <QUrl>

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <utility>

namespace bpr::adapters::github::detail {
	namespace {
		using device_failure = device_authorization_contract_failure;

		constexpr std::string_view device_code_url
			= "https://github.com/login/device/code";
		constexpr std::string_view token_url
			= "https://github.com/login/oauth/access_token";
		constexpr std::string_view device_grant_type
			= "urn:ietf:params:oauth:grant-type:device_code";
		constexpr std::string_view refresh_grant_type              = "refresh_token";
		constexpr std::string_view expected_verification_authority = "github.com";
		constexpr std::string_view official_error_uri_authority    = "docs.github.com";

		[[nodiscard]] auto form_safe(const char aCharacter) noexcept -> bool
		{
			const auto value = static_cast<unsigned char>(aCharacter);
			return (std::isalnum(value) != 0) || aCharacter == '-'
				|| aCharacter == '.' || aCharacter == '_' || aCharacter == '~';
		}

		[[nodiscard]] auto form_encode(const std::string_view aValue)
			-> std::string
		{
			constexpr std::string_view digits = "0123456789ABCDEF";
			std::string                encoded;
			encoded.reserve(aValue.size());
			for (const auto character : aValue) {
				if (form_safe(character)) {
					encoded.push_back(character);
					continue;
				}
				const auto value = static_cast<unsigned char>(character);
				encoded.push_back('%');
				encoded.push_back(digits[value >> 4U]);
				encoded.push_back(digits[value & 0x0FU]);
			}
			return encoded;
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
			-> std::expected<QJsonObject, device_failure>
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
				|| !QString::fromStdString(contentType->Value)
						.startsWith(
							QStringLiteral("application/json"),
							Qt::CaseInsensitive)) {
				return std::unexpected(device_failure::unexpected_content_type);
			}
			auto       parseError = QJsonParseError{ };
			const auto document   = QJsonDocument::fromJson(
				QByteArray::fromStdString(aResponse.Body),
				&parseError);
			if (parseError.error != QJsonParseError::NoError
				|| !document.isObject()) {
				return std::unexpected(device_failure::malformed_response);
			}
			return document.object();
		}

		[[nodiscard]] auto exact_keys(
			const QJsonObject&                             aObject,
			const std::initializer_list<QLatin1StringView> aKeys) -> bool
		{
			if (aObject.size() != static_cast<qsizetype>(aKeys.size())) {
				return false;
			}
			return std::ranges::all_of(
				aKeys,
				[&aObject](const QLatin1StringView aKey) {
					return aObject.contains(aKey);
				});
		}

		[[nodiscard]] auto bounded_seconds(const QJsonValue& aValue)
			-> std::expected<std::uint32_t, device_failure>
		{
			if (!aValue.isDouble()) {
				return std::unexpected(device_failure::invalid_field_value);
			}
			const auto integral = aValue.toInteger(-1);
			if (integral <= 0
				|| integral > std::numeric_limits<std::int32_t>::max()
				|| static_cast<double>(integral) != aValue.toDouble()) {
				return std::unexpected(device_failure::invalid_field_value);
			}
			return static_cast<std::uint32_t>(integral);
		}

		[[nodiscard]] auto non_empty_text(const QJsonValue& aValue)
			-> std::expected<std::string, device_failure>
		{
			if (!aValue.isString()) {
				return std::unexpected(device_failure::invalid_field_value);
			}
			auto text = aValue.toString().toStdString();
			if (text.empty()
				|| text.size() > maximum_device_transient_code_bytes) {
				return std::unexpected(device_failure::invalid_field_value);
			}
			return text;
		}

		[[nodiscard]] auto official_error_uri(const QJsonValue& aValue) noexcept
			-> bool
		{
			if (!aValue.isString()) {
				return false;
			}
			const auto url = QUrl{ aValue.toString() };
			return url.isValid() && url.scheme() == QStringLiteral("https")
				&& url.host()
				== QLatin1StringView{
					   official_error_uri_authority.data(),
					   static_cast<qsizetype>(
						   official_error_uri_authority.size())
				   };
		}

		[[nodiscard]] auto error_object_code(const QJsonObject& aObject)
			-> std::expected<std::string, device_failure>
		{
			const auto plain = exact_keys(
				aObject,
				{ QLatin1StringView{ "error" },
				  QLatin1StringView{ "error_description" },
				  QLatin1StringView{ "error_uri" } });
			const auto withInterval = exact_keys(
				aObject,
				{ QLatin1StringView{ "error" },
				  QLatin1StringView{ "error_description" },
				  QLatin1StringView{ "error_uri" },
				  QLatin1StringView{ "interval" } });
			if (!plain && !withInterval) {
				return std::unexpected(device_failure::unexpected_field);
			}
			const auto code = aObject.value(QLatin1StringView{ "error" });
			if (!code.isString() || code.toString().isEmpty()) {
				return std::unexpected(device_failure::invalid_field_value);
			}
			if (!aObject.value(QLatin1StringView{ "error_description" })
					 .isString()) {
				return std::unexpected(device_failure::invalid_field_value);
			}
			if (!official_error_uri(
					aObject.value(QLatin1StringView{ "error_uri" }))) {
				return std::unexpected(device_failure::unofficial_error_uri);
			}
			auto text = code.toString().toStdString();
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
		[[nodiscard]] auto token_grant_body(const QJsonObject& aObject)
			-> std::expected<device_token_grant, device_failure>
		{
			if (!exact_keys(
					aObject,
					{ QLatin1StringView{ "access_token" },
					  QLatin1StringView{ "expires_in" },
					  QLatin1StringView{ "refresh_token" },
					  QLatin1StringView{ "refresh_token_expires_in" },
					  QLatin1StringView{ "scope" },
					  QLatin1StringView{ "token_type" } })) {
				return std::unexpected(device_failure::unexpected_field);
			}

			const auto tokenType = aObject.value(
				QLatin1StringView{ "token_type" });
			if (!tokenType.isString()
				|| tokenType.toString() != QStringLiteral("bearer")) {
				return std::unexpected(device_failure::invalid_field_value);
			}
			const auto scope = aObject.value(QLatin1StringView{ "scope" });
			if (!scope.isString() || !scope.toString().isEmpty()) {
				return std::unexpected(device_failure::invalid_field_value);
			}

			auto grant = device_token_grant{ };

			const auto accessToken = non_empty_text(
				aObject.value(QLatin1StringView{ "access_token" }));
			if (!accessToken.has_value()) {
				return std::unexpected(accessToken.error());
			}
			grant.AccessToken = *accessToken;

			const auto refreshToken = non_empty_text(
				aObject.value(QLatin1StringView{ "refresh_token" }));
			if (!refreshToken.has_value()) {
				return std::unexpected(refreshToken.error());
			}
			grant.RefreshToken = *refreshToken;

			const auto expires = bounded_seconds(
				aObject.value(QLatin1StringView{ "expires_in" }));
			if (!expires.has_value()) {
				return std::unexpected(expires.error());
			}
			grant.ExpiresInSeconds = *expires;

			const auto refreshExpires = bounded_seconds(
				aObject.value(QLatin1StringView{ "refresh_token_expires_in" }));
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
			.Body     = "client_id=" + form_encode(aClientId),
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
			.Body     = "client_id=" + form_encode(aClientId) + "&device_code="
				+ form_encode(aDeviceCode) + "&grant_type="
				+ form_encode(device_grant_type),
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
				{ QLatin1StringView{ "device_code" },
				  QLatin1StringView{ "expires_in" },
				  QLatin1StringView{ "interval" },
				  QLatin1StringView{ "user_code" },
				  QLatin1StringView{ "verification_uri" } })) {
			return std::unexpected(device_failure::unexpected_field);
		}

		auto grant = device_code_grant{ };

		const auto deviceCode
			= non_empty_text(object->value(QLatin1StringView{ "device_code" }));
		if (!deviceCode.has_value()) {
			return std::unexpected(deviceCode.error());
		}
		grant.DeviceCode = *deviceCode;

		const auto userCode
			= non_empty_text(object->value(QLatin1StringView{ "user_code" }));
		if (!userCode.has_value()) {
			return std::unexpected(userCode.error());
		}
		grant.UserCode = *userCode;

		const auto verification = object->value(
			QLatin1StringView{ "verification_uri" });
		if (!verification.isString()) {
			return std::unexpected(device_failure::invalid_field_value);
		}
		const auto verificationUrl = QUrl{ verification.toString() };
		if (!verificationUrl.isValid()
			|| verificationUrl.scheme() != QStringLiteral("https")
			|| verificationUrl.host()
				!= QLatin1StringView{
					expected_verification_authority.data(),
					static_cast<qsizetype>(
						expected_verification_authority.size()) }) {
			return std::unexpected(
				device_failure::off_authority_verification_uri);
		}
		grant.VerificationUri = verification.toString().toStdString();

		const auto expires
			= bounded_seconds(object->value(QLatin1StringView{ "expires_in" }));
		if (!expires.has_value()) {
			return std::unexpected(expires.error());
		}
		grant.ExpiresInSeconds = *expires;

		const auto interval
			= bounded_seconds(object->value(QLatin1StringView{ "interval" }));
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

		if (object->contains(QLatin1StringView{ "error" })) {
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
					object->value(QLatin1StringView{ "interval" }));
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
			.Body     = "client_id=" + form_encode(aClientId) + "&grant_type="
				+ form_encode(refresh_grant_type) + "&refresh_token="
				+ form_encode(aRefreshToken),
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

		if (object->contains(QLatin1StringView{ "error" })) {
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
