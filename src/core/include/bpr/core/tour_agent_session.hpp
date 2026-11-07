#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_TOUR_AGENT_SESSION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_TOUR_AGENT_SESSION_HPP

#include "bpr/core/review_preparation.hpp"
#include "bpr/core/review_tour.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1900]], RFC-BPR-0092 amendment. Tour authoring as its own
/// agent session: which tour requests read the repository, which tool uses
/// a read-only run may make, the session's transcript, and the whole-tour
/// proposal with its anchors.
///
/// \details The owner, 2026-11-06: "to me the tour itself is much more of a
/// second agent session, separate and mostly background from my main agent
/// chat session", and "then it can fill its tour stops and problem and
/// solution from all that context". Everything here is a value and a pure
/// decision; the host runs the agent and reads the workspace, and Qt shows
/// what these say.
namespace bpr::core {
	/// \brief What the tour session is asked to do.
	enum class tour_agent_request : std::uint8_t {
		/// Draft the tour: Problem, Solution and the stops, as a proposal.
		draft_tour,
		/// "Draft from PR context" on an empty Problem or Solution.
		draft_field,
		/// "Suggest an outline".
		outline,
		/// A small rewrite: Enhance a field, Generate comment, a selection.
		rewrite,
	};
	inline constexpr std::size_t tour_agent_request_count = 4U;
	static_assert(static_cast<std::size_t>(tour_agent_request::rewrite) + 1U == tour_agent_request_count, "tour_agent_request_count must cover every request");

	/// \brief Which tools a tour run gets.
	enum class tour_agent_tools : std::uint8_t {
		/// BPR-1375's tool-free writing launch: an empty folder, no tools.
		none,
		/// The review's private workspace with read, search and read-only
		/// Git; no edit, write, network or MCP.
		read_only,
	};

	/// \brief The split the card asks to record: drafting reads the
	/// repository, small rewrites stay tool-free and fast.
	[[nodiscard]] auto tools_for(tour_agent_request aRequest) noexcept -> tour_agent_tools;
	/// \brief The request a writing hook press is: the outline, an empty
	/// Problem or Solution ("Draft from PR context"), or a rewrite.
	[[nodiscard]] auto tour_request_of(writing_hook aHook, writing_scope aScope) noexcept -> tour_agent_request;

	/// \brief The tools a read-only Claude run is launched with
	/// (`--tools`), and the only tool names its init frame and its tool uses
	/// may name.
	inline constexpr std::array<std::string_view, 4> read_only_claude_tools = { "Read", "Grep", "Glob", "Bash" };
	/// \brief The programs a read-only run's shell command may start, each
	/// segment of a pipeline on its own. Git is admitted only with
	/// `read_only_git_commands`.
	///
	/// \details [[BPR-1900]], installed Claude Code 2.1.310: Claude runs its
	/// own read-only commands (`cd` into the workspace, `wc`, `sort`, `uniq`,
	/// `cut`, `sed -n`) without asking, so the check admits the read-only ones
	/// it may run and still refuses anything that writes, runs a script or
	/// reaches the network. `sed` is admitted only to print (`-n` and a line
	/// script ending in `p`).
	inline constexpr std::array<std::string_view, 27> read_only_programs     = { "rg", "grep", "ls", "cat", "head", "tail", "wc", "sort", "uniq", "find", "git", "nl", "cd", "pwd", "echo", "printf", "cut", "tr", "diff", "file", "stat", "du", "basename", "dirname", "realpath", "sed", "tree" };
	inline constexpr std::array<std::string_view, 14> read_only_git_commands = { "log", "show", "blame", "diff", "grep", "ls-files", "status", "rev-parse", "shortlog", "describe", "cat-file", "ls-tree", "merge-base", "name-rev" };

