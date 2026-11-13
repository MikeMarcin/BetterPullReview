#include "bpr/core/prompt_macro.hpp"

/// \file
/// \brief RFC-BPR-0125. The shipped suggested prompts: the Agent tab's
/// ([[BPR-1872]], [[BPR-1773]]) and the tour chats' ([[BPR-1934]]), as
/// macros. With nothing stored each place offers what it offered before,
/// under the same facts, in the same order.
namespace bpr::core {
	namespace {
		/// A built-in whose name is its template.
		auto plain(std::string aId, const prompt_place aPlace, const std::string& aText, const prompt_subject aSubject, std::vector<prompt_condition> aConditions = { }, const prompt_kind aKind = prompt_kind::question) -> prompt_builtin
		{
			return {
				.Macro = {
					.Id      = std::move(aId),
					.BuiltIn = true,
					.Place   = aPlace,
					.Body    = { .Name = aText, .Template = aText, .Subject = aSubject, .Kind = aKind, .Conditions = std::move(aConditions) },
				},
			};
		}

		/// A built-in shown as `aName`, sending `aTemplate`.
		auto named(prompt_builtin aBuiltin, std::string aName) -> prompt_builtin
		{
			aBuiltin.Macro.Body.Name = std::move(aName);
			return aBuiltin;
		}

		auto is(const prompt_fact aFact) -> prompt_condition { return { .Fact = aFact }; }
		auto is_not(const prompt_fact aFact) -> prompt_condition { return { .Fact = aFact, .Negated = true }; }
		auto lines(const prompt_lines_value aValue) -> prompt_condition { return { .Fact = prompt_fact::selected_lines, .Lines = aValue }; }
	} // namespace

	auto shipped_prompt_macros() -> std::vector<prompt_builtin>
	{
		using enum prompt_subject;
		const auto empty = [] { return std::vector{ is(prompt_fact::empty_chat) }; };
		return std::vector<prompt_builtin>{
			// The Agent tab: owner decision (a), 2026-10-23, and looks v004
			// and v005 (a block says so, and names its file and lines).
			plain("agent-tab.review-pull-request", prompt_place::agent_tab, "Review this pull request", pull_request, { }, prompt_kind::review),
			plain("agent-tab.review-open-file", prompt_place::agent_tab, "Review {file}", open_file, { }, prompt_kind::review),
			plain("agent-tab.what-could-break", prompt_place::agent_tab, "What could this change break?", pull_request),
			plain("agent-tab.explain-line", prompt_place::agent_tab, "Explain {file} {lines}", selected_lines, { lines(prompt_lines_value::one) }),
			plain("agent-tab.explain-block", prompt_place::agent_tab, "Explain the selected block ({file} {lines})", selected_lines, { lines(prompt_lines_value::several) }),
			// [[BPR-1773]]: a reply only where one can be written.
			plain("conversation.summarize", prompt_place::conversation, "Summarize this conversation", prompt_subject::conversation),
			plain("conversation.concern-right", prompt_place::conversation, "Is the concern raised here right?", prompt_subject::conversation),
			plain("conversation.draft-reply", prompt_place::conversation, "Draft a reply to this conversation", prompt_subject::conversation, { is(prompt_fact::takes_replies) }),
			plain("conversation.explain-note-code", prompt_place::conversation, "Explain the code this note is on", prompt_subject::conversation, { is_not(prompt_fact::takes_replies) }),
			// [[BPR-1934]]: the selected block whatever the chat holds, the
			// rest while it is empty.
			plain("tour-stop.explain-selection", prompt_place::tour_stop, "Explain the selected block", selected_text),
			plain("tour-stop.what-does-this-do", prompt_place::tour_stop, "What does this code do?", stop, empty()),
			plain("tour-stop.why-it-matters", prompt_place::tour_stop, "Why does this change matter?", stop, empty()),
			plain("tour-stop.rewrite-simply", prompt_place::tour_stop, "Rewrite this explanation more simply", stop, empty()),
			plain("tour-stop.right-place", prompt_place::tour_stop, "Is this the right place for this stop?", stop, empty()),
			plain("tour-part.explain-selection", prompt_place::tour_part, "Explain the selected block", selected_text),
			plain("tour-part.make-clearer", prompt_place::tour_part, "Make this clearer", part, empty()),
			plain("tour-part.whats-missing", prompt_place::tour_part, "What's missing?", part, empty()),
			plain("tour-part.make-shorter", prompt_place::tour_part, "Make it shorter", part, empty()),
			plain("tour.too-many-stops", prompt_place::tour_whole, "Too many stops?", tour, empty()),
			plain("tour.better-order", prompt_place::tour_whole, "Suggest a better order", tour, empty()),
			// Finished in the question field, so its name is not its text.
			named(plain("tour.focus-on", prompt_place::tour_whole, "Focus on {cursor}", tour, empty()), "Focus on \xE2\x80\xA6"),
			plain("tour.not-covered", prompt_place::tour_whole, "What's not covered?", tour, empty()),
		};
	}
} // namespace bpr::core
