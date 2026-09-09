#ifndef BPR_ADAPTERS_SQLITE_PROFILE_RUNTIME_LEASE_HPP
#define BPR_ADAPTERS_SQLITE_PROFILE_RUNTIME_LEASE_HPP

#include "bpr/application/profile_runtime_lease.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <stop_token>

namespace bpr::adapters::sqlite {
	/// \brief Nonblocking exclusive native admission for a canonical private profile root.
	/// \details The kernel releases ownership on last lease destruction or process exit.
	[[nodiscard]] auto acquire_profile_runtime_lease(const std::filesystem::path& aRoot, std::stop_token aStop = { })
		-> std::expected<std::shared_ptr<application::profile_runtime_lease>, application::profile_runtime_lease_failure>;
}
#endif