	/// \brief Why a tool use is refused, or `none`.
	enum class tour_tool_refusal : std::uint8_t {
		none,
		/// The tool is not one of `read_only_claude_tools`.
		tool,
		/// A shell command with redirection, a sequence, a substitution, a
		/// background job or a newline.
		shell_syntax,
		/// A program not in `read_only_programs`, or Git with a command not in
		/// `read_only_git_commands`.
		program,
		/// An argument that writes or runs something: `find -delete`/`-exec`,
		/// `sort -o`, `rg --pre`, Git's `--output`, `-c` or `--exec-path`.
		argument,
	};
	/// \brief Decides one tool use of a read-only run. `aCommand` is the
	/// shell command when `aTool` is `Bash`, and ignored otherwise.
	[[nodiscard]] auto decide_read_only_tool(std::string_view aTool, std::string_view aCommand) -> tour_tool_refusal;

	/// \brief One line of the tour session's transcript.
	enum class tour_transcript_kind : std::uint8_t {
		/// What the author asked, in the control's own words.
		asked,
		/// A search: Grep, Glob, `rg`, `grep`, `find`, `ls`.
		searched,
		/// A file read: Read, `cat`, `head`, `tail`, `nl`.
		read,
		/// Read-only Git: log, show, blame, diff.
		ran,
		/// What the agent said, bounded.
		answered,
		/// How the run ended when it did not answer.
		ended,
	};
	/// \brief The kind a tool use is shown as.
	[[nodiscard]] auto transcript_kind_of(std::string_view aTool, std::string_view aCommand) noexcept -> tour_transcript_kind;

	struct tour_transcript_entry
	{
		tour_transcript_kind Kind = tour_transcript_kind::asked;
		std::string          Text;
		auto                 operator==(const tour_transcript_entry&) const -> bool = default;
	};

	enum class tour_agent_run_state : std::uint8_t {
		running,
		answered,
		failed,
		cancelled,
	};

	using tour_agent_run_id = strong_id<struct tour_agent_run_id_tag>;

	/// \brief One request of the tour session and what it did.
	struct tour_agent_run
	{
		tour_agent_run_id                  Id;
		tour_agent_request                 Request = tour_agent_request::draft_tour;
		tour_agent_tools                   Tools   = tour_agent_tools::none;
		tour_agent_run_state               State   = tour_agent_run_state::running;
		std::vector<tour_transcript_entry> Entries;
		/// Entries not kept once `maximum_tour_transcript_entries` was reached.
		std::size_t Dropped                                         = 0U;
		auto        operator==(const tour_agent_run&) const -> bool = default;
	};

	inline constexpr std::size_t maximum_tour_agent_runs         = 32U;
	inline constexpr std::size_t maximum_tour_transcript_entries = 200U;
	inline constexpr std::size_t maximum_tour_transcript_bytes   = 2048U;

	/// \brief The tour session of one review. Separate from the Agent tab's
	/// conversation: nothing here is a chat turn and no chat turn is here.
	/// Each request is a fresh run (no `--resume`): the tour so far is its
	/// context, not an earlier conversation.
	struct tour_agent_session
	{
		std::vector<tour_agent_run> Runs;
		std::uint64_t               LastRun                                             = 0U;
		auto                        operator==(const tour_agent_session&) const -> bool = default;
	};

	/// \brief Starts a run. Refused (nullopt) while another run is running:
	/// the session asks one thing at a time. The oldest run is forgotten past
	/// `maximum_tour_agent_runs`.
	[[nodiscard]] auto begin_tour_run(tour_agent_session& aSession, tour_agent_request aRequest, tour_agent_tools aTools, std::string_view aAsked) -> std::optional<tour_agent_run_id>;
	/// \brief Adds lines to a running run, bounded; false when it is not running.
	auto record_tour_activity(tour_agent_session& aSession, tour_agent_run_id aRun, const std::vector<tour_transcript_entry>& aEntries) -> bool;
	/// \brief Ends a running run; false when it is not running.
	auto               finish_tour_run(tour_agent_session& aSession, tour_agent_run_id aRun, tour_agent_run_state aState, std::string_view aLast) -> bool;
	[[nodiscard]] auto running_tour_run(const tour_agent_session& aSession) noexcept -> std::optional<tour_agent_run_id>;
	[[nodiscard]] auto find_tour_run(const tour_agent_session& aSession, tour_agent_run_id aRun) noexcept -> const tour_agent_run*;

