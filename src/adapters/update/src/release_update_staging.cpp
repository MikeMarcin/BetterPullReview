#include "bpr/adapters/update/release_update_staging.hpp"
#if !defined(__APPLE__)
namespace bpr::adapters::update {
	auto release_update_installation_capability(const std::filesystem::path&, std::stop_token) -> core::release_update_capability
	{
		return core::release_update_capability::unsupported_platform;
	}
	auto make_release_update_staging(std::filesystem::path, std::filesystem::path, application::effect_executor&)
		-> std::shared_ptr<release_update_staging> { return { }; }
}
#endif
