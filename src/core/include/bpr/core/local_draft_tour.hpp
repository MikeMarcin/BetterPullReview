#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_DRAFT_TOUR_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_DRAFT_TOUR_HPP

#include "bpr/core/diff_model.hpp"
#include "bpr/core/git_objects.hpp"
#include "bpr/core/local_draft_check.hpp"
#include "bpr/core/review_tour.hpp"
#include "bpr/core/tour_stop_acceptance.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief RFC-BPR-0110 ([[BPR-2026]], [[BPR-2027]]). A local draft's
/// description and Review Tour, prepared before any pull request exists.
/// \details The preparation belongs to the local draft, never to an account,
/// host or pull request: its stops bind to a candidate's exact lines by
/// ordinal, path and bytes, never to a guessed future commit. The full
/// description is the publication body's one authority; the tour's Problem
/// and Solution never overwrite it and it never overwrites them. Agent
/// proposals are inspectable records an author accepts or rejects; one made
/// against an earlier field revision or an earlier candidate is stale and
/// changes nothing. Conversations are scoped to the draft, the candidate,
/// the thread and the agent entry. The core decides; the host reads a
/// candidate's bytes and hands them in.
namespace bpr::core {
	struct local_draft;
	struct local_draft_candidate;

	/// \brief The description body's bound: GitHub's pull request body holds
	/// 65,536 characters, and a byte bound never admits more. Longer text is
	/// refused, never cut.
	inline constexpr std::size_t maximum_local_description_bytes = 65536U;
	/// \brief The most bytes of a stop's lines its binding keeps, as captured.
	inline constexpr std::size_t maximum_local_excerpt_bytes = 16U * 1024U;
	inline constexpr std::size_t maximum_local_proposals     = 256U;
	inline constexpr std::size_t maximum_local_conversations = 256U;
	/// \brief Turns one conversation keeps; a full one starts a new session.
	inline constexpr std::size_t maximum_local_turns      = 256U;
	inline constexpr std::size_t maximum_local_turn_bytes = 64U * 1024U;
	/// \brief The bound of a proposal's producer name and prompt version.
	inline constexpr std::size_t maximum_local_producer_bytes = 256U;
	/// \brief Replay's bounds, as a pull request's tour chat replays: the
	/// latest 12 turns within 48 KiB of context.
	inline constexpr std::size_t maximum_local_replay_turns = 12U;
	inline constexpr std::size_t maximum_local_replay_bytes = 48U * 1024U;

	// --- Binding to candidate material ----------------------------------------

	/// \brief One range of one candidate's file, with the exact bytes of its
	/// lines when it was bound. Lines are 1-based and inclusive.
	struct local_source_binding
	{
		std::uint64_t Candidate = 0;
		std::string   Path;
		diff_side     Side      = diff_side::right;
		std::uint32_t FirstLine = 0;
		std::uint32_t LastLine  = 0;
		/// The lines' bytes, each line with its terminator except perhaps the last.
		std::string Excerpt;
		auto        operator<=>(const local_source_binding&) const = default;
	};

	/// \brief How a stop's lines correspond to another candidate.
	enum class local_correspondence : std::uint8_t {
		/// The same bytes at one place in the same file.
		exact,
		/// The file is there and the lines are not.
		changed,
		/// The lines are there at more than one place.
		ambiguous,
		/// The candidate deletes the file.
		missing,
		/// The candidate's bytes for the file are not readable here (not
		/// among its changes, binary, or not kept).
		unavailable,
	};
	[[nodiscard]] auto name(local_correspondence aCorrespondence) noexcept -> std::string_view;

	/// \brief Where a binding lands on one candidate.
	struct local_destination
	{
		std::uint64_t        Candidate                                   = 0;
		local_correspondence Correspondence                              = local_correspondence::unavailable;
		std::uint32_t        FirstLine                                   = 0;
		std::uint32_t        LastLine                                    = 0;
		auto                 operator<=>(const local_destination&) const = default;
	};

	/// \brief One file of a candidate as the host read it: its text, or
	/// nothing when it is not readable, and whether the candidate deletes it.
	struct local_file_reading
	{
		std::string                Path;
		bool                       Deleted = false;
		std::optional<std::string> Text;
		auto                       operator<=>(const local_file_reading&) const = default;
	};

