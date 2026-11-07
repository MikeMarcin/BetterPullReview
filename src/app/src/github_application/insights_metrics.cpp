#include "github_application/insights_metrics.hpp"

#include "bpr/adapters/process_support/process_supervision.hpp"

#include <chrono>

namespace bpr::app {
	auto native_insights_metrics::read() -> std::optional<core::insights_process_counters>
	{
		const auto counters = adapters::process_support::self_process_counters();
		if (!counters) {
			return std::nullopt;
		}
		core::insights_process_counters result;
		result.SteadyMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		result.UnixMicroseconds   = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		result.CpuMicroseconds    = counters->CpuMicroseconds;
		result.Wakeups            = counters->Wakeups;
		return result;
	}
}
