#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_QUEUE_ENROLLMENT_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_QUEUE_ENROLLMENT_STORE_HPP

#include "bpr/application/review_queue_enrollment.hpp"

#include <filesystem>
#include <memory>

namespace bpr::adapters::sqlite {
	struct queue_enrollment_configuration
	{
		std::filesystem::path StorageRoot;
	};

	/// \brief Creates a lazy-opening durable queue store.
	///
	/// \details Construction performs no filesystem or SQLite work. The first
	/// store operation opens the database on the caller's effect lane.
	[[nodiscard]] auto make_queue_enrollment_store(
		queue_enrollment_configuration aConfiguration)
		-> std::unique_ptr<application::queue_enrollment_store_port>;
} // namespace bpr::adapters::sqlite

#endif // BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_QUEUE_ENROLLMENT_STORE_HPP