	/// \brief The exact lines [aFirst, aLast] of `aText`, or nothing outside it.
	[[nodiscard]] auto local_excerpt(std::string_view aText, std::uint32_t aFirst, std::uint32_t aLast) -> std::optional<std::string>;
	/// \brief How many lines `aText` holds.
	[[nodiscard]] auto local_line_count(std::string_view aText) noexcept -> std::uint32_t;

	/// \brief Where `aBinding`'s lines are in `aFile` of candidate `aCandidate`.
	/// \details The same bytes at the same lines, or at exactly one other
	/// place, are exact; several places are ambiguous and none is chosen; the
	/// nearest similar lines are never taken.
	[[nodiscard]] auto correspond(const local_source_binding& aBinding, std::uint64_t aCandidate, const local_file_reading& aFile) -> local_destination;

	// --- The tour ---------------------------------------------------------------

	struct local_tour_stop
	{
		/// Stable within the draft; never reused.
		std::uint64_t Id = 0;
		std::string   Title;
		std::string   Text;
		/// The lines as the author last chose them.
		local_source_binding Source;
		/// Bindings the author replaced, oldest first: the original material stays.
		std::vector<local_source_binding> Earlier;
		/// A question to the reviewer in its text asks for attention.
		bool Attention = false;
		/// Where its lines are on the draft's latest candidate.
		local_destination Current;
		/// Every text or binding edit advances it; a proposal names the one it read.
		std::uint64_t Revision = 1;
		/// The author's sign-off, on what `local_stop_fingerprint` read then.
		bool                         Accepted = false;
		std::array<std::uint8_t, 32> AcceptedFingerprint{ };
		auto                         operator<=>(const local_tour_stop&) const = default;
	};

	struct local_tour
	{
		std::string                  Problem;
		std::string                  Solution;
		std::uint64_t                ProblemRevision  = 1;
		std::uint64_t                SolutionRevision = 1;
		std::vector<local_tour_stop> Stops;
		std::uint64_t                NextStop                             = 1;
		auto                         operator<=>(const local_tour&) const = default;
	};

	/// \brief The draft's full publication body. The title is the draft's own.
	struct local_description
	{
		std::string   Body;
		std::uint64_t Revision                                    = 1;
		std::uint64_t TitleRevision                               = 1;
		auto          operator<=>(const local_description&) const = default;
	};

	// --- Proposals ----------------------------------------------------------------

	/// \brief What a proposal would change.
	enum class local_proposal_target : std::uint8_t {
		title,
		description,
		problem,
		solution,
		/// A stop's text (and title, when given).
		stop,
		/// A new stop at `Source`.
		new_stop,
	};
	[[nodiscard]] auto name(local_proposal_target aTarget) noexcept -> std::string_view;

	enum class local_proposal_state : std::uint8_t {
		pending,
		accepted,
		rejected,
		/// Its lines are not in the candidate it was made on; never accepted as written.
		outside_candidate,
	};
	[[nodiscard]] auto name(local_proposal_state aState) noexcept -> std::string_view;

	struct local_proposal
	{
		std::uint64_t         Id     = 0;
		local_proposal_target Target = local_proposal_target::description;
		/// The conversation it came from, or 0.
		std::uint64_t Conversation = 0;
		/// The candidate it was made on.
		std::uint64_t Candidate = 0;
		/// For `stop`, the stop.
		std::uint64_t Stop = 0;
		/// The revision of the field it read; 0 for a new stop.
		std::uint64_t BaseRevision = 0;
		std::string   Title;
		std::string   Text;
		/// For `new_stop`: the lines, bound on `Candidate` when valid.
		std::optional<local_source_binding> Source;
		/// The agent entry's name and the prompt version it ran under.
		std::string          Producer;
		std::string          Version;
		local_proposal_state State                                    = local_proposal_state::pending;
		auto                 operator<=>(const local_proposal&) const = default;
	};

	// --- Conversations --------------------------------------------------------------

	/// \brief What a question is about (RFC-BPR-0102's subjects; no scope list).
	enum class local_subject : std::uint8_t {
		whole_change,
		title,
		description,
		problem,
		solution,
		stop,
		/// Attached lines of the candidate.
		lines,
	};
	[[nodiscard]] auto name(local_subject aSubject) noexcept -> std::string_view;

