#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_LAUNCH_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_LAUNCH_HPP

#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/agent_workspace_lifetime_guard.hpp"
#include "bpr/application/review_ledger_port.hpp"
#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/agent_finding.hpp"
#include "bpr/core/agent_profile.hpp"
#include "bpr/core/agent_setup.hpp"
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

		auto operator<=>(const agent_context_observation&) const = default;
	};

	enum class agent_context_failure : std::uint8_t {
		invalid_observation,
		selected_note_missing,
		duplicate_selected_note,
		selected_draft_missing,
		selected_draft_stale,
		selected_draft_invalid,
	};

	/// \brief Builds context only from caller-supplied bounded observations.
	[[nodiscard]] auto build_agent_context_supplement(
		const agent_context_observation& aObservation,
		const core::agent_session&       aSession)
		-> std::expected<core::agent_context_supplement, agent_context_failure>;

	/// \brief One store-validated full body, ordered exactly as the admitted selection.
	struct agent_private_source_body
	{
		core::private_source_reference              Source;
		std::string                                 Body;
		std::optional<core::retained_agent_finding> Finding = { };
	};
	[[nodiscard]] auto serialize_private_finding(const core::retained_agent_finding&) -> std::string;
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
	};

	/// \brief Effect-lane source claim, released after the private input write or refusal.
	class agent_input_claim
	{
	public:
		virtual ~agent_input_claim()                                  = default;
		[[nodiscard]] virtual auto permitted() const noexcept -> bool = 0;
		virtual void               release() noexcept                 = 0;
	};
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
	/// A plugin added by a later Claude is refused by the init frame instead.
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
	inline constexpr std::array<std::string_view, 13> claude_writing_restrictions = {
		"--tools", "", "--disallowedTools", "*", "--strict-mcp-config", "--mcp-config", R"({"mcpServers":{}})", "--setting-sources", "",
		"--disable-slash-commands", "--no-session-persistence", "--settings", claude_writing_settings
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
	};

	/// \brief The number of declared protocol failures.
	///
	/// \details Exists so a sweep over the causes can be exhaustive rather than
	/// a sample: a new failure that nothing distinguishes fails the count.
	inline constexpr std::size_t agent_protocol_failure_count = 15U;

	static_assert(
		static_cast<std::size_t>(
			agent_protocol_failure::not_signed_in)
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

	/// \brief Builds the next App Server message after a decoded control event.
	[[nodiscard]] auto build_agent_protocol_followup(
		const agent_launch_request& aRequest,
		const agent_protocol_event& aControl)
		-> std::expected<std::string, agent_protocol_failure>;

	/// \brief Incrementally decodes one selected protocol's stdout frames.
	class agent_protocol_decoder final
	{
	public:
		explicit agent_protocol_decoder(core::agent_protocol aProtocol, bool aWritingOnly = false);

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

	private:
		[[nodiscard]] auto           retain_output(std::vector<agent_protocol_event>&) -> bool;
		std::vector<std::string>     AssistantIds;
		std::vector<std::string>     AssistantItems;
		std::vector<bool>            AssistantComplete;
		std::size_t                  AssistantBytes = 0;
		core::agent_runtime_metadata Metadata;
		core::agent_protocol         Protocol;
		bool                         WritingOnly = false;
		std::string                  Pending;
		std::size_t                  TotalBytes   = 0U;
		bool                         Failed       = false;
		std::uint8_t                 Phase        = 0U;
		bool                         SawAssistant = false;
		std::string                  RemoteSession;
		std::string                  RemoteThread;
		std::string                  RemoteTurn;
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

		auto operator<=>(const agent_run_result&) const = default;
	};

	using agent_output_sink = std::function<void(const agent_stream_event&)>;

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
	};

	/// \brief Applies the core's immutable session decision to a process result.
	[[nodiscard]] auto finalize_agent_run(
		agent_run_result                    aResult,
		const core::agent_session&          aSession,
		const core::agent_revision_binding& aObserved,
		core::agent_run_id                  aExpectedRun = { }) -> agent_run_result;

	/// \brief [[BPR-PENDING-AQ2]], RFC-BPR-0091 Q3 A. The two runs of one
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
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_LAUNCH_HPP
