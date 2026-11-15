// RFC-BPR-0128 ([[BPR-2163]]). Quick asks without Qt: a macro's Quick ask and
// scopes with their legality, the built-in action rows, and what each Ask
// menu lists, greys (with a typed reason) or leaves out (AC03, AC04).
#include "bpr/core/quick_ask.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto shipped_set() -> std::vector<prompt_macro> { return merge_prompt_macros(shipped_prompt_macros(), { }).macros(); }

	auto labels(const quick_ask_section& aSection) -> std::vector<std::string>
	{
		auto result = std::vector<std::string>{ };
		for (const auto& row : aSection.Rows) {
			result.push_back(row.Label);
		}
		return result;
	}

	/// #42 after a large push: a file open, three threads on it by others,
	/// one on another file, one about the pull request.
	auto returning_facts() -> quick_ask_facts
	{
		auto facts               = quick_ask_facts{ };
		facts.Base.PullRequest   = prompt_pull_request{ .Number = 42U, .Title = "Render retained frames" };
		facts.Base.Repository    = "MikeMarcin/BPR-Test";
		facts.Base.Draft         = false;
		facts.Base.AuthorIsMe    = false;
		facts.Base.ChecksFailing = false;
		facts.Reviewed           = true;
		facts.LatestUpload       = latest_upload_state::offered;
		facts.CommitsSinceReview = 18U;
		facts.File               = "src/render/renderer.cpp";
		facts.Thread             = "t10";
		facts.Files              = { "src/render/renderer.cpp", "src/render/frame_pool.cpp" };
		facts.Threads            = {
			{ .Identity = "t10", .Path = "src/render/renderer.cpp", .TakesReplies = true, .Changed = true, .CommitsSince = 18U },
			{ .Identity = "t22", .Path = "src/render/renderer.cpp", .TakesReplies = true, .Changed = true, .CommitsSince = 4U },
			{ .Identity = "t31", .Path = "src/render/renderer.cpp", .TakesReplies = true, .Changed = false, .CommitsSince = 0U },
			{ .Identity = "t-pool", .Path = "src/render/frame_pool.cpp", .TakesReplies = true, .Changed = true, .CommitsSince = 2U },
			{ .Identity = "pr", .Path = { }, .TakesReplies = true, .Changed = true, .CommitsSince = 1U },
		};
		return facts;
	}

	void test_legality()
	{
		using enum prompt_scope;
		expect(subject_scope(prompt_subject::conversation) == thread && subject_scope(prompt_subject::open_file) == file
				   && subject_scope(prompt_subject::pull_request) == pull_request && !subject_scope(prompt_subject::selected_lines) && !subject_scope(prompt_subject::tour),
			   "each subject's own scope; lines and the tour have none");
		expect(prompt_scope_allowed(prompt_place::conversation, prompt_subject::conversation, thread) && prompt_scope_allowed(prompt_place::conversation, prompt_subject::conversation, file)
				   && prompt_scope_allowed(prompt_place::conversation, prompt_subject::conversation, pull_request),
			   "a thread's question can be asked at a file or the pull request: once for each");
		expect(!prompt_scope_allowed(prompt_place::agent_tab, prompt_subject::open_file, thread) && prompt_scope_allowed(prompt_place::agent_tab, prompt_subject::open_file, pull_request),
			   "a file's question cannot be on at a thread");
		expect(!prompt_scope_allowed(prompt_place::agent_tab, prompt_subject::pull_request, file) && !prompt_scope_allowed(prompt_place::agent_tab, prompt_subject::pull_request, thread),
			   "the pull request's question only at the pull request");
		expect(!prompt_scope_allowed(prompt_place::tour_stop, prompt_subject::stop, thread), "a tour chat's macro is never a quick ask");

		const auto problems = [](const prompt_macro& aMacro) {
			auto names = std::vector<prompt_problem>{ };
			for (const auto& problem : validate_prompt_macro(aMacro)) {
				names.push_back(problem.Problem);
			}
			return names;
		};
		auto own = prompt_macro{ .Id = "own", .Body = { .Name = "Security pass on {file.name}", .Template = "Check {file}", .Subject = prompt_subject::open_file } };
		expect(problems(own).empty(), "an ordinary macro needs no scope");
		own.Body.QuickAsk = true;
		expect(problems(own) == std::vector{ prompt_problem::quick_ask_without_scope }, "a quick ask needs a scope");
		own.Body.Scopes.File = true;
		expect(problems(own).empty() && prompt_macro_valid(own), "a file's question at the file");
		own.Body.Scopes.Thread = true;
		const auto narrower    = validate_prompt_macro(own);
		expect(narrower.size() == 1U && narrower.front().Problem == prompt_problem::scope_narrower_than_subject && narrower.front().Detail == "thread" && prompt_problem_blocks(narrower.front().Problem),
			   "a scope narrower than the About blocks, naming the scope");
		auto lines = prompt_macro{ .Id = "lines", .Body = { .Name = "Explain {lines}", .Template = "Explain {file} {lines}", .Subject = prompt_subject::selected_lines, .QuickAsk = true } };
		expect(problems(lines) == std::vector{ prompt_problem::quick_ask_not_for_subject }, "selected lines are never a quick ask");

		auto scopes = prompt_scopes{ };
		scopes.set(file, true);
		expect(scopes.has(file) && !scopes.has(thread) && scopes.any() && stable_name(pull_request) == "pull-request", "scopes as toggles");
	}

	void test_builtin_rows()
	{
		const auto set   = shipped_set();
		const auto named = [&set](const std::string_view aId) -> const prompt_macro& {
			const auto found = std::ranges::find(set, aId, &prompt_macro::Id);
			expect(found != set.end(), aId);
			return *found;
		};
		const auto& changed   = named("agent-tab.what-changed");
		const auto& addressed = named("conversation.was-addressed");
		const auto& explain   = named("agent-tab.explain-file");
		expect(changed.Action == prompt_action::what_changed && changed.Body.Scopes == prompt_scopes{ .PullRequest = true } && changed.BuiltIn && changed.Body.QuickAsk, "What changed: a built-in row at the pull request");
		expect(addressed.Action == prompt_action::was_addressed && addressed.Body.Scopes == prompt_scopes{ .Thread = true, .File = true, .PullRequest = true },
			   "Was this addressed?: thread, file and pull request");
		expect(explain.Action == prompt_action::explain_file && explain.Body.Scopes == prompt_scopes{ .File = true }, "Explain this file: at the file");
		for (const auto& macro : set) {
			expect(prompt_macro_valid(macro), macro.Id);
		}

		// The chips are as before: an action row is never a chip.
		auto facts     = prompt_facts{ };
		facts.Place    = prompt_place::agent_tab;
		facts.OpenFile = "a.cpp";
		for (const auto& offer : decide_prompt_offers(set, facts).Chips) {
			expect(offer.Id != "agent-tab.what-changed" && offer.Id != "agent-tab.explain-file", "no action row among the chips");
		}

		// Stored changes keep the action (it is not the reviewer's to set), and
		// an own macro claiming one is dropped.
		auto body     = explain.Body;
		body.QuickAsk = false;
		const auto merged
			= merge_prompt_macros(shipped_prompt_macros(), { .Changes = { { .Id = "agent-tab.explain-file", .Body = body, .WrittenAgainst = 1U } }, .Own = { { .Id = "mine", .Body = body, .Action = prompt_action::what_changed } } });
		const auto entry = std::ranges::find(merged.Entries, std::string{ "agent-tab.explain-file" }, [](const prompt_set_entry& aEntry) { return aEntry.Macro.Id; });
		expect(entry != merged.Entries.end() && entry->Changed && !entry->Macro.Body.QuickAsk && entry->Macro.Action == prompt_action::explain_file, "a changed action row keeps its action");
		expect(std::ranges::find(merged.Dropped, std::string{ "mine" }) != merged.Dropped.end(), "an own macro cannot be an action");
	}

	void test_review_bar_menu()
	{
		const auto set  = shipped_set();
		const auto menu = decide_quick_asks(set, returning_facts(), quick_ask_anchor::review_bar);
		expect(menu.Sections.size() == 3U && menu.Sections[0].Scope == prompt_scope::pull_request && menu.Sections[1].Scope == prompt_scope::file && menu.Sections[2].Scope == prompt_scope::thread,
			   "the pull request's, then the file's, then the thread's (Q2)");
		expect(labels(menu.Sections[0])
				   == std::vector<std::string>{ "What changed since my review", "Was this addressed?", "Review this pull request", "What could this change break?" },
			   "the pull request: action rows first");
		expect(labels(menu.Sections[1]) == std::vector<std::string>{ "Explain this file", "Was this addressed?", "Review src/render/renderer.cpp" }, "the file");
		expect(labels(menu.Sections[2])
				   == std::vector<std::string>{ "Was this addressed?", "Summarize this conversation", "Is the concern raised here right?", "Draft a reply to this conversation" },
			   "the thread: the reply-taking one, not the Local comment's");
		const auto* const changed = find_quick_ask(menu, "agent-tab.what-changed", prompt_scope::pull_request);
		expect(changed != nullptr && changed->Enabled && changed->Count == 18U && changed->Unit == quick_ask_unit::commits, "What changed reads 18 commits");
		const auto* const everything = find_quick_ask(menu, "conversation.was-addressed", prompt_scope::pull_request);
		expect(everything != nullptr && everything->Enabled && everything->Unit == quick_ask_unit::threads && everything->Count == 3U
				   && everything->Each == std::vector<std::string>{ "t10", "t22", "t-pool" },
			   "every review thread on a file with commits since it started; one run each");
		const auto* const file = find_quick_ask(menu, "conversation.was-addressed", prompt_scope::file);
		expect(file != nullptr && file->Each == std::vector<std::string>{ "t10", "t22" } && file->Count == 2U, "the file's threads");
		const auto* const thread = find_quick_ask(menu, "conversation.was-addressed", prompt_scope::thread);
		expect(thread != nullptr && thread->Enabled && thread->Count == 18U && thread->Unit == quick_ask_unit::commits && thread->Each == std::vector<std::string>{ "t10" },
			   "a thread I did not write: commits since it started (Q6)");
		expect(decide_quick_asks(set, returning_facts(), quick_ask_anchor::review_bar) == menu, "deterministic");
	}

	void test_greyed_and_hidden()
	{
		const auto set = shipped_set();
		// Q1: subject there, cannot run yet: greyed with a reason.
		auto facts         = returning_facts();
		facts.LatestUpload = latest_upload_state::hidden;
		facts.Thread       = "t31";
		auto        menu   = decide_quick_asks(set, facts, quick_ask_anchor::review_bar);
		const auto* row    = find_quick_ask(menu, "agent-tab.what-changed", prompt_scope::pull_request);
		expect(row != nullptr && !row->Enabled && row->Reason == quick_ask_reason::no_commits_since_review, "no commits since the review: greyed");
		row = find_quick_ask(menu, "conversation.was-addressed", prompt_scope::thread);
		expect(row != nullptr && !row->Enabled && row->Reason == quick_ask_reason::no_commits_since_thread, "no commits since this thread: greyed");
		facts.Reviewed = false;
		menu           = decide_quick_asks(set, facts, quick_ask_anchor::review_bar);
		row            = find_quick_ask(menu, "agent-tab.what-changed", prompt_scope::pull_request);
		expect(row != nullptr && row->Reason == quick_ask_reason::not_reviewed, "never reviewed: greyed, saying so");

		// A file with no threads: its Was this addressed? greyed.
		facts      = returning_facts();
		facts.File = "src/render/scene.cpp";
		menu       = decide_quick_asks(set, facts, quick_ask_anchor::file);
		row        = find_quick_ask(menu, "conversation.was-addressed", prompt_scope::file);
		expect(row != nullptr && !row->Enabled && row->Reason == quick_ask_reason::no_threads, "no threads on this file: greyed");

		// Subject absent: not listed. No file, no thread.
		facts = returning_facts();
		facts.File.reset();
		facts.Thread.reset();
		menu = decide_quick_asks(set, facts, quick_ask_anchor::review_bar);
		expect(menu.Sections.size() == 1U && menu.Sections.front().Scope == prompt_scope::pull_request, "no file and no thread: their sections are not listed");
		expect(decide_quick_asks(set, facts, quick_ask_anchor::file).Sections.empty() && decide_quick_asks(set, facts, quick_ask_anchor::thread).Sections.empty(), "nothing to ask about");

		// A conversation about the pull request is not a review thread on a
		// file: Was this addressed? is not listed there, the others are.
		facts        = returning_facts();
		facts.Thread = "pr";
		menu         = decide_quick_asks(set, facts, quick_ask_anchor::thread);
		expect(menu.Sections.size() == 1U && !find_quick_ask(menu, "conversation.was-addressed", prompt_scope::thread), "a pull request conversation: no Was this addressed?");

		// A condition that fails hides (the chips' rule).
		facts                              = returning_facts();
		facts.Threads.front().TakesReplies = false;
		menu                               = decide_quick_asks(set, facts, quick_ask_anchor::thread);
		const auto& threadRows             = menu.Sections.front();
		expect(std::ranges::find(threadRows.Rows, std::string{ "conversation.draft-reply" }, &quick_ask_row::Id) == threadRows.Rows.end()
				   && std::ranges::find(menu.Hidden, std::string{ "conversation.draft-reply" }, &prompt_withheld::Id) != menu.Hidden.end(),
			   "a condition that fails is not listed, with its reason kept");

		// Owner, 2026-11-14 ("a plain click queues it"): the agent's state
		// greys nothing; a row says what it will wait for.
		facts       = returning_facts();
		facts.Agent = quick_ask_agent::busy;
		menu        = decide_quick_asks(set, facts, quick_ask_anchor::review_bar);
		row         = find_quick_ask(menu, "agent-tab.what-changed", prompt_scope::pull_request);
		expect(row != nullptr && row->Enabled && row->Waits == quick_ask_reason::agent_busy, "busy: still offered, waiting its turn");
		facts.Agent = quick_ask_agent::not_set_up;
		menu        = decide_quick_asks(set, facts, quick_ask_anchor::file);
		row         = find_quick_ask(menu, "agent-tab.explain-file", prompt_scope::file);
		expect(row != nullptr && row->Enabled && row->Waits == quick_ask_reason::agent_not_set_up, "no agent: still offered, waiting for setup");
	}

	void test_reviewer_choices()
	{
		// Quick ask off: not listed. An own quick ask at its scope is.
		auto set = shipped_set();
		for (auto& macro : set) {
			if (macro.Id == "agent-tab.what-could-break") {
				macro.Body.QuickAsk = false;
			}
		}
		set.push_back({ .Id   = "own.security",
						.Body = { .Name = "Security pass on {file.name}", .Template = "Check {file}", .Subject = prompt_subject::open_file, .QuickAsk = true, .Scopes = { .File = true, .PullRequest = true } } });
		set.push_back({ .Id   = "own.cpp-only",
						.Body = { .Name = "C++ only", .Template = "Check {file}", .Subject = prompt_subject::open_file, .Conditions = { { .Fact = prompt_fact::path, .Patterns = { "*.cpp" } } }, .QuickAsk = true, .Scopes = { .PullRequest = true } } });
		const auto menu = decide_quick_asks(set, returning_facts(), quick_ask_anchor::review_bar);
		expect(!find_quick_ask(menu, "agent-tab.what-could-break", prompt_scope::pull_request), "Quick ask off: not in the menu");
		const auto* const security = find_quick_ask(menu, "own.security", prompt_scope::file);
		expect(security != nullptr && security->Enabled && security->Label == "Security pass on renderer.cpp", "an own quick ask, its name expanded");
		expect(find_quick_ask(menu, "own.security", prompt_scope::pull_request)->Label == "Security pass on each file", "once for each file, named so");
		const auto* const each = find_quick_ask(menu, "own.security", prompt_scope::pull_request);
		expect(each != nullptr && each->Unit == quick_ask_unit::files && each->Count == 2U, "a file's question at the pull request: once for each file");
		const auto* const cpp = find_quick_ask(menu, "own.cpp-only", prompt_scope::pull_request);
		expect(cpp != nullptr && cpp->Each.size() == 2U, "each file meeting its conditions");
	}
} // namespace