	struct local_turn
	{
		/// The author's question, or the agent's answer.
		bool          Author  = true;
		local_subject Subject = local_subject::whole_change;
		std::string   Text;
		/// Proposals the answer made.
		std::vector<std::uint64_t> Proposals;
		/// Its cost in millionths of a dollar, when the agent reported one.
		std::optional<std::uint64_t> CostMicros;
		std::int64_t                 At = 0;
		/// RFC-BPR-0120. An answer that is a check's plan card or its report.
		std::optional<local_check> Check;
		auto                       operator<=>(const local_turn&) const = default;
	};

	/// \brief One conversation: one candidate, one thread and one agent entry.
	/// Nothing here names an account, host or pull request.
	struct local_conversation
	{
		std::uint64_t Id        = 0;
		std::uint64_t Candidate = 0;
		/// `change`, `problem`, `solution`, or `stop:N` for a stop's own chat.
		std::string Thread;
		/// The agent entry's identity and its name when the session began.
		std::string Agent;
		std::string AgentName;
		/// The agent's own resume handle, when it gave one.
		std::string             Handle;
		std::vector<local_turn> Turns;
		/// New session ended it; it stays readable.
		bool Ended                                        = false;
		auto operator<=>(const local_conversation&) const = default;
	};

	/// \brief The session scope a local-draft conversation is keyed by:
	/// profile-private, with no account, host or pull request in it.
	[[nodiscard]] auto                local_session_scope(std::string_view aDraft, std::uint64_t aCandidate, std::string_view aThread, std::string_view aAgent) -> std::string;
	[[nodiscard]] auto                local_stop_thread(std::uint64_t aStop) -> std::string;
	inline constexpr std::string_view local_change_thread = "change";

	// --- The preparation ------------------------------------------------------------

	struct local_draft_preparation
	{
		local_description               Description;
		local_tour                      Tour;
		std::vector<local_proposal>     Proposals;
		std::uint64_t                   NextProposal = 1;
		std::vector<local_conversation> Conversations;
		std::uint64_t                   NextConversation = 1;
		/// RFC-BPR-0120. Each agent's two grants for this draft's checks.
		std::vector<local_check_grants> CheckGrants;
		auto                            operator<=>(const local_draft_preparation&) const = default;
	};
	/// \brief Every bound of RFC-BPR-0110 and of the tour (256 stops, 64 KiB a
	/// field, 32 related ranges, 4 MiB in all) holds and every identity is well formed.
	[[nodiscard]] auto valid(const local_draft_preparation& aPreparation) -> bool;
	[[nodiscard]] auto encode_draft_preparation(const local_draft_preparation& aPreparation) -> std::string;
	[[nodiscard]] auto decode_draft_preparation(std::string_view aText) -> std::optional<local_draft_preparation>;

	// --- Edits ------------------------------------------------------------------------

	enum class preparation_refusal : std::uint8_t {
		/// The draft changed since it was read.
		stale,
		/// Longer than the field's bound; nothing is cut.
		too_long,
		invalid,
		/// No stop, proposal or conversation has that identity.
		unknown,
		/// The tour or the proposals hold as many as they keep.
		full,
		/// The lines are not in the candidate.
		outside_candidate,
		/// The field or candidate changed since the proposal was made.
		proposal_stale,
		/// The proposal was decided already.
		decided,
		/// Accept needs lines on the latest candidate: these changed, are
		/// ambiguous, missing or unavailable there.
		lines_needed,
		/// Accept needs a title and an explanation.
		text_needed,
	};
	[[nodiscard]] auto name(preparation_refusal aRefusal) noexcept -> std::string_view;

	template <typename T>
	using preparation_result = std::expected<T, preparation_refusal>;

	/// \brief Saves the title and the full description body as one explicit edit.
	[[nodiscard]] auto edit_description(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string aTitle, std::string aBody, std::int64_t aNow)
		-> preparation_result<local_draft>;
	/// \brief Saves the tour's Problem and Solution; the description is untouched.
	[[nodiscard]] auto edit_tour_brief(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string aProblem, std::string aSolution, std::int64_t aNow)
		-> preparation_result<local_draft>;
	/// \brief Copies a passage the author chose into the description, at its
	/// end: the deliberate transfer. Nothing moves the other way implicitly.
	[[nodiscard]] auto transfer_to_description(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string_view aPassage, std::int64_t aNow)
		-> preparation_result<local_draft>;

