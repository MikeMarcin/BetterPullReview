#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_RELEASE_UPDATE_SERVICE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_RELEASE_UPDATE_SERVICE_HPP

#include "bpr/application/release_update_application_host.hpp"

namespace bpr::application {
	/// \brief Application-wide updater lifetime and serialized durable preferences.
	/// \details All public methods are decision-lane confined. Stores and effects
	/// outlive quiescence; shutdown flushes the latest snapshot before retirement.
	class release_update_service final
	{
	public:
		release_update_service(std::string aInstalled, core::release_update_capability aCapability,
							   release_update_clock_port& aClock, decision_executor& aDecision, effect_executor& aEffects,
							   std::shared_ptr<release_update_settings_port>     aSettings,
							   std::shared_ptr<release_update_verification_port> aVerifier,
							   release_update_projection_port&                   aProjection,
							   std::shared_ptr<release_update_install_port>      aInstaller = { });
		~release_update_service();
		void               start();
		void               select_access(std::string aBinding, std::shared_ptr<release_update_access_port> aAccess);
		void               revoke_access();
		void               dispatch(core::release_update_intent aIntent);
		void               set_automatic_checks(bool aEnabled);
		void               tick();
		[[nodiscard]] auto observe_installation_result(bool aInstalled) -> bool;
		void               shutdown();
		[[nodiscard]] auto ready() const -> bool;
		[[nodiscard]] auto durable() const -> bool;
		[[nodiscard]] auto quiescent() const -> bool;
		[[nodiscard]] auto account_binding() const -> std::string;
		[[nodiscard]] auto projection() const -> release_update_projection;

	private:
		struct implementation;
		std::shared_ptr<implementation> Impl;
	};
}
#endif
