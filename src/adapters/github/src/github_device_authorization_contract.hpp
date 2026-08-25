#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_DEVICE_AUTHORIZATION_CONTRACT_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_DEVICE_AUTHORIZATION_CONTRACT_HPP

#include "github_read_contract.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/// The device-authorization wire contract compiles exact bounded
/// form-encoded requests for GitHub's device-code and token endpoints and
/// parses their responses into sanitized typed records. The shapes are bound
/// to the recorded BPR-0276 contracts: the token endpoint reports its
/// resolution errors with HTTP status 200, only the device-code endpoint
/// refuses with 400, and `slow_down` alone carries an interval. Secret
/// material — the device code and the returned tokens — exists only inside
/// the typed grant records handed to the host; every failure is a bare enum
/// so no diagnostic can carry a response byte.
namespace bpr::adapters::github::detail {
	inline constexpr std::size_t maximum_device_authorization_response_bytes
		= 64U * 1024U;
	inline constexpr std::size_t maximum_device_client_id_length     = 64U;
	inline constexpr std::size_t maximum_device_transient_code_bytes = 512U;

	enum class device_authorization_endpoint : std::uint8_t {
		device_code,
		token,
	};

	enum class device_authorization_contract_failure : std::uint8_t {
		unsupported_server_contract,
		invalid_client_id,
		invalid_device_code,
		unexpected_status,
		unexpected_content_type,
		response_limit,
		malformed_response,
		unexpected_field,
		missing_field,
		invalid_field_value,
		unofficial_error_uri,
		unknown_error_code,
		off_authority_verification_uri,
	};

	/// \brief Describes the device authorization wire request record.
	struct device_authorization_wire_request
	{
		device_authorization_endpoint Endpoint
			= device_authorization_endpoint::device_code;
		http_method              Method = http_method::post;
		std::string              Url;
		std::vector<http_header> Headers;
		std::string              Body;

		auto operator==(const device_authorization_wire_request&) const
			-> bool = default;
	};

	/// \brief Describes the device code grant record.
	struct device_code_grant
	{
		std::string   DeviceCode;
		std::string   UserCode;
		std::string   VerificationUri;
		std::uint32_t ExpiresInSeconds = 0;
		std::uint32_t IntervalSeconds  = 0;

		auto operator==(const device_code_grant&) const -> bool = default;
	};

	/// \brief Describes the device token grant record.
	struct device_token_grant
	{
		std::string   AccessToken;
		std::string   RefreshToken;
		std::uint32_t ExpiresInSeconds             = 0;
		std::uint32_t RefreshTokenExpiresInSeconds = 0;

		auto operator==(const device_token_grant&) const -> bool = default;
	};

	enum class device_token_delay_kind : std::uint8_t {
		authorization_pending,
		slow_down,
	};

	/// \brief Describes the device token delay record.
	struct device_token_delay
	{
		device_token_delay_kind Kind
			= device_token_delay_kind::authorization_pending;
		std::uint32_t SlowDownIntervalSeconds = 0;

		auto operator==(const device_token_delay&) const -> bool = default;
	};

	enum class device_token_refusal_kind : std::uint8_t {
		access_denied,
		expired_token,
	};

	/// \brief Describes the device token refusal record.
	struct device_token_refusal
	{
		device_token_refusal_kind Kind = device_token_refusal_kind::access_denied;

		auto operator==(const device_token_refusal&) const -> bool = default;
	};

	using device_token_outcome
		= std::variant<device_token_grant, device_token_delay, device_token_refusal>;

	enum class device_code_refusal_kind : std::uint8_t {
		device_flow_disabled,
	};

	/// \brief Describes the device code refusal record.
	struct device_code_refusal
	{
		device_code_refusal_kind Kind
			= device_code_refusal_kind::device_flow_disabled;

		auto operator==(const device_code_refusal&) const -> bool = default;
	};

	using device_code_outcome
		= std::variant<device_code_grant, device_code_refusal>;

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid_device_client_id(std::string_view aClientId) noexcept
		-> bool;

	/// \brief Computes the compile device code request result.
	[[nodiscard]] auto compile_device_code_request(
		server_contract  aServer,
		std::string_view aClientId)
		-> std::expected<
			device_authorization_wire_request,
			device_authorization_contract_failure>;

	/// \brief Computes the compile device token request result.
	[[nodiscard]] auto compile_device_token_request(
		server_contract  aServer,
		std::string_view aClientId,
		std::string_view aDeviceCode)
		-> std::expected<
			device_authorization_wire_request,
			device_authorization_contract_failure>;

	/// \brief Computes the parse device code response result.
	[[nodiscard]] auto parse_device_code_response(const wire_response& aResponse)
		-> std::expected<
			device_code_outcome,
			device_authorization_contract_failure>;

	/// \brief Computes the parse device token response result.
	[[nodiscard]] auto parse_device_token_response(const wire_response& aResponse)
		-> std::expected<
			device_token_outcome,
			device_authorization_contract_failure>;
}

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_DEVICE_AUTHORIZATION_CONTRACT_HPP
