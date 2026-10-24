#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SETUP_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SETUP_HOST_HPP

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_program_resolution.hpp"
#include "bpr/application/agent_setup_preferences.hpp"
#include "bpr/application/claude_process_wrapper.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/agent_profile.hpp"
#include "bpr/core/agent_setup.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bpr::application {
	/// \brief What the presentation is given, as values.
	struct agent_setup_projection
	{
		core::agent_setup_state State;
		core::setup_step        Step = core::setup_step::agent;
		/// The agents a search looked for under the current host's policy;
		/// the none-found state names these.
		std::vector<core::setup_agent> Searched;
		agent_setup_preferences        Preferences;
		/// Detection has run at least once since setup showed.
		bool Detected  = false;
		bool Detecting = false;
		bool Checking  = false;
		/// Bounded text for Show details after a failed check: the run's
		/// outcome name and its last output.
		std::string CheckDetail;
		/// Owner look v006. What each added agent's harness said it offers,
		/// by entry id, once asked; an entry being asked is in `Cataloguing`.
		std::map<std::uint32_t, agent_harness_catalog> Catalogs;
		std::vector<std::uint32_t>                     Cataloguing;
		/// Raised by every published change.
		std::uint64_t Generation = 0U;

		auto operator<=>(const agent_setup_projection&) const = default;
	};

	using agent_setup_sink = std::function<void(const agent_setup_projection&)>;

	/// \brief The longest a status or version run may take, in milliseconds.
	inline constexpr int agent_status_timeout_milliseconds = 15'000;

	/// \brief The longest a check's details may be, in bytes.
	inline constexpr std::size_t maximum_agent_check_detail_bytes = 4U * 1024U;

	/// \brief Everything one setup host uses, named.
	struct agent_setup_dependencies
	{
		agent_setup_store_port* Store = nullptr;
		/// The check's own process port, never the agent host's.
		agent_process_port*                                  Process = nullptr;
		agent_program_locator                                Locate;
		agent_wrapper_inspector                              Inspect;
		std::shared_ptr<const claude_process_wrapper_source> ClaudeWrapper;
		/// Written whenever the saved programs change, so every launch of the
		/// window runs what setup saved.
		std::shared_ptr<agent_program_source> Programs;
		environment_reader                    Parent;
		/// [[BPR-1753]]. The profile's Claude › Model, moved into the
		/// Claude Code entry once.
		std::function<std::string()>     LegacyClaudeModel;
		std::vector<core::agent_profile> Profiles;
		/// Metadata reads and store writes.
		effect_executor* Effects = nullptr;
		/// The check's runs, which may take their whole bound: a lane of
		/// their own, so they never hold up other effects.
		effect_executor*   Checks   = nullptr;
		decision_executor* Decision = nullptr;
		agent_setup_sink   Publish;
	};

	/// \brief [[BPR-1749]], RFC-BPR-0092 M2. The host half of agent
	/// setup: detection, the bounded status check and the saved choice.
	///
	/// \details State lives on the decision lane. Detection (Q2) reads file
	/// metadata on the effect lane and starts no process: the saved Claude
	/// wrapper, Codex program and harness program are inspected again, and
	/// `claude` and `codex` are looked for when none is saved. The check
	/// (Q3 A) runs the chosen agent's own status command and `--version` on
	/// the check lane through the host's own process port, each bounded by
	/// `agent_status_timeout_milliseconds`, with no review context; Stop
	/// cancels it. The plain-text harness has no status command: its check
	/// passes when its program can be run. The core decides rows, the choice
	/// and the step; this host never rewrites a verdict.
	///
	/// The choice is saved when the reviewer makes it and when a check of an
	/// agent setup chose passes. Shut down, then wait for quiescence, before
	/// the store, the process port or any lane ends.
	class agent_setup_host final
	{
	public:
		explicit agent_setup_host(agent_setup_dependencies aDependencies);
		~agent_setup_host();
		agent_setup_host(const agent_setup_host&)                    = delete;
		auto operator=(const agent_setup_host&) -> agent_setup_host& = delete;

		/// Reads the stored preferences and publishes the saved programs.
		void load();
		/// The current review's host policy; absent when no review is open.
		void observe_policy(std::optional<core::agent_host_policy> aPolicy);
		/// Looks for the agents again (Q2, and Look again).
		void detect();
		void choose(core::setup_agent aAgent);
		/// Runs Check it works' agent row.
		void check();
		void stop();
		/// Saves a program path for Codex or the plain-text harness after the
		/// adapter inspected it; empty clears it. Answers on the decision lane.
		void set_program(core::setup_agent aAgent, std::string aPath, std::function<void(std::optional<core::agent_wrapper_refusal>)> aAnswered = { });
		void set_codex_protocol(core::codex_protocol_choice aChoice);
		/// \brief [[BPR-1753]], owner look v002. Saves the model and
		/// effort a Codex launch runs with, or the effort of a Claude launch
		/// (Claude's model has its own setting); empty is the agent's own
		/// default. Text the agent's command line would not take is refused
		/// and nothing changes.
		[[nodiscard]] auto set_run_settings(core::setup_agent aAgent, std::string aModel, std::string aEffort) -> bool;
		/// RFC-BPR-0092 Repository: the repository has a linked clone or a
		/// recorded choice of the private copy.
		/// \brief [[BPR-1753]], owner amendment 2026-10-23. The agent
		/// list: add (and, with `aPrefer`, choose it for setup), edit, remove,
		/// reorder and choose. A refused change answers why and keeps nothing.
		using agent_entry_answer = std::function<void(std::optional<core::agent_entry_refusal>)>;
		void add_agent(core::agent_entry aEntry, bool aPrefer = false, agent_entry_answer aAnswered = { });
		void update_agent(core::agent_entry aEntry, agent_entry_answer aAnswered = { });
		void remove_agent(std::uint32_t aId);
		void move_agent(std::uint32_t aId, int aDelta);
		void prefer_agent(std::uint32_t aId);
		/// \brief Owner decision 2026-10-23: the run time limit in minutes,
		/// 0 for none. Answers whether it was taken.
		[[nodiscard]] auto set_run_limit(std::uint32_t aMinutes) -> bool;
		/// \brief Owner look v004: the Agent tab's Suggestions toggle, kept.
		void set_suggestions_hidden(bool aHidden);
		/// \brief Owner look v006 ("Codex is totally missing its model
		/// settings"). Asks an added agent's harness what it offers, once per
		/// window: a Codex protocol's `model/list` through its own program and
		/// fixed arguments, or Claude Code's `--help` for its effort levels.
		/// Nothing is asked of a plain-text harness or an agent already asked.
		void load_catalog(std::uint32_t aEntry);
		/// \brief Owner look v006 ("reset agents which clears all settings and
		/// goes back to the wizard fresh"). Every added agent, its settings and
		/// the setup's choices go back to their defaults, and the setup starts
		/// again at Agent; the list is not moved in from the old settings again.
		void reset();
		void set_repository_chosen(bool aChosen);
		/// RFC-BPR-0092 Check it works: the worktree row's result.
		void set_workspace_check(core::setup_check aCheck);
		/// Asks for setup again: the chosen agent stays selected, the checks
		/// start over and detection runs again.
		void restart();

		[[nodiscard]] auto projection() const -> agent_setup_projection;
		void               shutdown() noexcept;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SETUP_HOST_HPP
