#ifndef BPR_APP_PROFILE_ADMISSION_HPP
#define BPR_APP_PROFILE_ADMISSION_HPP
#include "bpr/application/profile_runtime_lease.hpp"
#include "bpr/core/profile_window.hpp"
#include "tour_cli_types.hpp"
#include <filesystem>
#include <memory>
#include <string>
namespace bpr::app {
	/// \brief One terminal command's admission to an existing profile (BPR-1167).
	struct profile_admission
	{
		std::filesystem::path Root;
		core::bpr_profile_id  Profile;
		/// Empty when another process owns the profile: nothing here may write it.
		std::shared_ptr<application::profile_runtime_lease> Lease;
	};
	/// \brief Admits the profile named by exact label or decimal ID (the default
	/// profile when empty) under an owner-private application root, and takes its
	/// runtime lease unless another process holds it. Shared by `bpr tour` and
	/// `bpr cycle`; blocking, so run it off any lane that must stay responsive.
	[[nodiscard]] auto admit_profile(const std::filesystem::path& aBase, const std::string& aProfile) -> tour_cli_result<profile_admission>;
}
#endif
