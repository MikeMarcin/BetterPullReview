#ifndef BPR_SRC_ADAPTERS_AGENT_INCLUDE_BPR_ADAPTERS_AGENT_AGENT_PROCESS_LAUNCHER_HPP
#define BPR_SRC_ADAPTERS_AGENT_INCLUDE_BPR_ADAPTERS_AGENT_AGENT_PROCESS_LAUNCHER_HPP

#include "bpr/application/agent_launch.hpp"
#include "bpr/core/agent_wrapper.hpp"

#include <QObject>

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace bpr::adapters::agent {
	/// \brief [[BPR-1301]]. Whether a Claude process wrapper path names a
	/// program this user may run, under this platform's path rules.
	///
	/// \details The text is read first (`core::inspect_agent_wrapper_text`).
	/// Then the file: it must exist and, after any symbolic links, be a
	/// regular file; on Unix this user must be allowed to execute it, and on
	/// Windows it must be an `.exe`, the only kind `CreateProcessW` starts
	/// without a shell. Asked when a wrapper is saved and again before every
	/// launch through it. Reads metadata only; opens and runs nothing.
	[[nodiscard]] auto inspect_claude_process_wrapper(std::string_view aPath) -> std::optional<core::agent_wrapper_refusal>;

	/// \brief [[BPR-1301]], owner 2026-10-07. The `claude` a Claude launch
	/// with no saved wrapper runs: the first of
	/// `core::claude_search_candidates(aPathVariable, aUsualFolders, this
	/// platform)` that `inspect_claude_process_wrapper` accepts, as that
	/// absolute path.
	///
	/// \details The caller passes the application's own `PATH`; nothing here
	/// reads the environment, starts a shell or runs what it finds. Asked
	/// when Settings shows the row and again before every launch, so a
	/// `claude` removed since is not the one that runs.
	[[nodiscard]] auto locate_claude(std::string_view aPathVariable, std::span<const std::string> aUsualFolders) -> std::optional<core::claude_search_candidate>;

	/// \brief The same over this platform's `core::usual_claude_folders` for
	/// the home folder `aHome`: what a shipped launch searches.
	[[nodiscard]] auto locate_claude(std::string_view aPathVariable, std::string_view aHome) -> std::optional<core::claude_search_candidate>;

	/// \brief Owns one bounded child-process and protocol lifecycle.
	///
	/// \details The child environment is constructed empty and receives only
	/// request pairs. Context is written only to stdin. Stdout is decoded a
	/// partial read at a time while stderr is drained independently, with fixed
	/// byte/frame ceilings. On POSIX, a bundled supervisor owns the child group
	/// and terminates it on cancellation, timeout or application lifetime-channel
	/// closure, including forced application death. Windows uses a suspended
	/// native launch assigned to a kill-on-close Job Object before first resume.
	class agent_process_launcher final
		: public QObject,
		  public application::agent_process_port
	{
	public:
		explicit agent_process_launcher(QObject* aParent = nullptr);
		~agent_process_launcher() override;

		agent_process_launcher(const agent_process_launcher&)                    = delete;
		auto operator=(const agent_process_launcher&) -> agent_process_launcher& = delete;
		agent_process_launcher(agent_process_launcher&&)                         = delete;
		auto operator=(agent_process_launcher&&) -> agent_process_launcher&      = delete;

		/// \brief Runs one agent and synchronously emits complete decoded frames.
		///
		/// \details `aSink` executes before more child output is read, providing
		/// natural bounded backpressure. Its events carry the exact session ID;
		/// callers still apply core staleness policy to the terminal result.
		[[nodiscard]] auto run(
			const application::agent_launch_request& aGiven,
			int                                      aTimeoutMilliseconds,
			const application::agent_output_sink&    aSink = { },
			std::stop_token                          aStop = { })
			-> application::agent_run_result override;

		/// \brief Requests cancellation without cross-thread toolkit access.
		///
		/// \details The run loop observes the atomic request and performs all
		/// toolkit and process-tree operations on its owning thread.
		void cancel() noexcept override;

#if defined(BPR_BUILD_TESTING) && defined(Q_OS_WIN)
		/// \brief Forces the pre-resume Job Object assignment seam to fail.
		static void force_job_assignment_failure_for_testing(bool aEnabled) noexcept;
#endif

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
} // namespace bpr::adapters::agent

#endif // BPR_SRC_ADAPTERS_AGENT_INCLUDE_BPR_ADAPTERS_AGENT_AGENT_PROCESS_LAUNCHER_HPP
