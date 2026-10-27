#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CHAT_SCOPE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CHAT_SCOPE_HPP

#include "bpr/core/agent_profile.hpp"
#include "bpr/core/agent_session.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/diff_semantics.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace bpr::core {
	/// \brief Every explicit AI action a reviewer may command.
	///
	/// \details The eight `docs/ux/ai-experience.md` names, and nothing else.
	/// A closed set is the point: an action nobody enumerated cannot be asked
	/// for, so "every AI action originates from an explicit visible user
	/// command" is decided here rather than trusted to whatever the adapter
	/// happens to wire up.
	///
	/// The order is the documented order and is load-bearing only in that the
	/// last enumerator pins the count below.
	enum class agent_entry_point : std::uint8_t {
		ask_about_selection,
		analyze_current_file,
		analyze_latest_upload,
		evaluate_proposed_fix,
		workshop_comment,
		run_ai_review,
		open_full_agent_session,
		/// [[BPR-1773]]. Ask about one conversation: a review thread or a
		/// private note, with the lines it is attached to. Its subject is the
		/// conversation the reviewer chose, never the selection.
		ask_about_conversation,
	};

	/// \brief The number of declared entry points.
	///
	/// \details Eight. Every switch over `agent_entry_point` in this slice
	/// omits a `default` label, so a ninth entry point fails the build until
	/// somebody says what it acts on and what it is called; this constant fails
	/// the build until somebody says it is a ninth.
	inline constexpr std::size_t agent_entry_point_count = 8U;

	static_assert(
		static_cast<std::size_t>(agent_entry_point::ask_about_conversation) + 1U
			== agent_entry_point_count,
		"agent_entry_point_count must cover every declared entry point");

	[[nodiscard]] auto stable_name(agent_entry_point aEntryPoint) noexcept
		-> std::string_view;

	/// \brief Whether the supplied value is one of the declared entry points.
	///
	/// \details A cast enumerator is not an action. Checked at runtime because
	/// an entry point arriving from a decoder or a widget is a number until
	/// something says otherwise.
	[[nodiscard]] auto declared(agent_entry_point aEntryPoint) noexcept -> bool;

	/// \brief Whether an entry point acts on what the reviewer has selected.
	///
	/// \details True for the three actions that have no subject without one:
	/// asking about the selection, analyzing the current file — the selection
	/// is the only input that names a file, so with nothing selected there is
	/// no current file and the core will not guess which one was meant — and
	/// evaluating a proposed fix, which is a fix *to* something.
	///
	/// False for the four that take their subject from the session's own
	/// `agent_revision_binding`: the latest upload, a workshopped comment
	/// (which may be a pull-request-level comment anchored to no line at all),
	/// the review, and a full agent session.
	[[nodiscard]] auto acts_on_selection(agent_entry_point aEntryPoint) noexcept
		-> bool;

	/// \brief Every operation the comment workshop may be asked to perform.
	///
	/// \details The six `docs/ux/ai-experience.md` names, in the documented
	/// order, and nothing else. The set is closed for the reason the entry
	/// points are: `workshop_comment` is one entry point, so what varies
	/// between one workshop request and the next is *this* value, and an
	/// operation nobody enumerated cannot be asked for. A seventh arrives by
	/// being named here — which fails every switch that omits a `default`
	/// label and the constant below — rather than by an adapter inventing a
	/// string the agent is left to interpret.
	enum class workshop_operation : std::uint8_t {
		clarify,
		shorten,
		make_actionable,
		phrase_as_a_question,
		make_more_direct,
		check_the_premise,
	};

	/// \brief The number of declared workshop operations.
	inline constexpr std::size_t workshop_operation_count = 6U;

	static_assert(
		static_cast<std::size_t>(workshop_operation::check_the_premise) + 1U
			== workshop_operation_count,
		"workshop_operation_count must cover every declared operation");

	[[nodiscard]] auto stable_name(workshop_operation aOperation) noexcept
		-> std::string_view;

	/// \brief Whether the supplied value is one of the declared operations.
	///
	/// \details A cast enumerator is not an operation, and is checked at
	/// runtime for the same reason a cast entry point is: an operation
	/// arriving from a decoder or a widget is a number until something says
	/// otherwise.
	[[nodiscard]] auto declared(workshop_operation aOperation) noexcept -> bool;

	/// \brief The operation one entry point may carry, and nothing else.
	///
	/// \details `workshop_comment` is the only entry point an operation means
	/// anything for, so this is where "carried through the existing entry
	/// point rather than as six new ones" is decided rather than left to each
	/// caller to remember. Every other entry point carries no operation, and
	/// an undeclared one carries none either — a supplement that named an
	/// operation for "run AI review" would be asking for an action nobody
	/// enumerated under the name of one that was.
	[[nodiscard]] auto workshop_operation_for(
		agent_entry_point                 aEntryPoint,
		std::optional<workshop_operation> aOperation) noexcept
		-> std::optional<workshop_operation>;

	/// \brief Something that happened while a review was open.
	///
	/// \details These are the four the RFC names as invoking nothing. They are
	/// declared here so that the sweep proving they invoke nothing has
	/// something to sweep, and so that a fifth one arrives through the same
	/// refusal rather than through a new path nobody reviewed.
	enum class agent_observation : std::uint8_t {
		refresh,
		navigation,
		comment_arrival,
		revision_update,
	};

	/// \brief The number of declared observations.
	inline constexpr std::size_t agent_observation_count = 4U;

	static_assert(
		static_cast<std::size_t>(agent_observation::revision_update) + 1U
			== agent_observation_count,
		"agent_observation_count must cover every declared observation");

	[[nodiscard]] auto stable_name(agent_observation aObservation) noexcept
		-> std::string_view;

	/// \brief All an observation may become: a record that it happened.
	///
	/// \details **There is no field here an invocation, an intent, or an entry
	/// point could occupy.** That absence is the whole design. An observation
	/// that carried an entry point would be an invocation waiting for someone
	/// to notice it was free, and the first convenience anybody adds is
	/// re-running the review on refresh.
	struct agent_observation_record
	{
		agent_observation Observation = agent_observation::refresh;

		auto operator<=>(const agent_observation_record&) const = default;
	};

	/// \brief Records that something happened, and starts nothing.
	[[nodiscard]] auto note_agent_observation(
		agent_observation aObservation) noexcept -> agent_observation_record;

	/// \brief One explicit reviewer command naming exactly one entry point.
	///
	/// \details Not default constructible, and not constructible from anything
	/// but an entry point. There is deliberately no way to obtain an intent
	/// without naming the action it is for: a default-constructed intent would
	/// mean "some AI action", which is the shape of the accidental invocation
	/// this whole RFC exists to make impossible.
	///
	/// The constructor is `explicit` so an entry point never converts into an
	/// intent at a call site. Passing `agent_entry_point::run_ai_review` where
	/// an intent is expected does not compile, so the command a reviewer gave
	/// is written out at the place it is acted on.
	class agent_action_intent
	{
	public:
		agent_action_intent() = delete;

		explicit constexpr agent_action_intent(
			const agent_entry_point aEntryPoint) noexcept
			: EntryPoint{ aEntryPoint }
		{
		}

		[[nodiscard]] constexpr auto entry_point() const noexcept
			-> agent_entry_point
		{
			return EntryPoint;
		}

		auto operator<=>(const agent_action_intent&) const = default;

	private:
		agent_entry_point EntryPoint;
	};

	/// \brief The intent an observation carries.
	///
	/// \returns Nothing, for every observation, unconditionally. The function
	/// exists so the refusal is one reviewable place instead of an absence
	/// somebody fills in locally; the day "refresh re-runs the review" is
	/// wanted, it has to be written here and argued about.
	[[nodiscard]] auto intent_for(
		const agent_observation_record& aRecord) noexcept
		-> std::optional<agent_action_intent>;

	/// \brief What the reviewer is pointing at, in RFC-BPR-0004 identities.
	///
	/// \details A file identity and a semantic selection — the same
	/// `semantic_selection` `agent_context_supplement` already admits. **There
	/// is no field here a coordinate could occupy**: not a y-position, not a
	/// viewport offset, not a widget row, not a ribbon path. A chat that
	/// remembered where a line was drawn would be about different code the
	/// moment anything above it moved.
	struct agent_selection
	{
		file_identity      File;
		semantic_selection Selection;

		auto operator<=>(const agent_selection&) const = default;
	};

	/// \brief Checks whether the supplied selection satisfies its contract.
	[[nodiscard]] auto valid(const agent_selection& aSelection) noexcept -> bool;

	/// \brief How much of the review one side-chat turn is about.
	///
	/// \details Ordered narrowest to widest, and the order *is* the widening
	/// ladder: `wider(a, b)` is a comparison of these enumerators. A chat
	/// starts at the reviewer's selection because that is where the reviewer
	/// already is, and everything past it is context the reviewer did not ask
	/// to send.
	enum class agent_chat_scope_kind : std::uint8_t {
		selected_lines,
		file,
		pull_request,
		repository,
	};

	/// \brief The number of declared chat scopes.
	inline constexpr std::size_t agent_chat_scope_kind_count = 4U;

	static_assert(
		static_cast<std::size_t>(agent_chat_scope_kind::repository) + 1U
			== agent_chat_scope_kind_count,
		"agent_chat_scope_kind_count must cover every declared scope");

	[[nodiscard]] auto stable_name(agent_chat_scope_kind aKind) noexcept
		-> std::string_view;

	/// \brief Whether the supplied value is one of the declared scopes.
	[[nodiscard]] auto declared(agent_chat_scope_kind aKind) noexcept -> bool;

	/// \brief Whether the left scope is strictly wider than the right one.
	[[nodiscard]] auto wider(
		agent_chat_scope_kind aLeft, agent_chat_scope_kind aRight) noexcept
		-> bool;

	/// \brief What one side-chat turn is about.
	///
	/// \details Exactly one shape is engaged per kind, checked by `valid`:
	///
	/// - `selected_lines` engages `Selection` and nothing else;
	/// - `file` engages `File` and `Comparison` and nothing else;
	/// - `pull_request` and `repository` engage nothing, because the session's
	///   own `agent_revision_binding` already names the host, the repository,
	///   and the pull request. A scope that named its own pull request could
	///   name a different one from the session it belongs to.
	///
	/// Every engaged member is an RFC-BPR-0004 identity.
	struct agent_chat_scope
	{
		agent_chat_scope_kind                 Kind = agent_chat_scope_kind::selected_lines;
		std::optional<agent_selection>        Selection;
		std::optional<file_identity>          File;
		std::optional<file_comparison_digest> Comparison;

		auto operator<=>(const agent_chat_scope&) const = default;
	};

	/// \brief Checks whether the supplied scope satisfies its contract.
	[[nodiscard]] auto valid(const agent_chat_scope& aScope) noexcept -> bool;

	/// \brief Where one side chat currently is, and where it was widened from.
	///
	/// \details `WidenedFrom` is engaged only after an explicit widening, and
	/// then it stays engaged: the drawer has to be able to say "widened from
	/// your selection" for as long as the widened scope lasts, and a record
	/// that expired after one turn would leave the reviewer sending a file to
	/// an agent with nothing on screen saying so.
	struct agent_chat_state
	{
		agent_chat_scope                Scope;
		std::optional<agent_chat_scope> WidenedFrom;

		auto operator<=>(const agent_chat_state&) const = default;
	};

	/// \brief Starts a side chat at the reviewer's own selection.
	///
	/// \returns Nothing when the selection is not a well-formed selection. A
	/// chat that started at a malformed scope would widen from one.
	[[nodiscard]] auto begin_agent_chat(const agent_selection& aSelection) noexcept
		-> std::optional<agent_chat_state>;

	/// \brief An explicit context action asking for a wider scope.
	///
	/// \details Not default constructible and `explicit`, for the same reason
	/// `agent_action_intent` is: widening sends the agent context the reviewer
	/// did not select, so it is never something a value arrives already
	/// carrying.
	class agent_scope_widening_intent
	{
	public:
		agent_scope_widening_intent() = delete;

		explicit constexpr agent_scope_widening_intent(
			const agent_chat_scope_kind aTarget) noexcept
			: Target{ aTarget }
		{
		}

		[[nodiscard]] constexpr auto target() const noexcept
			-> agent_chat_scope_kind
		{
			return Target;
		}

		auto operator<=>(const agent_scope_widening_intent&) const = default;

	private:
		agent_chat_scope_kind Target;
	};

	/// \brief Why a widening did not happen.
	enum class agent_scope_refusal : std::uint8_t {
		/// Nothing was refused. Either nothing was asked, or the widening held.
		none,
		/// The scope the chat is already at is not a well-formed scope.
		state_invalid,
		/// The target is the current scope or a narrower one. Narrowing is not
		/// a widening with the arrow reversed: a chat goes back to a selection
		/// by starting at one, which is a fresh explicit act.
		not_a_widening,
		/// The target is an enumerator nobody declared.
		unknown_scope,
	};

	/// \brief The number of declared scope refusals.
	inline constexpr std::size_t agent_scope_refusal_count = 4U;

	static_assert(
		static_cast<std::size_t>(agent_scope_refusal::unknown_scope) + 1U
			== agent_scope_refusal_count,
		"agent_scope_refusal_count must cover every declared scope refusal");

	[[nodiscard]] auto stable_name(agent_scope_refusal aRefusal) noexcept
		-> std::string_view;

	/// \brief The scope one further chat turn happens at.
	///
	/// \details A refused turn returns the state it was given, so a caller
	/// that ignores the refusal still does not widen.
	struct agent_chat_turn
	{
		bool                Widened = false;
		agent_scope_refusal Refusal = agent_scope_refusal::state_invalid;
		agent_chat_state    State;

		auto operator<=>(const agent_chat_turn&) const = default;
	};

	/// \brief Takes one chat turn, widening only if explicitly asked to.
	///
	/// \details The widening intent is an argument rather than a field of the
	/// state, so there is exactly one place a scope can change and it is named
	/// at every call site. With `std::nullopt` this is the identity on scope:
	/// no branch below it can widen, and neither navigating, scrolling, nor
	/// selecting something else is an argument to it. That is what "navigation
	/// does not silently add context" means when it is a property rather than
	/// a habit.
	///
	/// A widening records the scope it came from, so the turn that sent a
	/// whole file remembers that it began at eleven selected lines.
	[[nodiscard]] auto next_agent_chat_turn(
		const agent_chat_state&                           aState,
		const std::optional<agent_scope_widening_intent>& aWidening) noexcept
		-> agent_chat_turn;

	/// \brief Why one explicit action was refused.
	enum class agent_action_refusal : std::uint8_t {
		none,
		/// The profile is not permitted on this host. The verdict carries the
		/// `agent_invocation_verdict` that says which of those reasons it was.
		profile_refused,
		/// The session is not a usable session.
		session_invalid,
		/// The action names a profile the session was not started with. An
		/// action that switched harnesses mid-session would put half a
		/// conversation into a tool the other half never saw.
		session_profile_mismatch,
		/// The entry point acts on the reviewer's selection and there is none.
		selection_required,
		/// The intent names an entry point nobody declared.
		unknown_entry_point,
	};

	/// \brief The number of declared action refusals.
	inline constexpr std::size_t agent_action_refusal_count = 6U;

	static_assert(
		static_cast<std::size_t>(agent_action_refusal::unknown_entry_point) + 1U
			== agent_action_refusal_count,
		"agent_action_refusal_count must cover every declared action refusal");

	[[nodiscard]] auto stable_name(agent_action_refusal aRefusal) noexcept
		-> std::string_view;

	/// \brief The verdict on one explicit action.
	///
	/// \details `EntryPoint` is the action this verdict is about, copied from
	/// the intent in every state. A permitted verdict therefore authorizes
	/// exactly one action — the one that was asked for — and a caller cannot
	/// carry a permission for "run AI review" over to "open full agent
	/// session" without the mismatch being visible in the value.
	///
	/// `Invocation` is engaged once the host policy has been consulted, and
	/// absent when the action was refused before that. Absent is not a quiet
	/// "permitted": the default verdict here is a refusal, because a verdict
	/// that had to be filled in to mean "no" would eventually be returned
	/// half-filled.
	struct agent_action_verdict
	{
		bool                                    Permitted  = false;
		agent_action_refusal                    Refusal    = agent_action_refusal::session_invalid;
		agent_entry_point                       EntryPoint = agent_entry_point::ask_about_selection;
		std::optional<agent_invocation_verdict> Invocation;

		auto operator<=>(const agent_action_verdict&) const = default;
	};

	/// \brief Decides whether one explicit action may be invoked.
	///
	/// \details A sibling of `decide_agent_invocation` rather than a widening
	/// of it. That function answers "may this profile run on this host at
	/// all", is already called by the application host and described by the Qt
	/// launch panel, and giving it defaulted intent, selection, and session
	/// parameters would have created exactly the implied intent this slice
	/// forbids. This one answers "may this reviewer take this action now", and
	/// asks the older function for the half it already owns — so there is one
	/// host-policy rule, not two that can drift.
	///
	/// Pure, in the five supplied values: no process, no clock, no provider,
	/// no store. Decided in this order, and the order is the policy.
	///
	/// 1. Whether the intent names a declared entry point. An action nobody
	///    enumerated is refused before anything else is consulted, so a cast
	///    enumerator cannot reach a branch that permits it.
	/// 2. Whether the host permits the profile, by `decide_agent_invocation`.
	/// 3. Whether the session is usable and is this profile's session.
	/// 4. Whether the entry point has the selection it acts on.
	///
	/// \returns A verdict naming the entry point it decided, in every state.
	[[nodiscard]] auto decide_agent_action(
		const agent_profile&                    aProfile,
		const std::optional<agent_host_policy>& aPolicy,
		const std::optional<agent_selection>&   aSelection,
		const agent_session&                    aSession,
		const agent_action_intent&              aIntent) noexcept
		-> agent_action_verdict;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CHAT_SCOPE_HPP
