#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SETUP_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SETUP_HOST_HPP

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_program_resolution.hpp"
#include "bpr/application/agent_setup_preferences.hpp"
#include "bpr/application/claude_process_wrapper.hpp"
#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/agent_entry.hpp"
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
		/// The Agent step's cards: every agent in the list, then the popular
		/// agents found and not added (`core::agent_setup_choices`).
		std::vector<core::agent_setup_choice> Choices;
		/// The agents a search looked for under the current host's policy;
		/// the none-found state names these.
		std::vector<core::setup_agent> Searched;
		agent_setup_preferences        Preferences;
		/// Detection has run at least once since setup showed.
		bool Detected  = false;
		bool Detecting = false;
		bool Checking  = false;
		/// The chosen preset uses a saved Claude wrapper, whose own status
		/// command reports sign-in for its configured provider.
		bool ChecksWrapper = false;
		/// Only preset Claude/Codex checks actually ask for sign-in status.
		bool ChecksSignIn = true;
		/// Bounded text for Show details after a failed check: the run's
		/// outcome name and its last output.
		std::string CheckDetail;
		/// Bounded, adapter-redacted workspace diagnostic text, local to this
		/// setup window and separate from the agent's output.
		std::string WorkspaceDetail;
		/// Owner look v006. What each added agent's harness said it offers,
		/// by entry id, once asked; an entry being asked is in `Cataloguing`.
		std::map<std::uint32_t, agent_harness_catalog> Catalogs;
		std::vector<std::uint32_t>                     Cataloguing;
		/// [[BPR-2051]]. The added agent a check runs on now, by entry id;
		/// none while no check runs or a check's agent is not in the list.
		std::optional<std::uint32_t> CheckingEntry;
		/// [[BPR-2177]], look ask-v003 ("The footer does not say fixture
		/// agent"). What each popular agent is called where it is named, when
		/// not its plain name: a tests-on build's fixture agent.
		std::map<core::setup_agent, std::string> Names;
		/// [[BPR-2254]]. The saved agent settings could not be read, so
		/// `Preferences` are defaults and not the reviewer's: every surface
		/// that shows agents says so, naming `SettingsFile`, and every change
		/// to them is refused rather than kept only until the app quits.
		bool        SettingsUnreadable = false;
		std::string SettingsFile;
		/// [[BPR-2256]]. No host policy has been observed for the review
		/// being set up. Every row is then refused
		/// (`core::decide_agent_invocation` permits nothing without a policy),
		/// but nothing was decided: the presentation shows the pull request
		/// still loading, not an agent refused. A host starts pending; a
		/// projection built by hand is not.
		bool PolicyPending = false;
		/// Raised by every published change.
		std::uint64_t Generation = 0U;

		auto operator<=>(const agent_setup_projection&) const = default;
	};

	using agent_setup_sink = std::function<void(const agent_setup_projection&)>;

	/// \brief [[BPR-1859]]. What Settings' Check agent is decided from,
	/// read from a published projection; `aReviewOpen` says whether a review
	/// is open whose workspace can be made.
	[[nodiscard]] auto agent_check_facts_of(const agent_setup_projection& aProjection, bool aReviewOpen) -> core::agent_check_facts;

	/// \brief The longest a status or version run may take, in milliseconds.
	inline constexpr int agent_status_timeout_milliseconds = 15'000;

	/// \brief The longest a check's details may be, in bytes.
	inline constexpr std::size_t maximum_agent_check_detail_bytes = 4U * 1024U;

	/// \brief Everything one setup host uses, named.
	struct agent_setup_dependencies
	{
		agent_setup_store_port* Store = nullptr;
		/// [[BPR-2254]]. The store's file as the reviewer finds it, named
		/// when it cannot be read.
		std::string StoreFile;
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
		/// [[BPR-2177]]. Names the popular agents go by (`agent_setup_projection::Names`).
		std::map<core::setup_agent, std::string> Names;
		/// Metadata reads and store writes.
		effect_executor* Effects = nullptr;
		/// The check's runs, which may take their whole bound: a lane of
		/// their own, so they never hold up other effects.
		effect_executor*   Checks   = nullptr;
		decision_executor* Decision = nullptr;
		agent_setup_sink   Publish;
		/// [[BPR-1828]]. Each check's start and end, as `agent.setup_check`
		/// records of its kind, typed failure, last run outcome, exit code,
		/// run count and duration; never its program, output or detail.
		/// Optional.
		std::shared_ptr<diagnostic_log_port> Log;
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
		/// \brief The current review's host policy, as soon as its host is
		/// known; absent when that host maps none, which refuses every agent.
		void observe_policy(std::optional<core::agent_host_policy> aPolicy);
		/// \brief [[BPR-2256]]. No review's host is known (none is open):
		/// no policy, and none observed, so no row reads as refused by one.
		void forget_policy();
		/// Looks for the agents again (Q2, and Look again).
		void detect();
		void choose(core::setup_agent aAgent);
		/// \brief Chooses an agent already in the list for setup, as Custom
		/// agent… chooses the one it adds: its kind is chosen, the checks start
		/// over and the agents are looked for again with its program. An entry
		/// not in the list, or the one already chosen, changes nothing.
		void choose_entry(std::uint32_t aId);
		/// Runs Check it works' agent row.
		void check();
		/// \brief [[BPR-2051]], owner decision 2026-11-11 ("A + footer 1").
		/// Checks every added agent in turn, each as Check it works checks the
		/// chosen one, and keeps each one's result for the Agents roster. The
		/// chosen agent stays chosen.
		void check_all();
		/// \brief [[BPR-2051]]. Checks one added agent, from its sheet.
		void check_entry(std::uint32_t aId);
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
		/// \brief RFC-BPR-0104 as amended 2026-11-11. The scratch folder
		/// limit in MiB and the days an unused folder is kept (0: until
		/// cleared). Answers whether they were taken.
		[[nodiscard]] auto set_scratch_policy(std::uint32_t aLimitMiB, std::uint32_t aKeepDays) -> bool;
		/// \brief Owner look v004: the Agent tab's Suggestions toggle, kept.
		void set_suggestions_hidden(bool aHidden);
		/// \brief RFC-BPR-0126 Q4/Q5: Queue (false, the default) or Steer.
		void set_follow_up_steer(bool aSteer);
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
		void set_workspace_check(core::setup_check aCheck, std::string aDetail = { });
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
