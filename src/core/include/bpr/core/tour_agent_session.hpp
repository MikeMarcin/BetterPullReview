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
		/// [[BPR-1914]]. A turn of the tour conversation: a question about the
		/// tour or a selected passage, or a change asked for in words.
		ask,
	};
	inline constexpr std::size_t tour_agent_request_count = 5U;
	static_assert(static_cast<std::size_t>(tour_agent_request::ask) + 1U == tour_agent_request_count, "tour_agent_request_count must cover every request");

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
		/// A search: Grep, Glob, `rg`, `grep`, `find`, `ls`, `tree`.
		searched,
		/// A file read: Read, `cat`, `head`, `tail`, `nl`, `sed`, `awk`.
		read,
		/// Any other command: read-only Git (log, show, blame, diff), `wc`,
		/// and a command outside the read-only list that Claude ran
		/// ([[BPR-1912]]).
		ran,
		/// What the agent said, bounded.
		answered,
		/// How the run ended when it did not answer.
		ended,
		/// [[BPR-1900]] owner look v002. A proposed stop that pointed outside
		/// the pull request's files and lines, left out of the proposal.
		left_out,
	};
	/// \brief The kind a tool use is shown as.
	[[nodiscard]] auto transcript_kind_of(std::string_view aTool, std::string_view aCommand) noexcept -> tour_transcript_kind;

	/// \brief [[BPR-1912]]. What became of a shell command outside
	/// `read_only_programs`. Claude's own permission check is the gate: it
	/// approves read-only commands of its own beyond `--allowedTools`, and a
	/// wrapper's or policy's settings can widen that. This app reads what it
	/// decided and names what ran outside its own list.
	enum class tour_transcript_note : std::uint8_t {
		/// Inside the list, or not a shell command.
		none,
		/// Claude denied it; it did not run.
		not_run,
		/// Claude ran it.
		ran_outside_list,
		/// Its result was never matched to it: past the held commands' bound.
		unchecked,
		/// [[BPR-1918]]. A check would have ended the run, and the custom
		/// profile's strict run checks are off: a tool or MCP server init
		/// loaded, a tool used outside the list, or a block that is not text.
		not_stopped,
	};

	struct tour_transcript_entry
	{
		tour_transcript_kind Kind = tour_transcript_kind::asked;
		std::string          Text;
		tour_transcript_note Note                                                   = tour_transcript_note::none;
		auto                 operator==(const tour_transcript_entry&) const -> bool = default;
	};

	enum class tour_agent_run_state : std::uint8_t {
		running,
		answered,
		failed,
		cancelled,
	};

	using tour_agent_run_id = strong_id<struct tour_agent_run_id_tag>;

	/// \brief [[BPR-1914]]. Which part of the tour a selected passage is from.
	enum class tour_passage_field : std::uint8_t {
		problem,
		solution,
		stop,
		/// [[BPR-1914]] v002. Lines of the code a stop points at (or lines
		/// selected in the code while the stop is open).
		stop_code,
	};
	/// \brief [[BPR-1914]]. The passage an Ask about this turn is about: the
	/// selected text and where it was selected.
	struct tour_passage
	{
		tour_passage_field Field = tour_passage_field::solution;
		/// For a stop: its zero-based place in the tour when it was asked.
		std::size_t Stop = 0U;
		/// For a stop: its title then.
		std::string StopTitle;
		std::string Text;
		/// For stop code: the head path and one-based lines it shows.
		std::string   Path;
		std::uint32_t FirstLine                                     = 0U;
		std::uint32_t LastLine                                      = 0U;
		auto          operator==(const tour_passage&) const -> bool = default;
	};

	/// \brief [[BPR-1914]] v002, owner 2026-11-06: "I want to ask questions
	/// about the specific parts of the tour, how do I do that if I can't see
	/// the tour or the code?" Each turn belongs to the thread of the part it
	/// was asked from, shown under that part: the tour as a whole, the
	/// Problem, the Solution, or one stop (its explanation and its code).
	enum class tour_thread_kind : std::uint8_t {
		tour,
		problem,
		solution,
		stop,
	};
	struct tour_thread
	{
		tour_thread_kind Kind = tour_thread_kind::tour;
		/// For a stop's thread: the stop.
		tour_stop_id Stop;
		auto         operator==(const tour_thread&) const -> bool = default;
	};
	inline constexpr std::size_t maximum_tour_passage_bytes  = 4096U;
	inline constexpr std::size_t maximum_tour_question_bytes = 4096U;
	inline constexpr std::size_t maximum_tour_reply_bytes    = 16U * 1024U;

	/// \brief [[BPR-1914]]. What a turn proposed, counted for its line in the
	/// conversation.
	struct tour_proposal_tally
	{
		bool        Problem                                              = false;
		bool        Solution                                             = false;
		std::size_t Stops                                                = 0U;
		std::size_t StopTexts                                            = 0U;
		auto        operator==(const tour_proposal_tally&) const -> bool = default;
	};

	/// \brief One request of the tour session and what it did.
	struct tour_agent_run
	{
		tour_agent_run_id                  Id;
		tour_agent_request                 Request = tour_agent_request::draft_tour;
		tour_agent_tools                   Tools   = tour_agent_tools::none;
		tour_agent_run_state               State   = tour_agent_run_state::running;
		std::vector<tour_transcript_entry> Entries;
		/// Entries not kept once `maximum_tour_transcript_entries` was reached.
		std::size_t Dropped = 0U;
		/// [[BPR-1912]]. Shell commands outside the read-only list that Claude
		/// ran, counted whether or not their entries were kept.
		std::size_t OutsideList = 0U;
		/// [[BPR-1914]]. An ask turn: the passage it was about, if any, the
		/// question in full, the reply as the agent wrote it (Markdown), and
		/// what it proposed.
		std::optional<tour_passage> Passage;
		std::string                 Question;
		std::string                 Reply;
		tour_proposal_tally         Proposed;
		/// [[BPR-1914]] v002. The thread an ask turn belongs to.
		tour_thread Thread;
		/// [[BPR-1914]] v003. "Update this section from our chat": a turn that
		/// asks for its thread's part rewritten from the conversation.
		bool Update = false;
		/// [[BPR-1914]] v003. How long the run worked, as the host measured
		/// it, for "Worked for Ns".
		std::optional<std::uint32_t> Seconds;
		auto                         operator==(const tour_agent_run&) const -> bool = default;
	};

	inline constexpr std::size_t maximum_tour_agent_runs         = 32U;
	inline constexpr std::size_t maximum_tour_transcript_entries = 200U;
	inline constexpr std::size_t maximum_tour_transcript_bytes   = 2048U;

	/// \brief The tour session of one review. Separate from the Agent tab's
	/// conversation: nothing here is a chat turn and no chat turn is here.
	/// Each request is a fresh run (no `--resume`). [[BPR-1914]] supersedes
	/// RFC-BPR-0092 T1: the session is one conversation per tour, replayed.
	/// An ask turn is given the tour so far and the earlier turns kept here
	/// (`tour_conversation_of`), so a follow-up keeps its context whatever
	/// the agent.
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
	/// \brief Adds lines to a running run, bounded, and counts those that ran
	/// outside the read-only list; false when it is not running.
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

	/// \brief One-based head lines, first to last.
	struct tour_line_span
	{
		std::uint32_t First                                           = 0U;
		std::uint32_t Last                                            = 0U;
		auto          operator==(const tour_line_span&) const -> bool = default;
	};
	inline constexpr std::size_t maximum_patch_hunks = 512U;
	/// \brief [[BPR-1900]]. The head side of each hunk of a pull request
	/// file's patch, as GitHub's `files` answer gives it (`@@ -a,b +c,d @@`,
	/// context lines included): `c` to `c + d - 1`. A hunk that only deletes
	/// has no head lines and is skipped. Nothing for a header it cannot read
	/// or more than `maximum_patch_hunks`.
	[[nodiscard]] auto parse_patch_head_hunks(std::string_view aPatch) -> std::optional<std::vector<tour_line_span>>;

	/// \brief A file of the pull request, as the host read it: on the head or
	/// not (deleted), its head line count when the host measured it, and its
	/// hunks' head lines when GitHub gave its patch.
	struct tour_anchor_file
	{
		std::string                                Path;
		bool                                       OnHead = true;
		std::optional<std::uint32_t>               HeadLines;
		std::optional<std::vector<tour_line_span>> Hunks;
		auto                                       operator==(const tour_anchor_file&) const -> bool = default;
	};

	/// \brief [[BPR-1900]], owner 2026-11-06: "It should strongly prefer on
	/// hunk. I'm okay with high value off-hunk that will work on live or i can
	/// relocate to on a hunk or in the PR". Where an anchored stop sits
	/// against the change, as tour publication will place it.
	enum class tour_stop_placement : std::uint8_t {
		/// Wholly inside one hunk (context lines included), or the file's
		/// hunks are unknown, which publication also places natively.
		on_change,
		/// One line outside every hunk: published as a line comment where
		/// GitHub takes one off the diff, otherwise as a file comment with a
		/// permalink. It works on GitHub as it is.
		near_change,
		/// Several lines not wholly inside one hunk: a range has no file
		/// comment, so it is not published until it is moved onto a change.
		off_change,
	};
	[[nodiscard]] auto place_tour_stop(const proposed_tour_stop& aStop, const tour_anchor_file& aFile) -> tour_stop_placement;

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
		/// [[BPR-1914]]. A new explanation for a stop already in the tour.
		stop_text,
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
		/// For a stop: where it sits against the change.
		tour_stop_placement Placement = tour_stop_placement::on_change;
		/// The author already wrote this field: accepting replaces it, and
		/// says so before it does.
		bool ReplacesAuthorText = false;
		/// For a stop_text: the stop of the tour it rewrites (its title is in
		/// `Stop.Title`).
		tour_stop_id StopId;
		/// [[BPR-1914]] v002. The run that proposed it: an ask turn's parts
		/// are shown in that turn's thread.
		tour_agent_run_id Run;
		auto              operator==(const tour_proposal_item&) const -> bool = default;
	};
	/// \brief A proposed stop the core left out, and why.
	struct tour_left_out_stop
	{
		proposed_tour_stop  Stop;
		tour_anchor_verdict Verdict                                             = tour_anchor_verdict::not_in_pull_request;
		auto                operator==(const tour_left_out_stop&) const -> bool = default;
	};
	/// \brief The proposal of one draft_tour run.
	struct tour_proposal
	{
		tour_agent_run_id               Run;
		std::vector<tour_proposal_item> Items;
		/// [[BPR-1900]] owner look v002 ("What's up with this?"): stops that
		/// do not anchor are never shown as items; they are counted in one
		/// quiet line and kept in the run's transcript.
		std::vector<tour_left_out_stop> LeftOut;
		auto                            operator==(const tour_proposal&) const -> bool = default;
	};
	/// \brief The proposal an answer makes against the draft as it is now:
	/// Problem, Solution, then the anchored stops: those on a change first,
	/// then those near one, then those off it, each group in the agent's
	/// order. A stop that does not anchor goes to `LeftOut`, never to `Items`.
	/// Empty fields of the answer are left out.
	[[nodiscard]] auto propose_tour(tour_agent_run_id aRun, const tour_draft_answer& aAnswer, const std::vector<tour_anchor_file>& aFiles, const review_tour& aDraft) -> tour_proposal;
	/// \brief Whether an item may still be accepted: pending, with text, and
	/// for a stop, anchored.
	[[nodiscard]] auto acceptable(const tour_proposal_item& aItem) noexcept -> bool;
	/// \brief Decides one pending item. Accept takes `aText` (the author's
	/// edit of the proposed text) and is refused for an item that is not
	/// `acceptable`; Discard is refused only for a decided item.
	auto decide_tour_proposal(tour_proposal& aProposal, std::size_t aIndex, tour_proposal_decision aDecision, std::string_view aText) -> bool;
	/// \brief Move to nearest change: moves pending stop `aIndex` onto the
	/// hunk of its file nearest its lines, keeping as many of its lines as
	/// fit, and places it on the change. Refused for a part that is not a
	/// pending stop, or whose file's hunks are unknown or empty.
	auto relocate_tour_stop(tour_proposal& aProposal, std::size_t aIndex, const std::vector<tour_anchor_file>& aFiles) -> bool;
	/// \brief The stop an accepted proposed stop becomes: at its head lines
	/// as a destination (zero-based), to be confirmed in the code.
	[[nodiscard]] auto tour_stop_of(const proposed_tour_stop& aStop, const git_commit_oid& aHead) -> tour_stop;
	/// \brief The transcript lines that keep a proposal's left-out stops:
	/// each stop's path, lines, title and why it was left out.
	[[nodiscard]] auto left_out_entries(const tour_proposal& aProposal) -> std::vector<tour_transcript_entry>;
	/// \brief Adds lines to a run that has ended (the left-out stops are
	/// known only once its answer is read); bounded as a running run's are.
	auto note_finished_tour_run(tour_agent_session& aSession, tour_agent_run_id aRun, const std::vector<tour_transcript_entry>& aEntries) -> bool;

	/// \brief [[BPR-1914]]. Starts an ask turn: `aQuestion` as asked (not
	/// blank, valid UTF-8, at most `maximum_tour_question_bytes`) and the
	/// passage it is about (its text not blank, at most
	/// `maximum_tour_passage_bytes`). Refused as `begin_tour_run` is, and for
	/// a question or passage outside those bounds.
	[[nodiscard]] auto begin_tour_ask(tour_agent_session& aSession, const tour_thread& aThread, const std::optional<tour_passage>& aPassage, std::string_view aQuestion, bool aUpdate = false) -> std::optional<tour_agent_run_id>;
	/// \brief [[BPR-1914]] v003. Records how long a run worked, once it has
	/// ended; false for a running or unknown run.
	auto time_tour_run(tour_agent_session& aSession, tour_agent_run_id aRun, std::uint32_t aSeconds) -> bool;
	/// \brief [[BPR-1914]] v003. An Update turn proposes only its thread's part:
	/// the Problem, the Solution, or the stop's new explanation; the tour's
	/// thread keeps everything. The rest is left out of the proposal.
	[[nodiscard]] auto keep_thread_part(tour_proposal aProposal, const tour_thread& aThread) -> tour_proposal;
	/// \brief [[BPR-1914]] v002. The ask turns of `aThread`, oldest first.
	[[nodiscard]] auto thread_runs(const tour_agent_session& aSession, const tour_thread& aThread) -> std::vector<const tour_agent_run*>;
	/// \brief Whether proposal item `aItem` came from an ask turn, and so is
	/// shown in that turn's thread rather than with a whole-tour draft.
	[[nodiscard]] auto proposed_in_thread(const tour_agent_session& aSession, const tour_proposal_item& aItem) noexcept -> bool;
	/// \brief Ends a running ask turn as answered with the agent's reply (cut
	/// to `maximum_tour_reply_bytes` at a character) and what it proposed.
	auto answer_tour_ask(tour_agent_session& aSession, tour_agent_run_id aRun, std::string_view aReply, const tour_proposal_tally& aProposed) -> bool;

	/// \brief One earlier turn as the next one is given it.
	struct tour_conversation_turn
	{
		std::string                 Asked;
		std::optional<tour_passage> Passage;
		/// The reply, or for a Draft the tour run what it proposed.
		std::string Reply;
		tour_thread Thread;
		auto        operator==(const tour_conversation_turn&) const -> bool = default;
	};
	struct tour_conversation
	{
		/// Oldest first.
		std::vector<tour_conversation_turn> Turns;
		/// Earlier turns left out to keep within the bounds.
		std::size_t Dropped                                            = 0U;
		auto        operator==(const tour_conversation&) const -> bool = default;
	};
	inline constexpr std::size_t maximum_replayed_tour_turns = 12U;
	inline constexpr std::size_t maximum_replayed_tour_bytes = 48U * 1024U;
	/// \brief The conversation a new turn replays: every answered ask turn and
	/// every answered Draft the tour run, oldest first, keeping the newest
	/// `maximum_replayed_tour_turns` that fit in `maximum_replayed_tour_bytes`.
	/// Failed and stopped turns, and small rewrites, are not replayed.
	[[nodiscard]] auto tour_conversation_of(const tour_agent_session& aSession) -> tour_conversation;
	/// \brief [[BPR-1914]] v003. One side chat is one thread: the turns a new
	/// turn in `aThread` replays are that thread's own answered turns (and, for
	/// the tour's thread, the answered Draft the tour runs), bounded as above.
	/// Each part's chat is independent of the others.
	[[nodiscard]] auto tour_conversation_of(const tour_agent_session& aSession, const tour_thread& aThread) -> tour_conversation;

	/// \brief A new explanation an ask turn proposes for stop `Stop` (one-based,
	/// as the tour so far numbered it).
	struct proposed_stop_text
	{
		std::size_t Stop = 0U;
		std::string Explanation;
		auto        operator==(const proposed_stop_text&) const -> bool = default;
	};
	/// \brief An ask turn's answer, as decoded: the reply, and any change it
	/// proposes. Nothing here is checked yet.
	struct tour_ask_answer
	{
		std::string                     Reply;
		tour_draft_answer               Changes;
		std::vector<proposed_stop_text> StopTexts;
		auto                            operator==(const tour_ask_answer&) const -> bool = default;
	};
	/// \brief The proposal an ask turn makes: as `propose_tour` for the
	/// Problem, the Solution and new stops, then a `stop_text` item for each
	/// stop rewrite that names a stop of `aDraft` (others are dropped).
	[[nodiscard]] auto propose_tour_changes(tour_agent_run_id aRun, const tour_ask_answer& aAnswer, const std::vector<tour_anchor_file>& aFiles, const review_tour& aDraft) -> tour_proposal;
	/// \brief What a proposal holds, counted.
	[[nodiscard]] auto tally_of(const tour_proposal& aProposal) noexcept -> tour_proposal_tally;
	/// \brief The proposal shown after `aIncoming` arrives: `aIncoming`'s items,
	/// then the pending items of `aCurrent` it does not supersede (a pending
	/// Problem or Solution when it proposes one, a pending stop_text of the
	/// same stop). Decided items of `aCurrent` are let go. Nothing proposed
	/// leaves `aCurrent` as it is.
	[[nodiscard]] auto merge_tour_proposal(const std::optional<tour_proposal>& aCurrent, tour_proposal aIncoming) -> std::optional<tour_proposal>;
}
#endif
