#include "bpr/core/prompt_macro.hpp"

/// \file
/// \brief RFC-BPR-0125. The shipped suggested prompts: the Agent tab's
/// ([[BPR-1872]], [[BPR-1773]]) and the tour chats' ([[BPR-1934]]), as
/// macros. With nothing stored each place offers what it offered before,
/// under the same facts, in the same order.
namespace bpr::core {
	namespace {
		/// A built-in whose name is its template.
		auto plain(std::string aId, const prompt_place aPlace, const std::string& aText, const prompt_subject aSubject, std::vector<prompt_condition> aConditions = { }, const prompt_ask aKind = prompt_ask::question) -> prompt_builtin
		{
			// [[BPR-2179]]: a built-in's row does what its conditions did
			// before the choice was the prompt's (a reply where none can be
			// written, lines not selected: hidden).
			const auto when = migrated_when_unmet(aConditions, { });
			return {
				.Macro = {
					.Id      = std::move(aId),
					.BuiltIn = true,
					.Place   = aPlace,
					.Body    = { .Name = aText, .Template = aText, .Subject = aSubject, .Kind = aKind, .Conditions = std::move(aConditions), .WhenUnmet = when },
				},
			};
		}

		/// A built-in shown as `aName`, sending `aTemplate`.
		auto named(prompt_builtin aBuiltin, std::string aName) -> prompt_builtin
		{
			aBuiltin.Macro.Body.Name = std::move(aName);
			return aBuiltin;
		}

		/// RFC-BPR-0128. A built-in listed in the Ask menus at `aScopes`.
		auto quick(prompt_builtin aBuiltin, const prompt_scopes aScopes) -> prompt_builtin
		{
			aBuiltin.Macro.Body.QuickAsk = true;
			aBuiltin.Macro.Body.Scopes   = aScopes;
			return aBuiltin;
		}

		/// RFC-BPR-0128 (Q13). One of the host's actions as a built-in row:
		/// what it sends stays in Built-in prompts.
		auto action(std::string aId, const prompt_place aPlace, const std::string& aName, const prompt_subject aSubject, const prompt_action aAction, const prompt_scopes aScopes)
			-> prompt_builtin
		{
			auto builtin         = quick(plain(std::move(aId), aPlace, aName, aSubject), aScopes);
			builtin.Macro.Action = aAction;
			return builtin;
		}

		constexpr auto at_thread       = prompt_scopes{ .Thread = true };
		constexpr auto at_file         = prompt_scopes{ .File = true };
		constexpr auto at_pull_request = prompt_scopes{ .PullRequest = true };
		constexpr auto everywhere      = prompt_scopes{ .Thread = true, .File = true, .PullRequest = true };

		auto is(const prompt_fact aFact) -> prompt_condition { return { .Fact = aFact }; }
		auto is_not(const prompt_fact aFact) -> prompt_condition { return { .Fact = aFact, .Negated = true }; }
		auto lines(const prompt_lines_value aValue) -> prompt_condition { return { .Fact = prompt_fact::selected_lines, .Lines = aValue }; }
	} // namespace

	auto sample_prompt_facts(const prompt_place aPlace) -> prompt_facts
	{
		auto facts             = prompt_facts{ };
		facts.Place            = aPlace;
		facts.PullRequest      = prompt_pull_request{ .Number = 42U, .Title = "Render retained frames before presenting", .Url = "https://github.com/octo/render/pull/42", .Author = "octocat", .Base = "main", .Head = "retained-frames" };
		facts.Repository       = "octo/render";
		facts.OpenFile         = "src/render/renderer.cpp";
		facts.Lines            = prompt_line_range{ .First = 5U, .Last = 8U };
		facts.ConversationFile = "src/render/renderer.cpp";
		facts.TakesReplies     = true;
		facts.Draft            = false;
		facts.AuthorIsMe       = false;
		facts.ChecksFailing    = false;
		facts.ChatEmpty        = true;
		facts.Commits          = { "9fceb02d0ae598e95dc970b74767f19372d61af8", "e83c5163316f89bfbde7d9ab23ca2e25604af290" };
		if (aPlace == prompt_place::tour_stop || aPlace == prompt_place::tour_part || aPlace == prompt_place::tour_whole) {
			facts.Thread = prompt_tour_part{ .Place = aPlace, .Part = aPlace == prompt_place::tour_stop ? "Stop 3" : aPlace == prompt_place::tour_part ? "the Problem"
																																					   : "",
											 .StopNumber = 3U,
											 .StopTitle  = "The frame pool" };
		}
		return facts;
	}

