#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_RELEASE_UPDATE_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_RELEASE_UPDATE_PROJECTION_HPP

#include "bpr/core/release_update.hpp"

#include <functional>

namespace bpr::application {
	/// \brief Accepted updater facts; contains no transport, credential, path or URL.
	struct release_update_projection
	{
		std::optional<core::release_update_version>            Installed;
		core::release_update_phase                             Phase  = core::release_update_phase::idle;
		core::release_update_reason                            Reason = core::release_update_reason::unavailable;
		std::optional<core::release_update_candidate>          Candidate;
		core::release_update_preferences                       Preferences;
		std::array<core::release_update_command_projection, 6> Commands     = core::release_update_commands({ });
		std::uint64_t                                          Bytes        = 0U;
		bool                                                   OfferVisible = false;
		std::string                                            Notes;
		bool                                                   AutomaticAvailable = false;
		std::optional<bool>                                    InstallationOutcome;
	};

	/// \brief Settings and release offer share the application's semantic dispatch.
	struct release_update_dispatch
	{
		std::function<void(core::release_update_intent)> Intent;
		std::function<void(bool)>                        Automatic;
		std::function<void()>                            ViewRelease;
	};

	/// \brief Observational publication; exceptions cannot veto core acceptance.
	class release_update_projection_port
	{
	public:
		virtual ~release_update_projection_port()                                                    = default;
		virtual void publish(const release_update_projection& aProjection, bool aPreferencesChanged) = 0;
	};
}
#endif
