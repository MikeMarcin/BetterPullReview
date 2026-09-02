#include "bpr/application/agent_chat_session_host.hpp"

#include <algorithm>
#include <utility>

namespace bpr::application {
	namespace {
		/// \brief Whether the supplied value is one of the two core declares.
		///
		/// \details `agent_continuation` is a closed pair and there is no third,
		/// so a choice outside it arrived from a cast rather than from a
		/// reviewer. Checked here because a continuation reaching this host from
		/// a widget is a number until something says otherwise.
		[[nodiscard]] constexpr auto declared(
			const core::agent_continuation aChoice) noexcept -> bool
		{
			return aChoice == core::agent_continuation::start_new_session
				|| aChoice
				== core::agent_continuation::continue_against_old_revision;
		}
	} // namespace

	auto name(const agent_chat_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case agent_chat_refusal::none:
			return "none";
		case agent_chat_refusal::no_session:
			return "no_session";
		case agent_chat_refusal::other_session:
			return "other_session";
		case agent_chat_refusal::session_stale:
			return "session_stale";
		case agent_chat_refusal::action_refused:
			return "action_refused";
		case agent_chat_refusal::scope_refused:
			return "scope_refused";
		case agent_chat_refusal::private_context_refused:
			return "private_context_refused";
		case agent_chat_refusal::empty_message:
			return "empty_message";
		case agent_chat_refusal::unknown_continuation:
			return "unknown_continuation";
		case agent_chat_refusal::binding_mismatch:
			return "binding_mismatch";
		case agent_chat_refusal::launch_refused:
			return "launch_refused";
		}
		return { };
	}

	agent_chat_session_host::agent_chat_session_host(
		std::vector<core::agent_profile>     aProfiles,
		std::vector<core::agent_host_policy> aPolicies,
		const core::agent_profile_id         aDefaultProfile,
		turn_dispatcher                      aDispatcher,
		private_prose_supplier               aProse,
		agent_chat_projection_port&          aProjection)
		: Profiles{ std::move(aProfiles) }
		, Policies{ std::move(aPolicies) }
		, DefaultProfile{ aDefaultProfile }
		, Dispatcher{ std::move(aDispatcher) }
		, Prose{ std::move(aProse) }
		, Projection{ aProjection }
	{
	}

	auto agent_chat_session_host::initial_chat_state() noexcept
		-> core::agent_chat_state
	{
		return core::agent_chat_state{
			.Scope = core::agent_chat_scope{
				.Kind = core::agent_chat_scope_kind::pull_request,
			},
		};
	}

	auto agent_chat_session_host::profile_for(
		const core::agent_profile_id aProfile) const
		-> std::optional<core::agent_profile>
	{
		const auto found = std::ranges::find(
			Profiles, aProfile, &core::agent_profile::Id);
		if (found == Profiles.end()) {
			return std::nullopt;
		}
		return *found;
	}

	auto agent_chat_session_host::refuse(
		const core::agent_session_id aSession,
		const agent_chat_refusal     aRefusal) -> agent_chat_outcome
	{
		return present(agent_chat_outcome{
			.Session = aSession,
			.Refusal = aRefusal,
		});
	}

	auto agent_chat_session_host::present(agent_chat_outcome aOutcome)
		-> agent_chat_outcome
	{
		if (aOutcome.Refusal != agent_chat_refusal::none) {
			++Statistics.Refusals;
		}
		++Statistics.PresentedOutcomes;
		Projection.present(aOutcome);
		return aOutcome;
	}

	void agent_chat_session_host::present_state()
	{
		const auto shown = projection();
		if (!shown.has_value()) {
			Projection.clear();
			return;
		}
		++Statistics.Presentations;
		Projection.present(*shown);
	}

	void agent_chat_session_host::observe(
		const core::agent_revision_binding& aObserved)
	{
		// Recording the revision under review is all an observation may become
		// here. There is no branch below this that retargets a session, and no
		// argument to it that could: a session whose binding stopped matching
		// becomes stale, which is what it is.
		if (Observed.has_value() && *Observed != aObserved) {
			// A continuation answered the revision it was chosen against. The
			// revision moved again, so it is asked again.
			Chosen.reset();
		}
		Observed = aObserved;
		if (!Session.has_value()) {
			Session = core::agent_session{
				.Id      = core::agent_session_id{ 1U },
				.Profile = DefaultProfile,
				.Binding = aObserved,
			};
			OptIn = core::agent_private_context_optin{ .Session = Session->Id };
			++Statistics.SessionsStarted;
		}
		present_state();
	}

	void agent_chat_session_host::admitted(const core::agent_session& aSession)
	{
		if (!core::valid_agent_session(aSession)) {
			return;
		}
		// The launch path allocates agent session identities, and the findings
		// it delivers are keyed to the one it allocated. A chat that kept its
		// own identity here would count somebody else's delivery as its reply,
		// or its own as nobody's.
		const auto adopted = !Session.has_value() || Session->Id != aSession.Id;
		Session            = aSession;
		if (OptIn.Session != aSession.Id) {
			// An opt-in made for the previous conversation admits nothing into
			// this one, which is the whole reason the core keys it by session.
			OptIn = core::agent_private_context_optin{ .Session = aSession.Id };
		}
		if (adopted) {
			++Statistics.SessionsAdopted;
		}
		present_state();
	}

	void agent_chat_session_host::clear()
	{
		Session.reset();
		Observed.reset();
		Selection.reset();
		Chosen.reset();
		Chat          = initial_chat_state();
		OptIn         = core::agent_private_context_optin{ };
		AwaitingReply = false;
		Projection.clear();
	}

	void agent_chat_session_host::deliver(
		const agent_finding_delivery& aDelivery)
	{
		// The delivery is `deliver_agent_findings`' own product and there is no
		// other way to obtain one, so a reply counted here has already passed
		// the finding bounds. Nothing in this function decodes, validates, or
		// re-anchors anything.
		if (!Session.has_value() || aDelivery.Session != Session->Id) {
			return;
		}
		++Statistics.Replies;
		AwaitingReply = false;
	}

	void agent_chat_session_host::observe_selection(
		const std::optional<core::agent_selection>& aSelection)
	{
		Selection = aSelection;
	}

	auto agent_chat_session_host::send(const agent_chat_send& aSend)
		-> agent_chat_outcome
	{
		++Statistics.Intents;
		if (!Session.has_value() || !Observed.has_value()) {
			return refuse(aSend.Session, agent_chat_refusal::no_session);
		}
		if (aSend.Session != Session->Id) {
			return refuse(aSend.Session, agent_chat_refusal::other_session);
		}
		if (aSend.Text.empty()) {
			return refuse(aSend.Session, agent_chat_refusal::empty_message);
		}

		// 1. Whether a session that stopped matching may still be talked to.
		//    The offer is the core's, carried whole, and `Required` is what
		//    says the reviewer has to answer before the chat goes on.
		const auto offer = core::offer_agent_continuations(*Session, *Observed);
		if (offer.Required && !Chosen.has_value()) {
			auto outcome = agent_chat_outcome{
				.Session   = aSend.Session,
				.Refusal   = agent_chat_refusal::session_stale,
				.Staleness = offer.Status.Staleness,
			};
			return present(std::move(outcome));
		}

		// 2. Whether the core permits this action, for this profile, on this
		//    host, in this session, with what the reviewer is pointing at. An
		//    unknown profile is handed to the core as the invalid value it is,
		//    so the refusal is still the core's rather than one written here.
		const auto profile = profile_for(Session->Profile);
		const auto verdict = core::decide_agent_action(
			profile.has_value() ? *profile : core::agent_profile{ },
			core::policy_for_host(Policies, Session->Binding.Host),
			Selection,
			*Session,
			aSend.Action);
		if (!verdict.Permitted) {
			auto outcome = agent_chat_outcome{
				.Session = aSend.Session,
				.Refusal = agent_chat_refusal::action_refused,
				.Action  = verdict.Refusal,
			};
			return present(std::move(outcome));
		}

		// 3. The scope one further turn happens at. With no widening intent
		//    this is the identity on scope, which is the point: sending is not
		//    an argument that can widen what a chat is about.
		const auto turn = core::next_agent_chat_turn(Chat, std::nullopt);
		if (turn.Refusal != core::agent_scope_refusal::none) {
			auto outcome = agent_chat_outcome{
				.Session = aSend.Session,
				.Refusal = agent_chat_refusal::scope_refused,
				.Scope   = turn.Refusal,
			};
			return present(std::move(outcome));
		}

		// 4. The private prose this exact session opted into. An opt-in
		//    belonging to another session admits none of it, and a turn that
		//    quietly dropped the draft the reviewer chose to send would ask a
		//    question they did not ask.
		const auto prose    = Prose ? Prose() : agent_chat_private_prose{ };
		const auto admitted = core::admit_private_context(
			*Session, OptIn, prose.Notes, prose.Drafts);
		const auto asked = (OptIn.Notes && !prose.Notes.empty())
			|| (OptIn.Drafts && !prose.Drafts.empty());
		if (asked && admitted.Notes.empty() && admitted.Drafts.empty()) {
			return refuse(
				aSend.Session, agent_chat_refusal::private_context_refused);
		}

		auto request = agent_chat_turn_request{
			.Session        = *Session,
			.EntryPoint     = verdict.EntryPoint,
			.Scope          = turn.State.Scope,
			.PrivateContext = OptIn,
			.IncludeNotes   = !admitted.Notes.empty(),
			.IncludeDraft   = !admitted.Drafts.empty(),
			.Message        = aSend.Text,
		};
		if (!Dispatcher || !Dispatcher(request)) {
			return refuse(aSend.Session, agent_chat_refusal::launch_refused);
		}
		++Statistics.Turns;
		AwaitingReply = true;
		Chat          = turn.State;
		auto outcome  = agent_chat_outcome{
			.Session = aSend.Session,
			.Refusal = agent_chat_refusal::none,
			.Turn    = std::move(request),
		};
		return present(std::move(outcome));
	}

	auto agent_chat_session_host::widen(const agent_chat_widening& aWidening)
		-> agent_chat_outcome
	{
		++Statistics.Intents;
		if (!Session.has_value() || !Observed.has_value()) {
			return refuse(aWidening.Session, agent_chat_refusal::no_session);
		}
		if (aWidening.Session != Session->Id) {
			return refuse(aWidening.Session, agent_chat_refusal::other_session);
		}
		const auto turn = core::next_agent_chat_turn(Chat, aWidening.Widening);
		if (turn.Refusal != core::agent_scope_refusal::none) {
			auto outcome = agent_chat_outcome{
				.Session = aWidening.Session,
				.Refusal = agent_chat_refusal::scope_refused,
				.Scope   = turn.Refusal,
			};
			return present(std::move(outcome));
		}
		Chat = turn.State;
		++Statistics.Widenings;
		present_state();
		return present(agent_chat_outcome{
			.Session = aWidening.Session,
			.Refusal = agent_chat_refusal::none,
		});
	}

	auto agent_chat_session_host::set_private_context(
		const core::agent_private_context_optin& aOptIn) -> agent_chat_outcome
	{
		++Statistics.Intents;
		if (!Session.has_value() || !Observed.has_value()) {
			return refuse(aOptIn.Session, agent_chat_refusal::no_session);
		}
		// Recorded as the reviewer gave it, including an opt-in naming another
		// session. Silently correcting it here would hide the mistake until a
		// turn carried prose nobody agreed to; the core refuses it on the next
		// send instead, and the reviewer is told which session it was for.
		OptIn = aOptIn;
		present_state();
		return present(agent_chat_outcome{
			.Session = aOptIn.Session,
			.Refusal = agent_chat_refusal::none,
		});
	}

	auto agent_chat_session_host::choose(
		const agent_chat_continuation_choice& aChoice) -> agent_chat_outcome
	{
		++Statistics.Intents;
		if (!Session.has_value() || !Observed.has_value()) {
			return refuse(aChoice.Session, agent_chat_refusal::no_session);
		}
		if (aChoice.Session != Session->Id) {
			return refuse(aChoice.Session, agent_chat_refusal::other_session);
		}
		if (!declared(aChoice.Choice)) {
			return refuse(
				aChoice.Session, agent_chat_refusal::unknown_continuation);
		}
		const auto offer = core::offer_agent_continuations(*Session, *Observed);
		if (
			std::ranges::find(offer.Choices, aChoice.Choice)
			== offer.Choices.end()) {
			return refuse(
				aChoice.Session, agent_chat_refusal::unknown_continuation);
		}
		if (aChoice.OfferedBinding != offer.ContinueAgainst) {
			return refuse(aChoice.Session, agent_chat_refusal::binding_mismatch);
		}
		++Statistics.Continuations;
		Chosen       = aChoice.Choice;
		auto outcome = agent_chat_outcome{
			.Session   = aChoice.Session,
			.Refusal   = agent_chat_refusal::none,
			.Staleness = offer.Status.Staleness,
		};
		if (aChoice.Choice == core::agent_continuation::start_new_session) {
			// Exactly one session, bound to what the reviewer is looking at
			// now. The identity is provisional until the launch path admits
			// the one a turn runs under; what is not provisional is that the
			// old session keeps the binding it was asked about.
			const auto started = core::agent_session{
				.Id      = core::agent_session_id{ Session->Id.value() + 1U },
				.Profile = Session->Profile,
				.Binding = *Observed,
			};
			Session = started;
			OptIn   = core::agent_private_context_optin{ .Session = started.Id };
			Chat    = core::agent_chat_state{ .Scope = Chat.Scope };
			++Statistics.SessionsStarted;
			outcome.Started = started;
		}
		present_state();
		return present(std::move(outcome));
	}

	auto agent_chat_session_host::session() const noexcept
		-> const std::optional<core::agent_session>&
	{
		return Session;
	}

	auto agent_chat_session_host::observed() const noexcept
		-> const std::optional<core::agent_revision_binding>&
	{
		return Observed;
	}

	auto agent_chat_session_host::projection() const
		-> std::optional<agent_chat_projection>
	{
		if (!Session.has_value() || !Observed.has_value()) {
			return std::nullopt;
		}
		auto offer = core::offer_agent_continuations(*Session, *Observed);
		if (Chosen.has_value()) {
			// The one field this host answers, and only because the reviewer
			// already did. `Status` is untouched, so a session that stopped
			// matching still says `head_moved` for as long as it has.
			offer.Required = false;
		}
		return agent_chat_projection{
			.Session        = *Session,
			.Continuations  = std::move(offer),
			.Chat           = Chat,
			.PrivateContext = OptIn,
			.EntryPoint     = core::agent_entry_point::ask_about_selection,
		};
	}

	auto agent_chat_session_host::statistics() const noexcept
		-> agent_chat_session_statistics
	{
		return Statistics;
	}
} // namespace bpr::application