	/// \brief A new stop on lines of the latest candidate, read by the host
	/// from `aFile`. It is not accepted.
	[[nodiscard]] auto add_local_stop(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string aTitle, std::string aText, const local_file_reading& aFile, diff_side aSide,
									  std::uint32_t aFirst, std::uint32_t aLast, std::int64_t aNow) -> preparation_result<local_draft>;
	/// \brief Saves a stop's title and text and, when `aFile` is given, new
	/// lines on the latest candidate. Save is the author's sign-off: the
	/// stop is Accepted on what it holds after the save (RFC-BPR-0106).
	[[nodiscard]] auto save_local_stop(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aStop, std::string aTitle, std::string aText,
									   const std::optional<local_file_reading>& aFile, diff_side aSide, std::uint32_t aFirst, std::uint32_t aLast, std::int64_t aNow)
		-> preparation_result<local_draft>;
	/// \brief The pull request window's Tour panel saves a stop's title and
	/// text as they are typed, and Use Selected Lines rebinds it to new lines
	/// of the latest candidate (`aFile`); neither is the author's sign-off, so
	/// the stop is not accepted here (Save, then Accept, accepts it). An
	/// accepted stop whose title, text or lines change turns yellow, edited.
	[[nodiscard]] auto edit_local_stop(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aStop, std::string aTitle, std::string aText,
									   const std::optional<local_file_reading>& aFile, diff_side aSide, std::uint32_t aFirst, std::uint32_t aLast, std::int64_t aNow)
		-> preparation_result<local_draft>;
	[[nodiscard]] auto accept_local_stop(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aStop, std::int64_t aNow) -> preparation_result<local_draft>;
	/// \brief Mark not accepted: the author withdraws a stop's sign-off.
	[[nodiscard]] auto unaccept_local_stop(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aStop, std::int64_t aNow) -> preparation_result<local_draft>;
	[[nodiscard]] auto remove_local_stop(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aStop, std::int64_t aNow) -> preparation_result<local_draft>;
	/// \brief Moves a stop to `aIndex` in the route.
	[[nodiscard]] auto move_local_stop(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aStop, std::size_t aIndex, std::int64_t aNow)
		-> preparation_result<local_draft>;

	/// \brief Recomputes every stop's correspondence on the latest candidate
	/// from the host's readings of that candidate's files. Call after a
	/// successor candidate is recorded; the stops' own bindings never move.
	[[nodiscard]] auto carry_local_tour(const local_draft& aDraft, std::span<const local_file_reading> aLatest) -> local_draft;

	/// \brief Why a local stop is yellow, first reason first.
	enum class local_stop_reason : std::uint8_t {
		none,
		not_accepted,
		/// Its title, text or lines differ from what was accepted.
		edited,
		/// Its lines on the latest candidate: changed, ambiguous, missing or unavailable.
		changed,
		ambiguous,
		missing,
		unavailable,
	};
	[[nodiscard]] auto name(local_stop_reason aReason) noexcept -> std::string_view;
	struct local_stop_decision
	{
		tour_acceptance_state State                                         = tour_acceptance_state::not_accepted;
		local_stop_reason     Reason                                        = local_stop_reason::not_accepted;
		auto                  operator<=>(const local_stop_decision&) const = default;
	};
	[[nodiscard]] auto decide_local_stop(const local_tour_stop& aStop) -> local_stop_decision;
	/// \brief The tour head's words: its stops and how many are still to accept.
	struct local_tour_head
	{
		std::size_t Stops                                     = 0;
		std::size_t ToAccept                                  = 0;
		auto        operator<=>(const local_tour_head&) const = default;
	};
	[[nodiscard]] auto decide_local_tour_head(const local_draft& aDraft) -> local_tour_head;

	// --- Proposals ------------------------------------------------------------------

