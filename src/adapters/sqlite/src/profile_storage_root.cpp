#include "bpr/adapters/sqlite/profile_storage_root.hpp"

#include "storage_internal.hpp"

namespace bpr::adapters::sqlite {
	namespace {
		[[nodiscard]] auto translate(const detail::state_directory_failure aFailure) -> profile_storage_root_failure
		{
			switch (aFailure) {
			case detail::state_directory_failure::invalid_path:        return profile_storage_root_failure::invalid_path;
			case detail::state_directory_failure::invalid_permissions: return profile_storage_root_failure::invalid_permissions;
			default:                                                   return profile_storage_root_failure::unavailable;
			}
		}
	}

	auto resolve_profile_storage_root(const std::filesystem::path&                  aBase,
									  const application::profile_storage_authority& aAuthority, const std::stop_token aStop)
		-> std::expected<std::filesystem::path, profile_storage_root_failure>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(profile_storage_root_failure::cancelled);
		}
		try {
			if (auto admitted = detail::validate_private_storage_root(aBase); !admitted) {
				return std::unexpected(translate(admitted.error()));
			}
			std::error_code error;
			auto            base = std::filesystem::canonical(aBase, error);
			if (error) {
				return std::unexpected(profile_storage_root_failure::invalid_path);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(profile_storage_root_failure::cancelled);
			}
			if (aAuthority.root_mode() == application::profile_root_mode::legacy) {
				return base;
			}
			const auto parent = detail::ensure_private_storage_child(base, "profiles");
			if (!parent) {
				return std::unexpected(translate(parent.error()));
			}
			if (aStop.stop_requested()) {
				return std::unexpected(profile_storage_root_failure::cancelled);
			}
			const auto root = detail::ensure_private_storage_child(*parent, aAuthority.root_identity());
			if (!root) {
				return std::unexpected(translate(root.error()));
			}
			if (aStop.stop_requested()) {
				return std::unexpected(profile_storage_root_failure::cancelled);
			}
			auto canonical = std::filesystem::canonical(*root, error);
			if (error || canonical != *root) {
				return std::unexpected(profile_storage_root_failure::invalid_path);
			}
			return canonical;
		} catch (const std::filesystem::filesystem_error&) {
			return std::unexpected(profile_storage_root_failure::invalid_path);
		} catch (...) {
			return std::unexpected(profile_storage_root_failure::unavailable);
		}
	}
}
