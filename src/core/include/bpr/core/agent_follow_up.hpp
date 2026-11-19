#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_FOLLOW_UP_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_FOLLOW_UP_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-2286]] (owner, 2026-11-19: "I want to be able to easily ask a
/// follow-up and perhaps update a suggested comment from a report or agent tab
/// response ... I also want to ask about a comment from a comment or draft
/// composer, and either be able to optionally update the draft or draft a
/// reply to a thread after one or more rounds of conversation with the
/// agent").
///
/// \details One follow-up is a conversation about one review item: a comment
/// an agent suggested (a finding of a report or of an Agent tab answer), a
/// draft the reviewer is writing, or a review thread. The core holds the
/// conversation's state, decides each round's message and identity, reads a
/// proposal from an answer and decides whether a proposal may be applied.
/// Each round is one fresh, tool-free run that carries the item and the
/// rounds so far, so the conversation never depends on an agent's own
/// session. Nothing here posts anything: an applied proposal replaces a
/// suggestion, or puts text in a composer, and only the reviewer publishes.
/// Pure.
namespace bpr::core {
	/// \brief What a follow-up is about.
	enum class follow_up_subject_kind : std::uint8_t {
		/// A comment an agent suggested for a finding, not yet drafted or
		/// posted.
		suggested_comment,
		/// A draft the reviewer is writing: a new comment or a reply.
		draft,
		/// A review thread on the pull request.
		thread,
	};
	inline constexpr std::size_t follow_up_subject_kind_count = 3U;
	static_assert(static_cast<std::size_t>(follow_up_subject_kind::thread) + 1U == follow_up_subject_kind_count, "follow_up_subject_kind_count must cover every kind");
	[[nodiscard]] auto stable_name(follow_up_subject_kind aKind) noexcept -> std::string_view;

	/// \brief What a proposal, once accepted, does.
	enum class follow_up_apply : std::uint8_t {
		/// Replaces the finding's suggested comment.
		update_suggestion,
		/// Replaces the draft's text in its composer.
		update_draft,
		/// Puts a reply in the thread's reply composer.
		draft_reply,
	};
	inline constexpr std::size_t follow_up_apply_count = 3U;
	static_assert(static_cast<std::size_t>(follow_up_apply::draft_reply) + 1U == follow_up_apply_count, "follow_up_apply_count must cover every action");
	[[nodiscard]] auto stable_name(follow_up_apply aApply) noexcept -> std::string_view;

	/// \brief What every round's run is told to be, as its system prompt.
	inline constexpr std::string_view follow_up_instructions
		= "You help a code reviewer think through one item of a pull request review: a comment an agent suggested, a draft the reviewer is writing, or a review conversation. "
		  "Each message gives you the item and the conversation so far. Answer plainly and briefly. You have no tools, and nothing you write is posted: the reviewer decides. "
		  "When you are asked to rewrite a comment or write a reply, answer with only that text, exactly as it would be posted.";
	/// \brief The first line of every follow-up message, by which it is known.
	inline constexpr std::string_view follow_up_marker = "Follow-up about one review item (bpr.follow-up.v1)";
	/// \brief The most bytes an item's text, a thread post or a question may
	/// carry.
	inline constexpr std::size_t maximum_follow_up_text_bytes = 16U * 1024U;
	/// \brief The most bytes a thread's posts carry together.
	inline constexpr std::size_t maximum_follow_up_thread_bytes = 32U * 1024U;
	/// \brief The most bytes the earlier rounds carry; older rounds past it
	/// are left out and said to be.
	inline constexpr std::size_t maximum_follow_up_transcript_bytes = 48U * 1024U;
	/// \brief The most rounds one follow-up holds.
	inline constexpr std::size_t maximum_follow_up_rounds = 32U;
	/// \brief The longest proposal: GitHub's comment body limit.
	inline constexpr std::size_t maximum_follow_up_proposal_characters = 65'536U;

	/// \brief One post of a thread, as the conversation shows it.
	struct follow_up_post
	{
		std::string Author;
		std::string Body;

		auto operator==(const follow_up_post&) const -> bool = default;
	};

	/// \brief The item, as the composition read it when the follow-up opened
	/// (and each time it was asked about again).
	struct follow_up_subject
	{
		follow_up_subject_kind Kind = follow_up_subject_kind::suggested_comment;
		/// The composition's own name for the item; one follow-up a key.
		std::string Key;
		/// Where it is: empty for the pull request; zero lines for a file.
		std::string   Path;
		std::uint32_t First = 0U;
		std::uint32_t Last  = 0U;
		/// The finding in a few words, or the thread's first words.
		std::string Title;
		/// The suggested comment, or the draft's text; empty for a thread.
		std::string Text;
		/// The thread's posts: a thread's, or the thread a draft replies in.
		std::vector<follow_up_post> Thread;
		/// A thread that takes replies: Draft reply is offered.
		bool Replies = false;

		auto operator==(const follow_up_subject&) const -> bool = default;
	};

	/// \brief Whether a subject names an item within its bounds.
	[[nodiscard]] auto valid(const follow_up_subject& aSubject) -> bool;

	/// \brief What may be applied from a follow-up about `aSubject`: Update
	/// suggested comment on a suggestion, Update draft on a draft, Draft reply
	/// on a thread that takes replies.
	[[nodiscard]] auto follow_up_offers(const follow_up_subject& aSubject) -> std::vector<follow_up_apply>;

	/// \brief One finished round: the reviewer's question and the answer.
	struct follow_up_round
	{
		std::string Question;
		std::string Answer;

		auto operator==(const follow_up_round&) const -> bool = default;
	};