	/// \brief What an agent's answer proposed, before it is recorded.
	struct local_proposal_draft
	{
		local_proposal_target Target = local_proposal_target::description;
		std::uint64_t         Stop   = 0;
		std::string           Title;
		std::string           Text;
		/// For a new stop.
		std::string   Path;
		std::uint32_t FirstLine                                      = 0;
		std::uint32_t LastLine                                       = 0;
		auto          operator<=>(const local_proposal_draft&) const = default;
	};
	/// \brief Records proposals an answer made on candidate `aCandidate`, each
	/// against the revision of the field as it is now. A new stop's lines are
	/// bound from `aFiles` (that candidate's readings); lines outside them
	/// are recorded as `outside_candidate`, inspectable and never accepted.
	[[nodiscard]] auto record_proposals(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aConversation, std::uint64_t aCandidate,
										std::span<const local_proposal_draft> aProposals, std::span<const local_file_reading> aFiles, std::string_view aProducer,
										std::string_view aVersion, std::int64_t aNow) -> preparation_result<std::pair<local_draft, std::vector<std::uint64_t>>>;
	/// \brief Accepts a proposal: puts it in the draft. A field edited since,
	/// or a candidate refreshed since, makes it stale and changes nothing.
	[[nodiscard]] auto accept_proposal(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aProposal, std::int64_t aNow) -> preparation_result<local_draft>;
	/// \brief The field it read was edited since, or a later candidate exists:
	/// it stays readable and Accept is refused.
	[[nodiscard]] auto proposal_stale(const local_draft& aDraft, const local_proposal& aProposal) -> bool;
	[[nodiscard]] auto reject_proposal(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aProposal, std::int64_t aNow) -> preparation_result<local_draft>;

	// --- Conversations ---------------------------------------------------------------

	/// \brief The conversation a question on `aThread` with `aAgent` continues
	/// on the latest candidate, if one is open.
	[[nodiscard]] auto current_conversation(const local_draft& aDraft, std::string_view aThread, std::string_view aAgent) -> const local_conversation*;
	/// \brief Appends a turn, opening a conversation on the latest candidate
	/// when none is open for that thread and agent. Returns its identity.
	[[nodiscard]] auto append_turn(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string_view aThread, std::string_view aAgent, std::string_view aAgentName,
								   local_turn aTurn, std::int64_t aNow) -> preparation_result<std::pair<local_draft, std::uint64_t>>;
	/// \brief Records the agent's resume handle for a conversation.
	[[nodiscard]] auto record_handle(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aConversation, std::string aHandle, std::int64_t aNow)
		-> preparation_result<local_draft>;
	/// \brief RFC-BPR-0120. A check's question and its answer (a plan card
	/// or a report), appended to conversation `aConversation` whichever
	/// candidate it is on: a report on an older candidate is kept there.
	[[nodiscard]] auto append_check_turns(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aConversation, local_turn aQuestion, local_turn aAnswer,
										  std::int64_t aNow) -> preparation_result<local_draft>;
	/// \brief Discard, or Run: the plan card of turn `aTurn` of `aConversation`
	/// moves from planned to `aState`. A plan decided already is refused.
	[[nodiscard]] auto settle_check_plan(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aConversation, std::size_t aTurn, local_check_state aState,
										 std::int64_t aNow) -> preparation_result<local_draft>;
	/// \brief Remembers one agent's two grants for this draft.
	[[nodiscard]] auto remember_check_grants(const local_draft& aDraft, std::uint64_t aExpectedVersion, local_check_grants aGrants, std::int64_t aNow) -> preparation_result<local_draft>;
	/// \brief New session: ends the open conversation; it stays in history.
	[[nodiscard]] auto end_conversation(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aConversation, std::int64_t aNow) -> preparation_result<local_draft>;
	/// \brief The turns a replay sends: the latest within 12 turns and 48 KiB,
	/// oldest first, and how many were left out.
	struct local_replay
	{
		std::vector<local_turn> Turns;
		std::size_t             Omitted = 0;
	};
	[[nodiscard]] auto replay_turns(const local_conversation& aConversation) -> local_replay;
	/// \brief The cost of a conversation, in millionths of a dollar, when every
	/// answer reported one.
	[[nodiscard]] auto conversation_cost(const local_conversation& aConversation) -> std::optional<std::uint64_t>;

	// --- The request and the answer -----------------------------------------------------

	/// \brief A request's bound, as a pull request's tour chat's.
	inline constexpr std::size_t maximum_local_request_bytes = 160U * 1024U;

