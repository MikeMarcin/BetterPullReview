#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DEVICE_AUTHORIZATION_MODEL_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DEVICE_AUTHORIZATION_MODEL_HPP

#include "bpr/core/github_model.hpp"
#include "bpr/core/model.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

/// The core owns the device-authorization interaction as a bounded sequence
/// of typed observations and decides which commands are legal in each stage.
/// Every record here is strong ids and closed enums only: no type in this
/// header can represent a client identifier, a service endpoint, a poll
/// interval, a device or user code, or token material. The user-facing
/// presentation of the flow is a host-owned projection that never enters
/// core state.
namespace bpr::core {
	struct device_authorization_generation_tag;

	using device_authorization_generation
		= strong_id<device_authorization_generation_tag>;

	enum class device_authorization_stage : std::uint8_t {
		idle,
		requested,
		awaiting_authorization,
		cancelling,
		authorized,
		denied,
		expired,
		disabled,
		unavailable,
		cancelled,
		failed,
	};

	/// \brief Reports whether the terminal condition holds.
	[[nodiscard]] auto terminal(device_authorization_stage aStage) noexcept
		-> bool;

	enum class device_authorization_failure : std::uint8_t {
		network,
		certificate,
		redirect,
		timeout,
		malformed_response,
		response_too_large,
		vault,
	};

	/// \brief Describes the device authorization request context record.
	struct device_authorization_request_context
	{
		request_id                      Request;
		application_session_id          ApplicationSession;
		host_profile_id                 Profile;
		github_identity_generation      IdentityGeneration;
		device_authorization_generation Generation;

		auto operator<=>(const device_authorization_request_context&) const
			= default;
	};

	/// \brief Describes the begin device authorization intent record.
	struct begin_device_authorization_intent
	{
		host_profile_id Profile;

		auto operator<=>(const begin_device_authorization_intent&) const
			= default;
	};

	/// \brief Describes the cancel device authorization intent record.
	struct cancel_device_authorization_intent
	{
		auto operator<=>(const cancel_device_authorization_intent&) const
			= default;
	};

	/// \brief Describes the device authorization awaiting event record.
	struct device_authorization_awaiting_event
	{
		device_authorization_request_context Context;

		auto operator<=>(const device_authorization_awaiting_event&) const
			= default;
	};

	/// \brief Describes the device authorization authorized event record.
	struct device_authorization_authorized_event
	{
		device_authorization_request_context Context;

		auto operator<=>(const device_authorization_authorized_event&) const
			= default;
	};

	/// \brief Describes the device authorization denied event record.
	struct device_authorization_denied_event
	{
		device_authorization_request_context Context;

		auto operator<=>(const device_authorization_denied_event&) const
			= default;
	};

	/// \brief Describes the device authorization expired event record.
	struct device_authorization_expired_event
	{
		device_authorization_request_context Context;

		auto operator<=>(const device_authorization_expired_event&) const
			= default;
	};

	/// \brief Describes the device authorization disabled event record.
	struct device_authorization_disabled_event
	{
		device_authorization_request_context Context;

		auto operator<=>(const device_authorization_disabled_event&) const
			= default;
	};

	/// \brief Describes the device authorization unavailable event record.
	struct device_authorization_unavailable_event
	{
		device_authorization_request_context Context;

		auto operator<=>(const device_authorization_unavailable_event&) const
			= default;
	};

	/// \brief Describes the device authorization failed event record.
	struct device_authorization_failed_event
	{
		device_authorization_request_context Context;
		device_authorization_failure         Failure
			= device_authorization_failure::network;

		auto operator<=>(const device_authorization_failed_event&) const
			= default;
	};

	/// \brief Describes the device authorization cancelled event record.
	struct device_authorization_cancelled_event
	{
		device_authorization_request_context Context;

		auto operator<=>(const device_authorization_cancelled_event&) const
			= default;
	};

	using device_authorization_event = std::variant<
		begin_device_authorization_intent,
		cancel_device_authorization_intent,
		device_authorization_awaiting_event,
		device_authorization_authorized_event,
		device_authorization_denied_event,
		device_authorization_expired_event,
		device_authorization_disabled_event,
		device_authorization_unavailable_event,
		device_authorization_failed_event,
		device_authorization_cancelled_event>;

