#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_EPOCH_CATALOG_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_EPOCH_CATALOG_HPP

#include "bpr/application/epoch_catalog_port.hpp"

#include <filesystem>
#include <memory>
#include <stop_token>

namespace bpr::adapters::sqlite {
	/// \brief Describes the epoch catalog configuration record.
	struct epoch_catalog_configuration
	{
		// The owner-private application storage directory. The adapter owns the
		// fixed state/v1/epoch-catalog.sqlite3 path beneath it.
		std::filesystem::path StorageRoot;
	};

	/// \brief Creates the epoch catalog.
	[[nodiscard]] auto make_epoch_catalog(
		const epoch_catalog_configuration& aConfiguration,
		std::stop_token                    aStop = { })
		-> application::epoch_catalog_result<
			std::unique_ptr<application::epoch_catalog_port>>;
} // namespace bpr::adapters::sqlite

#endif // BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_EPOCH_CATALOG_HPP
