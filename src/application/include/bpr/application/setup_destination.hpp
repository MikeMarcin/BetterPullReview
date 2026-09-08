#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SETUP_DESTINATION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SETUP_DESTINATION_HPP

#include "bpr/application/setup_destination_projection.hpp"

#include <functional>
#include <span>

namespace bpr::application {
	using setup_profile_observation = core::setup_profile_observation;

	/// \brief Host-owned default-environment observations and setup capabilities.
	class setup_destination_port
	{
	public:
		virtual ~setup_destination_port()                                                                            = default;
		[[nodiscard]] virtual auto profiles() const -> std::vector<setup_profile_observation>                        = 0;
		[[nodiscard]] virtual auto owner() const -> core::setup_owner_id                                             = 0;
		[[nodiscard]] virtual auto shorthand_hostname() const -> std::string                                         = 0;
		virtual void               configure(const core::pending_setup_destination& aPending)                        = 0;
		virtual void               authenticate(const core::pending_setup_destination& aPending)                     = 0;
		virtual void               open(core::host_profile_id aProfile, const core::setup_destination& aDestination) = 0;
		virtual void               present(const setup_destination_projection& aProjection)                          = 0;
	};

	/// \brief Retains one exact pending navigation on the host's serialized decision lane.
	class setup_destination_host
	{
	public:
		explicit setup_destination_host(setup_destination_port& aPort);
		void               submit(std::string_view aText);
		void               continue_setup(core::setup_request_id aRequest);
		void               committed(core::setup_request_id aRequest);
		void               configured(core::setup_request_id aRequest);
		void               authenticated(core::setup_request_id aRequest, core::host_profile_id aProfile, std::string_view aHostname);
		void               cancel();
		[[nodiscard]] auto pending() const -> const std::optional<core::pending_setup_destination>&;

	private:
		void                                           resolve();
		void                                           present(std::string aNotice = { }, bool aOpened = false);
		setup_destination_port&                        Port;
		std::optional<core::pending_setup_destination> Pending;
		std::uint64_t                                  NextRequest = 1U;
	};
}
#endif
