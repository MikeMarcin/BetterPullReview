#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_PROGRAM_RESOLUTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_PROGRAM_RESOLUTION_HPP

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/claude_process_wrapper.hpp"
#include "bpr/core/agent_entry.hpp"
#include "bpr/core/agent_setup.hpp"
#include "bpr/core/agent_wrapper.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::application {
	/// \brief [[BPR-1748]], RFC-BPR-0092 Q5. Finds a program a launch
	/// with no saved path runs, by the agent adapter, each time it is asked:
	/// an absolute path to an executable regular file, or nothing. Nothing here
	/// reads the environment or touches a file.
	using agent_program_locator = std::function<std::optional<core::agent_search_candidate>(core::agent_program)>;

	/// \brief The line a Codex launch with no saved program ends with when no
	/// `codex` was found.
	inline constexpr std::string_view codex_not_found_message
		= "codex was not found on PATH or in the usual install folders; set a Codex program in Settings > Agents & context.";

	/// \brief The line a plain-text harness launch ends with when the reviewer
	/// has chosen no program for it. It is never searched for.
	inline constexpr std::string_view plain_text_harness_unset_message
		= "the plain-text harness has no program; choose one in Settings > Agents & context.";

	/// \brief The one line a refused saved program is reported in: "Codex
	/// program <path>: <reason>" or "Plain-text harness program <path>: ...".
	[[nodiscard]] auto describe_program_refusal(core::setup_agent aAgent, std::string_view aPath, core::agent_wrapper_refusal aRefusal) -> std::string;

	/// \brief [[BPR-1748]]. The program paths a window's profile saved
	/// for Codex and the plain-text harness, written on the decision lane as
	/// preferences publish and read on whichever lane runs a launch. Claude's
	/// is the process wrapper, `claude_process_wrapper_source`.
	class agent_program_source final
	{
	public:
		void               set(core::setup_agent aAgent, std::string aPath);
		[[nodiscard]] auto current(core::setup_agent aAgent) const -> std::string;

	private:
		mutable std::mutex Mutex;
		std::string        Codex;
		std::string        PlainText;
	};

	/// \brief [[BPR-1748]], RFC-BPR-0092 Q5 (owner, 2026-10-22). The
	/// one place a Codex or plain-text harness launch is given an absolute
	/// program.
	///
	/// \details The shipped profiles name the bare `codex` and
	/// `bpr-agent-terminal`, which the supervised launcher never starts: on
	/// POSIX its supervisor refuses a program that is not absolute, and on
	/// Windows `CreateProcessW` does not search for an application name. So a
	/// shipped build could not start either. This port, in front of the
	/// launcher, replaces only the executable of such a request, exactly as
	/// `wrapped_claude_process` does for Claude:
	///
	/// - Codex (either protocol): the saved Codex program, inspected again
	///   now, and refused as `failed_to_start` naming it when it cannot run;
	///   else the `codex` the locator finds now on `PATH` or in a usual
	///   folder; else `failed_to_start` with `codex_not_found_message`.
	/// - The plain-text harness: only the saved program, inspected again now;
	///   with none saved, `failed_to_start` with
	///   `plain_text_harness_unset_message`. It is never searched for.
	///
	/// A request that already names its program by path, and every Claude
	/// request, passes through unchanged. Arguments, working directory,
	/// environment, input and workspace guard are exactly as built, and the
	/// core profile and host policy never see the found path.
	class located_agent_process final : public agent_process_port
	{
	public:
		located_agent_process(agent_process_port& aInner, std::shared_ptr<const agent_program_source> aSource, agent_wrapper_inspector aInspect,
							  agent_program_locator aLocate = { });

		[[nodiscard]] auto run(const agent_launch_request& aRequest, int aTimeoutMilliseconds, const agent_output_sink& aSink = { },
							   std::stop_token aStop = { }) -> agent_run_result override;
		void               cancel() noexcept override;

	private:
		agent_process_port&                         Inner;
		std::shared_ptr<const agent_program_source> Source;
		agent_wrapper_inspector                     Inspect;
		agent_program_locator                       Locate;
	};
	/// \brief [[BPR-1753]], owner amendment 2026-10-23. The agents the
	/// reviewer added, as setup last published them, for the launches.
	class agent_entry_source final
	{
	public:
		void               set(std::vector<core::agent_entry> aEntries);
		[[nodiscard]] auto current() const -> std::vector<core::agent_entry>;

	private:
		mutable std::mutex             Mutex;
		std::vector<core::agent_entry> Entries;
	};

	inline constexpr std::string_view agent_entry_removed_message = "that agent is no longer in your list; choose another in the question field.";

	/// \brief Starts an added agent as its entry says: its own program (a
	/// popular agent's own one looked for again when it names none, and
	/// inspected again either way), and its fixed arguments before the
	/// protocol's own. A launch of any other profile goes to `aOthers`, the
	/// built-in profiles' resolution, unchanged.
	///
	/// \details The arguments are the entry's, as saved; nothing from the
	/// request is put into them and no shell runs them. A launch for an entry
	/// that is gone, or whose program cannot run, ends `failed_to_start` and
	/// starts nothing.
	class agent_entry_process final : public agent_process_port
	{
	public:
		agent_entry_process(agent_process_port& aDirect, agent_process_port& aOthers, std::shared_ptr<const agent_entry_source> aSource, agent_wrapper_inspector aInspect,
							claude_locator aClaude, agent_program_locator aLocate);

		[[nodiscard]] auto run(const agent_launch_request& aRequest, int aTimeoutMilliseconds, const agent_output_sink& aSink = { },
							   std::stop_token aStop = { }) -> agent_run_result override;
		void               cancel() noexcept override;

	private:
		agent_process_port&                       Direct;
		agent_process_port&                       Others;
		std::shared_ptr<const agent_entry_source> Source;
		agent_wrapper_inspector                   Inspect;
		claude_locator                            Claude;
		agent_program_locator                     Locate;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_PROGRAM_RESOLUTION_HPP