	/// \brief One stop as the agent proposed it: a head path and one-based
	/// head lines, its title and its explanation.
	struct proposed_tour_stop
	{
		std::string   Path;
		std::uint32_t FirstLine = 0U;
		std::uint32_t LastLine  = 0U;
		std::string   Title;
		std::string   Explanation;
		auto          operator==(const proposed_tour_stop&) const -> bool = default;
	};
	/// \brief A whole-tour answer, as decoded; nothing here is checked yet.
	struct tour_draft_answer
	{
		std::optional<std::string>      Problem;
		std::optional<std::string>      Solution;
		std::vector<proposed_tour_stop> Stops;
		auto                            operator==(const tour_draft_answer&) const -> bool = default;
	};
	inline constexpr std::size_t   maximum_proposed_tour_stops = 24U;
	inline constexpr std::uint32_t maximum_proposed_stop_lines = 400U;

	/// \brief A file of the pull request, as the host read it: on the head or
	/// not (deleted), and its head line count when the host measured it.
	struct tour_anchor_file
	{
		std::string                  Path;
		bool                         OnHead = true;
		std::optional<std::uint32_t> HeadLines;
		auto                         operator==(const tour_anchor_file&) const -> bool = default;
	};

	enum class tour_anchor_verdict : std::uint8_t {
		/// The lines are real lines of a head file of the pull request.
		anchored,
		/// The path is not a file of this pull request.
		not_in_pull_request,
		/// The file was deleted, so it has no head lines.
		not_on_head,
		/// The lines run past the end of the file.
		beyond_file,
		/// No line, lines out of order, or more than
		/// `maximum_proposed_stop_lines`.
		invalid_lines,
		/// The file's lines could not be counted.
		unmeasured,
	};
	[[nodiscard]] auto anchor_tour_stop(const proposed_tour_stop& aStop, const std::vector<tour_anchor_file>& aFiles) -> tour_anchor_verdict;

	enum class tour_proposal_part : std::uint8_t {
		problem,
		solution,
		stop,
	};
	enum class tour_proposal_decision : std::uint8_t {
		pending,
		accepted,
		discarded,
	};
	/// \brief One field or stop of a proposal, decided on its own.
	struct tour_proposal_item
	{
		tour_proposal_part     Part = tour_proposal_part::problem;
		std::string            Text;
		proposed_tour_stop     Stop;
		tour_anchor_verdict    Verdict  = tour_anchor_verdict::anchored;
		tour_proposal_decision Decision = tour_proposal_decision::pending;
		/// The author already wrote this field: accepting replaces it, and
		/// says so before it does.
		bool ReplacesAuthorText                                  = false;
		auto operator==(const tour_proposal_item&) const -> bool = default;
	};
	/// \brief The proposal of one draft_tour run.
	struct tour_proposal
	{
		tour_agent_run_id               Run;
		std::vector<tour_proposal_item> Items;
		auto                            operator==(const tour_proposal&) const -> bool = default;
	};
	/// \brief The proposal an answer makes against the draft as it is now:
	/// Problem, Solution, then the stops in the agent's order, each stop
	/// anchored or flagged. Empty fields of the answer are left out.
	[[nodiscard]] auto propose_tour(tour_agent_run_id aRun, const tour_draft_answer& aAnswer, const std::vector<tour_anchor_file>& aFiles, const review_tour& aDraft) -> tour_proposal;
	/// \brief Whether an item may still be accepted: pending, with text, and
	/// for a stop, anchored.
	[[nodiscard]] auto acceptable(const tour_proposal_item& aItem) noexcept -> bool;
	/// \brief Decides one pending item. Accept takes `aText` (the author's
	/// edit of the proposed text) and is refused for an item that is not
	/// `acceptable`; Discard is refused only for a decided item.
	auto decide_tour_proposal(tour_proposal& aProposal, std::size_t aIndex, tour_proposal_decision aDecision, std::string_view aText) -> bool;
	/// \brief The stop an accepted proposed stop becomes: at its head lines
	/// as a destination (zero-based), to be confirmed in the code.
	[[nodiscard]] auto tour_stop_of(const proposed_tour_stop& aStop, const git_commit_oid& aHead) -> tour_stop;
}
#endif
