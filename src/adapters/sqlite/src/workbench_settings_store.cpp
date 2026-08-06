#include "bpr/adapters/sqlite/workbench_settings_store.hpp"

#include "state_directory.hpp"
#include "workbench_settings_database.hpp"

#include <memory>
#include <utility>

namespace bpr::adapters::sqlite {
	namespace {
		constexpr auto SettingsName = "workbench-settings.sqlite3";

		/// \brief Maps one database failure to the port's own vocabulary.
		///
		/// \details The port publishes exactly two failures. `cancelled` is kept
		/// distinct because a caller that stopped its own work must be able to
		/// tell that from a store it cannot use — retrying the first is correct
		/// and retrying the second is a loop.
		[[nodiscard]] auto translate(
			const detail::catalog_database_failure aFailure) noexcept
			-> application::workbench_settings_failure
		{
			if (aFailure == detail::catalog_database_failure::cancelled) {
				return application::workbench_settings_failure::cancelled;
			}
			return application::workbench_settings_failure::unavailable;
		}

		[[nodiscard]] auto translate(
			const detail::state_directory_failure) noexcept
			-> application::workbench_settings_failure
		{
			return application::workbench_settings_failure::unavailable;
		}

		class sqlite_workbench_settings_store final
			: public application::workbench_settings_port
		{
		public:
			explicit sqlite_workbench_settings_store(
				detail::workbench_settings_database aDatabase) noexcept
				: Database{ std::move(aDatabase) }
			{
			}

			[[nodiscard]] auto load(std::stop_token aStop)
				-> application::workbench_settings_result<
					application::workbench_layout_settings> override
			{
				auto loaded = Database.load(std::move(aStop));
				if (!loaded) {
					return std::unexpected(application::workbench_settings_error{
						.Failure = translate(loaded.error()) });
				}
				return *loaded;
			}

			[[nodiscard]] auto store(
				const application::workbench_layout_settings& aSettings,
				std::stop_token                               aStop)
				-> application::workbench_settings_result<void> override
			{
				auto stored = Database.store(aSettings, std::move(aStop));
				if (!stored) {
					return std::unexpected(application::workbench_settings_error{
						.Failure = translate(stored.error()) });
				}
				return { };
			}

		private:
			detail::workbench_settings_database Database;
		};
	} // namespace

	auto make_workbench_settings_store(
		const workbench_settings_configuration& aConfiguration,
		std::stop_token                         aStop)
		-> std::expected<
			std::unique_ptr<application::workbench_settings_port>,
			application::workbench_settings_failure>
	{
		// The same bootstrap every other store uses, rather than creating the
		// directory here. Whichever store opens first would otherwise decide
		// what the directory is, and the weakest policy would win the race.
		auto directory = detail::open_state_directory(aConfiguration.StorageRoot);
		if (!directory) {
			return std::unexpected(translate(directory.error()));
		}

		auto opened = detail::workbench_settings_database::open(
			directory->path() / SettingsName,
			std::move(aStop));
		if (!opened) {
			return std::unexpected(translate(opened.error()));
		}

		return std::make_unique<sqlite_workbench_settings_store>(
			std::move(*opened));
	}
} // namespace bpr::adapters::sqlite
