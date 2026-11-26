#include "bpr/core/chat_header.hpp"

#include <algorithm>

namespace bpr::core {
	namespace {
		/// The chosen agent as a sentence names it.
		[[nodiscard]] auto named(const std::string_view aAgent) -> std::string { return aAgent.empty() ? std::string{ "The agent" } : std::string{ aAgent }; }
	}

	auto chat_actions_offered(const chat_kind aKind) -> std::vector<chat_header_action>
	{
		using action = chat_header_action;
		switch (aKind) {
		case chat_kind::agent_tab:
			// The drawer closes the tab, and the pull request has no one
			// place to show.
			return { action::history, action::new_session, action::delete_all_sessions, action::copy_conversation, action::show_every_run };
		case chat_kind::tour_part:
			return { action::show, action::history, action::new_session, action::delete_all_sessions, action::other_tour_chats, action::copy_conversation, action::close };
		case chat_kind::whole_tour:
			return { action::history, action::new_session, action::delete_all_sessions, action::other_tour_chats, action::copy_conversation, action::close };
		case chat_kind::local_draft_discussion:
		case chat_kind::review_item:
			// One conversation, each round a fresh run: Copy is the header's
			// own tool, and there is no History to hold it.
			return { action::show, action::copy_conversation, action::close };
		}
		return { };
	}

	auto chat_header::offers(const chat_header_action aAction) const noexcept -> bool { return std::ranges::find(Offered, aAction) != Offered.end(); }

	auto chat_revision_words(const std::string_view aRevision) -> std::string { return std::string{ aRevision.substr(0U, 7U) }; }

	auto plan_chat_header(const chat_header_facts& aFacts) -> chat_header
	{
		using action        = chat_header_action;
		auto       header   = chat_header{ };
		const auto sessions = chat_keeps_sessions(aFacts.Kind);
		header.Offered      = chat_actions_offered(aFacts.Kind);
		if (!aFacts.CanShow) {
			std::erase(header.Offered, action::show);
		}
		header.Tool  = sessions ? chat_header_tool::history : chat_header_tool::copy_conversation;
		header.Close = header.offers(action::close);
		header.Show  = !header.offers(action::show) ? chat_show::none : aFacts.Kind == chat_kind::review_item ? chat_show::place
																											  : chat_show::in_code;
		// The Agent tab's row says the name the reviewer gave the open
		// session, and its tooltip the pull request it belongs to. A side
		// chat's row keeps saying which part or item it stands beside.
		header.SessionName = sessions ? aFacts.SessionName : std::string{ };
		if (aFacts.Kind == chat_kind::agent_tab && !header.SessionName.empty()) {
			header.Title     = header.SessionName;
			header.TitleNote = aFacts.Subject.empty() ? std::string{ } : header.SessionName + ", in " + aFacts.Subject;
		} else {
			header.Title     = aFacts.Subject;
			header.TitleNote = aFacts.About;
		}
		header.Place        = aFacts.Place;
		header.RevisionRead = sessions ? chat_revision_words(aFacts.RevisionRead) : std::string{ };
		if (!sessions && aFacts.Cost) {
			header.ToolNote = "This conversation has cost " + agent_cost_words(*aFacts.Cost) + " so far.";
		}
		return header;
	}

	auto chat_session_notice(const chat_continuation aContinuation, const std::string_view aAgent) -> std::string
	{
		switch (aContinuation) {
		case chat_continuation::native: break;
		case chat_continuation::carried_over:
			return "Carried over from a saved transcript. " + named(aAgent) + " has the questions and answers, not what the earlier agent read.";
		case chat_continuation::cannot_continue:
			// The owner accepted "Start a new session, or carry the
			// transcript over."; the action has since been named New session
			// wherever it shows.
			return named(aAgent) + " can't pick up this conversation where it stopped. Choose New session, or carry the transcript over.";
		}
		return { };
	}

	auto chat_carry_over_words(const std::string_view aAgent) -> std::string
	{
		return "Carry this conversation over to " + (aAgent.empty() ? std::string{ "the chosen agent" } : std::string{ aAgent });
	}

	auto chat_carry_over_tip(const std::size_t aTurnsLeftOut, const bool aTextCut) -> std::string
	{
		auto words = std::string{ "Long conversations are shortened: the oldest turns are left out." };
		if (aTurnsLeftOut == 1U) {
			words += " This one left out 1 older turn.";
		} else if (aTurnsLeftOut > 1U) {
			words += " This one left out " + std::to_string(aTurnsLeftOut) + " older turns.";
		}
		if (aTextCut) {
			words += " Some of its longest text was cut.";
		}
		return words;
	}

	auto chat_revision_read_words(const std::string_view aRevision) -> std::string
	{
		return aRevision.empty() ? std::string{ } : "Reads the pull request at " + chat_revision_words(aRevision);
	}
}
