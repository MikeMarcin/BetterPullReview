#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEVICE_AUTHORIZATION_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEVICE_AUTHORIZATION_PROJECTION_HPP

#include "bpr/core/device_authorization_model.hpp"

#include <compare>
#include <cstdint>
#include <string>

namespace bpr::application {
	/// \brief Describes the device authorization presentation record.
	///
	/// \details Four fields, and the list is closed. The reviewer has to read a
	/// code off the screen and type it into a browser, so the user code and the
	/// address it goes to have to be presentable; everything else the flow
	/// touches — the device code, the tokens, the client identifier, the poll
	/// interval, the endpoint — has no presentation and is absent by
	/// construction rather than by omission.
	///
	/// `VerificationUri` arrives already checked against the server authority by
	/// the wire parser, so presentation never decides whether an address is safe
	/// to show. `RemainingValiditySeconds` counts to whichever terminal comes
	/// first, and both text fields are empty at every terminal, including the
	/// successful one: a user code that outlives its flow is a code the reviewer
	/// can still be tricked into typing.
	struct device_authorization_presentation
	{
		core::device_authorization_stage Stage
			= core::device_authorization_stage::idle;
		std::string   UserCode;
		std::string   VerificationUri;
		std::uint32_t RemainingValiditySeconds = 0U;

		auto operator<=>(const device_authorization_presentation&) const = default;
	};

	/// \brief Provides the device authorization projection port contract.
	class device_authorization_projection_port
	{
	public:
		virtual ~device_authorization_projection_port() = default;

		// Delivery is synchronous and observational, on every stage change. The
		// host contains projection exceptions; presentation cannot veto an
		// accepted core transition or an enrolled credential.
		virtual void publish(
			const device_authorization_presentation& aPresentation) = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEVICE_AUTHORIZATION_PROJECTION_HPP