	/// \brief Describes the begin device authorization request record.
	struct begin_device_authorization_request
	{
		device_authorization_request_context Context;

		auto operator<=>(const begin_device_authorization_request&) const
			= default;
	};

	/// \brief Describes the cancel device authorization request record.
	struct cancel_device_authorization_request
	{
		device_authorization_request_context Context;

		auto operator<=>(const cancel_device_authorization_request&) const
			= default;
	};

	using device_authorization_host_request = std::variant<
		begin_device_authorization_request,
		cancel_device_authorization_request>;

	enum class device_authorization_command_id : std::uint8_t {
		begin_device_authorization,
		cancel_device_authorization,
	};

	enum class device_authorization_reason_code : std::uint8_t {
		none,
		flow_in_progress,
		no_active_flow,
		cancellation_in_progress,
		stale_completion,
		authorization_denied,
		authorization_expired,
		device_flow_disabled,
		device_flow_unavailable,
		cancelled,
		network_unavailable,
		certificate_rejected,
		redirect_rejected,
		timed_out,
		malformed_response,
		response_too_large,
		vault_unavailable,
	};

	/// \brief Computes the reason for result.
	[[nodiscard]] auto reason_for(device_authorization_failure aFailure) noexcept
		-> device_authorization_reason_code;

	/// \brief Describes the device authorization command projection record.
	struct device_authorization_command_projection
	{
		device_authorization_command_id Id
			= device_authorization_command_id::begin_device_authorization;
		bool                             Enabled = true;
		device_authorization_reason_code DisabledReason
			= device_authorization_reason_code::none;

		auto operator<=>(const device_authorization_command_projection&) const
			= default;
	};

	/// \brief Describes the device authorization status delta record.
	struct device_authorization_status_delta
	{
		device_authorization_stage Stage = device_authorization_stage::idle;

		auto operator<=>(const device_authorization_status_delta&) const
			= default;
	};

	/// \brief Describes the device authorization diagnostic record.
	struct device_authorization_diagnostic
	{
		device_authorization_reason_code Code
			= device_authorization_reason_code::none;

		auto operator<=>(const device_authorization_diagnostic&) const = default;
	};

	enum class device_authorization_changed_domain : std::uint8_t {
		none        = 0,
		commands    = 1U << 0U,
		status      = 1U << 1U,
		diagnostics = 1U << 2U,
	};

	[[nodiscard]] constexpr auto operator|(
		const device_authorization_changed_domain aLeft,
		const device_authorization_changed_domain aRight) noexcept
		-> device_authorization_changed_domain
	{
		return static_cast<device_authorization_changed_domain>(
			static_cast<std::uint8_t>(aLeft) | static_cast<std::uint8_t>(aRight));
	}

	/// \brief Reports whether the has changed condition holds.
	[[nodiscard]] constexpr auto has_changed(
		const device_authorization_changed_domain aValue,
		const device_authorization_changed_domain aExpected) noexcept -> bool
	{
		return (
				   static_cast<std::uint8_t>(aValue)
				   & static_cast<std::uint8_t>(aExpected))
			!= 0U;
	}

	/// \brief Describes the device authorization transition output record.
	struct device_authorization_transition_output
	{
		device_authorization_changed_domain Changed
			= device_authorization_changed_domain::none;
		std::vector<device_authorization_host_request>       Requests;
		std::vector<device_authorization_command_projection> Commands;
		std::optional<device_authorization_status_delta>     Status;
		std::vector<device_authorization_diagnostic>         Diagnostics;

		auto operator<=>(const device_authorization_transition_output&) const
			= default;
	};

	/// \brief Describes the device authorization state record.
	struct device_authorization_state
	{
		application_session_id                              ApplicationSession;
		request_id                                          NextRequest{ 1 };
		github_identity_generation                          IdentityGeneration;
		device_authorization_generation                     Generation;
		device_authorization_stage                          Stage = device_authorization_stage::idle;
		std::optional<device_authorization_request_context> Pending;
		std::optional<host_profile_id>                      Profile;
		device_authorization_reason_code                    TerminalReason
			= device_authorization_reason_code::none;

		auto operator<=>(const device_authorization_state&) const = default;
	};
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DEVICE_AUTHORIZATION_MODEL_HPP
