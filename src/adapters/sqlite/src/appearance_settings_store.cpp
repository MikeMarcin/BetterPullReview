#include "bpr/adapters/sqlite/appearance_settings_store.hpp"

#include "appearance_settings_database.hpp"
#include "state_directory.hpp"

#include <memory>
#include <utility>

namespace bpr::adapters::sqlite {
	namespace {
		constexpr auto SettingsName = "appearance-settings.sqlite3";

		/// \brief Maps one database failure to the port's own vocabulary.
		///
		/// \details `cancelled` stays distinct from `unavailable` because a
		/// caller that stopped its own work should retry, and a caller facing
		/// an unusable store that retries is a loop.
		[[nodiscard]] auto translate(
			const detail::catalog_database_failure aFailure) noexcept
			-> application::appearance_settings_failure
		{
			if (aFailure == detail::catalog_database_failure::cancelled) {
				return application::appearance_settings_failure::cancelled;
			}
			return application::appearance_settings_failure::unavailable;
		}

		[[nodiscard]] auto translate(const detail::state_directory_failure) noexcept
			-> application::appearance_settings_failure
		{
			return application::appearance_settings_failure::unavailable;
		}

		class sqlite_appearance_settings_store final
			: public application::appearance_settings_port
		{
		public:
			explicit sqlite_appearance_settings_store(
				detail::appearance_settings_database aDatabase) noexcept
				: Database{ std::move(aDatabase) }
			{
			}

			[[nodiscard]] auto load(std::stop_token aStop)
				-> application::appearance_settings_result<
					std::optional<application::appearance_settings>> override
			{
				auto loaded = Database.load(std::move(aStop));
				if (!loaded) {
					return std::unexpected(application::appearance_settings_error{
						.Failure = translate(loaded.error()) });
				}
				return *loaded;
			}

			[[nodiscard]] auto store(
				const application::appearance_settings& aSettings,
				std::stop_token                         aStop)
				-> application::appearance_settings_result<void> override
			{
				auto stored = Database.store(aSettings, std::move(aStop));
				if (!stored) {
					return std::unexpected(application::appearance_settings_error{
						.Failure = translate(stored.error()) });
				}
				return { };
			}

		private:
			detail::appearance_settings_database Database;
		};
	} // namespace

	auto make_appearance_settings_store(
		const appearance_settings_configuration& aConfiguration,
		std::stop_token                          aStop)
		-> std::expected<
			std::unique_ptr<application::appearance_settings_port>,
			application::appearance_settings_failure>
	{
		// The same bootstrap every other store uses. Creating the directory
		// here would let whichever store opened first decide what it is, and
		// the weakest policy would win the race.
		auto directory = detail::open_state_directory(aConfiguration.StorageRoot);
		if (!directory) {
			return std::unexpected(translate(directory.error()));
		}

		auto opened = detail::appearance_settings_database::open(
			directory->path() / SettingsName,
			std::move(aStop));
		if (!opened) {
			return std::unexpected(translate(opened.error()));
		}

		return std::make_unique<sqlite_appearance_settings_store>(
			std::move(*opened));
	}
} // namespace bpr::adapters::sqlite
