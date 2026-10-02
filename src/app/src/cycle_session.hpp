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
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
namespace bpr::app {
	/// \brief The seed of every store operation one cycle command makes
	/// (BPR-1168): a pure function of the caller's operation identity and of
	/// which of its requests this is. The same request repeated makes the same
	/// operations, which the store returns rather than repeating; reused for
	/// another request, the store refuses them.
	[[nodiscard]] auto cycle_operation_seed(std::string_view aOperation, std::uint64_t aStep) -> std::array<std::uint8_t, 32>;

	/// \brief One `bpr cycle` command's composition (BPR-1167): this profile's
	/// own cycle store under the same Review Cycles runtime the desktop
	/// composes, the read-only workspace source and nothing else. No window,
	/// account, provider, credential, agent or network is composed.
	/// \details A terminal session holds the profile's runtime lease for its
	/// whole life. The profile owner's session holds none: the owner's own
	/// lease covers it (BPR-1168).
	class cycle_session final
	{
	public:
		/// \brief A terminal's session on a profile it holds the lease of.
		/// `aCancelled` is polled while a call is outstanding.
		cycle_session(profile_admission aAdmission, const std::array<std::uint8_t, 32>& aSeed, std::function<bool()> aCancelled);
		/// \brief The profile owner's session for one forwarded command.
		cycle_session(std::filesystem::path aRoot, core::bpr_profile_id aProfile, const std::array<std::uint8_t, 32>& aSeed, std::function<bool()> aCancelled);
		~cycle_session();
		cycle_session(const cycle_session&)                    = delete;
		auto operator=(const cycle_session&) -> cycle_session& = delete;
		cycle_session(cycle_session&&)                         = delete;
		auto operator=(cycle_session&&) -> cycle_session&      = delete;

		[[nodiscard]] auto runtime() noexcept -> application::review_cycle_runtime& { return *Runtime; }
		[[nodiscard]] auto workspace() noexcept -> application::workspace_source_port& { return *Source; }
		[[nodiscard]] auto profile() const noexcept -> core::bpr_profile_id { return Admission.Profile; }
		/// \brief The next operation identity of this command.
		[[nodiscard]] auto operation() -> application::cycle_operation_id;

		/// \brief Runs one runtime call to its completion on this thread. A
		/// cancellation shuts the runtime down, so the call completes `cancelled`.
		template <typename T, typename TStart>
		[[nodiscard]] auto await(TStart aStart) -> application::cycle_store_result<T>
		{
			auto result = std::optional<application::cycle_store_result<T>>{ };
			if (!aStart([&result](application::cycle_store_result<T> aValue) { result = std::move(aValue); })) {
				return std::unexpected(application::cycle_store_error{ .Failure = application::cycle_store_failure::cancelled });
			}
			while (!result) {
				if (Cancelled && Cancelled()) {
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
		profile_admission                                   Admission;
		std::function<bool()>                               Cancelled;
		worker_executor                                     Effects;
		terminal_decisions                                  Decisions;
		std::unique_ptr<application::workspace_source_port> Source;
		std::unique_ptr<application::review_cycle_runtime>  Runtime;
		std::array<std::uint8_t, 32>                        Seed{ };
		std::uint64_t                                       Counter = 0;
	};
}
#endif
