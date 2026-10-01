#ifndef BPR_SRC_APP_PROFILE_WINDOW_LAYOUT_PORT_HPP
#define BPR_SRC_APP_PROFILE_WINDOW_LAYOUT_PORT_HPP

#include "bpr/adapters/sqlite/workbench_settings_store.hpp"

#include <atomic>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>
#include <vector>

namespace bpr::app {
	/// \brief Serializes window storage; each adapter lifetime stays inside an effect call.
	class profile_window_layout_service final
	{
	public:
		profile_window_layout_service(std::filesystem::path aRoot, std::vector<core::bpr_window_id> aKeep)
			: Root{ std::move(aRoot) }
			, Keep{ std::move(aKeep) }
		{ }
		template <class Result, class Operation>
		auto perform(core::bpr_window_id aWindow, std::stop_token aStop, Operation aOperation) -> Result
		{
			const auto lock = std::scoped_lock{ Mutex };
			if (!Reconciled) {
				auto defaults = adapters::sqlite::make_workbench_defaults_store({ .StorageRoot = Root }, aStop);
				if (!defaults) {
					return std::unexpected(application::workbench_settings_error{ .Failure = defaults.error() });
				}
				auto retained = (*defaults)->retain_windows(Keep, aStop);
				if (!retained) {
					return std::unexpected(retained.error());
				}
				Reconciled = true;
			}
			auto store = adapters::sqlite::make_workbench_settings_store({ .StorageRoot = Root, .Window = aWindow }, aStop);
			if (!store) {
				return std::unexpected(application::workbench_settings_error{ .Failure = store.error() });
			}
			return aOperation(**store);
		}
		auto erase(core::bpr_window_id aWindow) -> application::workbench_settings_result<void>
		{
			return perform<application::workbench_settings_result<void>>(aWindow, { }, [aWindow](auto& aStore) { return aStore.erase_window(aWindow, { }); });
		}

	private:
		std::filesystem::path                  Root;
		const std::vector<core::bpr_window_id> Keep;
		std::mutex                             Mutex;
		bool                                   Reconciled = false;
	};

	/// \brief Routes one window layout and observes its serialized store results.
	/// \details The workbench host admits one store at a time and drains its terminal
	/// before shutdown inspects the result. In-flight stores retain the last result.
	class profile_window_layout_port final : public application::workbench_settings_port
	{
	public:
		profile_window_layout_port(std::shared_ptr<profile_window_layout_service> aService, core::bpr_window_id aWindow)
			: Service{ std::move(aService) }
			, Window{ aWindow }
		{ }
		auto window() const noexcept -> core::bpr_window_id override { return Window; }
		auto profile_scoped() const noexcept -> bool override { return true; }
		auto load(std::stop_token aStop) -> application::workbench_settings_result<application::workbench_layout_settings> override
		{
			return Service->perform<application::workbench_settings_result<application::workbench_layout_settings>>(Window, aStop, [aStop](auto& aStore) { return aStore.load(aStop); });
		}
		auto store(const application::workbench_layout_settings& aSettings, std::stop_token aStop) -> application::workbench_settings_result<void> override
		{
			InFlight.fetch_add(1U, std::memory_order_acq_rel);
			Attempted.fetch_add(1U, std::memory_order_acq_rel);
			const auto result = perform_store(aSettings, aStop);
			LastFailure.store(result ? -1 : static_cast<int>(result.error().Failure), std::memory_order_release);
			if (result) {
				Completed.fetch_add(1U, std::memory_order_acq_rel);
			}
			InFlight.fetch_sub(1U, std::memory_order_acq_rel);
			return result;
		}
		/// \brief Whether the most recently finished store refused; false before any result.
		auto write_failed() const noexcept -> bool { return last_write_failure().has_value(); }
		/// \brief Stores entered but not yet returned, independent of the last result.
		auto writes_in_flight() const noexcept -> std::size_t { return InFlight.load(std::memory_order_acquire); }
		// BPR-0875 counters retain their meaning: attempts and successful stores.
		// Their difference includes refusals, so it is not an in-flight count.
		auto writes_attempted() const noexcept -> std::size_t { return Attempted.load(std::memory_order_acquire); }
		auto writes_completed() const noexcept -> std::size_t { return Completed.load(std::memory_order_acquire); }
		/// \brief The latest finished store's refusal, cleared by a later success.
		auto last_write_failure() const noexcept -> std::optional<application::workbench_settings_failure>
		{
			const auto recorded = LastFailure.load(std::memory_order_acquire);
			return recorded < 0 ? std::nullopt
								: std::optional{ static_cast<application::workbench_settings_failure>(recorded) };
		}

	private:
		// [[BPR-1262]]. Each terminal is returned where it is built. A
		// reassigned expected<void, E> read as uninitialized to GCC 15 at -O3,
		// as it did in the appearance host ([[BPR-0816]]).
		auto perform_store(const application::workbench_layout_settings& aSettings, std::stop_token aStop) -> application::workbench_settings_result<void>
		{
			try {
				return Service->perform<application::workbench_settings_result<void>>(Window, aStop, [&](auto& aStore) { return aStore.store(aSettings, aStop); });
			} catch (...) {
				// The host maps a throwing port to unavailable; return the same
				// terminal here so cleanup cannot mistake the exception for success.
				return std::unexpected(application::workbench_settings_error{ .Failure = application::workbench_settings_failure::unavailable });
			}
		}

		std::shared_ptr<profile_window_layout_service> Service;
		core::bpr_window_id                            Window;
		std::atomic_size_t                             InFlight    = 0U;
		std::atomic_size_t                             Attempted   = 0U;
		std::atomic_size_t                             Completed   = 0U;
		std::atomic_int                                LastFailure = -1;
	};
} // namespace bpr::app

#endif
