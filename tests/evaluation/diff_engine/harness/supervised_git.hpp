#ifndef BPR_TESTS_EVALUATION_DIFF_ENGINE_HARNESS_SUPERVISED_GIT_HPP
#define BPR_TESTS_EVALUATION_DIFF_ENGINE_HARNESS_SUPERVISED_GIT_HPP

// [[RFC-BPR-0082]] [[BPR-1466]]. The process shape as the Git adapter runs
// it: a new session per launch, an isolated configuration, output read in
// wait slices, and on a stop the adapter's process-group kill and bounded reap.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bpr::evaluation {
	struct supervised_run
	{
		std::optional<std::string> Output;
		bool                       Stopped = false;
		/// Whether the stopped child was reaped within the bound.
		bool Reaped = true;
		/// When the run returned to its caller.
		std::chrono::steady_clock::time_point Returned;
	};

	/// \brief Runs Git once under the shared supervision.
	/// \param aStop Polled between wait slices; a set flag kills the child's
	/// process group and reaps it before the run returns.
	[[nodiscard]] auto run_supervised_git(const std::string& aGit, const std::vector<std::string>& aArguments, const std::string& aDirectory,
										  const std::atomic_bool* aStop) -> supervised_run;
} // namespace bpr::evaluation

#endif // BPR_TESTS_EVALUATION_DIFF_ENGINE_HARNESS_SUPERVISED_GIT_HPP
