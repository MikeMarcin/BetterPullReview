#ifndef BPR_ADAPTERS_SQLITE_PROFILE_ACTIVATION_HPP
#define BPR_ADAPTERS_SQLITE_PROFILE_ACTIVATION_HPP

#include "bpr/application/profile_activation.hpp"
#include "bpr/application/profile_runtime_lease.hpp"

#include <expected>
#include <filesystem>
#include <stop_token>

namespace bpr::adapters::sqlite {
	/// \brief Binds a private native endpoint under an existing runtime lease.
	[[nodiscard]] auto make_profile_activation_server(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, std::shared_ptr<application::profile_runtime_lease> aLease)
		-> std::expected<std::shared_ptr<application::profile_activation_server>, application::profile_activation_outcome>;
	/// \brief Sends once with bounded cancellation and no retry after ambiguous delivery.
	[[nodiscard]] auto send_profile_activation(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, core::profile_launch_intent aIntent, std::stop_token aStop = { })
		-> std::expected<application::profile_activation_outcome, application::profile_activation_outcome>;
}
#endif
