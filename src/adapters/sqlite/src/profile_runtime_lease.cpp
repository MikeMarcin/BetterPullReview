#include "bpr/adapters/sqlite/profile_runtime_lease.hpp"

#include "storage_internal.hpp"

namespace bpr::adapters::sqlite {
	auto acquire_profile_runtime_lease(const std::filesystem::path& aRoot, const std::stop_token aStop)
		-> std::expected<std::shared_ptr<application::profile_runtime_lease>, application::profile_runtime_lease_failure>
	{
		try {
			return detail::acquire_native_profile_runtime_lease(aRoot, aStop);
		} catch (...) {
			return std::unexpected(application::profile_runtime_lease_failure::unavailable);
		}
	}
}
