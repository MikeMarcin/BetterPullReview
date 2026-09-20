#ifndef BPR_SRC_ADAPTERS_UPDATE_INCLUDE_BPR_ADAPTERS_UPDATE_RELEASE_UPDATE_STAGING_HPP
#define BPR_SRC_ADAPTERS_UPDATE_INCLUDE_BPR_ADAPTERS_UPDATE_RELEASE_UPDATE_STAGING_HPP
#include "bpr/application/port.hpp"
#include "bpr/application/release_update_port.hpp"
#include <filesystem>

namespace bpr::adapters::update {
	/// \brief Owns private archives and native validation; has no replacement operation.
	class release_update_staging : public application::release_update_archive_store,
								   public application::release_update_verification_port
	{
	public:
		/// \brief Repeats exact-tree and native trust validation before any future handoff.
		[[nodiscard]] virtual auto revalidate(std::shared_ptr<const application::release_update_verified_bundle> aBundle,
											  std::stop_token                                                    aStop) -> core::release_update_reason = 0;
	};
	/// \brief Observes native installation eligibility without obtaining update credentials.
	[[nodiscard]] auto release_update_installation_capability(const std::filesystem::path& aInstalled,
															  std::stop_token              aStop) -> core::release_update_capability;
	/// \brief Lazy effects; cleanup executor must remain live until every artifact is released.
	[[nodiscard]] auto make_release_update_staging(std::filesystem::path aPrivateRoot,
												   std::filesystem::path aInstalled, application::effect_executor& aCleanup) -> std::shared_ptr<release_update_staging>;
}
#endif
