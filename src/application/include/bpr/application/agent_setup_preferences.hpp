#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SETUP_PREFERENCES_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SETUP_PREFERENCES_HPP

#include "bpr/core/agent_entry.hpp"
#include "bpr/core/agent_setup.hpp"
#include "bpr/core/agent_wrapper.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bpr::application {
	/// \brief [[BPR-PENDING-AQ2]], RFC-BPR-0092. What setup keeps for one
	/// application profile. Identities and paths only: there is no field a
	/// credential, token or environment value could occupy.
	struct agent_setup_preferences
	{
		/// The agent the reviewer chose, or setup chose and checked.
		std::optional<core::setup_agent> Chosen;
		/// Q5: the program a Codex launch runs in place of the shipped bare
		/// `codex`. Empty: search for it.
		std::string CodexProgram;
		/// The program the plain-text harness runs. Empty: none chosen; it is
		/// never searched for.
		std::string HarnessProgram;
		/// Q4: which of Codex's protocols a Codex launch speaks.
		core::codex_protocol_choice CodexProtocol = core::codex_protocol_choice::app_server;
		/// [[BPR-PENDING-AQ6]], owner look v002. The effort Claude Code runs at
		/// (`--effort`); empty: Claude's own setting. Claude's model is the
		/// profile's existing Claude › Model setting.
		std::string ClaudeEffort;
		/// The model and reasoning effort Codex runs with; empty: Codex's own.
		std::string CodexModel;
		std::string CodexEffort;
		/// [[BPR-PENDING-AQ6]], owner amendment 2026-10-23. The agents the
		/// reviewer added, in their order; the picker offers these and nothing
		/// else. `AgentsMoved` says the settings above were moved into the
		/// list once, so a removed entry does not come back.
		std::vector<core::agent_entry> Agents;
		bool                           AgentsMoved = false;
		/// The entry setup last chose; 0: none.
		std::uint32_t PreferredAgent = 0U;
		/// Owner decision 2026-10-23: a time limit on each agent run, in
		/// minutes, for every agent; 0, the default, is no limit.
		std::uint32_t RunLimitMinutes = 0U;

		auto operator<=>(const agent_setup_preferences&) const = default;
	};

	/// \brief The longest run time limit a reviewer may set: a day.
	inline constexpr std::uint32_t maximum_run_limit_minutes = 24U * 60U;

	/// \brief Whether the preferences may be stored: each program empty or
	/// storable under the wrapper rules (`core::storable_agent_wrapper`).
	[[nodiscard]] inline auto valid(const agent_setup_preferences& aPreferences) -> bool
	{
		return core::storable_agent_wrapper(aPreferences.CodexProgram) && core::storable_agent_wrapper(aPreferences.HarnessProgram)
			&& (aPreferences.ClaudeEffort.empty() || core::valid_agent_effort(core::agent_program::claude, aPreferences.ClaudeEffort))
			&& core::storable_claude_model(aPreferences.CodexModel)
			&& (aPreferences.CodexEffort.empty() || core::valid_agent_effort(core::agent_program::codex, aPreferences.CodexEffort))
			&& !core::inspect_agent_entries(aPreferences.Agents) && aPreferences.PreferredAgent <= core::maximum_agent_entry_id
			&& aPreferences.RunLimitMinutes <= maximum_run_limit_minutes;
	}

	/// \brief Where a profile's setup preferences live.
	class agent_setup_store_port
	{
	public:
		virtual ~agent_setup_store_port() = default;
		/// \brief The stored preferences, defaults when none were stored, or
		/// nothing when the store cannot be read.
		[[nodiscard]] virtual auto load() -> std::optional<agent_setup_preferences>           = 0;
		[[nodiscard]] virtual auto store(const agent_setup_preferences& aPreferences) -> bool = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SETUP_PREFERENCES_HPP
