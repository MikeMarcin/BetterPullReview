#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_RUN_POLICY_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_RUN_POLICY_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

/// \file
/// \brief The owner's agent launch policy of 2026-11-12: which agent runs
/// are fresh and which resume ([[BPR-2138]], RFC-BPR-0101), whether an
/// agent's own MCP servers load ([[BPR-2139]], RFC-BPR-0104), and how a
/// built-in prompt's instructions reach the agent ([[BPR-2140]],
/// RFC-BPR-0107). Pure: the host reads these when it builds a launch.
/// [[BPR-2304]]: and the rules every Claude launch's own arguments keep.
namespace bpr::core {
	/// \brief Whether a run keeps a conversation.
	enum class agent_session_mode : std::uint8_t {
		/// A disposable run: no native session is started or resumed and
		/// nothing is saved. Its result lands where the action is.
		fresh,
		/// A native session that keeps its history and is resumed.
		resumed,
	};
	[[nodiscard]] auto stable_name(agent_session_mode aMode) noexcept -> std::string_view;

	/// \brief Every way the app launches an agent, as the owner classified
	/// them (2026-11-12): "some of our one-off things like enhance writing
	/// buttons can be non-resumed disposable sessions. But I think a large
	/// number of our interactions do benefit from being able to reference
	/// the chat history."
	enum class agent_run_path : std::uint8_t {
		/// Enhance on a comment, a reply or a note.
		enhance_writing,
		/// Enhance on a tour stop or a tour part ([[BPR-2138]]: a proposal
		/// on the stop, never a turn of its side chat).
		tour_enhance,
		/// Generate a stop's comment; draft an empty field; suggest an outline.
		tour_field_draft,
		/// The review-feedback hook.
		review_feedback_hook,
		/// Draft Tour's first draft.
		draft_tour_first_draft,
		/// A local draft's first description.
		local_description_first_draft,
		/// A local draft's commit message.
		local_commit_message,
		/// An agent check and its probes (Settings, setup).
		agent_check,
		/// The first answer of an agent-assistance action (RFC-BPR-0116 to 0121).
		assistance_first_answer,
		/// The Agent tab conversation.
		agent_tab,
		/// A tour side chat (a stop or a part).
		tour_side_chat,
		/// The whole-tour conversation.
		tour_conversation,
		/// A local draft's discussion.
		local_draft_discussion,
		/// A self-review or fix loop.
		self_review_loop,
		/// An address-feedback run.
		address_feedback_run,
		/// [[BPR-2286]]. One round of a follow-up about a finding, a draft or
		/// a thread: fresh, its earlier rounds carried in its message.
		follow_up,
	};
	inline constexpr std::size_t agent_run_path_count = 16U;
	static_assert(static_cast<std::size_t>(agent_run_path::follow_up) + 1U == agent_run_path_count, "agent_run_path_count must cover every launch path");

	[[nodiscard]] auto stable_name(agent_run_path aPath) noexcept -> std::string_view;

	/// \brief The owner's classification: one-offs are fresh, conversations
	/// resume. Every switch over `agent_run_path` omits a `default`.
	[[nodiscard]] constexpr auto session_mode_of(const agent_run_path aPath) noexcept -> agent_session_mode
	{
		switch (aPath) {
		case agent_run_path::enhance_writing:
		case agent_run_path::tour_enhance:
		case agent_run_path::tour_field_draft:
		case agent_run_path::review_feedback_hook:
		case agent_run_path::draft_tour_first_draft:
		case agent_run_path::local_description_first_draft:
		case agent_run_path::local_commit_message:
		case agent_run_path::agent_check:
		case agent_run_path::assistance_first_answer:
		case agent_run_path::follow_up:
			return agent_session_mode::fresh;
		case agent_run_path::agent_tab:
		case agent_run_path::tour_side_chat:
		case agent_run_path::tour_conversation:
		case agent_run_path::local_draft_discussion:
		case agent_run_path::self_review_loop:
		case agent_run_path::address_feedback_run:
			return agent_session_mode::resumed;
		}
		return agent_session_mode::fresh;
	}

	/// \brief [[BPR-2138]]. The resumed path a fresh answer's Ask more
	/// promotes into, seeded with that answer; none for a fresh path that
	/// offers no conversation (an Enhance on a comment, reply or note, a
	/// commit message, a check). A tour part's Enhance continues in that
	/// part's side chat.
	[[nodiscard]] constexpr auto ask_more_path(const agent_run_path aPath) noexcept -> std::optional<agent_run_path>
	{
		switch (aPath) {
		case agent_run_path::draft_tour_first_draft:        return agent_run_path::tour_conversation;
		case agent_run_path::assistance_first_answer:       return agent_run_path::agent_tab;
		case agent_run_path::local_description_first_draft: return agent_run_path::local_draft_discussion;
		case agent_run_path::tour_enhance:                  return agent_run_path::tour_side_chat;
		case agent_run_path::enhance_writing:
		case agent_run_path::tour_field_draft:
		case agent_run_path::review_feedback_hook:
		case agent_run_path::local_commit_message:
		case agent_run_path::agent_check:
		case agent_run_path::agent_tab:
		case agent_run_path::tour_side_chat:
		case agent_run_path::tour_conversation:
		case agent_run_path::local_draft_discussion:
		case agent_run_path::self_review_loop:
		case agent_run_path::address_feedback_run:
		case agent_run_path::follow_up:
			return std::nullopt;
		}
		return std::nullopt;
	}

