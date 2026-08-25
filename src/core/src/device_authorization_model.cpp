#include "bpr/core/device_authorization_model.hpp"

namespace bpr::core {
	auto terminal(const device_authorization_stage aStage) noexcept -> bool
	{
		switch (aStage) {
		case device_authorization_stage::authorized:
		case device_authorization_stage::denied:
		case device_authorization_stage::expired:
		case device_authorization_stage::disabled:
		case device_authorization_stage::unavailable:
		case device_authorization_stage::cancelled:
		case device_authorization_stage::failed:
			return true;
		case device_authorization_stage::idle:
		case device_authorization_stage::requested:
		case device_authorization_stage::awaiting_authorization:
		case device_authorization_stage::cancelling:
			return false;
		}
		return false;
	}

	auto reason_for(const device_authorization_failure aFailure) noexcept
		-> device_authorization_reason_code
	{
		switch (aFailure) {
		case device_authorization_failure::network:
			return device_authorization_reason_code::network_unavailable;
		case device_authorization_failure::certificate:
			return device_authorization_reason_code::certificate_rejected;
		case device_authorization_failure::redirect:
			return device_authorization_reason_code::redirect_rejected;
		case device_authorization_failure::timeout:
			return device_authorization_reason_code::timed_out;
		case device_authorization_failure::malformed_response:
			return device_authorization_reason_code::malformed_response;
		case device_authorization_failure::response_too_large:
			return device_authorization_reason_code::response_too_large;
		case device_authorization_failure::vault:
			return device_authorization_reason_code::vault_unavailable;
		}
		return device_authorization_reason_code::network_unavailable;
	}
}
