#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SETUP_DESTINATION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SETUP_DESTINATION_HPP

#include "bpr/application/setup_destination_projection.hpp"

#include <functional>
#include <span>

namespace bpr::application {
	using setup_profile_observation = core::setup_profile_observation;

	/// \brief [[BPR-0987]]. How the host answered an accepted destination.
	///
	/// \details `navigated` means the receiving window already showed that
	/// pull request, so no new read was issued and its view is kept. The
	/// composed window no longer answers it: every reopen reads fresh
	/// ([[BPR-1475]], owner 2026-10-13).
	enum class setup_destination_opening : std::uint8_t {
		reading,
		navigated,
		repository_prepared,
		unavailable,
	};

	/// \brief Host-owned default-environment observations and setup capabilities.
	class setup_destination_port
	{
	public:
		virtual ~setup_destination_port()                                                        = default;
		[[nodiscard]] virtual auto profiles() const -> std::vector<setup_profile_observation>    = 0;
		[[nodiscard]] virtual auto owner() const -> core::setup_owner_id                         = 0;
		[[nodiscard]] virtual auto shorthand_hostname() const -> std::string                     = 0;
		virtual void               configure(const core::pending_setup_destination& aPending)    = 0;
		virtual void               authenticate(const core::pending_setup_destination& aPending) = 0;
		/// \brief [[BPR-0732]]. Activates the profile signed out, then reports `previewed`.
		virtual void preview(const core::pending_setup_destination& aPending) = 0;
		/// \brief Opens the exact accepted destination in the receiving window.
		[[nodiscard]] virtual auto open(core::host_profile_id aProfile, const core::setup_destination& aDestination) -> setup_destination_opening = 0;
		virtual void               present(const setup_destination_projection& aProjection)                                                       = 0;
	};

	/// \brief Retains one exact pending navigation on the host's serialized decision lane.
	class setup_destination_host
	{
	public:
		explicit setup_destination_host(setup_destination_port& aPort);
		void submit(std::string_view aText);
		/// \brief Reuses explicit setup/sign-in; completion grants no repository save.
		void submit_repository(std::string_view aText);
		void continue_setup(core::setup_request_id aRequest);
		/// \brief Binds an account choice to the current exact-host navigation.
		[[nodiscard]] auto begin_authentication(core::setup_request_id aRequest, core::host_profile_id aProfile) -> bool;
		void               committed(core::setup_request_id aRequest);
		void               configured(core::setup_request_id aRequest);
		void               authenticated(core::setup_request_id aRequest, core::host_profile_id aProfile, std::string_view aHostname);
		/// \brief [[BPR-0732]]. The signed-out session for the pending profile exists; opens the destination.
		void               previewed(core::setup_request_id aRequest, core::host_profile_id aProfile, std::string_view aHostname);
		void               cancel();
		[[nodiscard]] auto pending() const -> const std::optional<core::pending_setup_destination>&;

	private:
		void                                           resolve();
		void                                           present(std::string aNotice = { }, bool aOpened = false, bool aNavigated = false);
		setup_destination_port&                        Port;
		std::optional<core::pending_setup_destination> Pending;
		std::uint64_t                                  NextRequest = 1U;
	};
}
#endif
