#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_CHAT_SESSION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_CHAT_SESSION_HOST_HPP

#include "bpr/application/agent_finding_delivery.hpp"
#include "bpr/core/agent_chat_scope.hpp"
#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/agent_profile.hpp"
#include "bpr/core/agent_session.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::application {
	/// \brief Asks for one chat turn to be sent, as the reviewer typed it.
	///
	/// \details The application's restatement of `qt::send_chat_message_intent`,
	/// kept here for the reason `agent_finding_action` is: this host may not
	/// depend on the adapter that presents it. `Action` is a
	/// `core::agent_action_intent`, which is neither default constructible nor
	/// implicitly convertible from an entry point, so this type is not default
	/// constructible either — a send that named no action could not be built.
	struct agent_chat_send
	{
		core::agent_session_id    Session;
		core::agent_action_intent Action;
		/// The scope shown when the reviewer sent it, not one resolved later.
		core::agent_chat_scope Scope;
		std::string            Text;

		auto operator<=>(const agent_chat_send&) const = default;
	};

	/// \brief Asks for the chat's scope to widen, which is a context action.
	struct agent_chat_widening
	{
		core::agent_session_id            Session;
		core::agent_scope_widening_intent Widening;

		auto operator<=>(const agent_chat_widening&) const = default;
	};

	/// \brief Chooses how to continue a session that no longer matches.
	///
	/// \details `OfferedBinding` is the binding the reviewer was shown, and it
	/// is compared against the offer this host asks the core for. A choice
	/// carrying anything else is refused rather than acted on: the surface that
	/// offered a continuation and the host that performs it must be talking
	/// about the same revision.
	struct agent_chat_continuation_choice
	{
		core::agent_session_id       Session;
		core::agent_continuation     Choice = core::agent_continuation::start_new_session;
		core::agent_revision_binding OfferedBinding;

		auto operator<=>(const agent_chat_continuation_choice&) const = default;
	};

	/// \brief The reviewer's own prose, as the core admission function takes it.
	///
	/// \details Supplied by the composition and read at call time rather than
	/// captured, for the reason `agent_finding_host::review_ledger_store_supplier`
	/// is: the notes and the draft belong to whichever review is open, and a
	/// host that captured one set would hand the agent prose from a review
	/// nobody is looking at. Empty is a complete answer.
	struct agent_chat_private_prose
	{
		std::vector<core::supplement_note>  Notes;
		std::vector<core::supplement_draft> Drafts;

		auto operator<=>(const agent_chat_private_prose&) const = default;
	};

	/// \brief Why one side-chat gesture produced no turn and no session.
	///
	/// \details Separate from the core's own `agent_action_refusal`,
	/// `agent_scope_refusal`, and `agent_session_staleness` for the reason
	/// `agent_finding_action_refusal` is separate from
	/// `agent_finding_promotion_refusal`: these say *which question could not
	/// be put, or which core answer came back no*, and the core's own verdict
	/// travels beside them on the outcome rather than being folded into them.
	/// Folding the two would let "this host holds no session" read as "the core
	/// refused the action".
	enum class agent_chat_refusal : std::uint8_t {
		/// Not a refusal.
		none,
		/// This host holds no session, or has observed no revision to compare
		/// one against. There is nothing for a turn to be about.
		no_session,
		/// The gesture names a session this host is not holding. A turn sent
		/// against somebody else's session is a host mistake, and answering it
		/// with this session's agent is worse than answering it with nothing.
		other_session,
		/// The session no longer matches the revision under review and the
		/// reviewer has not chosen a continuation. `Staleness` carries the
		/// core's own reason.
		session_stale,
		/// `core::decide_agent_action` refused. `Action` carries its reason.
		action_refused,
		/// `core::next_agent_chat_turn` refused. `Scope` carries its reason.
		scope_refused,
		/// The core admitted none of the private prose this session's opt-in
		/// asked for, which is what an opt-in belonging to another session
		/// produces. A turn that quietly dropped the reviewer's draft would
		/// send a question they did not ask.
		private_context_refused,
		/// Nothing was typed. A turn with no message is not a message.
		empty_message,
		/// The choice is not one `core::offer_agent_continuations` offers.
		unknown_continuation,
		/// The choice was made against a binding the current offer does not
		/// name.
		binding_mismatch,
		/// The composed launch path refused the turn, or there is none. The
		/// reviewer is told rather than left watching a chat that did nothing.
		launch_refused,
	};

	/// \brief The number of declared chat refusals.
	inline constexpr std::size_t agent_chat_refusal_count = 11U;

	static_assert(
		static_cast<std::size_t>(agent_chat_refusal::launch_refused) + 1U
			== agent_chat_refusal_count,
		"agent_chat_refusal_count must cover every declared chat refusal");

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(agent_chat_refusal aRefusal) noexcept
		-> std::string_view;

	/// \brief One legal chat turn, ready for the launch path that already exists.
	///
	/// \details **There is no executable, no argument vector, no workspace, and
	/// no environment here.** This host decides *that* a turn is legal and what
	/// it is about; starting a child is `agent_invocation_host`'s job and stays
	/// there. That absence is what "no second launch path" is, as a property of
	/// the type rather than a convention the composition is trusted to keep.
	///
	/// `IncludeNotes` and `IncludeDraft` are what `core::admit_private_context`
	/// admitted for this exact session, never what the gesture asked for.
	struct agent_chat_turn_request
	{
		core::agent_session     Session;
		core::agent_entry_point EntryPoint
			= core::agent_entry_point::ask_about_selection;
		core::agent_chat_scope            Scope;
		core::agent_private_context_optin PrivateContext;
		bool                              IncludeNotes = false;
		bool                              IncludeDraft = false;
		/// What the reviewer typed, carried as a value. Whether it rides the
		/// protocol frame is `agent_launch`'s framing question and not this
		/// host's; nothing here reframes anything.
		std::string Message;

		auto operator<=>(const agent_chat_turn_request&) const = default;
	};

	/// \brief What one side-chat gesture produced, including nothing and why.
	///
	/// \details A refusal is a result, not an absence. The core's three
	/// vocabularies travel whole — the action verdict's reason, the staleness
	/// the offer reported, and the scope refusal — so a presentation names what
	/// the core decided rather than a summary this host wrote.
	struct agent_chat_outcome
	{
		core::agent_session_id     Session;
		agent_chat_refusal         Refusal = agent_chat_refusal::no_session;
		core::agent_action_refusal Action
			= core::agent_action_refusal::none;
		core::agent_session_staleness Staleness
			= core::agent_session_staleness::current;
		core::agent_scope_refusal Scope = core::agent_scope_refusal::none;
		/// Engaged when exactly one turn was handed to the launch path.
		std::optional<agent_chat_turn_request> Turn;
		/// Engaged when exactly one session was started by this gesture.
		std::optional<core::agent_session> Started;

		/// \brief Whether this gesture produced exactly one dispatched turn.
		[[nodiscard]] constexpr auto sent() const noexcept -> bool
		{
			return Refusal == agent_chat_refusal::none && Turn.has_value();
		}

		auto operator<=>(const agent_chat_outcome&) const = default;
	};

	/// \brief Everything the side chat is told to show, and nothing it computed.
	///
	/// \details The first five members of `qt::side_chat_projection`, which is
	/// the whole of what the core decides about one chat. The provider name,
	/// the Git host, the workspace, and the chips are the composition's to add;
	/// **there is no field here the revision now under review could occupy**,
	/// so no branch below this can retarget a session to a head the reviewer
	/// happens to be looking at.
	///
	/// `Continuations` is the core's own offer, with one field answered:
	/// `Required` is cleared once the reviewer has chosen a continuation,
	/// because "the reviewer has to answer" stops being true when they have.
	/// `Status` — the staleness verdict itself — is never touched, so a session
	/// that stopped matching still says so for as long as it does.
	struct agent_chat_projection
	{
		core::agent_session               Session;
		core::agent_continuation_offer    Continuations;
		core::agent_chat_state            Chat;
		core::agent_private_context_optin PrivateContext;
		core::agent_entry_point           EntryPoint
			= core::agent_entry_point::ask_about_selection;

		auto operator<=>(const agent_chat_projection&) const = default;
	};

	/// \brief Records directly observable side-chat session activity.
	struct agent_chat_session_statistics
	{
		std::uint64_t Intents = 0U;
		/// Turns handed to the launch path. One send, at most one turn.
		std::uint64_t Turns = 0U;
		/// Validated deliveries adopted as replies to this session's turns.
		std::uint64_t Replies  = 0U;
		std::uint64_t Refusals = 0U;
		/// Sessions this host started: the chat's first, and one per
		/// `start_new_session`. Adopting the identity the launch path allocated
		/// is not starting one, and is counted separately, so "a continuation
		/// choice starts exactly one session" is readable as a delta here.
		std::uint64_t SessionsStarted = 0U;
		std::uint64_t SessionsAdopted = 0U;
		std::uint64_t Continuations   = 0U;
		std::uint64_t Widenings       = 0U;
		/// Every outcome handed to the projection, refusal included. Equal to
		/// `Intents`, and asserted so, because an outcome that was computed and
		/// not presented is one the reviewer never saw.
		std::uint64_t PresentedOutcomes = 0U;
		std::uint64_t Presentations     = 0U;

		auto operator<=>(const agent_chat_session_statistics&) const = default;
	};

	/// \brief Where a chat's state is shown, and where its refusals are said.
	class agent_chat_projection_port
	{
	public:
		agent_chat_projection_port()                                  = default;
		agent_chat_projection_port(const agent_chat_projection_port&) = delete;
		auto operator=(const agent_chat_projection_port&)
			-> agent_chat_projection_port&                       = delete;
		agent_chat_projection_port(agent_chat_projection_port&&) = delete;
		auto operator=(agent_chat_projection_port&&)
			-> agent_chat_projection_port&    = delete;
		virtual ~agent_chat_projection_port() = default;

		/// \brief Adopts the session, offer, scope, and opt-in as they stand.
		virtual void present(const agent_chat_projection& aProjection) = 0;
		/// \brief Adopts what one gesture produced, or the reason it produced
		/// nothing.
		virtual void present(const agent_chat_outcome& aOutcome) = 0;
		/// \brief Reports that no chat remains presentable.
		virtual void clear() = 0;
	};

	/// \brief Owns one side-chat session and turns a gesture into one turn.
	///
	/// \details The chat half of RFC-BPR-0012. It holds the session, the
	/// revision it has observed, the scope the chat is at, and the reviewer's
	/// per-session private-prose opt-in — and it decides nothing itself. Every
	/// question it answers is put to the core, in this order and the order is
	/// the policy: `core::offer_agent_continuations` for whether a session that
	/// stopped matching may still be talked to at all,
	/// `core::decide_agent_action` for legality,
	/// `core::next_agent_chat_turn` for the scope, and
	/// `core::admit_private_context` for the prose.
	///
	/// Staleness is asked first deliberately. A reviewer whose head moved is
	/// owed "which revision are you asking about" before "is that action
	/// legal": answering the second first would refuse a send for a missing
	/// selection when the reason the selection is missing is the revision that
	/// moved under it.
	///
	/// **There is no process port, no launcher, no provider, and no workspace
	/// reachable from here.** A legal turn is handed to `turn_dispatcher`,
	/// which the composition points at the one `agent_invocation_host` that
	/// already exists, so a chat turn and a launch-panel start are the same
	/// launch and the same protocol path. That is why "no second launch path"
	/// survives composition instead of being restated in it.
	///
	/// **Head movement never retargets the session.** `observe` records the
	/// revision under review and nothing else; the session changes only when
	/// the reviewer chooses `start_new_session`, or when the launch path admits
	/// the session a dispatched turn runs under.
	class agent_chat_session_host final
	{
	public:
		/// \brief Hands one legal turn to the launch path that already exists.
		///
		/// \returns Whether the launch path accepted it. `false` is presented
		/// as `launch_refused` rather than counted as a turn.
		using turn_dispatcher
			= std::function<bool(const agent_chat_turn_request&)>;

		/// \brief Supplies the private prose open for the review right now.
		using private_prose_supplier
			= std::function<agent_chat_private_prose()>;

		agent_chat_session_host(
			std::vector<core::agent_profile>     aProfiles,
			std::vector<core::agent_host_policy> aPolicies,
			core::agent_profile_id               aDefaultProfile,
			turn_dispatcher                      aDispatcher,
			private_prose_supplier               aProse,
			agent_chat_projection_port&          aProjection);

		agent_chat_session_host(const agent_chat_session_host&) = delete;
		auto operator=(const agent_chat_session_host&)
			-> agent_chat_session_host&                    = delete;
		agent_chat_session_host(agent_chat_session_host&&) = delete;
		auto operator=(agent_chat_session_host&&)
			-> agent_chat_session_host& = delete;
		~agent_chat_session_host()      = default;

		/// \brief Records the revision now under review, and retargets nothing.
		///
		/// \details Starts the chat's first session when there is none, because
		/// a reviewer who opens a review has a chat to type into. It never
		/// replaces one: a session whose binding no longer matches becomes
		/// stale here, which is exactly what it is.
		void observe(const core::agent_revision_binding& aObserved);

		/// \brief Adopts the session the launch path admitted for a turn.
		///
		/// \details The only other way this host's session changes. The launch
		/// path allocates agent session identities, so a chat that kept its own
		/// would be talking about a session the delivered findings are not
		/// keyed to.
		void admitted(const core::agent_session& aSession);

		/// \brief Forgets the session, the observation, and the opt-in.
		void clear();

		/// \brief Adopts one validated delivery as a reply to this session.
		///
		/// \details The delivery is `deliver_agent_findings`' own product, so
		/// every reply this host counts has already passed the finding bounds
		/// BPR-0500 enforces. **Nothing here decodes, validates, or re-anchors
		/// anything**; a delivery for another session is not this chat's reply
		/// and is not counted as one.
		void deliver(const agent_finding_delivery& aDelivery);

		[[nodiscard]] auto send(const agent_chat_send& aSend)
			-> agent_chat_outcome;
		[[nodiscard]] auto widen(const agent_chat_widening& aWidening)
			-> agent_chat_outcome;
		[[nodiscard]] auto set_private_context(
			const core::agent_private_context_optin& aOptIn)
			-> agent_chat_outcome;
		[[nodiscard]] auto choose(
			const agent_chat_continuation_choice& aChoice)
			-> agent_chat_outcome;

		/// \brief Notes what the reviewer is pointing at, and asks nothing.
		///
		/// \details Separate from the scope on purpose. The entry points that
		/// act on a selection are refused without one by
		/// `core::decide_agent_action`, which takes the selection as its own
		/// argument; folding it into the scope would make navigating change
		/// what a chat is about.
		void observe_selection(
			const std::optional<core::agent_selection>& aSelection);

		[[nodiscard]] auto session() const noexcept
			-> const std::optional<core::agent_session>&;
		[[nodiscard]] auto observed() const noexcept
			-> const std::optional<core::agent_revision_binding>&;
		[[nodiscard]] auto projection() const -> std::optional<agent_chat_projection>;
		[[nodiscard]] auto statistics() const noexcept
			-> agent_chat_session_statistics;

	private:
		[[nodiscard]] auto profile_for(core::agent_profile_id aProfile) const
			-> std::optional<core::agent_profile>;
		[[nodiscard]] auto refuse(
			core::agent_session_id aSession, agent_chat_refusal aRefusal)
			-> agent_chat_outcome;
		[[nodiscard]] auto present(agent_chat_outcome aOutcome)
			-> agent_chat_outcome;
		void present_state();

		/// \brief Where a chat starts before the reviewer narrows it.
		///
		/// \details The pull request the session is already bound to, which
		/// engages no identity of its own. `agent_chat_scope_kind`'s own
		/// default enumerator is `selected_lines`, which engages a selection —
		/// so a default-constructed state is a malformed scope, and a chat that
		/// started there would widen from one.
		[[nodiscard]] static auto initial_chat_state() noexcept
			-> core::agent_chat_state;

		std::vector<core::agent_profile>     Profiles;
		std::vector<core::agent_host_policy> Policies;
		/// The profile the chat's own first session is started with, chosen by
		/// the composition rather than named here: which harness a review opens
		/// against is a composition decision, and a default written into this
		/// host would be a second place it is decided.
		core::agent_profile_id                      DefaultProfile;
		turn_dispatcher                             Dispatcher;
		private_prose_supplier                      Prose;
		agent_chat_projection_port&                 Projection;
		std::optional<core::agent_session>          Session;
		std::optional<core::agent_revision_binding> Observed;
		std::optional<core::agent_selection>        Selection;
		core::agent_chat_state                      Chat = initial_chat_state();
		core::agent_private_context_optin           OptIn;
		/// The continuation the reviewer chose against the current observation,
		/// cleared the moment the observed revision moves again. A choice that
		/// outlived the revision it answered would be an answer to a question
		/// nobody asked twice.
		std::optional<core::agent_continuation> Chosen;
		/// Whether a dispatched turn is still waiting for its delivery.
		bool                          AwaitingReply = false;
		agent_chat_session_statistics Statistics;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_CHAT_SESSION_HOST_HPP
