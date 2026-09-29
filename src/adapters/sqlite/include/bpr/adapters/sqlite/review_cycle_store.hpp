#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_REVIEW_CYCLE_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_REVIEW_CYCLE_STORE_HPP

#include "bpr/application/review_cycle_store.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>

namespace bpr::adapters::sqlite {
	struct review_cycle_store_options
	{
		/// Retained object budget; installs beyond it are refused, never
		/// satisfied by evicting referenced material.
		std::uint64_t RetainedBudgetBytes = application::default_cycle_retained_budget_bytes;
	};

	/// \brief Review Cycles store in `<root>/state/v1/review-cycles.sqlite3`,
	/// under the shared private-directory and connection policy.
	[[nodiscard]] auto make_review_cycle_store(std::filesystem::path aRoot, core::bpr_profile_id aProfile,
											   review_cycle_store_options aOptions = { })
		-> std::unique_ptr<application::review_cycle_store>;
}

#endif
