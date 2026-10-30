#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_HOST_OPERATION_DIAGNOSTICS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_HOST_OPERATION_DIAGNOSTICS_HPP

#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/markdown_image_ports.hpp"
#include "bpr/core/markdown_image.hpp"
#include "bpr/core/operation_diagnosis.hpp"
#include "bpr/core/source_search.hpp"

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <tuple>

namespace bpr::application {
	/// \brief [[BPR-1831]]. Records a high-frequency operation's ends without
	/// flooding the log.
	///
	/// \details Lines are grouped by action, failure and store. The first line
	/// of a group is recorded at once; after that the group records at most
	/// one line per window, whose `Count` is how many ends it stands for
	/// (those held back since the group's last line, and this one) and whose
	/// duration is the longest of them. `flush` records what is held back; the
	/// destructor flushes. The groups are a product of closed enums, so the
	/// table stays small. Thread-safe: a fetch completes on any thread.
	/// Recording never blocks on the log beyond its own `record`, which drops
	/// under backpressure like any other record.
	class aggregated_operation_log
	{
	public:
		using clock = std::chrono::steady_clock;

		explicit aggregated_operation_log(std::shared_ptr<diagnostic_log_port> aLog, clock::duration aWindow = std::chrono::minutes{ 1 });
		~aggregated_operation_log();
		aggregated_operation_log(const aggregated_operation_log&)                    = delete;
		auto operator=(const aggregated_operation_log&) -> aggregated_operation_log& = delete;

		/// \brief One end; its own `Count` is ignored.
		void record(const core::operation_diagnosis& aDiagnosis) noexcept;
		void record(const core::operation_diagnosis& aDiagnosis, clock::time_point aNow) noexcept;
		/// \brief Records every group's held-back ends.
		void flush() noexcept;
		void flush(clock::time_point aNow) noexcept;

	private:
		using key = std::tuple<core::operation_action, core::operation_failure, std::optional<core::operation_store>>;
		struct group
		{
			std::optional<clock::time_point> Recorded;
			std::uint32_t                    Held    = 0U;
			std::uint32_t                    Longest = 0U;
			std::uint32_t                    Schema  = 0U;
		};

		std::shared_ptr<diagnostic_log_port> Log;
		clock::duration                      Window;
		std::mutex                           Mutex;
		std::map<key, group>                 Groups;
	};

	/// \brief [[BPR-1831]]. How a source search terminal is logged: complete,
	/// partial and result-limited searches succeed; a cancelled one is a
	/// cancellation; the rest fail with their reason.
	[[nodiscard]] auto operation_failure_of(core::source_search_status aStatus) noexcept -> core::operation_failure;
	/// \brief [[BPR-1831]]. The failure an image fetch ended in.
	[[nodiscard]] auto operation_failure_of(core::markdown_image_state aState) noexcept -> core::operation_failure;

	/// \brief [[BPR-1831]]. Wraps the profile's image fetch port so every body
	/// image and avatar fetch leaves an aggregated `image.fetch` record.
	///
	/// \details A fetch is `avatar_fetch` when its policy keeps every hop to
	/// the avatar origins, `image_fetch` otherwise. Its end is `succeeded`,
	/// `cancelled` when its stop was requested, or `failed` with the state
	/// the fetch ended the image in. No address, host, login or byte reaches
	/// a line. The request, stop and result pass through unchanged.
	class diagnosed_image_fetch final : public markdown_image_fetch_port
	{
	public:
		diagnosed_image_fetch(markdown_image_fetch_port& aFetch, std::shared_ptr<aggregated_operation_log> aLog);

		void fetch(markdown_image_fetch_request aRequest, std::stop_token aStop, std::function<void(markdown_image_fetch_result)> aCompletion) override;

	private:
		markdown_image_fetch_port&                Fetch;
		std::shared_ptr<aggregated_operation_log> Log;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_HOST_OPERATION_DIAGNOSTICS_HPP
