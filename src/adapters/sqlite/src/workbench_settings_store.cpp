#include "bpr/adapters/sqlite/workbench_settings_store.hpp"

#include "state_directory.hpp"
#include "workbench_settings_database.hpp"

#include <memory>
#include <mutex>
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
			sqlite_workbench_settings_store(
				detail::state_directory aDirectory, detail::workbench_settings_database aDatabase, core::bpr_window_id aWindow) noexcept
				: Directory{ std::move(aDirectory) }
				, Database{ std::move(aDatabase) }
				, Window{ aWindow }
			{
			}

			[[nodiscard]] auto load(std::stop_token aStop)
				-> application::workbench_settings_result<
					application::workbench_layout_settings> override
			{
				const std::scoped_lock lock{ Mutex };
				auto                   loaded = Window.value() == 0 ? Database.load(aStop) : Database.load_window(Window, aStop);
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
				const std::scoped_lock lock{ Mutex };
				auto                   stored = [&]() -> std::expected<void, detail::failure> {
					if (Window.value() != 0) {
						return Database.store_window(Window, application::window_layout(aSettings), aStop);
					}
					auto preferences = Database.load(aStop);
					if (!preferences) {
						return std::unexpected(preferences.error());
					}
					auto settings          = application::combined_layout(application::window_layout(aSettings), preferences->Behaviour);
					settings.WritingHooks  = preferences->WritingHooks;
					settings.ClaudeWrapper = preferences->ClaudeWrapper;
					settings.ClaudeModel   = preferences->ClaudeModel;
					return Database.store(settings, aStop);
				}();
				if (!stored) {
					return std::unexpected(application::workbench_settings_error{
						.Failure = translate(stored.error()) });
				}
				return { };
			}

			[[nodiscard]] auto profile_scoped() const noexcept -> bool override { return true; }
			[[nodiscard]] auto window() const noexcept -> core::bpr_window_id override { return Window; }
			[[nodiscard]] auto load_preferences(std::stop_token aStop) -> application::workbench_settings_result<application::workbench_profile_preferences> override
			{
				const std::scoped_lock lock{ Mutex };
				auto                   loaded = Database.load(aStop);
				if (!loaded) {
					return std::unexpected(application::workbench_settings_error{ translate(loaded.error()) });
				}
				return application::workbench_profile_preferences{ loaded->Behaviour, application::window_layout(*loaded), loaded->WritingHooks, loaded->ClaudeWrapper, loaded->ClaudeModel };
			}
			[[nodiscard]] auto store_preferences(const application::workbench_profile_preferences& aPreferences, std::stop_token aStop) -> application::workbench_settings_result<void> override
			{
				const std::scoped_lock lock{ Mutex };
				auto                   settings = application::combined_layout(aPreferences.NewWindowDefaults, aPreferences.Behaviour);
				settings.WritingHooks           = aPreferences.WritingHooks;
				settings.ClaudeWrapper          = aPreferences.ClaudeWrapper;
				settings.ClaudeModel            = aPreferences.ClaudeModel;
				auto stored                     = Database.store(settings, aStop);
				if (!stored) {
					return std::unexpected(application::workbench_settings_error{ translate(stored.error()) });
				}
				return { };
			}
			[[nodiscard]] auto retain_windows(std::span<const core::bpr_window_id> aWindows, std::stop_token aStop) -> application::workbench_settings_result<void> override
			{
				const std::scoped_lock lock{ Mutex };
				const auto             retained = Database.retain_windows(aWindows, aStop);
				if (!retained) {
					return std::unexpected(application::workbench_settings_error{ translate(retained.error()) });
				}
				return { };
			}

			[[nodiscard]] auto erase_window(core::bpr_window_id aWindow, std::stop_token aStop) -> application::workbench_settings_result<void> override
			{
				const std::scoped_lock lock{ Mutex };
				auto                   erased = Database.erase_window(aWindow, aStop);
				if (!erased) {
					return std::unexpected(application::workbench_settings_error{ translate(erased.error()) });
				}
				return { };
			}

		private:
			// [[BPR-0883]]. Held for as long as the database is open, as
			// `state_directory.hpp` promises. Declared first, released last.
			detail::state_directory             Directory;
			detail::workbench_settings_database Database;
			core::bpr_window_id                 Window;
			std::mutex                          Mutex;
		};
	} // namespace

	static auto make_store(
		const workbench_settings_configuration& aConfiguration,
		std::stop_token                         aStop)
		-> std::expected<
			std::unique_ptr<application::workbench_settings_port>,
			application::workbench_settings_failure>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(application::workbench_settings_failure::cancelled);
		}
		// The same bootstrap every other store uses, rather than creating the
		// directory here. Whichever store opens first would otherwise decide
		// what the directory is, and the weakest policy would win the race.
		auto directory = detail::open_state_directory(aConfiguration.StorageRoot);
		if (!directory) {
			return std::unexpected(translate(directory.error()));
		}

		auto opened = detail::workbench_settings_database::open(
			*directory, SettingsName, std::move(aStop));
		if (!opened) {
			return std::unexpected(translate(opened.error()));
		}

		return std::make_unique<sqlite_workbench_settings_store>(
			std::move(*directory), std::move(*opened), aConfiguration.Window);
	}
	auto make_workbench_settings_store(const workbench_settings_configuration& aConfiguration, std::stop_token aStop)
		-> std::expected<std::unique_ptr<application::workbench_settings_port>, application::workbench_settings_failure>
	{
		if (aConfiguration.Window.value() == 0) {
			return std::unexpected(application::workbench_settings_failure::unavailable);
		}
		return make_store(aConfiguration, aStop);
	}
	auto make_workbench_defaults_store(const workbench_settings_configuration& aConfiguration, std::stop_token aStop)
		-> std::expected<std::unique_ptr<application::workbench_settings_port>, application::workbench_settings_failure>
	{
		if (aConfiguration.Window.value() != 0) {
			return std::unexpected(application::workbench_settings_failure::unavailable);
		}
		return make_store(aConfiguration, aStop);
	}

} // namespace bpr::adapters::sqlite
