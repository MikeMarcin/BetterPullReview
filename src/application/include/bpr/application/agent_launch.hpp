#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_LAUNCH_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_LAUNCH_HPP

#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/agent_workspace_lifetime_guard.hpp"
#include "bpr/application/review_ledger_port.hpp"
#include "bpr/core/agent_answer_findings.hpp"
#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/agent_finding.hpp"
#include "bpr/core/agent_profile.hpp"
#include "bpr/core/agent_setup.hpp"
#include "bpr/core/agent_turn.hpp"
#include "bpr/core/review_composition.hpp"
#include "bpr/core/review_round_trip.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::application {
	/// \brief One current file receipt supplied to context construction.
	struct agent_file_context_input
	{
		std::string        Path;
		core::file_receipt Receipt;
		bool               ChangedSinceReview = false;

		auto operator<=>(const agent_file_context_input&) const = default;
	};

	/// \brief Exact identity and revision of one explicitly selected draft.
	struct agent_draft_choice
	{
		core::review_draft_id Id;
		std::uint64_t         Revision = 0U;

		auto operator<=>(const agent_draft_choice&) const = default;
	};

	/// \brief The current unpublished draft observed by the application host.
	///
	/// \details Prose remains in the host observation and never enters the Qt
	/// intent. The choice must match both identity and revision at dispatch.
	struct agent_draft_observation
	{
		agent_draft_choice         Choice;
		core::review_comment_draft Draft;

		auto operator<=>(const agent_draft_observation&) const = default;
	};

	/// \brief Bounded observations approved for one agent invocation.
	///
	/// \details `Private` is the BPR-0238 value supplied by the caller. This
	/// type owns no ledger and performs no load. Notes cross only when their IDs
	/// also appear in `IncludedNotes`; drafts are explicit input values.
	struct agent_context_observation
	{
		core::application_session_id                     ApplicationSession;
		core::agent_revision_binding                     Binding;
		std::string                                      PullRequestUrl;
		std::optional<core::git_commit_oid>              PreviousUpload;
		std::vector<agent_file_context_input>            Files;
		private_ledger_observation                       Private;
		std::optional<core::supplement_semantic_context> Selection;
		std::optional<agent_draft_observation>           CurrentDraft;
		std::vector<local_note_id>                       IncludedNotes;
		std::optional<agent_draft_choice>                IncludedDraft;
		/// What the reviewer typed for this turn, when this is a chat turn.
		///
		/// \details Empty for every other invocation, and empty is what the
		/// supplement then carries. Keyed to a session here would be keyed to
		/// the wrong one: the launch path allocates the session identity, so
		/// `build_agent_context_supplement` is where the message is bound to
		/// the session it is actually being sent under.
		std::string ChatMessage;
		/// Which reviewer gesture this invocation is for.
		///
		/// \details BPR-0533. The launch panel's start and the side chat's
		/// send are both `ask_about_selection`; a comment-workshop press is
		/// `workshop_comment`. It is here rather than inferred from the fields
		/// below because the entry point is what decides whether an operation
		/// may be carried at all, and a rule that read "an operation is a
		/// workshop turn because an operation is present" would admit itself.
		core::agent_entry_point EntryPoint
			= core::agent_entry_point::ask_about_selection;
		/// Which of the six comment-workshop operations was asked for.
		///
		/// \details Absent for every invocation that is not a workshop press,
		/// and absent is what the supplement then carries. Present under an
		/// entry point that does not admit it is refused here rather than
		/// dropped: an operation quietly removed would ask the agent to revise
		/// a comment without saying how, and an operation quietly kept under a
		/// review would be a workshop request nobody made.
		std::optional<core::workshop_operation> WorkshopOperation;
		/// [[BPR-1091]]. The review-feedback guidance a workshop press sends.
		///
		/// \details Filled by the composition from
		/// `workshop_feedback_supplement` for a press only, and absent while
		/// the label helper is off. Present without an admitted workshop
		/// operation it is refused whole, as an operation riding on another
		/// gesture is.
		std::optional<core::supplement_feedback_guidance> FeedbackGuidance;
		/// [[BPR-1752]], RFC-BPR-0092. The review brief, already within
		/// its bounds (`core::bound_review_brief`); absent for a launch that is
		/// not a review request. The host sets its workspace kind once the
		/// workspace is made.
		std::optional<core::supplement_brief> Brief;
		/// [[BPR-1773]]. The conversation an `ask_about_conversation` turn
		/// is about, already within its bounds (`core::bound_conversation`);
		/// absent for every other launch.
		std::optional<core::supplement_conversation> Conversation;
		/// [[BPR-1916]]. Skills the prompt named, carried inline
		/// (`application::inline_skills`); empty for every launch that named
		/// none.
		std::vector<core::supplement_skill> Skills;
		/// [[BPR-1935]]. A read-only tour run's composed prompt: the whole
		/// request the tour session sends, built by the application and never
		/// by typed text alone. It travels in its own carrier
		/// (`core::agent_tour_prompt`, bounded by
		/// `core::maximum_tour_prompt_bytes`), not in `ChatMessage`, whose
		/// bound is for what a reviewer types. Empty for every other launch.
		std::string TourPrompt;

		auto operator<=>(const agent_context_observation&) const = default;
	};

	enum class agent_context_failure : std::uint8_t {
		invalid_observation,
		selected_note_missing,
		duplicate_selected_note,
		selected_draft_missing,
		selected_draft_stale,
		selected_draft_invalid,
		/// [[BPR-1935]]. A tour prompt longer than its carrier's bound.
		too_large,
		/// [[BPR-1935]]. A read-only tour run with no tour prompt.
		tour_prompt_missing,
		/// [[BPR-1935]]. A tour prompt on a launch that is not a read-only
		/// tour run.
		tour_prompt_outside_tour_run,
	};

	/// \brief [[BPR-1935]]. The stable name an invocation's refusal carries
	/// for a context failure: `agent_context_too_large`,
	/// `tour_prompt_missing`, `tour_prompt_outside_tour_run`, and
	/// `invalid_agent_context` for every other failure.
	[[nodiscard]] auto refusal_name(agent_context_failure aFailure) noexcept -> std::string_view;

	/// \brief What kind of launch a context is built for.
	enum class agent_context_run : std::uint8_t {
		/// Every launch that is not a read-only tour run.
		ordinary,
		/// [[BPR-1935]]. A read-only tour run ([[BPR-1900]]): the only launch
		/// a tour prompt is admitted on, and one that must carry one.
		read_only_tour,
	};

	/// \brief Builds context only from caller-supplied bounded observations.
	///
	/// \details [[BPR-1935]]. `aRun` says whether this is a read-only tour
	/// run. A tour prompt on any other launch is refused as
	/// `tour_prompt_outside_tour_run`, a tour run without one as
	/// `tour_prompt_missing`, and one over its bound as `too_large`.
	[[nodiscard]] auto build_agent_context_supplement(
		const agent_context_observation& aObservation,
		const core::agent_session&       aSession,
		agent_context_run                aRun = agent_context_run::ordinary)
		-> std::expected<core::agent_context_supplement, agent_context_failure>;

	/// \brief One store-validated full body, ordered exactly as the admitted selection.
	struct agent_private_source_body
	{
		core::private_source_reference              Source;
		std::string                                 Body;
		std::optional<core::retained_agent_finding> Finding = { };
	};
	[[nodiscard]] auto serialize_private_finding(const core::retained_agent_finding&) -> std::string;
	/// \brief [[BPR-1918]]. Whether this app ends a writing or read-only
	/// run on a check that a wrapper's or policy's settings can trip: extra
	/// tools or MCP servers in init, a tool_use outside the list, a block
	/// that is not text in a writing answer, and [[BPR-1916]]'s unreferenced
	/// skill. `relaxed` is a custom agent profile's opt-out: each such check
	/// reports what it would have refused (`check_relaxed`, and a
	/// `tool_activity` noted `relaxed_check_note`) and the run goes on. The
	/// restrictive arguments are passed either way, the workspace stays the
	/// private disposable one, and only text blocks ever become output.
	enum class agent_run_checks : std::uint8_t {
		strict,
		relaxed,
	};

	/// \brief Host admission snapshot and explicit settings for one invocation.
	/// The host persists AdmittedRun before dispatch; the builder verifies correlation,
	/// never allocates identities. Legacy callers omit it until durable host integration.
	struct agent_launch_options
	{
		core::agent_setting_facts                 Requested;
		core::agent_entry_point                   EntryPoint = core::agent_entry_point::ask_about_selection;
		std::optional<core::agent_run_provenance> AdmittedRun;
		std::vector<agent_private_source_body>    PrivateSources;
		bool                                      WritingOnly = false;
		/// [[BPR-1900]]. A tour drafting run: the workspace, with read-only
		/// tools only (`claude_read_only_restrictions`,
		/// `codex_read_only_arguments`).
		bool ReadOnlyTools = false;
		/// [[BPR-1918]]. Strict unless a custom profile opted out.
		agent_run_checks RunChecks = agent_run_checks::strict;
	};

	/// \brief Effect-lane source claim, released after the private input write or refusal.
	class agent_input_claim
	{
	public:
		virtual ~agent_input_claim()                                  = default;
		[[nodiscard]] virtual auto permitted() const noexcept -> bool = 0;
		virtual void               release() noexcept                 = 0;
	};
	/// \brief Owner look v006 ([[BPR-1753]]): what a run is for. A
	/// catalog run asks a Codex App Server for its models (`model/list`) and
	/// nothing else: no thread, no turn, no context.
	enum class agent_launch_purpose : std::uint8_t {
		conversation,
		model_catalog,
	};

	class agent_native_connection_port;

	/// \brief What one agent process is started with.
	struct agent_launch_request
	{
		core::agent_protocol     Protocol = core::agent_protocol::external_terminal;
		core::agent_session_id   Session;
		std::string              Executable;
		std::vector<std::string> Arguments;
		/// Explicit accepted workspace; the launcher never inherits host cwd.
		std::string WorkingDirectory;
		/// Materialised from the profile's environment-name allowlist.
		std::vector<std::pair<std::string, std::string>> Environment;
		/// Initial protocol input; complete context for non-App-Server modes.
		std::string Input;
		/// App Server context held until its initialized thread is identified.
		std::string DeferredContext;
		/// Present only for a workspace with durable recovery ownership. A
		/// requested guard must be retained or launch refuses before execution.
		std::optional<agent_workspace_lifetime_guard> WorkspaceLifetimeGuard;
		core::agent_run_id                            Run;
		core::agent_runtime_metadata                  Metadata;
		std::optional<core::agent_run_provenance>     AdmittedRun;
		bool                                          WritingOnly = false;
		std::shared_ptr<agent_input_claim>            PrivateInputClaim;
		/// [[BPR-1753]]. The profile launched, so an added agent's own
		/// program and fixed arguments are found for its slot.
		core::agent_profile_id Profile;
		agent_launch_purpose   Purpose = agent_launch_purpose::conversation;
		/// [[BPR-1900]]. The decoder admits only read-only tool uses.
		bool ReadOnlyTools = false;
		/// [[BPR-1918]]. How the decoder treats what a wrapper can trip.
		agent_run_checks RunChecks = agent_run_checks::strict;
		/// Durable host identity; empty keeps the one-shot protocol contract.
		std::string NativeConversation;
		/// Exact backend handle, never a most-recent lookup or diagnostic field.
		std::string   NativeHandle;
		std::uint64_t NativeTurn      = 2U;
		bool          NativeConnected = false;
		/// Adapter-owned exchange on its process thread; never supplied by Qt.
		agent_native_connection_port* NativeConnection = nullptr;
		/// Durably stores a newly observed handle before the question is written.
		std::shared_ptr<std::function<bool(const std::string&)>> SaveNativeHandle;

		auto operator<=>(const agent_launch_request&) const = default;
	};

	using environment_reader
		= std::function<std::optional<std::string>(const std::string&)>;

	[[nodiscard]] auto materialize_agent_environment(
		const core::agent_profile& aProfile,
		const environment_reader&  aParent)
		-> std::vector<std::pair<std::string, std::string>>;

	enum class agent_launch_failure : std::uint8_t {
		invalid_profile,
		profile_session_mismatch,
		unsupported_supplement_schema,
		invalid_runtime_settings,
		unsupported_runtime_settings,
		invalid_run_snapshot,
		input_too_large,
	};

	/// \brief [[BPR-1375]]. The settings a tool-free writing launch gives
	/// Claude, as one `--settings` argument.
	///
	/// \details No hook runs, auto-memory neither loads nor writes, and the
	/// three plugins Claude Code 2.1.296 builds in stay off (one of them
	/// otherwise starts a shell in the working directory). Turning that
	/// plugin off is the one use of its name the architecture boundary
	/// admits here.
	/// [[BPR-1891]]. Turning them off is hardening, not a gate. A plugin a
	/// later Claude builds in, or one policy holds on, still loads: the init
	/// frame is still refused for any tool or MCP server it brings, and the
	/// plugins it reports are named in the run's diagnostics rather than
	/// refused.
	/// [[BPR-1896]]. A wrapper may replace these settings outright: Claude
	/// keeps only the last `--settings` it is given, so a wrapper that adds
	/// its own drops all of this one (hooks, auto-memory and the plugins come
	/// back on). The init check enforces only tools and MCP servers; an
	/// auto-memory folder is reported as `memory=on`, not refused.
	inline constexpr std::string_view claude_writing_settings
		= R"({"disableAllHooks":true,"autoMemoryEnabled":false,"enabledPlugins":{"cc-plugin-agents-md@builtin":false,"cc-plugin-telemetry@builtin":false,"cc-plugin-plugin-authoring@builtin":false}})";

	/// \brief [[BPR-1375]]. What every tool-free writing launch appends to
	/// Claude's print-mode arguments.
	///
	/// \details Owner, 2026-10-09: writing runs the installed `claude -p` on
	/// the person's own sign-in. So there is no `--bare`, which reads only
	/// `ANTHROPIC_API_KEY` or an `apiKeyHelper` and never the Claude account
	/// sign-in. What `--bare` also skipped is held off another way: no tools,
	/// every tool denied, an empty strict MCP configuration (which also
	/// keeps the claude.ai connectors out), no settings file of any source,
	/// so no project or user `CLAUDE.md`, `.claude` settings or `.mcp.json`,
	/// no skills or slash commands, no session saved, and
	/// `claude_writing_settings`. The process also starts in an empty
	/// folder of its own rather than the review workspace.
	/// [[BPR-1896]]. A wrapper can drop or replace any of these, so a writing
	/// run is only as tool-free as the wrapper keeps it. What is enforced is
	/// read back from Claude's init frame: no tool and no MCP server.
	inline constexpr std::array<std::string_view, 13> claude_writing_restrictions = {
		"--tools", "", "--disallowedTools", "*", "--strict-mcp-config", "--mcp-config", R"({"mcpServers":{}})", "--setting-sources", "",
		"--disable-slash-commands", "--no-session-persistence", "--settings", claude_writing_settings
	};

	/// \brief [[BPR-1896]]. The variable a tool-free writing launch adds to
	/// Claude's environment; a chat launch never carries it.
	///
	/// \details Claude Code reads it over any settings value, so auto-memory
	/// stays off when a wrapper replaces `claude_writing_settings` but passes
	/// the environment through. Hardening, not a gate: a wrapper that drops
	/// it turns memory back on, and the init frame then reports it.
	inline constexpr std::pair<std::string_view, std::string_view> claude_writing_environment{ "CLAUDE_CODE_DISABLE_AUTO_MEMORY", "1" };

	/// \brief [[BPR-1900]], RFC-BPR-0092 amendment. What a read-only tour
	/// drafting launch appends to Claude's print-mode arguments.
	///
	/// \details The run starts in the review's private workspace. Only Read,
	/// Grep, Glob and Bash are loaded (`--tools`); Read, Grep, Glob and the
	/// read-only shell commands are approved (`--allowedTools`), so in print
	/// mode every other use is denied without a prompt; edits, writes, the
	/// network and subagents are named again in `--disallowedTools`. No MCP
	/// server, no settings file of any source, no slash commands, no session
	/// saved, and `claude_writing_settings`. The decoder also refuses an init
	/// frame that lists another tool or any MCP server, and any tool use
	/// `core::decide_read_only_tool` refuses (`tool_refused`).
	inline constexpr std::array<std::string_view, 17> claude_read_only_restrictions = {
		"--tools", "Read,Grep,Glob,Bash", "--allowedTools",
		"Read,Grep,Glob,Bash(rg:*),Bash(grep:*),Bash(ls:*),Bash(cat:*),Bash(head:*),Bash(tail:*),Bash(wc:*),Bash(nl:*),Bash(find:*),Bash(git log:*),Bash(git show:*),Bash(git blame:*),Bash(git diff:*),Bash(git grep:*),Bash(git ls-files:*)",
		"--disallowedTools", "Edit,Write,MultiEdit,NotebookEdit,WebFetch,WebSearch,Task", "--permission-mode", "default", "--strict-mcp-config", "--mcp-config", R"({"mcpServers":{}})",
		"--setting-sources", "", "--disable-slash-commands", "--no-session-persistence", "--settings", claude_writing_settings
	};

	/// \brief [[BPR-1900]]. What a read-only tour drafting launch gives
	/// `codex exec --json`, before its `-`: Codex's own read-only sandbox (no
	/// writes, no network), no approval prompts, no MCP server and no web
	/// search.
	inline constexpr std::array<std::string_view, 8> codex_read_only_arguments = {
		"--sandbox", "read-only", "-c", "approval_policy=\"never\"", "-c", "mcp_servers={}", "-c", "tools.web_search=false"
	};

	/// \brief Builds one protocol-specific process request.
	[[nodiscard]] auto build_agent_launch(
		const core::agent_profile&            aProfile,
		const core::agent_context_supplement& aSupplement,
		const environment_reader&             aParent,
		const agent_launch_options&           aOptions = { })
		-> std::expected<agent_launch_request, agent_launch_failure>;

	[[nodiscard]] inline auto build_agent_launch(
		const core::agent_profile&            aProfile,
		const core::agent_context_supplement& aSupplement,
		const environment_reader&             aParent,
		bool                                  aWritingOnly)
		-> std::expected<agent_launch_request, agent_launch_failure>
	{
		return build_agent_launch(
			aProfile, aSupplement, aParent, agent_launch_options{ .WritingOnly = aWritingOnly });
	}

	/// \brief [[BPR-1180]]. The launch of one Send to Agent: the profile's
	/// protocol and executable, its allowed environment, `aWorkingDirectory`,
	/// and the round's readable document as the only input, framed as the
	/// protocol frames a user message (Claude stream JSON, Codex App Server)
	/// or as the prompt itself (Codex JSONL, external terminal). A named
	/// session is continued only by a harness that can (`--resume`); nothing
	/// else is added to what the profile already launches with.
	[[nodiscard]] auto build_round_launch(const core::agent_profile& aProfile, std::string_view aDocument, const std::optional<std::string>& aContinue, std::string aWorkingDirectory,
										  const environment_reader& aParent) -> std::expected<agent_launch_request, agent_launch_failure>;

	/// \brief Configures one native turn or an exact-ID reconnect without replay.
	[[nodiscard]] auto prepare_native_agent_turn(agent_launch_request aRequest, bool aConnected)
		-> std::expected<agent_launch_request, agent_launch_failure>;

	/// \brief Renders every supplement field with explicit count/presence data.
	[[nodiscard]] auto encode_agent_supplement(
		const core::agent_context_supplement& aSupplement) -> std::string;

	inline constexpr std::size_t maximum_agent_input_bytes = 8U * 1024U * 1024U;
	/// \brief Validates encoded stdin including reserved App Server follow-ups.
	[[nodiscard]] auto admit_agent_input(const agent_launch_request& aRequest)
		-> std::expected<std::size_t, agent_launch_failure>;

	inline constexpr std::size_t maximum_agent_stdout_bytes = 1U * 1024U * 1024U;
	inline constexpr std::size_t maximum_agent_stderr_bytes = 1U * 1024U * 1024U;
	inline constexpr std::size_t maximum_agent_frame_bytes  = 64U * 1024U;
	/// \brief [[BPR-1827]]. Claude Code's frame and stream ceilings.
	///
	/// \details Claude Code echoes every tool result into its transcript: a
	/// `user` frame whose `tool_result` content and `tool_use_result` both
	/// carry what the tool read, so one read of a 90 KB source file is a
	/// 110 KB line and a review that reads a dozen files writes several
	/// megabytes. The generic 64 KiB frame and 1 MiB stream ceilings refused
	/// every such run as `frame_too_large` before it could answer. A frame is
	/// parsed and dropped, never retained, so these bound one line's working
	/// memory and the run's total, not what the result keeps; retained
	/// assistant text keeps its own `maximum_agent_stdout_bytes` ceiling.
	inline constexpr std::size_t maximum_claude_frame_bytes  = 16U * 1024U * 1024U;
	inline constexpr std::size_t maximum_claude_stdout_bytes = 256U * 1024U * 1024U;
	/// \brief [[BPR-1827]]. How much of the first line the decoder refused it
	/// keeps, for the reviewer's log; never written to diagnostics.
	inline constexpr std::size_t maximum_agent_rejected_line_bytes = 512U;
	/// \brief [[BPR-1891]]. How many plugins a writing run's diagnostics
	/// name; the rest are counted.
	inline constexpr std::size_t maximum_reported_plugins = 8U;
	/// \brief [[BPR-1891]]. The longest plugin name a diagnostic carries.
	inline constexpr std::size_t maximum_reported_plugin_bytes = 64U;

	/// \brief [[BPR-1891]]. Whether a plugin name can be written to a
	/// diagnostic as is: letters, digits and `._@+-`, so never a path, a
	/// space or the list's own `,`.
	[[nodiscard]] constexpr auto is_reported_plugin_token(const std::string_view aText) noexcept -> bool
	{
		if (aText.empty() || aText.size() > maximum_reported_plugin_bytes) {
			return false;
		}
		for (const char character : aText) {
			const auto plain = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9')
				|| character == '.' || character == '_' || character == '@' || character == '+' || character == '-';
			if (!plain) {
				return false;
			}
		}
		return true;
	}

	/// \brief [[BPR-1900]]. How many tool uses a read-only run's result
	/// keeps, and how much of what each was asked.
	inline constexpr std::size_t maximum_agent_tool_activity       = 256U;
	inline constexpr std::size_t maximum_agent_tool_activity_bytes = 512U;
	/// \brief [[BPR-1912]]. The note before a read-only Claude run's shell
	/// command outside `core::read_only_programs`, as its `tool_activity`
	/// carries it once its result says what Claude did: denied, ran, or
	/// never matched to a result (past `maximum_agent_tool_activity` held
	/// commands). `tour_transcript_of` reads them back as
	/// `core::tour_transcript_note`.
	inline constexpr std::string_view read_only_not_run_note   = "(not run, outside the read-only list) ";
	inline constexpr std::string_view read_only_ran_note       = "(ran, outside the read-only list) ";
	inline constexpr std::string_view read_only_unchecked_note = "(unchecked, outside the read-only list) ";

	/// \brief [[BPR-1918]]. The checks `agent_run_checks::relaxed` lets
	/// pass, each counted on its own. `skill` is [[BPR-1916]]'s check for a
	/// Skill tool_use no reference names; it honours the same switch.
	enum class relaxed_check : std::uint8_t {
		/// Init named a tool or an MCP server the run did not ask for.
		init,
		/// A tool_use outside the run's list.
		tool,
		/// A writing answer's block that is not text.
		block,
		/// A Skill tool_use for a skill no reference names.
		skill,
	};
	inline constexpr std::size_t relaxed_check_count = static_cast<std::size_t>(relaxed_check::skill) + 1U;
	/// \brief The check's stable name, as `check_relaxed` and the
	/// diagnostic carry it.
	[[nodiscard]] constexpr auto relaxed_check_name(const relaxed_check aCheck) noexcept -> std::string_view
	{
		switch (aCheck) {
		case relaxed_check::init:  return "init";
		case relaxed_check::tool:  return "tool";
		case relaxed_check::block: return "block";
		case relaxed_check::skill: return "skill";
		}
		return { };
	}
	/// \brief [[BPR-1918]]. Before what a relaxed check let pass, in a
	/// `tool_activity` item, for the transcript.
	inline constexpr std::string_view relaxed_check_note = "(not stopped, strict run checks off) ";

	/// \brief The longest structured-finding payload one turn may return.
	///
	/// \details Declared in the same style as the stdout, stderr, and frame
	/// bounds above, and deliberately smaller than all of them: a payload
	/// rides inside one result frame, so a bound at or above
	/// `maximum_agent_frame_bytes` could never be the reason anything was
	/// refused, and an unreachable bound is a bound no test can hold to
	/// account. The consequence is stated rather than hidden — the sum of the
	/// core's per-field maxima exceeds this, so a single maximal finding is
	/// refused as an oversize payload rather than truncated.
	inline constexpr std::size_t maximum_agent_finding_payload_bytes = 48U * 1024U;

	/// \brief The most findings one completed turn may return.
	inline constexpr std::size_t maximum_agent_findings_per_turn = 32U;

	static_assert(
		maximum_agent_finding_payload_bytes < maximum_agent_frame_bytes,
		"a finding payload rides inside one frame, so its bound must be "
		"reachable inside the frame bound");

	/// \brief The member every carrying protocol names its payload with.
	inline constexpr std::string_view agent_finding_payload_member = "bprFindings";

	/// \brief The line prefix an external-terminal child would have to write.
	///
	/// \details External terminal has no result framing, so this exists only so
	/// that an attempt to return findings over it is *refused* rather than
	/// silently read as ordinary output. Nothing decodes what follows it.
	inline constexpr std::string_view agent_finding_payload_sentinel
		= "BPR-FINDINGS/1";

	enum class agent_protocol_failure : std::uint8_t {
		malformed_frame,
		incomplete_frame,
		frame_too_large,
		output_too_large,
		remote_failed,
		remote_interrupted,
		causal_violation,
		/// The payload's own declared byte bound was exceeded.
		finding_payload_too_large,
		/// The turn returned more findings than one turn may carry.
		too_many_findings,
		/// The payload is not a well-formed finding payload: a missing or
		/// mistyped member, an identity of the wrong width, or an enumerator
		/// nobody declared. An anchor the frozen snapshot does not contain is
		/// **not** here; that is the core's verdict, not a wire error.
		malformed_finding_payload,
		/// The payload named a session this run was not started with.
		unknown_finding_session,
		/// The selected protocol has no result framing a payload could ride in,
		/// so one is refused rather than given framing nobody specified.
		unsupported_finding_transport,
		invalid_runtime_metadata,
		input_too_large,
		/// [[BPR-1375]]. The agent answered that it is not signed in (Claude's
		/// assistant frame with `error: "authentication_failed"`), before
		/// any model was asked. Told apart from a malformed or failed turn
		/// so the person is told to sign in rather than that it broke.
		not_signed_in,
		/// [[BPR-1900]]. A read-only tour run's Claude listed or used a tool
		/// `core::decide_read_only_tool` refuses (an edit, a write, the
		/// network, an MCP tool, or a shell command that is not a plain
		/// read-only pipeline). The run ends there.
		tool_refused,
	};

	/// \brief The number of declared protocol failures.
	///
	/// \details Exists so a sweep over the causes can be exhaustive rather than
	/// a sample: a new failure that nothing distinguishes fails the count.
	inline constexpr std::size_t agent_protocol_failure_count = 16U;

	static_assert(
		static_cast<std::size_t>(
			agent_protocol_failure::tool_refused)
				+ 1U
			== agent_protocol_failure_count,
		"agent_protocol_failure_count must cover every declared failure");

	enum class agent_protocol_event_kind : std::uint8_t {
		output,
		initialization_complete,
		context_channel_ready,
		/// The exact payload bytes lifted out of one result frame. Carried
		/// undecoded so that the shared loop, rather than the frame reader, is
		/// the one place a payload becomes findings.
		finding_payload,
		/// [[BPR-1753]]. What the harness says the turn cost, in US
		/// dollars as the decimal it wrote; Claude Code's result frame names
		/// it `total_cost_usd`.
		usage_report,
		/// [[BPR-1891]]. The plugins a writing run's Claude init frame says
		/// it loaded, as comma-separated tokens that pass
		/// `is_reported_plugin_token`, ending `+N` when more were named
		/// than `maximum_reported_plugins`. For diagnostics only.
		plugins_reported,
		/// [[BPR-1896]]. A writing run's Claude init frame named an
		/// auto-memory folder. Carries no text: the path holds the user's
		/// home folder name. For diagnostics only.
		auto_memory_reported,
		/// [[BPR-1899]]. A writing run's Claude wrote a `thinking` or
		/// `redacted_thinking` block. Carries no text: the block's content
		/// is dropped unread. For diagnostics only.
		thinking_reported,
		/// Owner look v006. A catalog run's `model/list` result, its exact
		/// bytes; the loop keeps it and closes the input.
		model_catalog,
		/// [[BPR-1900]]. A read-only run used a tool: `Text` is the tool's
		/// name and `Item` what it was asked (the path read, the pattern or
		/// the command), for the tour session's transcript.
		tool_activity,
		/// [[BPR-1912]]. A read-only Claude run's shell command outside
		/// `core::read_only_programs` ran: its result did not say it was
		/// denied. Carries no text; its `tool_activity` names it. Counted for
		/// the proposal and the diagnostic, never refused.
		outside_list_ran,
		/// [[BPR-1918]]. A check that `agent_run_checks::relaxed` let
		/// pass: `Text` is the check's stable name
		/// (`relaxed_check_name`), never what tripped it. Counted for the
		/// diagnostic.
		check_relaxed,
	};

	struct agent_protocol_event
	{
		agent_protocol_event_kind Kind = agent_protocol_event_kind::output;
		std::string               Text;
		std::string               Item;
		bool                      CompleteItem = true;

		auto operator<=>(const agent_protocol_event&) const = default;
	};

	/// \brief One completed turn's structured findings, exactly as returned.
	///
	/// \details **There is no verdict here.** Whether an anchor holds is
	/// `core::validate_agent_finding`'s answer against the session's frozen
	/// snapshot, and this type has no field it could occupy, so a decoder
	/// cannot accidentally declare a finding anchored. `Session` is the session
	/// the payload named; the loop refuses one that is not the running session
	/// before this value is ever built.
	///
	/// Every member is a value. Nothing here is a pointer, a reference, or a
	/// handle, so the decode path holds nothing it could write through.
	struct agent_finding_payload
	{
		core::agent_session_id           Session;
		std::vector<core::agent_finding> Findings;

		auto operator<=>(const agent_finding_payload&) const = default;
	};

	/// \brief Decodes one bounded structured-finding payload from a turn.
	///
	/// \details The wire shape is one JSON object per finding, carried inside
	/// the protocol's own result framing rather than in framing invented here:
	///
	/// - **Codex App Server** — `params.turn.bprFindings` on the
	///   `turn/completed` notification whose turn status is `completed`.
	/// - **Codex JSONL execution** — top-level `bprFindings` on the
	///   `turn.completed` frame, beside `usage`.
	/// - **Claude stream JSON** — top-level `bprFindings` on the terminal
	///   `result` frame whose subtype is `success` and `is_error` is false.
	/// - **External terminal** — nothing. That protocol emits newline-delimited
	///   text with no result frame, so a payload has no carrier; a line opening
	///   with `agent_finding_payload_sentinel` is refused with
	///   `unsupported_finding_transport` rather than given framing nobody
	///   specified, and rather than being read as ordinary output.
	///
	/// The payload itself is `{"session":<number>,"findings":[<finding>...]}`.
	/// Each finding states `provider`, `severity`, `confidence`, `category`,
	/// `path`, `lines` (`{"begin","end"}`), `anchor`, `claim`, `evidence`,
	/// `consequence`, `requiredContext`, `analyzedBase`, `analyzedHead`, and
	/// `analyzedCommits`; `suggestedComment` is the one optional member, and
	/// absent stays absent rather than collapsing into an empty string. An
	/// `anchor` carries an optional `line` or `boundary`
	/// (`{"comparison","side","line"|"boundary"}`) and optional `segment`,
	/// `group`, and `move` identities. Identities are 64 lowercase hex
	/// characters, matching what `encode_agent_supplement` sent; commit object
	/// identifiers are the canonical 40 or 64 hex characters, whose width
	/// selects the hash algorithm.
	///
	/// Severity and category are the stable names `core::name` already emits,
	/// so there is no second vocabulary to keep in step. Confidence is the one
	/// enumerator the core deliberately never bounds, because bounding it there
	/// would be a branch in which confidence changed an anchoring outcome —
	/// **this is the place an undeclared confidence is refused**.
	///
	/// An anchor the frozen snapshot does not contain is not a decode failure.
	/// It decodes, and `core::validate_agent_finding` refuses it as unanchored,
	/// which is why a finding the agent misplaced still reaches the reviewer.
	///
	/// \returns The decoded payload, or the exact typed cause. Oversize, an
	/// over-count, a malformation, an unknown session, and an unsupported
	/// transport are five distinct values.
	[[nodiscard]] auto decode_agent_finding_payload(
		core::agent_protocol   aProtocol,
		std::string_view       aPayload,
		core::agent_session_id aSession)
		-> std::expected<agent_finding_payload, agent_protocol_failure>;

	/// \brief The fence an agent's answer opens its review findings with.
	inline constexpr std::string_view agent_review_findings_fence = core::agent_findings_fence;

	/// \brief [[BPR-1753]], owner decision 2026-10-23 ("Yep build it
	/// like that"). The review findings an agent wrote in its answer, as the
	/// review skill asks: the last block fenced `bpr-findings`, holding a
	/// `findings` array (or the array alone) of objects with `path`,
	/// `startLine` and `endLine` (one-based, inclusive, at the head unless
	/// `side` is `left`), `severity`, `category` and `confidence` by the stable
	/// names the structured payload uses, `claim`, and optionally `evidence`,
	/// `consequence` and `suggestedComment`.
	///
	/// \details No harness lets its model add `bprFindings` to the result
	/// frame the harness writes, so this is how a real agent returns findings.
	/// The host states what the agent cannot know: the provider and analyzed
	/// revisions are the session's, and a finding on a file of `aSnapshot` is
	/// anchored to its first line in that file's frozen comparison. A finding
	/// on any other file carries no anchor, so `core::validate_agent_finding`
	/// delivers it unanchored rather than anywhere it was not about. An entry
	/// that is not a finding is left out; at most
	/// `maximum_agent_findings_per_turn` are read, from a block within
	/// `maximum_agent_finding_payload_bytes`.
	///
	/// \returns The payload, or none when the answer holds no such block.
	[[nodiscard]] auto decode_agent_review_findings(
		std::string_view                    aAnswer,
		const core::agent_session&          aSession,
		const core::agent_finding_snapshot& aSnapshot) -> std::optional<agent_finding_payload>;

	/// \brief Builds the next App Server message after a decoded control event.
	[[nodiscard]] auto build_agent_protocol_followup(
		const agent_launch_request& aRequest,
		const agent_protocol_event& aControl)
		-> std::expected<std::string, agent_protocol_failure>;

	/// \brief Incrementally decodes one selected protocol's stdout frames.
	class agent_protocol_decoder final
	{
	public:
		explicit agent_protocol_decoder(core::agent_protocol aProtocol, bool aWritingOnly = false, agent_launch_purpose aPurpose = agent_launch_purpose::conversation, bool aReadOnlyTools = false, agent_run_checks aChecks = agent_run_checks::strict);

		/// \brief Starts a turn on an already initialized native connection.
		void continue_native(std::string aHandle, std::uint64_t aTurn);
		/// \brief Complete result frame received, independently of process exit.
		[[nodiscard]] auto turn_complete() const noexcept -> bool;
		[[nodiscard]] auto native_handle() const noexcept -> const std::string&;
		void               expect_native_handle(std::string aHandle) { ExpectedNativeHandle = std::move(aHandle); }
		void               expect_response(std::uint64_t aTurn) { NativeTurn = aTurn; }

		[[nodiscard]] auto push(std::string_view aBytes)
			-> std::expected<std::vector<agent_protocol_event>, agent_protocol_failure>;
		[[nodiscard]] auto finish()
			-> std::expected<std::vector<agent_protocol_event>, agent_protocol_failure>;
		/// \brief Records that the host completely wrote one App Server follow-up.
		[[nodiscard]] auto acknowledge_followup(agent_protocol_event_kind aControl)
			-> std::expected<void, agent_protocol_failure>;

		/// \brief Only admitted protocol fields; no raw frame, stderr or prose scraping.
		[[nodiscard]] auto metadata() const noexcept -> const core::agent_runtime_metadata& { return Metadata; }

		/// \brief Ordered full assistant items; only a successful terminal makes them complete.
		[[nodiscard]] auto assistant_items() const noexcept -> const std::vector<std::string>& { return AssistantItems; }

		/// \brief [[BPR-1827]]. The start of the line the decoder refused,
		/// at most `maximum_agent_rejected_line_bytes`; empty when it refused
		/// none or refused a total rather than a line.
		[[nodiscard]] auto rejected_line() const noexcept -> const std::string& { return RejectedLine; }

		/// \brief [[BPR-1827]]. Every standard-output byte pushed so far.
		[[nodiscard]] auto pushed_bytes() const noexcept -> std::size_t { return TotalBytes; }

	private:
		[[nodiscard]] auto           retain_output(std::vector<agent_protocol_event>&) -> bool;
		[[nodiscard]] auto           frame_limit() const noexcept -> std::size_t;
		[[nodiscard]] auto           stream_limit() const noexcept -> std::size_t;
		void                         reject_line(std::string_view aLine);
		std::string                  RejectedLine;
		std::vector<std::string>     AssistantIds;
		std::vector<std::string>     AssistantItems;
		std::vector<bool>            AssistantComplete;
		std::size_t                  AssistantBytes = 0;
		core::agent_runtime_metadata Metadata;
		core::agent_protocol         Protocol;
		bool                         WritingOnly   = false;
		agent_launch_purpose         Purpose       = agent_launch_purpose::conversation;
		bool                         ReadOnlyTools = false;
		agent_run_checks             Checks        = agent_run_checks::strict;
		/// [[BPR-1900]], [[BPR-1912]]. Shell commands of a read-only run
		/// outside its list, awaiting their result: the tool_use's id and
		/// what it was asked, named once the result says what Claude did.
		std::vector<std::pair<std::string, std::string>> HeldTools;
		std::string                                      Pending;
		std::size_t                                      TotalBytes   = 0U;
		bool                                             Failed       = false;
		std::uint8_t                                     Phase        = 0U;
		bool                                             SawAssistant = false;
		std::string                                      RemoteSession;
		std::string                                      RemoteThread;
		std::string                                      RemoteTurn;
		std::string                                      ExpectedNativeHandle;
		std::uint64_t                                    NativeTurn = 2U;
	};

	/// \brief Every terminal condition the host exposes distinctly.
	enum class agent_run_outcome : std::uint8_t {
		success,
		private_storage_failure,
		failed_to_start,
		/// \brief The workspace could not be materialized, so no agent was launched.
		///
		/// \details Distinct from `failed_to_start`, which means an executable was
		/// reached and would not run. This one means the run never got that far:
		/// the failure is in the cache, several layers earlier, and BPR-0341 spent
		/// its diagnosis discovering that because both answered `failed_to_start`.
		/// An operator told the agent failed to start looks at the agent.
		workspace_unavailable,
		nonzero_exit,
		protocol_failure,
		cancelled,
		timeout,
		stale_result,
	};

	[[nodiscard]] auto name(agent_run_outcome aOutcome) noexcept -> std::string_view;
	/// \brief [[BPR-1847]]. How a turn that ran to this outcome ended, in the
	/// thread's own vocabulary.
	[[nodiscard]] auto turn_outcome(agent_run_outcome aOutcome) noexcept -> core::agent_turn_outcome;
	/// \brief [[BPR-1827]]. The failure's stable name, for logs and diagnostics.
	[[nodiscard]] auto name(agent_protocol_failure aFailure) noexcept -> std::string_view;

	struct agent_stream_event
	{
		core::agent_session_id Session;
		std::uint64_t          Sequence = 0U;
		std::string            Text;
		core::agent_run_id     Run;

		auto operator<=>(const agent_stream_event&) const = default;
	};

	struct agent_run_result
	{
		core::agent_session_id          Session;
		agent_run_outcome               Outcome = agent_run_outcome::failed_to_start;
		std::vector<agent_stream_event> Events;
		std::string                     StandardError;
		std::optional<int>              ExitCode;
		/// Exact decoder cause when `Outcome` is `protocol_failure`.
		std::optional<agent_protocol_failure> ProtocolFailure;
		/// [[BPR-1738]]. Why, when `Outcome` is `workspace_unavailable`.
		std::optional<agent_workspace_unavailable_reason> WorkspaceReason;
		/// The turn's decoded findings, before any verdict. Absent when the
		/// turn returned none. Unlike `Events` this survives a stale
		/// finalization, because a payload the reviewer can no longer act on
		/// is still a payload they are entitled to see refused by revision
		/// rather than to never learn about.
		std::optional<agent_finding_payload>         Findings;
		core::agent_run_id                           Run;
		core::agent_runtime_metadata                 Metadata;
		std::vector<core::agent_provenance_conflict> ProvenanceConflicts;
		std::vector<std::string>                     AssistantItems;
		/// [[BPR-1753]]. The turn's cost as the harness reported it, a
		/// decimal of US dollars; absent when it reported none.
		std::optional<std::string> ReportedCostUsd;
		/// [[BPR-1891]]. The plugins a writing run's Claude reported
		/// loading, as `plugins_reported` carries them; empty when none.
		std::string ReportedPlugins;
		/// [[BPR-1896]]. Whether a writing run's Claude reported an
		/// auto-memory folder, as `auto_memory_reported` says.
		bool ReportedAutoMemory = false;
		/// [[BPR-1899]]. Whether a writing run's Claude wrote a thinking
		/// block, as `thinking_reported` says. Never its content.
		bool ReportedThinking = false;
		/// [[BPR-1900]]. A read-only run's tool uses in order: the tool and
		/// what it was asked, at most `maximum_agent_tool_activity`.
		std::vector<std::pair<std::string, std::string>> ToolActivity;
		/// [[BPR-1912]]. How many of a read-only run's shell commands outside
		/// the list ran, as `outside_list_ran` counts them.
		std::size_t OutsideListCommands = 0U;
		/// [[BPR-1918]]. How many times each relaxed check let something
		/// pass, by `relaxed_check`; all zero for a strict run.
		std::array<std::uint32_t, relaxed_check_count> RelaxedCounts = { };
		/// [[BPR-1900]]. The head line count of each path the request asked
		/// to have measured in its workspace, as the host counted it before
		/// the agent started; a path it could not count is absent.
		std::vector<std::pair<std::string, std::uint32_t>> MeasuredLines;
		/// Owner look v006. A catalog run's `model/list` result, as written.
		std::optional<std::string> ModelCatalog;
		/// [[BPR-1827]]. Every byte the agent wrote to standard output,
		/// counted whether or not it decoded.
		std::size_t StandardOutputBytes = 0U;
		/// [[BPR-1827]]. The start of the first standard-output line this app
		/// could not read, bounded by `maximum_agent_rejected_line_bytes`, for
		/// the reviewer's own log only.
		std::string RejectedOutputLine;
		/// Host-owned opaque native identity; excluded from ordinary diagnostics.
		std::string NativeHandle;
		std::string Conversation;
		/// Process-local supervised connection identity, for lifecycle observations.
		std::uint64_t NativeProcess = 0U;

		auto operator<=>(const agent_run_result&) const = default;
	};

	using agent_output_sink = std::function<void(const agent_stream_event&)>;

	/// \brief Adapter-owned serialized turn exchange for one supervised process.
	class agent_native_connection_port
	{
	public:
		virtual ~agent_native_connection_port()                                  = default;
		[[nodiscard]] virtual auto next() -> std::optional<agent_launch_request> = 0;
		virtual void               complete(agent_run_result aResult)            = 0;
		[[nodiscard]] virtual auto timeout_milliseconds() const -> int           = 0;
		[[nodiscard]] virtual auto cancelled() const noexcept -> bool            = 0;
	};

	/// \brief Process effect boundary used by the agent application host.
	class agent_process_port
	{
	public:
		virtual ~agent_process_port() = default;

		[[nodiscard]] virtual auto run(
			const agent_launch_request& aRequest,
			int                         aTimeoutMilliseconds,
			const agent_output_sink&    aSink = { },
			std::stop_token             aStop = { }) -> agent_run_result = 0;
		virtual void cancel() noexcept                                   = 0;
		/// \brief Whether all requested native connection shutdowns have settled.
		[[nodiscard]] virtual auto native_connections_quiescent() const noexcept -> bool { return true; }
	};

	/// \brief Applies the core's immutable session decision to a process result.
	[[nodiscard]] auto finalize_agent_run(
		agent_run_result                    aResult,
		const core::agent_session&          aSession,
		const core::agent_revision_binding& aObserved,
		core::agent_run_id                  aExpectedRun = { }) -> agent_run_result;

	/// \brief [[BPR-1749]], RFC-BPR-0092 Q3 A. The two runs of one
	/// setup check: the agent's own status command and its `--version`.
	struct agent_status_launches
	{
		agent_launch_request Status;
		agent_launch_request Version;
	};

	/// \brief Builds the setup check's launches of `aProgram`, the absolute
	/// program setup found for `aAgent`.
	///
	/// \details `claude auth status --json`, or `codex login status`, and then
	/// `--version`. Neither carries a question, a supplement or a review: no
	/// input, no workspace, the profile's own environment allowlist, and an
	/// empty folder of its own as the working directory, as tool-free writing
	/// runs. Their output is read as plain lines. Nothing for the plain-text
	/// harness, which has no status command, or for a profile of another
	/// agent.
	[[nodiscard]] auto build_agent_status_launches(core::setup_agent aAgent, const core::agent_profile& aProfile, std::string aProgram, const environment_reader& aParent)
		-> std::optional<agent_status_launches>;

	/// \brief Decodes what the status run said, at the process boundary.
	///
	/// \details Claude answers JSON with a boolean `loggedIn`; Codex says
	/// "Logged in" and exits zero, or "Not logged in". Anything else, a run
	/// that did not finish, and a malformed answer are `unreadable`, never
	/// taken as signed in. A signed-in check keeps the version run's first
	/// word that reads as a version, bounded, or none.
	[[nodiscard]] auto decode_agent_status(core::setup_agent aAgent, const agent_run_result& aStatus, const agent_run_result& aVersion) -> core::agent_status_check;

	/// \brief Owner look v006 ("Codex is totally missing its model settings.
	/// And its effort is missing max"). One model a harness offers, as it
	/// names it, with the reasoning efforts it takes.
	struct agent_model_option
	{
		/// What `--model` (or App Server's `model`) takes.
		std::string Model;
		/// What the harness shows, such as "GPT-6.1-Sol".
		std::string              DisplayName;
		std::vector<std::string> Efforts;
		std::string              DefaultEffort;
		bool                     IsDefault = false;

		auto operator<=>(const agent_model_option&) const = default;
	};

	/// \brief What a harness says it offers: its models (none when it cannot
	/// list them) and its effort levels (none when it says none).
	struct agent_harness_catalog
	{
		std::vector<agent_model_option> Models;
		std::vector<std::string>        Efforts;

		auto operator<=>(const agent_harness_catalog&) const = default;
	};

	/// \brief The most models, and efforts per model, a catalog keeps.
	inline constexpr std::size_t maximum_catalog_models  = 64U;
	inline constexpr std::size_t maximum_catalog_efforts = 16U;

	/// \brief A Codex App Server catalog run of `aProgram`, after `aFixed`:
	/// `app-server`, then `initialize`, `initialized` and `model/list`. No
	/// workspace and no context, an empty folder of its own, the profile's
	/// environment allowlist; as the setup check runs.
	[[nodiscard]] auto build_agent_model_catalog_launch(const core::agent_profile& aProfile, std::string aProgram, std::vector<std::string> aFixed, const environment_reader& aParent)
		-> agent_launch_request;
	/// \brief The catalog run's models: each listed model not hidden, its
	/// name, display name, efforts and default, bounded; none when the run
	/// did not answer.
	[[nodiscard]] auto decode_agent_model_catalog(const agent_run_result& aResult) -> std::vector<agent_model_option>;
	/// \brief Claude Code's `--help` run of `aProgram`, after `aFixed`.
	[[nodiscard]] auto build_agent_help_launch(const core::agent_profile& aProfile, std::string aProgram, std::vector<std::string> aFixed, const environment_reader& aParent)
		-> agent_launch_request;
	/// \brief The effort levels `--help` lists for `--effort`, in its order:
	/// `--effort <level> ... (low, medium, high, xhigh, max)`; none when it
	/// lists none.
	[[nodiscard]] auto decode_agent_help_efforts(const agent_run_result& aResult) -> std::vector<std::string>;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_LAUNCH_HPP
