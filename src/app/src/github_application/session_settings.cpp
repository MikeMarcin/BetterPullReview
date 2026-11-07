#include "github_application/session_settings.hpp"

#include "bpr/adapters/sqlite/pull_request_activity_store.hpp"
#include "bpr/adapters/sqlite/view_filter_memory_store.hpp"

namespace bpr::app {
	pull_request_activity_store::pull_request_activity_store(std::filesystem::path aRoot)
		: Root{ std::move(aRoot) }
	{ }

	auto pull_request_activity_store::load() const -> core::activity_memory_read { return adapters::sqlite::load_pull_request_activity(Root); }

	auto pull_request_activity_store::store(const core::activity_memory& aMemory) -> bool { return adapters::sqlite::store_pull_request_activity(Root, aMemory); }

	view_filter_memory_store::view_filter_memory_store(std::filesystem::path aRoot)
		: Root{ std::move(aRoot) }
	{ }

	auto view_filter_memory_store::load() const -> core::view_filter_memory_read { return adapters::sqlite::load_view_filter_memory(Root); }

	auto view_filter_memory_store::store(const core::view_filter_defaults& aDefaults) -> bool { return adapters::sqlite::store_view_filter_memory(Root, aDefaults); }

	auto session_only_appearance_settings::load(const std::stop_token aStop)
		-> application::appearance_settings_result<
			std::optional<application::appearance_settings>>
	{
		if (aStop.stop_requested()) {
			return std::unexpected{
				application::appearance_settings_error{
					.Failure = application::appearance_settings_failure::cancelled,
				}
			};
		}
		return std::nullopt;
	}

	auto session_only_appearance_settings::store(
		const application::appearance_settings&,
		const std::stop_token aStop)
		-> application::appearance_settings_result<void>
	{
		return std::unexpected{ application::appearance_settings_error{
			.Failure = aStop.stop_requested()
				? application::appearance_settings_failure::cancelled
				: application::appearance_settings_failure::unavailable,
		} };
	}

	session_workbench_settings::session_workbench_settings(application::workbench_layout_settings aSettings)
		: Settings{ std::move(aSettings) }
	{ }

	auto session_workbench_settings::load(const std::stop_token aStop)
		-> application::workbench_settings_result<
			application::workbench_layout_settings>
	{
		if (aStop.stop_requested()) {
			return std::unexpected{ application::workbench_settings_error{
				.Failure = application::workbench_settings_failure::cancelled,
			} };
		}
		const auto lock = std::scoped_lock{ Mutex };
		return Settings;
	}

	auto session_workbench_settings::store(
		const application::workbench_layout_settings& aSettings,
		const std::stop_token                         aStop)
		-> application::workbench_settings_result<void>
	{
		if (aStop.stop_requested()) {
			return std::unexpected{ application::workbench_settings_error{
				.Failure = application::workbench_settings_failure::cancelled,
			} };
		}
		const auto lock = std::scoped_lock{ Mutex };
		Settings        = aSettings;
		return { };
	}

	lazy_workbench_defaults_port::lazy_workbench_defaults_port(std::filesystem::path aRoot)
		: Root{ std::move(aRoot) }
	{ }

	auto lazy_workbench_defaults_port::profile_scoped() const noexcept -> bool { return true; }

	auto lazy_workbench_defaults_port::load(std::stop_token aStop) -> application::workbench_settings_result<application::workbench_layout_settings>
	{
		return perform<application::workbench_settings_result<application::workbench_layout_settings>>(aStop, [aStop](auto& aStore) { return aStore.load(aStop); });
	}

	auto lazy_workbench_defaults_port::store(const application::workbench_layout_settings& aSettings, std::stop_token aStop) -> application::workbench_settings_result<void>
	{
		return perform<application::workbench_settings_result<void>>(aStop, [&](auto& aStore) { return aStore.store(aSettings, aStop); });
	}

	auto lazy_workbench_defaults_port::load_preferences(std::stop_token aStop) -> application::workbench_settings_result<application::workbench_profile_preferences>
	{
		return perform<application::workbench_settings_result<application::workbench_profile_preferences>>(aStop, [aStop](auto& aStore) { return aStore.load_preferences(aStop); });
	}

	auto lazy_workbench_defaults_port::store_preferences(const application::workbench_profile_preferences& aSettings, std::stop_token aStop) -> application::workbench_settings_result<void>
	{
		return perform<application::workbench_settings_result<void>>(aStop, [&](auto& aStore) { return aStore.store_preferences(aSettings, aStop); });
	}
}