	/// \brief [[BPR-2139]], owner 2026-11-12: "The MCP thing seems like it's
	/// an agent config we can add to settings and default MCPs to off".
	enum class agent_mcp_servers : std::uint8_t {
		/// No MCP server loads: Claude gets an empty strict `--mcp-config`,
		/// Codex `mcp_servers={}`. The default.
		off,
		/// The agent's own configuration: BPR passes nothing for MCP. The
		/// pull request's checkout's servers still never load.
		own,
	};
	[[nodiscard]] auto stable_name(agent_mcp_servers aServers) noexcept -> std::string_view;
	[[nodiscard]] auto parse_agent_mcp_servers(std::string_view aName) noexcept -> std::optional<agent_mcp_servers>;

	/// \brief A fresh one-off keeps MCP off whatever the agent's setting says.
	[[nodiscard]] constexpr auto mcp_servers_for(const agent_run_path aPath, const agent_mcp_servers aSetting) noexcept -> agent_mcp_servers
	{
		return session_mode_of(aPath) == agent_session_mode::fresh ? agent_mcp_servers::off : aSetting;
	}

	/// \brief [[BPR-2140]], P02 settled 2026-11-12. Where a built-in prompt's
	/// instructions go.
	enum class agent_instruction_delivery : std::uint8_t {
		/// Claude's inline `--append-system-prompt`, Codex `exec`'s
		/// `developer_instructions`. The default.
		system_prompt,
		/// Pasted at the start of the message: the fallback for a wrapper
		/// that drops the system prompt.
		message,
	};
	[[nodiscard]] auto stable_name(agent_instruction_delivery aDelivery) noexcept -> std::string_view;
	[[nodiscard]] auto parse_agent_instruction_delivery(std::string_view aName) noexcept -> std::optional<agent_instruction_delivery>;

	/// \brief Instructions longer than this once quoted are pasted, so a
	/// Windows command line (32,767 characters with everything else on it)
	/// never overflows.
	inline constexpr std::size_t maximum_system_prompt_argument_bytes = 24U * 1024U;

	/// \brief [[BPR-2142]], [[BPR-2304]]. The Claude options that take
	/// several values, as Claude Code 2.1.322's help lists them. Each takes
	/// every following argument up to the next option as one more value, so
	/// one left open takes the prompt, or whatever a wrapper adds after it.
	inline constexpr std::array<std::string_view, 9> claude_variadic_options = {
		"--add-dir", "--allowedTools", "--allowed-tools", "--disallowedTools", "--disallowed-tools", "--tools", "--mcp-config", "--betas", "--file"
	};

	/// \brief [[BPR-2139]]. The MCP configuration an MCP-off Claude launch
	/// names, with `--strict-mcp-config`: no server at all.
	inline constexpr std::string_view claude_empty_mcp_configuration = R"({"mcpServers":{}})";

	/// \brief [[BPR-2304]]. What a Claude launch's own arguments must keep,
	/// beyond the rules every launch keeps.
	struct claude_argument_expectation
	{
		/// Only the user's own settings load (`--setting-sources user`): a
		/// pull request's checkout, a tool-free writing or read-only run.
		bool UserSettingsOnly = false;
		/// No MCP server loads: `--strict-mcp-config` and only the empty
		/// configuration.
		bool McpOff = false;
	};

	/// \brief [[BPR-2304]]. The first rule a Claude launch's arguments break.
	enum class claude_argument_problem : std::uint8_t {
		/// Neither `-p` nor `--print`: the run would be interactive.
		no_print_mode,
		/// `--append-system-prompt-file` or `--system-prompt-file`. Through
		/// a wrapper the file form is dropped without a word, and the agent
		/// may spend turns looking for the file; only the inline form is used.
		instructions_file,
		/// An option that takes several values is not followed by another
		/// option (RFC-BPR-0104 AC11, amended 2026-11-19: where it stands,
		/// before or after `-p`, makes no difference).
		open_variadic,
		/// A `Write(...)` permission rule with no `Edit(...)` rule for the
		/// same path. Claude applies a path-scoped write permission only
		/// through the `Edit` rule; a `Write` rule alone writes nothing.
		write_rule_without_edit,
		/// A `--setting-sources` that names a source other than `user`: the
		/// working folder's, which in a pull request's checkout is the pull
		/// request's own.
		project_settings,
		/// The launch must read only the user's settings and names no
		/// `--setting-sources user`.
		settings_not_user_only,
		/// The launch must load no MCP server and lacks the strict empty
		/// configuration, or names another one.
		mcp_not_off,
	};
	[[nodiscard]] auto stable_name(claude_argument_problem aProblem) noexcept -> std::string_view;

	/// \brief [[BPR-2304]]. Checks the arguments this app gives a Claude
	/// stream-JSON launch, before an added agent's fixed arguments go in
	/// front of them, against what the owner's harness audits established
	/// (2026-11-13 and 2026-11-19).
	///
	/// \details Pure and deterministic: the first problem in the order of
	/// `claude_argument_problem`, or none. The host refuses a launch that
	/// breaks one rather than starting it.
	[[nodiscard]] auto inspect_claude_arguments(std::span<const std::string> aArguments, claude_argument_expectation aExpected) -> std::optional<claude_argument_problem>;
} // namespace bpr::core

#endif
