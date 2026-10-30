#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_RUN_DIAGNOSIS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_RUN_DIAGNOSIS_HPP

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/diagnostic_log_port.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

namespace bpr::application {
	/// \brief [[BPR-1827]]. One agent run diagnostic line: enums and counts only.
	///
	/// \details An agent reads a private review, so nothing here names a
	/// program, profile, repository, path, prompt or answer. A started line
	/// has no outcome; an ended line names the run's outcome, the decoder
	/// failure when there was one, the exit code when the process exited, and
	/// how many bytes the agent wrote to each stream.
	struct agent_run_diagnosis
	{
		core::agent_protocol                  Protocol = core::agent_protocol::claude_stream_json;
		std::optional<agent_run_outcome>      Outcome;
		std::optional<agent_protocol_failure> Failure;
		std::optional<int>                    ExitCode;
		std::uint64_t                         StandardOutputBytes = 0U;
		std::uint64_t                         StandardErrorBytes  = 0U;

		auto operator<=>(const agent_run_diagnosis&) const = default;
	};

	/// \brief The line's exact text, e.g. `protocol=claude_stream_json
	/// outcome=protocol_failure failure=frame_too_large exit=0
	/// stdout_bytes=223211 stderr_bytes=0`.
	[[nodiscard]] auto describe(const agent_run_diagnosis& aDiagnosis) -> std::string;
	/// \brief Reads exactly what `describe` writes, and nothing else.
	[[nodiscard]] auto parse_agent_run_diagnosis(std::string_view aText) -> std::optional<agent_run_diagnosis>;
	/// \brief The diagnostic outcome an ended run is recorded under.
	[[nodiscard]] auto diagnostic_outcome_of(agent_run_outcome aOutcome) noexcept -> diagnostic_outcome;

	/// \brief [[BPR-1827]]. Records every run it forwards as an
	/// `agent.run` started line and an ended line.
	///
	/// \details A decorator over the process port, so every caller of the
	/// port, the Agent tab and every other surface, leaves the same trace. A
	/// diagnostic that cannot be recorded changes nothing about the run.
	class diagnosed_agent_process final : public agent_process_port
	{
	public:
		diagnosed_agent_process(agent_process_port& aProcess, std::shared_ptr<diagnostic_log_port> aLog);

		[[nodiscard]] auto run(
			const agent_launch_request& aRequest,
			int                         aTimeoutMilliseconds,
			const agent_output_sink&    aSink = { },
			std::stop_token             aStop = { }) -> agent_run_result override;
		void cancel() noexcept override;

	private:
		void record(diagnostic_outcome aOutcome, const agent_run_diagnosis& aDiagnosis) noexcept;

		agent_process_port&                  Process;
		std::shared_ptr<diagnostic_log_port> Log;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_RUN_DIAGNOSIS_HPP