	/// \brief A proposed text, waiting for the reviewer to use or discard it.
	struct follow_up_proposal
	{
		/// The run that wrote it; what Use and Discard name.
		std::uint64_t   Id    = 0U;
		follow_up_apply Apply = follow_up_apply::update_suggestion;
		std::string     Text;
		/// The text it replaces, as it was when the proposal was asked for:
		/// an apply over anything else is refused.
		std::string Base;

		auto operator==(const follow_up_proposal&) const -> bool = default;
	};

	/// \brief What the follow-up is doing.
	enum class follow_up_phase : std::uint8_t {
		idle,
		/// A question's run is going.
		asking,
		/// A proposal's run is going.
		proposing,
	};

	/// \brief Why a step did nothing, or none.
	enum class follow_up_refusal : std::uint8_t {
		none,
		/// A run is going.
		busy,
		/// The question is blank or longer than one message carries.
		empty_question,
		/// The action is not offered for this item.
		not_offered,
		/// The conversation holds as many rounds as one follow-up keeps.
		too_many_rounds,
		/// The answer, or the apply, names a run that is not the one going or
		/// the proposal shown.
		stale,
		/// There is no proposal to use.
		no_proposal,
		/// The agent answered nothing.
		empty_answer,
		/// The proposal is the text it would replace.
		unchanged,
		/// The proposal is longer than GitHub keeps.
		too_long,
		/// The text the proposal replaces changed since it was asked for.
		target_changed,
		/// The run failed or was stopped.
		failed,
	};
	inline constexpr std::size_t follow_up_refusal_count = 12U;
	static_assert(static_cast<std::size_t>(follow_up_refusal::failed) + 1U == follow_up_refusal_count, "follow_up_refusal_count must cover every refusal");
	[[nodiscard]] auto stable_name(follow_up_refusal aRefusal) noexcept -> std::string_view;

	/// \brief One follow-up's whole state.
	struct follow_up_state
	{
		follow_up_subject            Subject;
		std::vector<follow_up_round> Rounds;
		follow_up_phase              Phase = follow_up_phase::idle;
		/// The run going; 0 when none.
		std::uint64_t Pending = 0U;
		/// The question going, or the action a proposal run is for.
		std::string                       PendingQuestion;
		std::optional<follow_up_apply>    PendingApply;
		std::string                       PendingBase;
		std::optional<follow_up_proposal> Proposal;
		/// Why the last step did nothing; none after one that did something.
		follow_up_refusal Refused = follow_up_refusal::none;
		/// Proposals used.
		std::uint64_t Applied = 0U;
		std::uint64_t NextId  = 1U;

		auto operator==(const follow_up_state&) const -> bool = default;
	};

	/// \brief A run to make: its identity and its message.
	struct follow_up_request
	{
		std::uint64_t Id = 0U;
		std::string   Message;

		auto operator==(const follow_up_request&) const -> bool = default;
	};

	/// \brief What one step decided: a run to make, or why none.
	struct follow_up_step
	{
		follow_up_refusal                Refusal = follow_up_refusal::none;
		std::optional<follow_up_request> Request;

		auto operator==(const follow_up_step&) const -> bool = default;
	};

	/// \brief A follow-up about `aSubject`, with no rounds.
	[[nodiscard]] auto open_follow_up(follow_up_subject aSubject) -> follow_up_state;

	/// \brief The item was asked about again: its text and posts as they are
	/// now. The rounds and any proposal stay; a subject with another key is
	/// ignored. \returns Whether it was taken.
	auto refresh_follow_up(follow_up_state& aState, follow_up_subject aSubject) -> bool;

	/// \brief Asks one question in the conversation.
	[[nodiscard]] auto ask_follow_up(follow_up_state& aState, std::string_view aQuestion) -> follow_up_step;

	/// \brief Asks for the text `aApply` would put in place, from the
	/// conversation so far. `aBase` is that place's text now: the suggestion
	/// or the draft (an Update), or the reply composer's (Draft reply, which
	/// adds to it and is never refused for it).
	[[nodiscard]] auto propose_follow_up(follow_up_state& aState, follow_up_apply aApply, std::string aBase) -> follow_up_step;

	/// \brief The run `aId` ended: its answer, or none when it failed or was
	/// stopped. A question's answer becomes a round; a proposal's answer is
	/// read (`read_follow_up_proposal`) and shown, or refused.
	/// \returns `stale` for a run that is not the one going (and nothing
	/// changes), the proposal's refusal, `failed`, or none.
	auto settle_follow_up(follow_up_state& aState, std::uint64_t aId, std::optional<std::string> aAnswer) -> follow_up_refusal;

	/// \brief What Use decided: the text to put in place, where, or why not.
	struct follow_up_application
	{
		follow_up_refusal Refusal = follow_up_refusal::none;
		follow_up_apply   Apply   = follow_up_apply::update_suggestion;
		std::string       Key;
		std::string       Text;

		auto operator==(const follow_up_application&) const -> bool = default;
	};

	/// \brief Use on proposal `aProposal`. `aCurrent` is the text it replaces
	/// as it is now (ignored for Draft reply): an Update over a text that
	/// changed since the proposal was asked for is refused, so nothing is
	/// silently replaced. A used Update becomes the item's text.
	[[nodiscard]] auto apply_follow_up(follow_up_state& aState, std::uint64_t aProposal, std::string_view aCurrent) -> follow_up_application;

	/// \brief Discard on proposal `aProposal`. \returns Whether it was shown.
	auto discard_follow_up(follow_up_state& aState, std::uint64_t aProposal) -> bool;

	/// \brief The text an answer proposes: trimmed, and unwrapped when the
	/// whole answer is one fenced block. None for an empty answer or one
	/// longer than GitHub keeps.
	[[nodiscard]] auto read_follow_up_proposal(std::string_view aAnswer) -> std::optional<std::string>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_FOLLOW_UP_HPP
