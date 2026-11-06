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
		/// [[BPR-1857]]. The observed canonical Git executable whose directory
		/// this acquisition may search. Empty keeps the ordinary empty-PATH rule
		/// on Windows; no ambient search path is ever admitted.
		std::filesystem::path LocalGitExecutable = { };
		/// [[BPR-1879]]. A kept clone's runner: its launches have no time
		/// limit; the reviewer's Cancel ends them.
		bool ManagedClone = false;
	};

	/// \brief [[BPR-1879]], owner decision 2026-11-05 ("I don't really care
	/// about a limit, a cancel and a progress would be better"). A kept
	/// clone's download has no time limit. The supervisor still needs a time
	/// point it can add its reap bound to without overflowing, so this is
	/// fifty years: never reached. Cancel is checked every wait slice.
	inline constexpr auto github_cli_managed_clone_no_deadline = std::chrono::hours{ 24 * 365 * 50 };
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

		/// \brief Stalls a test worker after its real child has started.
		/// \details Production factories never supply a seam. This exercises
		/// watchdog quarantine while credential-bearing descendants are alive.
		virtual auto after_started() -> void { }

		/// \brief Windows only: makes the next launch's standard-handle list
		/// fail to build.
		/// \details [[BPR-1114]]. The launch's standard error handle is
		/// withheld, as a null standard handle would be, so the runner takes
		/// the refusal it would take for a list it cannot build. Consulted once
		/// per execution, after `before_execute`.
		[[nodiscard]] virtual auto fail_standard_handle_list() -> bool
		{
			return false;
		}

		/// \brief Stands in for a child that survives SIGKILL.
		/// \details [[BPR-1436]]. The runner's kill-and-reap and background
		/// reap send no signal, so the child outlives both bounds and the
		/// runner must abandon it. Consulted at each kill. The test kills the
		/// child itself afterwards.
		[[nodiscard]] virtual auto child_survives_kill() -> bool
		{
			return false;
		}
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

	/// \brief [[BPR-1879]]. Makes the runner for one kept clone's download.
	/// \details No byte or time limit (owner decision 2026-11-05): no
	/// per-file ceiling, no store threshold, and no deadline; Cancel and the
	/// bytes-received progress are the controls. The same supervision,
	/// cancellation, write confinement (the request's own), credential
	/// handling and Git search path as every acquisition runner. Never takes a
	/// test seam.
	[[nodiscard]] auto make_github_cli_managed_clone_process_runner(const std::filesystem::path& aGit)
		-> std::unique_ptr<github_cli_process_port>;

	[[nodiscard]] auto make_github_cli_process_runner_for_test(
		std::shared_ptr<github_cli_process_runner_test_seam> aTestSeam)
		-> std::unique_ptr<github_cli_process_port>;
}

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_CLI_PROCESS_RUNNER_HPP
