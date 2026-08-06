#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_APPEARANCE_SETTINGS_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_APPEARANCE_SETTINGS_STORE_HPP

#include "bpr/application/appearance_runtime_ports.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <stop_token>

namespace bpr::adapters::sqlite {
	/// \brief Describes the appearance settings configuration record.
	struct appearance_settings_configuration
	{
		// The owner-private application storage directory. The adapter owns the
		// fixed state/v1/appearance-settings.sqlite3 path beneath it, separate
		// from every other store so that deleting one kind of local state
		// cannot take another with it.
		std::filesystem::path StorageRoot;
	};

	/// \brief Creates the durable appearance settings store.
	///
	/// \details Appearance carries no review scope, so this configuration names
	/// no host, account, repository, or pull request. A reviewer's theme and
	/// fonts are one preference across every review.
	[[nodiscard]] auto make_appearance_settings_store(
		const appearance_settings_configuration& aConfiguration,
		std::stop_token                          aStop = { })
		-> std::expected<
			std::unique_ptr<application::appearance_settings_port>,
			application::appearance_settings_failure>;
} // namespace bpr::adapters::sqlite

#endif // BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_APPEARANCE_SETTINGS_STORE_HPP