	/// \brief What one question sends: the prompt purpose's text, the
	/// question and its subject, the draft's own preparation, and only the
	/// captured candidate's bytes the host read from its store.
	struct local_question
	{
		local_subject Subject = local_subject::whole_change;
		std::string   Thread;
		/// For a stop's subject or thread.
		std::uint64_t Stop = 0;
		/// For attached lines.
		std::optional<local_source_binding> Lines;
		std::string                         Text;
		/// The prompt purpose's effective text and version.
		std::string Instructions;
		std::string Version;
		auto        operator<=>(const local_question&) const = default;
	};
	/// \brief The request's text and what it left out.
	struct local_request
	{
		/// [[BPR-2140]]. The prompt purpose's instructions and the answer
		/// format: the agent's system prompt, or pasted ahead of `Text`.
		std::string Instructions;
		/// The run's material: the draft, the files, the question.
		std::string Text;
		/// [[BPR-2138]]. The conversation's own session is resumed: it holds
		/// the earlier turns and the files already, so neither is sent again.
		bool Resumed = false;
		/// Changed files of the candidate sent whole, and in all.
		std::size_t FilesSent  = 0;
		std::size_t FilesTotal = 0;
		/// Earlier turns replayed, and left out.
		std::size_t Replayed = 0;
		std::size_t Omitted  = 0;
	};
	/// \brief Composes one request on the draft's latest candidate from
	/// `aFiles`, that candidate's readings from the private store (never the
	/// worktree), the subject's file first, within `maximum_local_request_bytes`.
	/// Absent when even the question and instructions do not fit.
	/// [[BPR-2138]]. A conversation with a handle resumes its session: no
	/// earlier turn and no file is sent again. One without a handle starts
	/// the session with its earlier turns replayed (a fresh first draft's
	/// question and answer among them: Ask more). No conversation is a fresh run.
	[[nodiscard]] auto compose_local_request(const local_draft& aDraft, const local_question& aQuestion, const local_conversation* aConversation,
											 std::span<const local_file_reading> aFiles) -> std::optional<local_request>;

	/// \brief An answer read: its prose, and the proposals of its one
	/// `bpr-proposals` block. A malformed block is left in the prose and
	/// proposes nothing.
	struct local_answer
	{
		std::string                       Prose;
		std::vector<local_proposal_draft> Proposals;
		bool                              Malformed = false;
	};
	[[nodiscard]] auto parse_local_answer(std::string_view aText) -> local_answer;

	// --- Publication ------------------------------------------------------------------

	/// \brief What a pull request created from this draft would carry, as
	/// exact text. Notes, conversations and proposals never enter it.
	struct local_publication_preview
	{
		std::string Title;
		/// The accepted full description, byte for byte.
		std::string Body;
		/// The tour as it would be published, with links still pending.
		std::string Tour;
		/// Links that exist only once the pull request's commits do.
		std::size_t PendingLinks = 0;
		/// Stops not yet Accepted: listed, they do not block draft creation.
		std::vector<std::uint64_t> Unaccepted;
		auto                       operator<=>(const local_publication_preview&) const = default;
	};
	[[nodiscard]] auto compose_local_publication(const local_draft& aDraft) -> local_publication_preview;

	/// \brief A pull request's result, as a publication returned it (or a
	/// course simulated it exactly): its identity, head and base, the body
	/// GitHub holds, and its files at the head.
	struct local_publication_result
	{
		std::string                     Host;
		std::string                     Owner;
		std::string                     Name;
		std::uint64_t                   Number = 0;
		git_commit_oid                  Head;
		git_commit_oid                  Base;
		std::string                     Body;
		std::vector<local_file_reading> Files;
		auto                            operator<=>(const local_publication_result&) const = default;
	};
	struct local_stop_mapping
	{
		std::uint64_t        Stop           = 0;
		local_correspondence Correspondence = local_correspondence::unavailable;
		/// The pull request's anchor; only for an exact correspondence.
		std::optional<tour_code_destination> Anchor;
		/// Its commit-pinned permalink; empty while the stop stays local.
		std::string Permalink;
		auto        operator==(const local_stop_mapping&) const -> bool = default;
	};
	struct local_publication_mapping
	{
		std::vector<local_stop_mapping> Stops;
		/// Stops that stay local and need attention.
		std::size_t Unresolved = 0;
		/// The pull request's body is the accepted description.
		bool DescriptionPreserved                                       = false;
		auto operator==(const local_publication_mapping&) const -> bool = default;
	};
	/// \brief Maps every stop to the pull request's exact head. A stop whose
	/// lines are not exactly there stays local with its reason; none is
	/// dropped or anchored elsewhere.
	[[nodiscard]] auto map_local_publication(const local_draft& aDraft, const local_publication_result& aResult) -> local_publication_mapping;

	/// \brief RFC-BPR-0110 X19: "Update PR description after publishing"
	/// starts off for a pull request associated with a local draft, on for
	/// every other.
	[[nodiscard]] constexpr auto update_description_by_default(const bool aAssociatedWithLocalDraft) noexcept -> bool { return !aAssociatedWithLocalDraft; }
}

#endif
