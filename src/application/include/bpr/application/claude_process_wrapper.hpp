#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_CLAUDE_PROCESS_WRAPPER_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_CLAUDE_PROCESS_WRAPPER_HPP

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/workbench_preferences_host.hpp"
#include "bpr/core/agent_wrapper.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

namespace bpr::application {
	/// \brief [[BPR-1301]]. Asks whether a path names a program this user
	/// may run. The agent adapter answers it; nothing here touches a file.
	using agent_wrapper_inspector = std::function<std::optional<core::agent_wrapper_refusal>(std::string_view)>;

	/// \brief [[BPR-1301]], owner 2026-10-07. Finds the `claude` a launch
	/// with no saved wrapper runs: an absolute path to an executable regular
	/// file, or nothing. The agent adapter answers it from the application's
	/// `PATH` and home folder each time it is asked; nothing here reads the
	/// environment or touches a file.
	using claude_locator = std::function<std::optional<core::claude_search_candidate>()>;

	/// \brief [[BPR-1301]]. The one line a refused wrapper is reported in:
	/// "Claude process wrapper <path>: <reason>".
	[[nodiscard]] auto describe_wrapper_refusal(std::string_view aPath, core::agent_wrapper_refusal aRefusal) -> std::string;

	/// \brief [[BPR-1301]]. The line a Claude launch with no wrapper ends
	/// with when no `claude` was found.
	inline constexpr std::string_view claude_not_found_message
		= "claude was not found on PATH or in the usual install folders; set a Claude process wrapper in Settings > Agents & context.";

	/// \brief [[BPR-1301]]. The wrapper a window's profile has saved, written
	/// on the decision lane as preferences publish and read on whichever
	/// lane runs a launch.
	class claude_process_wrapper_source final
	{
	public:
		void               set(std::string aPath);
		[[nodiscard]] auto current() const -> std::string;

	private:
		mutable std::mutex Mutex;
		std::string        Path;
	};

	/// \brief [[BPR-1301]]. The one place a Claude launch is pointed at the
	/// profile's wrapper.
	///
	/// \details Every Claude stream-JSON request this port is given runs the
	/// saved wrapper in place of its executable, with the request otherwise
	/// exactly as built: arguments, working directory, environment, input
	/// and workspace guard. The path is inspected again first; a wrapper
	/// removed or no longer runnable ends the request as `failed_to_start`
	/// naming it, and nothing is started, the bare `claude` included.
	///
	/// With no wrapper saved, a request that names a bare program (the
	/// shipped profile's `claude`) runs the `claude` the locator finds now,
	/// on `PATH` or in a usual install folder (owner, 2026-10-07, "Yes,
	/// search PATH (Recommended)"); none found ends the request as
	/// `failed_to_start` with `claude_not_found_message` and starts nothing.
	/// A request that already names an absolute program, and every request
	/// when no locator is given, passes through unchanged, as does every
	/// other protocol. The core profile and host policy never see the
	/// wrapper or the found path, so every verdict is the one the profile
	/// already had.
	class wrapped_claude_process final : public agent_process_port
	{
	public:
		wrapped_claude_process(agent_process_port& aInner, std::shared_ptr<const claude_process_wrapper_source> aSource,
							   agent_wrapper_inspector aInspect, claude_locator aLocate = { });

		[[nodiscard]] auto run(const agent_launch_request& aRequest, int aTimeoutMilliseconds, const agent_output_sink& aSink = { },
							   std::stop_token aStop = { }) -> agent_run_result override;
		void               cancel() noexcept override;

	private:
		agent_process_port&                                  Inner;
		std::shared_ptr<const claude_process_wrapper_source> Source;
		agent_wrapper_inspector                              Inspect;
		claude_locator                                       Locate;
	};

	/// \brief [[BPR-1301]]. What became of one Save or Clear.
	struct claude_wrapper_answer
	{
		std::string                                Path;
		std::optional<core::agent_wrapper_refusal> Refusal;
		/// The preferences host took the write. False with no refusal means
		/// the profile's preferences were not available to write.
		bool Submitted = false;
	};

	/// \brief [[BPR-1301]]. Saves a wrapper only after inspecting it.
	///
	/// \details The inspection runs on the effect lane and its answer comes
	/// back on the decision lane, where an accepted path is handed to the
	/// profile's preferences host. A refused path is not saved. Empty text
	/// clears the wrapper without inspection. Only the latest request is
	/// answered. Shut down, then wait for quiescence, before the preferences
	/// host or either lane ends.
	///
	/// It also asks, the same way, which `claude` a launch would run with no
	/// wrapper saved, so Settings can show it.
	class claude_wrapper_editor final
	{
	public:
		using answered = std::function<void(const claude_wrapper_answer&)>;
		using located  = std::function<void(const std::optional<core::claude_search_candidate>&)>;

		claude_wrapper_editor(workbench_preferences_host& aPreferences, effect_executor& aEffects, decision_executor& aDecision,
							  agent_wrapper_inspector aInspect, claude_locator aLocate = { });
		~claude_wrapper_editor();
		claude_wrapper_editor(const claude_wrapper_editor&)                    = delete;
		auto operator=(const claude_wrapper_editor&) -> claude_wrapper_editor& = delete;

		void choose(std::string aPath, answered aAnswered);
		/// Looks for `claude` on the effect lane and answers on the decision
		/// lane; only the latest look is answered. With no locator it answers
		/// "not found" at once.
		void locate(located aLocated);
		void shutdown() noexcept;
		/// No inspection or look is waiting to be answered on the decision lane.
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_CLAUDE_PROCESS_WRAPPER_HPP
