#ifndef BPR_APP_CYCLE_SESSION_HPP
#define BPR_APP_CYCLE_SESSION_HPP
#include "bpr/app/worker_executor.hpp"
#include "bpr/application/review_cycle_runtime.hpp"
#include "bpr/application/workspace_source_port.hpp"
#include "profile_admission.hpp"
#include "tour_decisions.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
namespace bpr::app {
	[[nodiscard]] auto cycle_command_cancelled() noexcept -> bool;

	/// \brief One `bpr cycle` command's composition (BPR-1167): this profile's
	/// own cycle store under the same Review Cycles runtime the desktop
	/// composes, the read-only workspace source and nothing else. No window,
	/// account, provider, credential, agent or network is composed.
	/// \details It holds the profile's runtime lease for its whole life; a
	/// command waiting on a person destroys it first and opens another after.
	class cycle_session final
	{
	public:
		/// \brief Admits the profile and takes its lease. A profile another
		/// process owns is refused: no second writer is opened.
		[[nodiscard]] static auto open(const std::filesystem::path& aBase, const std::string& aProfile) -> tour_cli_result<std::unique_ptr<cycle_session>>;
		~cycle_session();
		cycle_session(const cycle_session&)                    = delete;
		auto operator=(const cycle_session&) -> cycle_session& = delete;
		cycle_session(cycle_session&&)                         = delete;
		auto operator=(cycle_session&&) -> cycle_session&      = delete;

		[[nodiscard]] auto runtime() noexcept -> application::review_cycle_runtime& { return *Runtime; }
		[[nodiscard]] auto workspace() noexcept -> application::workspace_source_port& { return *Source; }
		[[nodiscard]] auto profile() const noexcept -> core::bpr_profile_id { return Admission.Profile; }
		/// \brief A fresh operation identity: never repeated across runs of the profile.
		[[nodiscard]] auto operation() -> application::cycle_operation_id;

		/// \brief Runs one runtime call to its completion on this thread. An
		/// interrupt shuts the runtime down, so the call completes `cancelled`.
		template <typename T, typename TStart>
		[[nodiscard]] auto await(TStart aStart) -> application::cycle_store_result<T>
		{
			auto result = std::optional<application::cycle_store_result<T>>{ };
			if (!aStart([&result](application::cycle_store_result<T> aValue) { result = std::move(aValue); })) {
				return std::unexpected(application::cycle_store_error{ .Failure = application::cycle_store_failure::cancelled });
			}
			while (!result) {
				if (cycle_command_cancelled()) {
					Runtime->shutdown();
				}
				Decisions.drain();
				if (!result) {
					std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
				}
			}
			return std::move(*result);
		}

	private:
		explicit cycle_session(profile_admission aAdmission);

		profile_admission                                   Admission;
		worker_executor                                     Effects;
		terminal_decisions                                  Decisions;
		std::unique_ptr<application::workspace_source_port> Source;
		std::unique_ptr<application::review_cycle_runtime>  Runtime;
		std::array<std::uint8_t, 32>                        Seed{ };
		std::uint64_t                                       Counter = 0;
	};
}
#endif
