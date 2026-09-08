#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_PROFILE_STORAGE_ROOT_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_PROFILE_STORAGE_ROOT_HPP

#include "bpr/application/profile_storage_authority.hpp"

#include <filesystem>
#include <stop_token>

namespace bpr::adapters::sqlite {
	enum class profile_storage_root_failure : std::uint8_t { invalid_path,
															 invalid_permissions,
															 unavailable,
															 cancelled };

	/// \brief Resolves an admitted profile into an owner-private canonical storage root.
	/// \details Default retains the existing base in place. Isolated profiles create only
	/// profiles/profile-N with private permissions; existing unsafe paths are refused without
	/// permission repair. This is admission-time validation, not a retained runtime lock.
	/// Cancellation can leave newly created empty private directories for an idempotent retry.
	[[nodiscard]] auto resolve_profile_storage_root(const std::filesystem::path&                  aBase,
													const application::profile_storage_authority& aAuthority, std::stop_token aStop = { })
		-> std::expected<std::filesystem::path, profile_storage_root_failure>;
}
#endif
