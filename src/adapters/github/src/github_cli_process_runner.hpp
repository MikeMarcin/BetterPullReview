#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_CLI_PROCESS_RUNNER_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_CLI_PROCESS_RUNNER_HPP

#include "github_cli_process.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>

namespace bpr::adapters::github::detail {
	/// \brief Bounds files created by a task-owned Git acquisition process.
	/// \details The kernel enforces the per-file ceiling; directory accounting
	/// is checked between bounded process waits, so it is an abort threshold.
	struct github_cli_process_file_limits
	{
		std::uint64_t         MaximumFileBytes = 0U;
		std::filesystem::path Directory;
		std::uint64_t         MaximumDirectoryBytes   = 0U;
		std::size_t           MaximumDirectoryEntries = 0U;
		// Optional task-owned stdout spool; clean success exposes no stdout bytes.
		std::filesystem::path StandardOutputFile;
		bool                  DiscardStandardOutput = false;
	};
	class github_cli_process_runner_test_seam
	{
	public:
		github_cli_process_runner_test_seam() = default;
		github_cli_process_runner_test_seam(
			const github_cli_process_runner_test_seam&) = delete;
		auto operator=(const github_cli_process_runner_test_seam&)
			-> github_cli_process_runner_test_seam&    = delete;
		virtual ~github_cli_process_runner_test_seam() = default;

		virtual auto before_execute() -> void = 0;
	};

	[[nodiscard]] auto make_github_cli_process_runner()
		-> std::unique_ptr<github_cli_process_port>;

	/// \brief Makes a production runner whose child files stay in one owned store.
	/// \details Returns null unless \p aFileLimits names an absolute store with
	/// a per-file ceiling and an abort threshold. Like the nullary factory, it
	/// never takes a test seam.
	[[nodiscard]] auto make_github_cli_git_acquisition_process_runner(
		github_cli_process_file_limits aFileLimits)
		-> std::unique_ptr<github_cli_process_port>;

	[[nodiscard]] auto make_github_cli_process_runner_for_test(
		std::shared_ptr<github_cli_process_runner_test_seam> aTestSeam)
		-> std::unique_ptr<github_cli_process_port>;
}

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_CLI_PROCESS_RUNNER_HPP
