#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DIFF_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DIFF_APPLICATION_HOST_HPP

#include "bpr/application/diff_line_matcher.hpp"
#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/insights_observer.hpp"
#include "bpr/application/port.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace bpr::application {
	/// \brief Provides the diff application host contract.
	class diff_application_host final
	{
	public:
		// Public operations and inspection are confined to the supplied decision
		// lane. Effect work posts immutable terminals back to that lane.
		// Referenced ports and executors remain live until is_quiescent().
		diff_application_host(
			core::diff_state                        aInitialState,
			diff_blob_port&                         aBlobs,
			effect_executor&                        aEffects,
			decision_executor&                      aDecisionLane,
			diff_projection_port&                   aProjections,
			std::shared_ptr<diff_line_matcher_port> aMatcher,
			diff_host_limits                        aLimits        = { },
			core::diff_compute_limits               aComputeLimits = { });
		~diff_application_host();

		diff_application_host(const diff_application_host&) = delete;
		auto operator=(const diff_application_host&)
			-> diff_application_host&                  = delete;
		diff_application_host(diff_application_host&&) = delete;
		auto operator=(diff_application_host&&)
			-> diff_application_host& = delete;

		void request(core::file_comparison_key aKey, core::diff_options aOptions, std::string aPath = { });
		/// \brief Replaces the comparisons to prepare in the background.
		///
		/// \details [[BPR-1327]]. In order, first most wanted. A prepared
		/// comparison is computed only while no requested comparison is
		/// outstanding, one at a time, and is stored without being published,
		/// so a later request() for it is answered from the cache. A request
		/// for the comparison being prepared waits for that preparation rather
		/// than computing it twice; a request for another stops it. An empty
		/// list stops preparation. Invalidation, cancellation of the
		/// authority and shutdown discard it.
		///
		/// [[BPR-1366]] A preparation stopped for the reviewer returns within
		/// milliseconds. [[BPR-1485]] Its work is abandoned: it is prepared
		/// again from the start, before any new preparation, and a request for
		/// it computes it from the start.
		void prepare(std::vector<prepared_diff_request> aRequests);
		void cancel();
		void invalidate(const core::pull_request_authority& aAuthority);
		void invalidate(const core::review_epoch_id& aEpoch);
		void shutdown();

		[[nodiscard]] auto state() const noexcept -> const core::diff_state&;
		[[nodiscard]] auto is_closing() const noexcept -> bool;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto outstanding_jobs() const noexcept -> std::size_t;
		[[nodiscard]] auto cache_entries() const noexcept -> std::size_t;
		[[nodiscard]] auto cache_bytes() const noexcept -> std::uint64_t;
		/// Entries and bytes of the cache held by prepared comparisons that
		/// the reviewer has not opened.
		[[nodiscard]] auto prepared_entries() const noexcept -> std::size_t;
		[[nodiscard]] auto prepared_bytes() const noexcept -> std::uint64_t;
		/// Prepared comparisons queued or being computed.
		[[nodiscard]] auto preparing() const noexcept -> std::size_t;
		/// Requests answered by a prepared comparison, from the cache or by
		/// waiting for the one being prepared.
		[[nodiscard]] auto prepared_hits() const noexcept -> std::uint64_t;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

		/// \brief Attaches a passive profile observer; existing work and admission are unchanged.
		void observe_work(std::shared_ptr<insights_observer> aObserver);

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DIFF_APPLICATION_HOST_HPP
