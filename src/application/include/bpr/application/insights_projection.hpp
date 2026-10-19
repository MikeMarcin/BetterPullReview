#ifndef BPR_APPLICATION_INSIGHTS_PROJECTION_HPP
#define BPR_APPLICATION_INSIGHTS_PROJECTION_HPP

#include "bpr/core/insights.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace bpr::application {
	/// \brief One local store's measured file bytes, including SQLite sidecars.
	struct insights_storage_row
	{
		std::string                  Name;
		std::optional<std::uint64_t> Bytes;
		std::optional<std::uint64_t> Budget;
		bool                         Repository = false;
	};

	/// \brief Local-only immutable snapshot; unavailable observations have no numeric value.
	struct insights_snapshot
	{
		std::int64_t                      BuiltUnixSeconds   = 0;
		std::int64_t                      StorageUnixSeconds = 0;
		std::vector<insights_storage_row> Storage;
		std::optional<std::uint64_t>      TotalBytes;
		std::optional<std::uint64_t>      GitBytes;
		std::string                       BackgroundWork;
		core::insights_github_snapshot    Github;
		core::insights_reviews_snapshot   Reviews;
		core::insights_energy_snapshot    Energy;
		core::insights_work_snapshot      Work;
	};

	/// \brief Presentation gestures; none gives Qt access to a storage or process capability.
	struct insights_dispatch
	{
		std::function<void(bool)> Refresh;
		std::function<void()>     RevealData;
		std::function<void()>     Export;
		std::function<void()>     ManageCaches;
	};
}

#endif
