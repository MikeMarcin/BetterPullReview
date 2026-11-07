#ifndef BPR_APP_GITHUB_APPLICATION_STARTUP_FAILURES_HPP
#define BPR_APP_GITHUB_APPLICATION_STARTUP_FAILURES_HPP
#include "bpr/application/profile_startup_admission.hpp"

#include <exception>
#include <optional>

// The first profile startup refusal and startup failure a run records,
// for the startup diagnostic.
namespace bpr::app {
	void note_profile_startup_refusal(const application::profile_startup_failure aFailure);
}
#endif
