#ifndef BPR_APP_GITHUB_APPLICATION_INSIGHTS_METRICS_HPP
#define BPR_APP_GITHUB_APPLICATION_INSIGHTS_METRICS_HPP
#include "bpr/application/insights_host.hpp"
#include "bpr/core/insights.hpp"

#include <optional>

// Process counters for the Insights window.
namespace bpr::app {
	class native_insights_metrics final : public application::process_metrics_port
	{
	public:
		auto read() -> std::optional<core::insights_process_counters> override;
	};
}
#endif