/// [[BPR-2184]] (review B1). A wide Was this addressed? over more
/// changed threads than one ask carries names exactly the threads a
/// press sends, the first in order, and how many more were left out.
void test_wide_ask_is_capped()
{
	auto facts = returning_facts();
	facts.Threads.clear();
	for (auto index = 0U; index < 40U; ++index) {
		facts.Threads.push_back({ .Identity = "t" + std::to_string(index), .Path = "src/render/renderer.cpp", .TakesReplies = true, .Changed = index % 10U != 9U, .CommitsSince = 1U });
	}
	const auto        menu = decide_quick_asks(shipped_set(), facts, quick_ask_anchor::review_bar);
	const auto* const wide = find_quick_ask(menu, "conversation.was-addressed", prompt_scope::pull_request);
	expect(wide != nullptr && wide->Each.size() == maximum_addressed_threads && wide->Count == maximum_addressed_threads && wide->Omitted == 4U,
		   "36 changed threads: the row names the 32 a press sends and says 4 more were left out");
	expect(wide->Each.front() == "t0" && wide->Each.back() == "t34", "the first 32 changed threads, in the pull request's order, the unchanged ones passed over");
	expect(std::ranges::none_of(wide->Each, [](const std::string& aIdentity) { return aIdentity == "t9" || aIdentity == "t19"; }), "an unchanged thread is never sent");
	const auto* const file = find_quick_ask(menu, "conversation.was-addressed", prompt_scope::file);
	expect(file != nullptr && file->Each == wide->Each && file->Omitted == 4U, "the file's row is capped the same way");
	auto       few   = returning_facts();
	const auto small = decide_quick_asks(shipped_set(), few, quick_ask_anchor::review_bar);
	expect(find_quick_ask(small, "conversation.was-addressed", prompt_scope::pull_request)->Omitted == 0U, "under the cap nothing is left out");
}

