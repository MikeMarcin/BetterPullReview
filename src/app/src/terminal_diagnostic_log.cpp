#include "terminal_diagnostic_log.hpp"
#if defined(BPR_TERMINAL_DIAGNOSTICS)
	#include "bpr/adapters/sqlite/diagnostic_preferences.hpp"
	#include "bpr/logging/quill_log.hpp"
#endif
namespace bpr::app {
#if defined(BPR_TERMINAL_DIAGNOSTICS)
	namespace {
		/// Opens the profile's log on the first record, so a command that
		/// records nothing adds no session file.
		class terminal_diagnostic_log final : public application::diagnostic_log_port
		{
		public:
			terminal_diagnostic_log(std::filesystem::path aRoot, const application::diagnostic_preferences aPreferences)
				: Root{ std::move(aRoot) }
				, Preferences{ aPreferences }
			{
			}
			~terminal_diagnostic_log() override
			{
				if (Log) {
					Log->close();
				}
			}
			terminal_diagnostic_log(const terminal_diagnostic_log&)                    = delete;
			auto operator=(const terminal_diagnostic_log&) -> terminal_diagnostic_log& = delete;
			terminal_diagnostic_log(terminal_diagnostic_log&&)                         = delete;
			auto operator=(terminal_diagnostic_log&&) -> terminal_diagnostic_log&      = delete;

			[[nodiscard]] auto record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
			{
				if (!Log && !Failed) {
					try {
						Log = std::make_unique<adapters::logging::quill_log>(Root, adapters::logging::diagnostic_file_limits{ }, Preferences);
					} catch (...) {
						Failed = true;
					}
				}
				return Log ? Log->record(aEvent) : application::diagnostic_log_status::unavailable;
			}
			[[nodiscard]] auto status() const noexcept -> application::diagnostic_log_status override
			{
				return Log ? Log->status() : Failed ? application::diagnostic_log_status::unavailable
													: application::diagnostic_log_status::available;
			}

		private:
			std::filesystem::path                         Root;
			application::diagnostic_preferences           Preferences;
			std::unique_ptr<adapters::logging::quill_log> Log;
			bool                                          Failed = false;
		};
	}
#endif

	auto open_terminal_diagnostic_log([[maybe_unused]] const std::filesystem::path& aRoot) -> std::unique_ptr<application::diagnostic_log_port>
	{
#if defined(BPR_TERMINAL_DIAGNOSTICS)
		const auto loaded = adapters::sqlite::load_diagnostic_preferences(aRoot);
		if (!loaded.Available || !loaded.Preferences.Enabled) {
			return nullptr;
		}
		try {
			return std::make_unique<terminal_diagnostic_log>(aRoot, loaded.Preferences);
		} catch (...) {
			return nullptr;
		}
#else
		return nullptr;
#endif
	}
}
