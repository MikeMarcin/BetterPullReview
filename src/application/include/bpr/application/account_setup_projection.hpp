#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_ACCOUNT_SETUP_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_ACCOUNT_SETUP_PROJECTION_HPP

#include "bpr/application/device_authorization_projection.hpp"
#include "bpr/core/account_setup.hpp"

#include <cstdint>
#include <functional>

namespace bpr::application {
	/// \brief RFC-BPR-0032 amendment r1. What the in-window account setup
	/// presents, as values.
	///
	/// \details `Device` is the device host's own presentation, forwarded
	/// unchanged: the code and address it already validated, empty at every
	/// terminal. It carries nothing the device-authorization projection does
	/// not already allow Qt to see. No host, port, provider or credential type
	/// is reachable from here.
	struct account_setup_projection
	{
		core::account_setup_state         State;
		core::account_setup_step          Step = core::account_setup_step::way;
		device_authorization_presentation Device;
		/// The composition can inspect gh on this platform.
		bool GithubCliOffered = false;
		/// The window already has an account, so the setup can be closed
		/// without adding one (Settings' Add account, Use another account).
		bool SignedIn = false;
		/// Raised by every published change.
		std::uint64_t Generation = 0U;

		auto operator<=>(const account_setup_projection&) const = default;
	};

	using account_setup_sink = std::function<void(const account_setup_projection&)>;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_ACCOUNT_SETUP_PROJECTION_HPP
