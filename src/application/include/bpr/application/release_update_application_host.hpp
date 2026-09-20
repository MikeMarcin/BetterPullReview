#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_RELEASE_UPDATE_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_RELEASE_UPDATE_APPLICATION_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/release_update_port.hpp"
#include "bpr/application/release_update_projection.hpp"

#include <memory>

namespace bpr::application {
	/// \brief One application-owned updater with separated decision and effect lanes.
	/// \details Public methods and projection delivery are decision-lane confined.
	/// A successful ingress returns true, including a core policy refusal visible in
	/// the projection/command state. False means the bounded reentrant FIFO was full.
	/// Cancellation retires policy immediately, while jobs remain counted through their
	/// terminals. Keep executors and clock alive until quiescent(). The projection may
	/// reenter or throw. The host never retrieves a credential or constructs an installer.
	class release_update_application_host final
	{
	public:
		release_update_application_host(core::application_session_id aSession,
										std::string_view aInstalled, core::release_update_capability aCapability,
										core::release_update_preferences aPreferences, release_update_clock_port& aClock,
										decision_executor& aDecision, effect_executor& aEffects,
										std::shared_ptr<release_update_verification_port> aVerifier, release_update_projection_port& aProjection,
										std::shared_ptr<release_update_install_port> aInstaller = { });
		~release_update_application_host();
		release_update_application_host(const release_update_application_host&)                    = delete;
		auto operator=(const release_update_application_host&) -> release_update_application_host& = delete;

		/// \brief Replace/revoke release authority and synchronously cancel its old jobs.
		void               select_access(std::shared_ptr<release_update_access_port> aAccess);
		[[nodiscard]] auto dispatch(core::release_update_intent aIntent) -> bool;
		[[nodiscard]] auto tick() -> bool;
		void               observe_installation_result(bool aInstalled);
		[[nodiscard]] auto set_automatic_checks(bool aEnabled) -> bool;
		void               shutdown();
		[[nodiscard]] auto quiescent() const noexcept -> bool;
		[[nodiscard]] auto projection() const -> release_update_projection;

	private:
		struct implementation;
		std::shared_ptr<implementation> Impl;
	};
}

#endif
