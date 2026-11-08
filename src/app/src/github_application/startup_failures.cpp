#include "github_application/startup_failures.hpp"

#include "bpr/app/github_application.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#if defined(BPR_BUILD_TESTING)
	#include "startup_diagnostic.hpp"
#endif

namespace bpr::app {
	namespace {
		// [[BPR-0877]]. The profile refusal a startup actually saw, as its enum
		// value, or -1 when none did. A value rather than a string because it is
		// written on a startup lane and read by whoever reports the refusal.
		std::atomic_int FirstProfileStartupRefusal{ -1 };

		// [[BPR-0882]]. The composition arm that refused a startup, with what it
		// caught in a tests-enabled graph. Guarded because one arm records from
		// a startup lane and the report reads it from another.
		std::mutex StartupFailureMutex;

		std::optional<std::string> FirstStartupFailure;
	}

	void note_profile_startup_refusal(const application::profile_startup_failure aFailure)
	{
		auto expected = -1;
		static_cast<void>(FirstProfileStartupRefusal.compare_exchange_strong(
			expected, static_cast<int>(aFailure), std::memory_order_acq_rel));
	}

	auto profile_startup_refusal_name() -> std::string
	{
		const auto recorded = FirstProfileStartupRefusal.load(std::memory_order_acquire);
		if (recorded < 0) {
			return { };
		}
		switch (static_cast<application::profile_startup_failure>(recorded)) {
		case application::profile_startup_failure::unknown_profile:      return "unknown_profile";
		case application::profile_startup_failure::invalid_registry:     return "invalid_registry";
		case application::profile_startup_failure::registry_unavailable: return "registry_unavailable";
		case application::profile_startup_failure::root_unavailable:     return "root_unavailable";
		case application::profile_startup_failure::owner_busy:           return "owner_busy";
		case application::profile_startup_failure::cancelled:            return "cancelled";
		case application::profile_startup_failure::busy:                 return "busy";
		case application::profile_startup_failure::closing:              return "closing";
		case application::profile_startup_failure::identity_exhausted:   return "identity_exhausted";
		}
		return "unknown";
	}

	auto startup_failure_source() -> std::string
	{
		const std::scoped_lock lock{ StartupFailureMutex };
		return FirstStartupFailure.value_or(std::string{ });
	}

	void detail::note_startup_failure(const std::string_view aSource, const std::exception_ptr& aCause) noexcept
	{
		try {
			const std::scoped_lock lock{ StartupFailureMutex };
			if (FirstStartupFailure) {
				return;
			}
			auto recorded = std::string{ aSource };
#if defined(BPR_BUILD_TESTING)
			if (aCause) {
				recorded += ": " + startup_failure_cause_text(aCause);
			}
#else
			// `what()` can carry a path, and a shipped build has no reader for it.
			static_cast<void>(aCause);
#endif
			FirstStartupFailure = std::move(recorded);
		} catch (...) { /* Naming a refusal must never turn it into a throw. */
		}
	}
}
