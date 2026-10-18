#ifndef BPR_APP_PROFILE_DATA_RESET_HPP
#define BPR_APP_PROFILE_DATA_RESET_HPP

#include "bpr/application/profile_runtime_lease.hpp"
#include "bpr/core/profile_data_reset.hpp"
#include "bpr/core/profile_window.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

namespace bpr::app {
	/// \brief [[BPR-1602]]. What one reset did, for the notice shown when the
	/// profile reopens.
	struct profile_data_reset_report
	{
		core::profile_data_reset_outcome Outcome;
		/// The new folder the removed stores were copied to, when one was.
		std::optional<std::filesystem::path> CopyFolder;
	};

	/// \brief The copy folder's name for a reset started at this local time:
	/// "BetterPullReview reset 2026-10-18 14.02.33".
	[[nodiscard]] auto profile_data_reset_copy_name() -> std::string;

	/// \brief Runs one confirmed reset of a profile's local data.
	/// \details [[BPR-1602]]. Call on the startup effect lane under the
	/// runtime lease, after maintenance recovery and before any store,
	/// logger or producer opens. Core admits the request and names its scope;
	/// the maintenance adapter removes exactly that scope under its journal.
	/// A requested copy goes to a new folder in Downloads (else Documents or
	/// home); when none is available nothing is removed. Reset settings then
	/// restores Home's queue display choices in the store it keeps.
	[[nodiscard]] auto run_profile_data_reset(const std::filesystem::path& aRoot, std::shared_ptr<application::profile_runtime_lease> aLease,
											  core::bpr_profile_id aProfile, const core::profile_data_reset_request& aRequest,
											  std::stop_token aStop) -> profile_data_reset_report;
}
#endif
