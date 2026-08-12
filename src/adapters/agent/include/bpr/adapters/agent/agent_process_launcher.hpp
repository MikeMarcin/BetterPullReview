#ifndef BPR_SRC_ADAPTERS_AGENT_INCLUDE_BPR_ADAPTERS_AGENT_AGENT_PROCESS_LAUNCHER_HPP
#define BPR_SRC_ADAPTERS_AGENT_INCLUDE_BPR_ADAPTERS_AGENT_AGENT_PROCESS_LAUNCHER_HPP

#include "bpr/application/agent_launch.hpp"

#include <QObject>

#include <memory>

namespace bpr::adapters::agent {
	/// \brief Owns one bounded child-process and protocol lifecycle.
	///
	/// \details The child environment is constructed empty and receives only
	/// request pairs. Context is written only to stdin. Stdout is decoded a
	/// partial read at a time while stderr is drained independently, with fixed
	/// byte/frame ceilings. On POSIX, cancellation and timeout terminate the
	/// process group, not only the immediate child. Windows uses a suspended
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
			const application::agent_launch_request& aRequest,
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
