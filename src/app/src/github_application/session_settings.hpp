#ifndef BPR_APP_GITHUB_APPLICATION_SESSION_SETTINGS_HPP
#define BPR_APP_GITHUB_APPLICATION_SESSION_SETTINGS_HPP
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"
#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/pull_request_activity_host.hpp"
#include "bpr/application/view_filter_memory_host.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/pull_request_activity.hpp"
#include "bpr/core/view_filter_memory.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>

// Session-only and lazily opened settings stores for activity, view
// filters, appearance and workbench preferences.
namespace bpr::app {
	/// \brief [[BPR-1844]]. The remembered view filters' store under the
	/// profile's data root.
	/// [[BPR-1850]]. Activity notifications' memory under the profile root.
	class pull_request_activity_store final : public application::pull_request_activity_store_port
	{
	public:
		explicit pull_request_activity_store(std::filesystem::path aRoot);
		[[nodiscard]] auto load() const -> core::activity_memory_read;
		[[nodiscard]] auto store(const core::activity_memory& aMemory) -> bool override;

	private:
		std::filesystem::path Root;
	};

	class view_filter_memory_store final : public application::view_filter_memory_port
	{
	public:
		explicit view_filter_memory_store(std::filesystem::path aRoot);
		[[nodiscard]] auto load() const -> core::view_filter_memory_read;
		[[nodiscard]] auto store(const core::view_filter_defaults& aDefaults) -> bool override;

	private:
		std::filesystem::path Root;
	};

	/// \brief Keeps the appearance host usable without claiming persistence.
	///
	/// \details A missing record is the System-mode composed default. Writes
	/// report unavailable because the host's `persisted_settings` counter is
	/// explicitly a durable-success counter; returning success here would turn
	/// a session-only change into false persistence evidence.
	class session_only_appearance_settings final
		: public application::appearance_settings_port
	{
	public:
		[[nodiscard]] auto load(const std::stop_token aStop)
			-> application::appearance_settings_result<
				std::optional<application::appearance_settings>> override;

		[[nodiscard]] auto store(
			const application::appearance_settings&,
			const std::stop_token aStop)
			-> application::appearance_settings_result<void> override;
	};

	/// \brief Keeps the workbench usable when its durable layout cannot open.
	///
	/// \details Unlike appearance persistence evidence, the workbench host's
	/// completed-store counter is not a durable-write marker. A successful
	/// store here therefore means exactly what the fallback promises: later
	/// workbench gestures in this process observe the session's last layout,
	/// while a restart begins from the composed defaults again.
	class session_workbench_settings final
		: public application::workbench_settings_port
	{
	public:
		explicit session_workbench_settings(application::workbench_layout_settings aSettings = { });
		[[nodiscard]] auto load(const std::stop_token aStop)
			-> application::workbench_settings_result<
				application::workbench_layout_settings> override;

		[[nodiscard]] auto store(
			const application::workbench_layout_settings& aSettings,
			const std::stop_token                         aStop)
			-> application::workbench_settings_result<void> override;

	private:
		std::mutex                             Mutex;
		application::workbench_layout_settings Settings;
	};

	class lazy_workbench_defaults_port final : public application::workbench_settings_port
	{
	public:
		explicit lazy_workbench_defaults_port(std::filesystem::path aRoot);
		auto profile_scoped() const noexcept -> bool override;
		template <class Result, class Operation>
		auto perform(std::stop_token aStop, Operation aOperation) -> Result
		{
			auto store = adapters::sqlite::make_workbench_defaults_store({ .StorageRoot = Root }, aStop);
			if (!store) {
				return std::unexpected(application::workbench_settings_error{ .Failure = store.error() });
			}
			return aOperation(**store);
		}
		auto load(std::stop_token aStop) -> application::workbench_settings_result<application::workbench_layout_settings> override;
		auto store(const application::workbench_layout_settings& aSettings, std::stop_token aStop) -> application::workbench_settings_result<void> override;
		auto load_preferences(std::stop_token aStop) -> application::workbench_settings_result<application::workbench_profile_preferences> override;
		auto store_preferences(const application::workbench_profile_preferences& aSettings, std::stop_token aStop) -> application::workbench_settings_result<void> override;

	private:
		const std::filesystem::path Root;
	};
}
#endif
