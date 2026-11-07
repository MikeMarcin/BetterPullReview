#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_TOUR_AGENT_CONVERSATION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_TOUR_AGENT_CONVERSATION_HPP

#include "bpr/application/tour_agent_drafting.hpp"
#include "bpr/core/tour_agent_session.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>

/// \file
/// \brief [[BPR-1914]]. A turn of the tour conversation: what it asks the
/// tour session, and how it reads the answer.
///
/// \details The owner, 2026-11-06: "I want to ask questions, get back and
/// forth and have it update it." Each turn is a fresh read-only run in the
/// review's workspace, as a drafting run is. It is given the pull request,
/// the tour as it is now (each stop's number, place and explanation), the
/// earlier turns the core keeps (`core::tour_conversation_of`), and this
/// turn's question and passage. The conversation is replayed, not resumed:
/// a read-only run saves no session (`--no-session-persistence`), and the
/// same request works for every agent kind. Any change it proposes comes
/// back as a proposal the author decides part by part.
namespace bpr::application {
	/// \brief The schema an ask turn answers in.
	inline constexpr std::string_view tour_ask_schema = "bpr.tour-ask.v1";

	/// \brief The request of one ask turn. Nothing when the evidence is not
	/// valid UTF-8 or too large.
	/// [[BPR-1914]] v002. `aThread` is the part of the tour the turn was asked
	/// from; every turn names its thread, so a follow-up refers to its own.
	/// v003: `aConversation` is that thread's side chat alone, and `aUpdate`
	/// asks for the part rewritten from it.
	[[nodiscard]] auto build_tour_ask_prompt(std::string_view aTitle, std::string_view aDescription, std::span<const tour_draft_file> aFiles, const core::review_tour& aDraft, const core::tour_conversation& aConversation, const core::tour_thread& aThread,
											 const std::optional<core::tour_passage>& aPassage, std::string_view aQuestion, bool aUpdate = false) -> std::optional<std::string>;

	/// \brief Reads an ask turn's answer from the agent's last message: one
	/// JSON object of `tour_ask_schema`, alone or in one fenced block, with
	/// exactly its keys. Nothing for anything else, or for an empty reply.
	/// Paths, lines and stop numbers are not checked here; the core does
	/// (`core::propose_tour_changes`).
	[[nodiscard]] auto decode_tour_ask_answer(std::string_view aText) -> std::optional<core::tour_ask_answer>;
}

#endif