	namespace {
		/// [[BPR-2219]] (owner, 2026-11-16: "How is 1 sentence useful to
		/// anyone trying to tweak the review request to their particular
		/// circumstances?"). A review prompt's body is its real instructions,
		/// visible and editable in Settings › Prompts: what to review, the
		/// focus, the sections, the severity scheme, the verdict and the
		/// tone. The form the application reads (the title, the verdict
		/// line, a finding's place) is not here: it is the application's,
		/// always added, and shown read-only under the editor.
		auto review_body(const std::string& aSubject, const std::string& aWhat) -> std::string
		{
			return "Review " + aSubject + " as a careful colleague would, and answer with a review report.\n\n"
										  "What to review: "
				+ aWhat + " Follow what changed into its callers, the types it relies on and the tests that cover it, and check every finding against the code before you report it. "
						  "Say whether the pull request introduced a problem or only touches code that already had it.\n\n"
						  "Focus: correctness first, then security, then what will be hard to maintain or test. Leave formatting to the formatter unless it hides a defect.\n\n"
						  "Sections, in this order, leaving out any that would be empty:\n"
						  "- Must fix: what has to change before this merges.\n"
						  "- Worth a look: everything else worth saying.\n"
						  "- Good: what was done well, only where it is earned.\n"
						  "- Questions for the author: what only they can answer.\n"
						  "- Not reviewed: each thing not read or not checked, named, then why. Write it about the thing, not about yourself.\n\n"
						  "Severity: head each finding with a Conventional Comments label: issue, suggestion, question, nitpick or praise, with (blocking) or (non-blocking) after issue and suggestion. "
						  "A blocking issue goes under Must fix.\n\n"
						  "Verdict: Needs changes when anything stands under Must fix, Ready with comments when only smaller things do, Ready to merge when nothing does.\n\n"
						  "Tone: plain and specific, as one engineer writes to another. No praise that is not earned, and no hedging.";
		}
	} // namespace

	auto shipped_prompt_macros() -> std::vector<prompt_builtin>
	{
		using enum prompt_subject;
		const auto empty = [] { return std::vector{ is(prompt_fact::empty_chat) }; };
		return std::vector<prompt_builtin>{
			// The Agent tab: owner decision (a), 2026-10-23, and looks v004
			// and v005 (a block says so, and names its file and lines).
			// [[BPR-2219]]. Shown by their names; what they send is their
			// body, the review's instructions.
			quick(named(plain("agent-tab.review-pull-request", prompt_place::agent_tab, review_body("this pull request", "the change, then the code around it."), pull_request, { }, prompt_ask::review),
						"Review this pull request"),
				  at_pull_request),
			quick(named(plain("agent-tab.review-open-file", prompt_place::agent_tab, review_body("{file}", "this file's change in the pull request, then the code around it; the rest of the pull request is context."), open_file, { },
							  prompt_ask::review),
						"Review {file}"),
				  at_file),
			quick(plain("agent-tab.what-could-break", prompt_place::agent_tab, "What could this change break?", pull_request), at_pull_request),
			plain("agent-tab.explain-line", prompt_place::agent_tab, "Explain {file} {lines}", selected_lines, { lines(prompt_lines_value::one) }),
			plain("agent-tab.explain-block", prompt_place::agent_tab, "Explain the selected block ({file} {lines})", selected_lines, { lines(prompt_lines_value::several) }),
			// RFC-BPR-0128 (Q13): the actions as built-in rows. The Ask menus
			// list action rows first; an action row is never a chip, so the
			// chips are as before.
			action("agent-tab.what-changed", prompt_place::agent_tab, "What changed since my review", pull_request, prompt_action::what_changed, at_pull_request),
			action("agent-tab.explain-file", prompt_place::agent_tab, "Explain this file", open_file, prompt_action::explain_file, at_file),
			// [[BPR-2248]]. The Commits tab's selection: what the selected
			// commits change and why, read against their messages, and a
			// review of their changes alone. The commits' own changes and
			// messages ride beside the question.
			named(plain("commits.explain", prompt_place::agent_tab,
						"Explain commits {commits}: what each one changes and why, one commit at a time, read against its commit message. "
						"Say plainly where a commit's change does not match its message: it does less than the message says, more, or something else. "
						"Merges of the base branch are named, not explained.",
						selected_commits),
				  "Explain these commits"),
			named(plain("commits.review", prompt_place::agent_tab,
						review_body("commits {commits}", "only the changes these commits make, given commit by commit; the rest of the pull request is context, not under review. Merges of the base branch are left out."),
						selected_commits, { }, prompt_ask::review),
				  "Review these commits"),
			// [[BPR-1773]]: a reply only where one can be written.
			quick(plain("conversation.summarize", prompt_place::conversation, "Summarize this conversation", prompt_subject::conversation), at_thread),
			quick(plain("conversation.concern-right", prompt_place::conversation, "Is the concern raised here right?", prompt_subject::conversation), at_thread),
			quick(plain("conversation.draft-reply", prompt_place::conversation, "Draft a reply to this conversation", prompt_subject::conversation, { is(prompt_fact::takes_replies) }), at_thread),
			quick(plain("conversation.explain-note-code", prompt_place::conversation, "Explain the code this comment is on", prompt_subject::conversation, { is_not(prompt_fact::takes_replies) }), at_thread),
			// RFC-BPR-0128: Was this addressed? on any thread, a file's threads
			// or every thread.
			action("conversation.was-addressed", prompt_place::conversation, "Was this addressed?", prompt_subject::conversation, prompt_action::was_addressed, everywhere),
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
