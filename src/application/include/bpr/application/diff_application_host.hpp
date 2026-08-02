#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DIFF_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DIFF_APPLICATION_HOST_HPP

#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/port.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace bpr::application {
	/// \brief Provides the diff application host contract.
	class diff_application_host final
	{
	public:
		// Public operations and inspection are confined to the supplied decision
		// lane. Effect work posts immutable terminals back to that lane.
		// Referenced ports and executors remain live until is_quiescent().
		diff_application_host(
			core::diff_state          aInitialState,
			diff_blob_port&           aBlobs,
			effect_executor&          aEffects,
			decision_executor&        aDecisionLane,
			diff_projection_port&     aProjections,
			diff_host_limits          aLimits        = { },
			core::diff_compute_limits aComputeLimits = { });
		~diff_application_host();

		diff_application_host(const diff_application_host&) = delete;
		auto operator=(const diff_application_host&)
			-> diff_application_host&                  = delete;
		diff_application_host(diff_application_host&&) = delete;
		auto operator=(diff_application_host&&)
			-> diff_application_host& = delete;

		void request(core::file_comparison_key aKey, core::diff_options aOptions);
		void cancel();
		void invalidate(const core::pull_request_authority& aAuthority);
		void shutdown();

		[[nodiscard]] auto state() const noexcept -> const core::diff_state&;
		[[nodiscard]] auto is_closing() const noexcept -> bool;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto outstanding_jobs() const noexcept -> std::size_t;
		[[nodiscard]] auto cache_entries() const noexcept -> std::size_t;
		[[nodiscard]] auto cache_bytes() const noexcept -> std::uint64_t;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DIFF_APPLICATION_HOST_HPP