/// [[BPR-2181]] (AC13, Prompts review B1). A chat's offers and a quick ask's
/// press are decided on the same facts: a prompt conditioned on pushes since
/// my review is offered in the Agent tab only once there were pushes, and
/// its press carries the expansion decided then.
void test_chat_prompts_read_the_review()
{
	auto again            = prompt_macro{ .Id = "own.look-again", .Place = prompt_place::agent_tab };
	again.Body.Name       = "Ready to look again?";
	again.Body.Template   = "Has {pr.title} changed enough since my review that I should look again?";
	again.Body.Conditions = { { .Fact = prompt_fact::pushes_since_review } };
	again.Body.QuickAsk   = true;
	again.Body.Scopes     = { .PullRequest = true };
	const auto set        = std::vector{ again };
	auto       facts      = returning_facts();
	const auto view       = prompt_chat_view{ .Place = prompt_place::agent_tab, .ChatEmpty = true };
	const auto pushed     = decide_chat_prompts(set, chat_prompt_facts(facts, view));
	expect(pushed.Chips.size() == 1U && pushed.Chips.front().Label == "Ready to look again?" && pushed.Pressable.size() == 1U, "18 commits since my review: the chip is offered");
	expect(pushed.Chips.front().Expansion.Text == "Has Render retained frames changed enough since my review that I should look again?", "expanded on the pull request's own facts");
	facts.CommitsSinceReview = 0U;
	expect(decide_chat_prompts(set, chat_prompt_facts(facts, view)).Chips.size() == 1U, "the reviewed head no longer listed (a force push) but a newer revision offered: pushes since my review");
	facts.LatestUpload = latest_upload_state::hidden;
	expect(decide_chat_prompts(set, chat_prompt_facts(facts, view)).Chips.empty(), "nothing newer than my review: not offered");
	facts.LatestUpload       = latest_upload_state::offered;
	facts.CommitsSinceReview = 18U;
	const auto        menu   = decide_quick_asks(set, facts, quick_ask_anchor::review_bar);
	const auto* const row    = find_quick_ask(menu, "own.look-again", prompt_scope::pull_request);
	expect(row != nullptr && row->Enabled, "the menu lists it on the same facts");
	const auto decided = decide_quick_ask_prompt(again, facts, { });
	expect(decided && *decided == pushed.Chips.front(), "a press carries the offer the chip shows");
	facts.Reviewed = false;
	expect(!decide_quick_ask_prompt(again, facts, { }), "no review to return to: a press decides nothing");
	// A conversation's chat reads its own thread's facts.
	auto open            = prompt_macro{ .Id = "own.still-open", .Place = prompt_place::conversation };
	open.Body.Name       = "Still open after the push?";
	open.Body.Template   = "Is it still open?";
	open.Body.Subject    = prompt_subject::conversation;
	open.Body.Conditions = { { .Fact = prompt_fact::thread_activity, .Activity = prompt_activity_value::pushes } };
	auto       threads   = returning_facts();
	const auto on        = decide_chat_prompts(std::vector{ open }, chat_prompt_facts(threads, prompt_chat_view{ .Place = prompt_place::conversation, .Thread = "t10", .TakesReplies = true }));
	const auto off       = decide_chat_prompts(std::vector{ open }, chat_prompt_facts(threads, prompt_chat_view{ .Place = prompt_place::conversation, .Thread = "t31", .TakesReplies = true }));
	expect(on.Chips.size() == 1U && off.Chips.empty(), "the thread pushes followed is offered it; the one they did not is not");
	expect(decide_quick_ask_prompt(open, threads, "t10").has_value() && !decide_quick_ask_prompt(open, threads, "t31"), "a thread's press decides on that thread");
}

auto main() -> int
{
	test_legality();
	test_builtin_rows();
	test_review_bar_menu();
	test_greyed_and_hidden();
	test_reviewer_choices();
	test_wide_ask_is_capped();
	test_chat_prompts_read_the_review();
	std::cout << "quick-ask-tests:pass\n";
	return EXIT_SUCCESS;
}
