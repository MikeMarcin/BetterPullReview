#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_RUN_DIAGNOSIS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_RUN_DIAGNOSIS_HPP

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/diagnostic_log_port.hpp"

#include <array>
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
	/// failure when there was one, the exit code when the process exited,
	/// how many bytes the agent wrote to each stream and, for a writing run
	/// whose Claude reported loading plugins, their names ([[BPR-1891]]):
	/// bounded tokens that are never a path; and `memory=on` when it
	/// reported an auto-memory folder, never the folder ([[BPR-1896]]);
	/// `thinking=on` when it wrote a thinking block, never its content
	/// ([[BPR-1899]]); and for a read-only Claude run how many shell commands
	/// outside the read-only list ran, never which ([[BPR-1912]]); and for a
	/// run whose custom profile opted out of strict run checks, `relaxed=on`
	/// and how many times each relaxed check let something pass, never what
	/// ([[BPR-1918]]).
	struct agent_run_diagnosis
	{
		core::agent_protocol                  Protocol = core::agent_protocol::claude_stream_json;
		std::optional<agent_run_outcome>      Outcome;
		std::optional<agent_protocol_failure> Failure;
		std::optional<int>                    ExitCode;
		std::uint64_t                         StandardOutputBytes = 0U;
		std::uint64_t                         StandardErrorBytes  = 0U;
		/// [[BPR-1891]]. As `agent_run_result::ReportedPlugins` carries them.
		std::string Plugins;
		/// [[BPR-1896]]. As `agent_run_result::ReportedAutoMemory` says.
		bool AutoMemory = false;
		/// [[BPR-1899]]. As `agent_run_result::ReportedThinking` says.
		bool Thinking = false;
		/// [[BPR-1912]]. A read-only Claude run's count of shell commands
		/// outside the read-only list that ran; absent for any other run.
		/// Never the commands: they can hold the repository's paths and
		/// search terms.
		std::optional<std::uint64_t> OutsideList;
		/// [[BPR-1918]]. A relaxed run's counts by `relaxed_check`; absent
		/// for a strict run.
		std::optional<std::array<std::uint64_t, relaxed_check_count>> Relaxed;
		/// [[BPR-2004]], RFC-BPR-0103. The helpers the runtime reported, by
		/// how they ended; absent when it reported none.
		std::optional<core::agent_helper_tally> Helpers;
		/// The longest time between two frames, in milliseconds; absent
		/// when none was measured.
		std::optional<std::uint64_t> LongestSilenceMs;
		/// [[BPR-2261]]. How long the turn was held after its answer for a
		/// report back's part, and the bound; absent when it was not held.
		std::optional<std::uint64_t> HeldMs;
		std::uint64_t                HeldBoundMs = 0U;
		/// [[BPR-2261]]. The bound ended the held turn on its answer.
		bool HeldOut = false;
		/// [[BPR-2261]]. Stopped after a successful result with no helper
		/// out: the run stands as answered.
		bool StoppedAfterAnswer = false;
		/// The frame types the decoder read past, comma separated; never
		/// their content.
		std::string ReadPast;
		/// [[BPR-2305]]. How many warnings the runtime wrote; absent when
		/// none. Never their text: it can name the person's own paths.
		std::optional<std::uint64_t> Warnings;
		/// [[BPR-2305]]. How many file-change paths failed and how many were
		/// written, when any failed. Never the paths.
		std::optional<std::uint64_t> WritesFailed;
		std::uint64_t                WritesWritten = 0U;

		auto operator<=>(const agent_run_diagnosis&) const = default;
	};

	/// \brief The line's exact text, e.g. `protocol=claude_stream_json
	/// outcome=protocol_failure failure=frame_too_large exit=0
	/// stdout_bytes=223211 stderr_bytes=0`, then
	/// `plugins=cc-plugin-sec-default@builtin` when plugins were reported,
	/// then `memory=on` when an auto-memory folder was, then `thinking=on`
	/// when a thinking block was, then `outside_list=N` for a read-only
	/// Claude run ([[BPR-1912]]), then `relaxed=on relaxed_init=N
	/// relaxed_tool=N relaxed_block=N relaxed_skill=N` for a relaxed run
	/// ([[BPR-1918]]), then `helpers_started=N helpers_finished=N
	/// helpers_failed=N helpers_abandoned=N` when helpers were reported,
	/// `longest_silence_ms=N`, `held_ms=N held_bound_ms=N` when the turn was
	/// held after its answer, `held_out=on` when the bound ended it and
	/// `stopped_after_answer=on` when a stop came after the answer
	/// ([[BPR-2261]]), `read_past=system/post_turn_summary,...`
	/// ([[BPR-2004]]), then `warnings=N` when the runtime wrote warnings and
	/// `writes_failed=N writes_written=N` when a file change failed
	/// ([[BPR-2305]]).
	[[nodiscard]] auto describe(const agent_run_diagnosis& aDiagnosis) -> std::string;
	/// \brief Reads exactly what `describe` writes, and nothing else.
	[[nodiscard]] auto parse_agent_run_diagnosis(std::string_view aText) -> std::optional<agent_run_diagnosis>;
	/// \brief The diagnostic outcome an ended run is recorded under.
	/// \brief [[BPR-2365]]. Accepts only a fixed catalog diagnostic line and
	/// returns its event outcome. No program, output or model names are admitted.
	[[nodiscard]] auto agent_catalog_outcome(std::string_view aText) -> std::optional<diagnostic_outcome>;

	/// \brief Records the terminal facts of one Codex model catalog run.
	void record_agent_catalog(diagnostic_log_port* aLog, core::agent_protocol aProtocol, const agent_run_result& aResult,
							  std::uint32_t aDurationMilliseconds, std::size_t aModelCount) noexcept;

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

		[[nodiscard]] auto native_connections_quiescent() const noexcept -> bool override { return Process.native_connections_quiescent(); }
		/// \brief [[BPR-2153]]. Steering goes to the connection the inner
		/// port keeps; nothing is started.
		[[nodiscard]] auto steer_support(const agent_steer_request& aRequest) const -> core::agent_steer_support override { return Process.steer_support(aRequest); }
		void               steer(agent_steer_request aRequest, agent_steer_answer aAnswer) override { Process.steer(std::move(aRequest), std::move(aAnswer)); }

	private:
		void record(diagnostic_outcome aOutcome, const agent_run_diagnosis& aDiagnosis) noexcept;

		agent_process_port&                  Process;
		std::shared_ptr<diagnostic_log_port> Log;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_RUN_DIAGNOSIS_HPP
