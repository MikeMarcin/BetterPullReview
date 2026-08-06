#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_WORKBENCH_SETTINGS_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_WORKBENCH_SETTINGS_STORE_HPP

#include "bpr/application/workbench_runtime_ports.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <stop_token>

namespace bpr::adapters::sqlite {
	/// \brief Describes the workbench settings configuration record.
	struct workbench_settings_configuration
	{
		// The owner-private application storage directory. The adapter owns the
		// fixed state/v1/workbench-settings.sqlite3 path beneath it, separate
		// from the epoch catalog, the review ledger, and the queue enrollment
		// store, so that deleting one kind of local state cannot take another
		// with it.
		std::filesystem::path StorageRoot;
	};

	/// \brief Creates the durable workbench layout settings store.
	///
	/// \details The layout carries no review scope, so unlike the review ledger
	/// this configuration names no host, account, repository, or pull request.
	/// Drawer geometry is one reviewer preference across every review, and
	/// scoping it per pull request would make a reviewer re-arrange their window
	/// for each one.
	[[nodiscard]] auto make_workbench_settings_store(
		const workbench_settings_configuration& aConfiguration,
		std::stop_token                         aStop = { })
		-> std::expected<
			std::unique_ptr<application::workbench_settings_port>,
			application::workbench_settings_failure>;
} // namespace bpr::adapters::sqlite

#endif // BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_WORKBENCH_SETTINGS_STORE_HPP
