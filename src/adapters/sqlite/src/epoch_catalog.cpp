#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "epoch_catalog_support.hpp"

#include "catalog_database.hpp"

#if defined(BPR_SQLITE_TESTING)
	#include "bpr/adapters/sqlite/testing.hpp"
	#include <atomic>
#endif

#include <expected>
#include <memory>
#include <stop_token>
#include <utility>

namespace bpr::adapters::sqlite {
	namespace {
		namespace app = application;
	} // namespace

#if defined(BPR_SQLITE_TESTING)
	namespace detail::testing {
		void set_epoch_journal_checkpoint(epoch_journal_checkpoint aCheckpoint) noexcept
		{
			epoch_catalog_detail::JournalCheckpoint.store(aCheckpoint, std::memory_order_release);
		}
	} // namespace detail::testing
#endif

	auto make_epoch_catalog(
		const epoch_catalog_configuration& aConfiguration,
		std::stop_token                    aStop)
		-> app::epoch_catalog_result<std::unique_ptr<app::epoch_catalog_port>>
	{
		auto database = detail::catalog_database::open(
			aConfiguration.StorageRoot, aStop);
		if (!database) {
			return std::unexpected(epoch_catalog_detail::map_failure(database.error()));
		}
		auto state = std::make_shared<epoch_catalog_detail::catalog_state>(std::move(*database));
		return std::make_unique<epoch_catalog_detail::sqlite_epoch_catalog_port>(std::move(state));
	}
} // namespace bpr::adapters::sqlite
