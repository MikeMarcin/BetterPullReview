// RFC-BPR-0125 milestone 1. Suggested prompts as macros, without Qt: the
// shipped set equal to today's lists, the template scan and expansion, the
// conditions, order and the five-chip limit, validation, and the merge of
// the shipped set with stored differences.
#include "bpr/core/prompt_macro.hpp"
#include "bpr/core/review_cycle_ignore.hpp"
#include "bpr/core/skill_references.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
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

	auto all_offers(const prompt_offers& aOffers) -> std::vector<prompt_offer>
	{
		auto all = aOffers.Chips;
		all.insert(all.end(), aOffers.More.begin(), aOffers.More.end());
		return all;
	}

	/// What a chip offers, as today's code offers it: its label, what a
	/// press sends (or puts in the field), its subject and whether it is a
	/// review, and whether a click finishes it in the field.
	using today_offer = std::tuple<std::string, std::string, prompt_subject, prompt_ask, bool>;

	auto offered(const prompt_offers& aOffers) -> std::vector<today_offer>
	{
		auto result = std::vector<today_offer>{ };
		for (const auto& offer : all_offers(aOffers)) {
			result.emplace_back(offer.Label, offer.Expansion.Text, offer.Subject, offer.Kind, offer.finish());
		}
		return result;
	}

	/// The Agent tab today: `agent_tab_view::refresh_suggestions` on lane
	/// `claude/v0-5-0-prerelease-targets-52265d` (RFC-BPR-0102), where each
	/// chip is a text, a suggested subject and a review flag. The lines are
	/// gathered only while a file is open (`window_agent_setup.cpp`), so the
	/// widget's no-file wording is unreachable and not transcribed.
	auto agent_tab_today(const std::optional<std::string>& aFile, const std::optional<prompt_line_range>& aLines) -> std::vector<today_offer>
	{
		auto       result = std::vector<today_offer>{ };
		const auto offer  = [&result](const std::string& aText, const prompt_subject aSubject, const bool aReview) {
			result.emplace_back(aText, aText, aSubject, aReview ? prompt_ask::review : prompt_ask::question, false);
		};
		offer("Review this pull request", prompt_subject::pull_request, true);
		if (aFile) {
			offer("Review " + *aFile, prompt_subject::open_file, true);
		}
		offer("What could this change break?", prompt_subject::pull_request, false);
		if (aLines && aFile) {
			const auto [first, last] = *aLines;
			offer(first == last ? "Explain " + *aFile + " line " + std::to_string(first)
								: "Explain the selected block (" + *aFile + " lines " + std::to_string(first) + "\xE2\x80\x93" + std::to_string(last) + ")",
				  prompt_subject::selected_lines, false);
		}
		return result;
	}

	auto conversation_today(const bool aReplies) -> std::vector<today_offer>
	{
		auto result = std::vector<today_offer>{ };
		for (const std::string text : { "Summarize this conversation", "Is the concern raised here right?", aReplies ? "Draft a reply to this conversation" : "Explain the code this comment is on" }) {
			result.emplace_back(text, text, prompt_subject::conversation, prompt_ask::question, false);
		}
		return result;
	}

	/// `application::tour_chat_prompts` today, transcribed: the core-only
	/// preset does not build the application layer.
	struct tour_prompt_today
	{
		std::string_view Text;
		bool             Finish    = false;
		bool             Selection = false;
	};

	auto tour_list_today(const prompt_place aPlace) -> std::vector<tour_prompt_today>
	{
		switch (aPlace) {
		case prompt_place::tour_stop:
			return { { .Text = "Explain the selected block", .Selection = true }, { .Text = "What does this code do?" }, { .Text = "Why does this change matter?" }, { .Text = "Rewrite this explanation more simply" }, { .Text = "Is this the right place for this stop?" } };
		case prompt_place::tour_part: return { { .Text = "Explain the selected block", .Selection = true }, { .Text = "Make this clearer" }, { .Text = "What's missing?" }, { .Text = "Make it shorter" } };
		case prompt_place::tour_whole:
			return { { .Text = "Too many stops?" }, { .Text = "Suggest a better order" }, { .Text = "Focus on ", .Finish = true }, { .Text = "What's not covered?" } };
		case prompt_place::agent_tab:
		case prompt_place::conversation: break;
		}
		return { };
	}

	auto subject_of(const prompt_place aPlace) -> prompt_subject
	{
		return aPlace == prompt_place::tour_stop ? prompt_subject::stop : aPlace == prompt_place::tour_part ? prompt_subject::part
																											: prompt_subject::tour;
	}

	/// `tour_side_chat::prompt_labels` and `choose_prompt` today.
	auto tour_today(const prompt_tour_part& aThread, const std::optional<prompt_tour_part>& aSelection, const bool aEmpty) -> std::vector<today_offer>
	{
		auto result = std::vector<today_offer>{ };
		if (aSelection) {
			for (const auto& prompt : tour_list_today(aSelection->Place)) {
				if (prompt.Selection) {
					const auto label = *aSelection == aThread ? std::string{ prompt.Text } : std::string{ prompt.Text } + " in " + aSelection->Part;
					result.emplace_back(label, std::string{ prompt.Text }, prompt_subject::selected_text, prompt_ask::question, false);
					break;
				}
			}
		}
		if (aEmpty) {
			for (const auto& prompt : tour_list_today(aThread.Place)) {
				if (!prompt.Selection) {
					const auto label = prompt.Finish ? std::string{ prompt.Text } + "\xE2\x80\xA6" : std::string{ prompt.Text };
					result.emplace_back(label, std::string{ prompt.Text }, subject_of(aThread.Place), prompt_ask::question, prompt.Finish);
				}
			}
		}
		return result;
	}

	auto pull_request_facts() -> prompt_pull_request
	{
		return { .Number = 482U, .Title = "Faster renderer", .Url = "https://github.com/octo/app/pull/482", .Author = "octo", .Base = "main", .Head = "fast-render" };
	}

	// AC01.
	void test_shipped_equals_today()
	{
		const auto set = shipped_set();
		// RFC-BPR-0128 added three action rows (What changed, Explain this file,
		// Was this addressed?), which are never chips.
		expect(set.size() == 25U, "25 built-ins");
		expect(std::ranges::all_of(set, [](const prompt_macro& aMacro) { return aMacro.BuiltIn && aMacro.On && prompt_macro_valid(aMacro); }), "every built-in is on and valid");
		const auto lineChoices = std::vector<std::optional<prompt_line_range>>{ std::nullopt, prompt_line_range{ 34U, 34U }, prompt_line_range{ 34U, 36U } };
		for (const auto& file : { std::optional<std::string>{ }, std::optional<std::string>{ "src/render/renderer.cpp" }, std::optional<std::string>{ "README.md" } }) {
			for (const auto& lines : lineChoices) {
				if (lines && !file) {
					continue;
				}
				for (const auto empty : { std::optional<bool>{ }, std::optional<bool>{ true }, std::optional<bool>{ false } }) {
					auto facts = prompt_facts{ .Place = prompt_place::agent_tab, .PullRequest = pull_request_facts(), .Repository = "octo/app", .OpenFile = file, .Lines = lines, .ChatEmpty = empty };
					expect(offered(decide_prompt_offers(set, facts)) == agent_tab_today(file, lines), "the Agent tab offers today's chips");
					facts.PullRequest.reset();
					facts.Repository.reset();
					expect(offered(decide_prompt_offers(set, facts)) == agent_tab_today(file, lines), "and needs no pull request names for them");
				}
			}
		}
		for (const auto replies : { true, false }) {
			const auto facts = prompt_facts{ .Place = prompt_place::conversation, .OpenFile = "a.cpp", .Lines = prompt_line_range{ 1U, 2U }, .TakesReplies = replies };
			expect(offered(decide_prompt_offers(set, facts)) == conversation_today(replies), "a conversation offers today's chips");
		}
		const auto parts = std::vector<prompt_tour_part>{
			{ .Place = prompt_place::tour_stop, .Part = "Stop 1", .StopNumber = 1U, .StopTitle = "Parser" },
			{ .Place = prompt_place::tour_stop, .Part = "Stop 2", .StopNumber = 2U, .StopTitle = "Cache" },
			{ .Place = prompt_place::tour_part, .Part = "the Problem" },
			{ .Place = prompt_place::tour_part, .Part = "the Solution" },
		};
		auto threads = parts;
		threads.push_back({ .Place = prompt_place::tour_whole });
		auto selections = std::vector<std::optional<prompt_tour_part>>{ std::nullopt };
		selections.insert(selections.end(), parts.begin(), parts.end());
		for (const auto& thread : threads) {
			for (const auto& selection : selections) {
				for (const auto empty : { true, false }) {
					const auto facts = prompt_facts{ .Place = thread.Place, .PullRequest = pull_request_facts(), .ChatEmpty = empty, .Thread = thread, .Selection = selection };
					const auto today = tour_today(thread, selection, empty);
					expect(offered(decide_prompt_offers(set, facts)) == today, "a tour chat offers today's chips for " + thread.Part);
				}
			}
		}
		const auto focus = decide_prompt_offers(set, { .Place = prompt_place::tour_whole, .ChatEmpty = true, .Thread = prompt_tour_part{ } });
		expect(focus.Chips.size() == 4U && focus.Chips[2].Expansion.Cursor == std::optional<std::size_t>{ 9U }, "Focus on … puts its caret after the space");
	}

	// AC02.
	void test_scan_and_expansion()
	{
		const auto plain = scan_prompt_template("Check {file} {{not a variable} {} { file } {JSON} }{ {\"a\": 1}", prompt_place::agent_tab);
		expect(plain.Refusals.empty(), "only {file} is a variable here; an upper-case name is not variable-shaped");
		expect(plain.Pieces.size() == 3U && plain.Pieces[1].Variable == prompt_variable::file, "literal, {file}, literal");
		expect(plain.Pieces[2].Literal == " {not a variable} {} { file } {JSON} }{ {\"a\": 1}", "{{ is one brace and every other brace is literal");

		const auto refused = scan_prompt_template("Look at {flie} and {stop.title}.", prompt_place::agent_tab);
		expect(refused.Refusals.size() == 2U, "two refusals");
		expect(refused.Refusals[0] == prompt_template_refusal{ .Reason = prompt_template_refusal_reason::unknown_variable, .Offset = 8U, .Length = 6U, .Name = "flie" }, "an unknown name, with its position");
		expect(refused.Refusals[1] == prompt_template_refusal{ .Reason = prompt_template_refusal_reason::other_place_variable, .Offset = 19U, .Length = 12U, .Name = "stop.title" }, "another place's name, with its position");
		expect(!expand_prompt_template(refused, { }), "a refused template never expands");
		expect(scan_prompt_template("{stop.title}", prompt_place::tour_stop).Refusals.empty(), "a stop fills in its title");

		auto facts               = prompt_facts{ .Place = prompt_place::agent_tab, .PullRequest = pull_request_facts(), .Repository = "octo/app", .OpenFile = "src/render/renderer.cpp", .Lines = prompt_line_range{ 36U, 34U } };
		facts.PullRequest->Title = "Use {file} and /security-review\nthen\r\x01 stop\xE2\x80\xA8.";
		const auto expansion     = expand_prompt_template(scan_prompt_template("#{pr.number} {pr.title} in {repo}: {file.name}, {lines} ({lines.first}-{lines.last}) {pr.author} {pr.base}..{pr.head} {pr.url}{cursor}", prompt_place::agent_tab), facts);
		expect(expansion.has_value(), "every value is known");
		expect(expansion->Text == "#482 Use {file} and /security-review then   stop . in octo/app: renderer.cpp, lines 34\xE2\x80\x93"
								  "36 (34-36) octo main..fast-render https://github.com/octo/app/pull/482",
			   "a value is inserted as written, on one line, and never read again");
		expect(expansion->Cursor == expansion->Text.size(), "the caret mark fills in nothing");
		facts.Lines = prompt_line_range{ 7U, 7U };
		expect(expand_prompt_template(scan_prompt_template("{lines}", prompt_place::agent_tab), facts)->Text == "line 7", "one line");
		facts.OpenFile.reset();
		expect(!expand_prompt_template(scan_prompt_template("{file}", prompt_place::agent_tab), facts), "a missing value expands to nothing at all");

		auto macro               = prompt_macro{ .Id = "own.title", .Body = { .Name = "Title", .Template = "About {pr.title}" } };
		facts.PullRequest->Title = std::string(maximum_prompt_expansion_bytes, 'x');
		const auto tooLong       = decide_prompt_offers(std::vector{ macro }, facts);
		expect(tooLong.Chips.empty() && tooLong.Withheld.size() == 1U && tooLong.Withheld[0].Unmet == prompt_unmet::expansion_too_long, "an expansion over 16 KiB is not offered, with that reason");
		expect(stable_name(prompt_unmet::expansion_too_long) == "expansion-too-long", "the reason's stable name");
		facts.PullRequest->Title = std::string(maximum_prompt_expansion_bytes - 6U, 'x');
		expect(decide_prompt_offers(std::vector{ macro }, facts).Chips.size() == 1U, "exactly 16 KiB is offered");

		expect(prompt_variables(prompt_place::conversation).size() == 9U && prompt_variables(prompt_place::agent_tab).size() == 13U && prompt_variables(prompt_place::tour_stop).size() == 11U,
			   "each place's variables");
		for (auto index = std::size_t{ 0 }; index < prompt_variable_count; ++index) {
			const auto variable = static_cast<prompt_variable>(index);
			expect(find_prompt_variable(stable_name(variable)) == variable, "a variable's name finds it");
		}
	}

	// AC03.
	void test_conditions()
	{
		const auto known   = prompt_facts{ .Place = prompt_place::agent_tab, .Repository = "Octo/App", .OpenFile = "src/render/renderer.cpp", .Lines = prompt_line_range{ 3U, 5U }, .TakesReplies = true, .Draft = true, .AuthorIsMe = false, .ChecksFailing = true, .ChatEmpty = false };
		const auto unknown = prompt_facts{ .Place = prompt_place::agent_tab };
		struct row
		{
			prompt_condition Condition;
			bool             Holds;
		};
		const auto rows = std::vector<row>{
			{ { .Fact = prompt_fact::draft }, true },
			{ { .Fact = prompt_fact::author_is_me }, false },
			{ { .Fact = prompt_fact::checks_failing }, true },
			{ { .Fact = prompt_fact::takes_replies }, true },
			{ { .Fact = prompt_fact::empty_chat }, false },
			{ { .Fact = prompt_fact::path, .Patterns = { "*.cpp" } }, true },
			{ { .Fact = prompt_fact::path, .Patterns = { "*.md", "src/**" } }, true },
			{ { .Fact = prompt_fact::path, .Patterns = { "render/*.cpp" } }, false },
			{ { .Fact = prompt_fact::path, .Patterns = { "/src/render/" } }, true },
			{ { .Fact = prompt_fact::path, .Patterns = { "*.CPP" } }, false },
			{ { .Fact = prompt_fact::repository, .Patterns = { "octo/*" } }, true },
			{ { .Fact = prompt_fact::repository, .Patterns = { "app" } }, true },
			{ { .Fact = prompt_fact::repository, .Patterns = { "other/app" } }, false },
		};
		for (const auto& [condition, holds] : rows) {
			auto negated    = condition;
			negated.Negated = true;
			const auto what = std::string{ stable_name(condition.Fact) };
			expect(prompt_condition_holds(condition, known) == holds && prompt_condition_holds(negated, known) == !holds, "\"is\" and \"is not\" each hold exactly for their value: " + what);
			expect(!prompt_condition_holds(condition, unknown) && !prompt_condition_holds(negated, unknown), "neither holds when the fact is unknown: " + what);
		}
		// The window always knows whether a file is open and lines are selected.
		const auto none = prompt_facts{ .Place = prompt_place::agent_tab, .OpenFile = "a.cpp" };
		for (const auto& [value, three, one] : { std::tuple{ prompt_lines_value::any, true, true }, std::tuple{ prompt_lines_value::one, false, true }, std::tuple{ prompt_lines_value::several, true, false } }) {
			const auto is    = prompt_condition{ .Fact = prompt_fact::selected_lines, .Lines = value };
			auto       isNot = is;
			isNot.Negated    = true;
			auto single      = none;
			single.Lines     = prompt_line_range{ 9U, 9U };
			expect(prompt_condition_holds(is, known) == three && prompt_condition_holds(isNot, known) == !three, "selected lines: several");
			expect(prompt_condition_holds(is, single) == one && prompt_condition_holds(isNot, single) == !one, "selected lines: one");
			expect(!prompt_condition_holds(is, none) && prompt_condition_holds(isNot, none), "selected lines: none");
		}
		expect(prompt_condition_holds({ .Fact = prompt_fact::file_open }, none) && prompt_condition_holds({ .Fact = prompt_fact::file_open, .Negated = true }, unknown), "a file is open, or not");

		// An implied need withholds without a row.
		const auto aboutFile = prompt_macro{ .Id = "own.file", .Body = { .Name = "Look", .Template = "Look at it.", .Subject = prompt_subject::open_file } };
		const auto usesLines = prompt_macro{ .Id = "own.lines", .Body = { .Name = "Lines", .Template = "Explain {lines}." } };
		expect(evaluate_prompt_macro(aboutFile, unknown).Unmet == prompt_unmet::needs_file && evaluate_prompt_macro(aboutFile, none).Offer, "an About withholds until its need holds");
		expect(evaluate_prompt_macro(usesLines, none).Unmet == prompt_unmet::needs_lines && evaluate_prompt_macro(usesLines, known).Offer, "a variable withholds until its value exists");
		const auto ruled = prompt_macro{ .Id = "own.ruled", .Body = { .Name = "Ruled", .Template = "x", .Conditions = { { .Fact = prompt_fact::file_open }, { .Fact = prompt_fact::draft, .Negated = true } } } };
		const auto eval  = evaluate_prompt_macro(ruled, known);
		expect(eval.Unmet == prompt_unmet::condition && eval.Condition == 1U, "the first failing row is named");
		auto off = ruled;
		off.On   = false;
		expect(evaluate_prompt_macro(off, known).Unmet == prompt_unmet::off, "off is never offered");

		// Refusals.
		auto five = prompt_macro{ .Id = "own.five", .Body = { .Name = "Five", .Template = "x" } };
		five.Body.Conditions.assign(5U, { .Fact = prompt_fact::draft });
		const auto has = [](const prompt_macro& aMacro, const prompt_problem aProblem) {
			return std::ranges::any_of(validate_prompt_macro(aMacro), [aProblem](const prompt_macro_problem& aFound) { return aFound.Problem == aProblem; });
		};
		expect(has(five, prompt_problem::too_many_conditions) && !prompt_macro_valid(five), "a fifth condition is refused");
		five.Body.Conditions.resize(4U);
		expect(prompt_macro_valid(five), "four are allowed");
		auto nine = prompt_macro{ .Id = "own.nine", .Body = { .Name = "Nine", .Template = "x", .Conditions = { { .Fact = prompt_fact::path, .Patterns = parse_prompt_patterns("a,b,c,d,e,f,g,h,i") } } } };
		expect(has(nine, prompt_problem::too_many_patterns) && !prompt_macro_valid(nine), "a ninth pattern is refused");
		nine.Body.Conditions[0].Patterns.pop_back();
		expect(prompt_macro_valid(nine), "eight are allowed");
		const auto bracket = prompt_macro{ .Id = "own.bracket", .Body = { .Name = "Bracket", .Template = "x", .Conditions = { { .Fact = prompt_fact::path, .Patterns = { "[abc" } } } } };
		const auto found   = validate_prompt_macro(bracket);
		expect(found.size() == 1U && found[0].Problem == prompt_problem::bad_pattern && found[0].Detail == "[abc" && !prompt_macro_valid(bracket), "an unterminated bracket is refused");
		expect(evaluate_prompt_macro(bracket, known).Unmet == prompt_unmet::invalid, "an invalid stored macro is not offered");
		expect(!wildcard_pattern_valid("a\\") && !wildcard_pattern_valid("[[:nope:]]") && wildcard_pattern_valid("[]]") && wildcard_pattern_valid("[!a-z]*.cpp") && wildcard_pattern_valid("[[:alpha:]]"),
			   "pattern validity follows the matcher");
		expect(parse_prompt_patterns(" src/**,  *.cpp ,, ") == std::vector<std::string>{ "src/**", "*.cpp" }, "the pattern field splits on commas");
		const auto place = prompt_macro{ .Id = "own.place", .Place = prompt_place::conversation, .Body = { .Name = "P", .Template = "x", .Subject = prompt_subject::conversation, .Conditions = { { .Fact = prompt_fact::file_open } } } };
		expect(has(place, prompt_problem::fact_not_for_place), "a fact the place does not hold is refused");
	}

	// AC04.
	void test_order_and_limit()
	{
		auto macros = std::vector<prompt_macro>{ };
		for (auto index = 0; index < 8; ++index) {
			macros.push_back({ .Id = "own." + std::to_string(index), .Body = { .Name = "Prompt " + std::to_string(index), .Template = "Text " + std::to_string(index) } });
		}
		macros[3].Body.Conditions = { { .Fact = prompt_fact::draft } };
		macros.push_back({ .Id = "own.tour", .Place = prompt_place::tour_whole, .Body = { .Name = "Tour", .Template = "x", .Subject = prompt_subject::tour } });
		const auto facts  = prompt_facts{ .Place = prompt_place::agent_tab, .Draft = false };
		const auto offers = decide_prompt_offers(macros, facts);
		const auto ids    = [](const std::vector<prompt_offer>& aOffers) {
			auto result = std::vector<std::string>{ };
			for (const auto& offer : aOffers) {
				result.push_back(offer.Id);
			}
			return result;
		};
		expect(ids(offers.Chips) == std::vector<std::string>{ "own.0", "own.1", "own.2", "own.4", "own.5" }, "the first five that hold, in order, are chips");
		expect(ids(offers.More) == std::vector<std::string>{ "own.6", "own.7" }, "the other two are More, in order");
		expect(offers.Withheld == std::vector<prompt_withheld>{ { .Id = "own.3", .Unmet = prompt_unmet::condition, .Condition = 0U } }, "the withheld one with its reason; another place's are not considered");
		expect(decide_prompt_offers(macros, facts) == offers && evaluate_prompt_macro(macros[3], facts) == evaluate_prompt_macro(macros[3], facts), "equal inputs give equal offers and reasons");
	}

	void test_validation_messages()
	{
		const auto problems = [](const prompt_macro& aMacro, const std::vector<prompt_macro>& aSiblings = { }) {
			auto result = std::vector<prompt_problem>{ };
			for (const auto& found : validate_prompt_macro(aMacro, aSiblings)) {
				result.push_back(found.Problem);
			}
			return result;
		};
		using enum prompt_problem;
		expect(problems({ .Id = "a", .Body = { .Name = " ", .Template = "\n" } }) == std::vector{ no_name, no_template }, "no name and nothing to send");
		const auto longName = validate_prompt_macro({ .Id = "a", .Body = { .Name = std::string(125U, 'n'), .Template = "x" } });
		expect(longName.size() == 1U && longName[0].Problem == name_too_long && longName[0].Excess == 5U, "a long name, by how much");
		expect(problems({ .Id = "a", .Body = { .Name = "Two\nlines", .Template = "x" } }) == std::vector{ name_not_one_line }, "a name is one line");
		const auto longText = validate_prompt_macro({ .Id = "a", .Body = { .Name = "n", .Template = std::string(maximum_prompt_template_bytes + 3U, 't') } });
		expect(longText.size() == 1U && longText[0].Problem == template_too_long && longText[0].Excess == 3U, "a long template, by how much");
		const auto inName = validate_prompt_macro({ .Id = "a", .Body = { .Name = "On {stop.title}", .Template = "{flie}" } });
		expect(inName.size() == 2U && inName[0].Problem == other_place_variable && inName[0].InName && inName[0].Offset == 3U && inName[1].Problem == unknown_variable && !inName[1].InName,
			   "variables are checked in the name and the template");
		expect(problems({ .Id = "a", .Place = prompt_place::conversation, .Body = { .Name = "n", .Template = "x" } }) == std::vector{ subject_not_for_place }, "About is the place's");
		expect(problems({ .Id = "a", .Body = { .Name = "n", .Template = "x", .Subject = prompt_subject::selected_lines, .Kind = prompt_ask::review } }) == std::vector{ kind_not_for_subject }, "lines are not reviewed");
		expect(problems({ .Id = "a", .Body = { .Name = "n", .Template = "x", .Conditions = { { .Fact = prompt_fact::path } } } }) == std::vector{ no_patterns }, "a pattern condition needs a pattern");
		expect(problems({ .Id = "a", .Body = { .Name = "n", .Template = "x", .Conditions = { { .Fact = prompt_fact::path, .Patterns = { std::string(129U, 'p') } } } } }) == std::vector{ pattern_too_long }, "a pattern is 128 bytes");

		// Advice does not block.
		const auto contradict = prompt_macro{ .Id = "a", .Body = { .Name = "n", .Template = "{file}", .Conditions = { { .Fact = prompt_fact::draft }, { .Fact = prompt_fact::file_open, .Negated = true } } } };
		expect(problems(contradict) == std::vector{ conditions_contradict } && prompt_macro_valid(contradict), "a condition against an implied need");
		expect(problems({ .Id = "a", .Body = { .Name = "n", .Template = "x", .Conditions = { { .Fact = prompt_fact::selected_lines, .Lines = prompt_lines_value::one }, { .Fact = prompt_fact::selected_lines, .Lines = prompt_lines_value::several } } } }) == std::vector{ conditions_contradict },
			   "one line and several lines");
		expect(problems({ .Id = "a", .Body = { .Name = "n", .Template = "x", .Conditions = { { .Fact = prompt_fact::draft }, { .Fact = prompt_fact::draft, .Negated = true } } } }) == std::vector{ conditions_contradict }, "is and is not");
		expect(problems({ .Id = "a", .Body = { .Name = "n", .Template = "x", .Conditions = { { .Fact = prompt_fact::selected_lines, .Negated = true }, { .Fact = prompt_fact::path, .Patterns = { "*" } } } } }).empty(), "no lines and a path can hold together");
		const auto shipped = shipped_set();
		expect(problems({ .Id = "own", .Body = { .Name = "Review this pull request", .Template = "x" } }, shipped) == std::vector{ duplicate_name }, "a name already used in this place");
		expect(problems(shipped.front(), shipped).empty(), "a macro is not its own duplicate");
		expect(problems({ .Id = "a", .Body = { .Name = "n", .Template = "Use ghp_abc" } }) == std::vector{ credential_prefix } && problems({ .Id = "a", .Body = { .Name = "n", .Template = "Assess the task-list risk-free" } }).empty(),
			   "a credential prefix is advice, at a word's start");
		expect(!prompt_problem_blocks(credential_prefix) && prompt_problem_blocks(bad_pattern) && stable_name(conditions_contradict) == "conditions-contradict", "which problems block, and their names");
	}

	void test_merge()
	{
		const auto shipped = shipped_prompt_macros();
		const auto plain   = merge_prompt_macros(shipped, { });
		expect(plain.Dropped.empty() && plain.Entries.size() == shipped.size(), "nothing stored is the shipped set");
		for (auto index = std::size_t{ 0 }; index < shipped.size(); ++index) {
			expect(plain.Entries[index].Macro == shipped[index].Macro && !plain.Entries[index].Changed, "in shipped order, unchanged");
		}

		auto changedBody     = shipped[2].Macro.Body;
		changedBody.Template = "What could {pr.title} break?";
		auto stored          = prompt_macro_differences{
			.Changes = {
				{ .Id = "agent-tab.what-could-break", .Body = changedBody, .WrittenAgainst = 0U },
				{ .Id = "agent-tab.review-pull-request", .On = false },
				{ .Id = "agent-tab.review-open-file", .Body = shipped[1].Macro.Body, .WrittenAgainst = 1U },
				{ .Id = "gone.in-this-build", .On = false },
			},
			.Own = {
				{ .Id = "own.1", .Body = { .Name = "Mine", .Template = "Mine" } },
				{ .Id = "agent-tab.explain-line", .Body = { .Name = "Thief", .Template = "x" } },
				{ .Id = "own.2", .BuiltIn = true, .Body = { .Name = "Claims built-in", .Template = "x" } },
				{ .Id = "own.3", .Place = prompt_place::tour_whole, .Body = { .Name = "Tour mine", .Template = "x", .Subject = prompt_subject::tour } },
			},
			.Orders = { { .Place = prompt_place::agent_tab, .Ids = { "own.1", "agent-tab.what-could-break", "agent-tab.review-pull-request", "nowhere" } } },
		};
		const auto merged = merge_prompt_macros(shipped, stored);
		expect(merged.Dropped == std::vector<std::string>{ "gone.in-this-build", "agent-tab.explain-line", "own.2" }, "rows that cannot apply are dropped, by identity");
		auto agentTab = std::vector<std::string>{ };
		for (const auto& entry : merged.Entries) {
			if (entry.Macro.Place == prompt_place::agent_tab) {
				agentTab.push_back(entry.Macro.Id);
			}
		}
		expect(agentTab == std::vector<std::string>{ "own.1", "agent-tab.what-could-break", "agent-tab.review-pull-request", "agent-tab.review-open-file", "agent-tab.explain-line", "agent-tab.explain-block", "agent-tab.what-changed", "agent-tab.explain-file" },
			   "the stored order, then the built-ins it does not name after the place's last built-in");
		const auto entry = [&merged](const std::string_view aId) {
			return *std::ranges::find_if(merged.Entries, [aId](const prompt_set_entry& aEntry) { return aEntry.Macro.Id == aId; });
		};
		expect(entry("agent-tab.what-could-break").Changed && entry("agent-tab.what-could-break").Outdated && entry("agent-tab.what-could-break").Macro.Body == changedBody, "a changed built-in keeps the reviewer's text");
		expect(!entry("agent-tab.review-pull-request").Macro.On && !entry("agent-tab.review-pull-request").Changed, "turning off is not a change of text");
		expect(!entry("agent-tab.review-open-file").Changed, "a body equal to the shipped text is not Changed");
		expect(merged.Entries.back().Macro.Id == "own.3" && merged.Entries.back().Macro.Place == prompt_place::tour_whole, "an own macro with no stored order goes last in its place");

		// A built-in added by a later release.
		auto       later = shipped;
		const auto added = prompt_builtin{ .Macro = { .Id = "agent-tab.new", .BuiltIn = true, .Body = { .Name = "New", .Template = "New" } } };
		later.insert(later.begin() + 5, added);
		auto ordered  = prompt_macro_differences{ .Own    = { { .Id = "own.1", .Body = { .Name = "Mine", .Template = "Mine" } } },
												  .Orders = { { .Place = prompt_place::agent_tab, .Ids = { "agent-tab.explain-block", "own.1", "agent-tab.review-pull-request", "agent-tab.review-open-file", "agent-tab.what-could-break", "agent-tab.explain-line" } } } };
		auto newPlace = std::vector<std::string>{ };
		for (const auto& item : merge_prompt_macros(later, ordered).Entries) {
			if (item.Macro.Place == prompt_place::agent_tab) {
				newPlace.push_back(item.Macro.Id);
			}
		}
		expect(newPlace == std::vector<std::string>{ "agent-tab.explain-block", "own.1", "agent-tab.review-pull-request", "agent-tab.review-open-file", "agent-tab.what-could-break", "agent-tab.explain-line", "agent-tab.new", "agent-tab.what-changed", "agent-tab.explain-file" },
			   "a new built-in appears on, after its place's other built-ins");

		// The profile bound.
		auto many = prompt_macro_differences{ };
		for (auto index = 0; index < 50; ++index) {
			many.Own.push_back({ .Id = "own." + std::to_string(index), .Body = { .Name = "n", .Template = "x" } });
		}
		const auto bounded = merge_prompt_macros(shipped, many);
		expect(bounded.Entries.size() == maximum_prompt_macros && bounded.Dropped.size() == 50U - (maximum_prompt_macros - shipped.size()), "at most 64 macros, the built-ins included");
		expect(merge_prompt_macros(shipped, stored) == merged, "the merge is deterministic");
	}

	/// Milestone 2, AC09. Only the template's own text names skills; a
	/// value is never read as one.
	void test_skill_text()
	{
		const auto expand = [](const std::string_view aTemplate, const std::string& aTitle) {
			auto facts               = prompt_facts{ .Place = prompt_place::agent_tab, .PullRequest = pull_request_facts(), .OpenFile = "src/a.cpp" };
			facts.PullRequest->Title = aTitle;
			return *expand_prompt_template(scan_prompt_template(aTemplate, prompt_place::agent_tab), facts);
		};
		const auto titled = expand("Summarize {pr.title}", "/some-skill please");
		expect(titled.Text == "Summarize /some-skill please" && titled.Values == std::vector<prompt_value_span>{ { .Begin = 10U, .End = 28U } }, "the value's place is recorded");
		expect(find_skill_references(titled.Text).size() == 1U && prompt_skill_text(titled).empty(), "a title naming a skill brings in none");
		const auto own = expand("Check {file}. /security-review then /tests", "x");
		expect(prompt_skill_text(own) == "/security-review /tests", "the template's own references stay, in order");
		expect(prompt_skill_text(expand("Use /skill{pr.title}", "-x")).empty(), "a value that would lengthen a reference makes it none");
		expect(prompt_skill_text(expand("{pr.title}/skill", "")).empty(), "an empty value that would start a reference makes it none");
		expect(prompt_skill_text(expand("{pr.title} /skill", "x")) == "/skill", "a reference after the template's own space is the template's");
		const auto plain = expand("Review this /review-skill", "x");
		expect(plain.Values.empty() && prompt_skill_text(plain) == "/review-skill", "with no values every reference counts, as in a typed question");

		expect(suggested_subject_of(prompt_subject::pull_request) == agent_suggested_subject::pull_request && suggested_subject_of(prompt_subject::open_file) == agent_suggested_subject::open_file
				   && suggested_subject_of(prompt_subject::selected_lines) == agent_suggested_subject::selected_lines,
			   "the Agent tab's three subjects are RFC-BPR-0102's");
		for (const auto subject : { prompt_subject::conversation, prompt_subject::stop, prompt_subject::part, prompt_subject::selected_text, prompt_subject::tour }) {
			expect(!suggested_subject_of(subject), "the other places' subjects name none");
		}
	}

	/// Milestone 2, AC07. The name a turn was sent under, kept bounded.
	void test_turn_names()
	{
		auto names = prompt_turn_names{ };
		expect(!names.remember({ .Asked = "Same", .Name = "Same" }) && !names.remember({ .Asked = "", .Name = "n" }) && !names.remember({ .Asked = "a", .Name = std::string(maximum_prompt_turn_name_bytes + 1U, 'n') }), "a name equal to its question, a blank one and an oversized one are not kept");
		expect(names.remember({ .Asked = "Check a.cpp for injection.", .Name = "Security pass" }) && !names.remember({ .Asked = "Check a.cpp for injection.", .Name = "Security pass" }), "a name is kept once");
		expect(names.recall("Check a.cpp for injection.") == std::optional<std::string>{ "Security pass" } && !names.recall("other"), "and recalled by its question");
		expect(names.remember({ .Asked = "Check a.cpp for injection.", .Name = "Renamed" }) && names.records().size() == 1U && names.recall("Check a.cpp for injection.") == std::optional<std::string>{ "Renamed" }, "one question has the last name it was sent under");
		auto restarted = prompt_turn_names{ };
		restarted.remember({ .Asked = "q2", .Name = "newer" });
		restarted.adopt({ { .Asked = "q1", .Name = "n1" }, { .Asked = "q2", .Name = "older" }, { .Asked = "x", .Name = "x" } });
		expect(restarted.records() == std::vector<prompt_turn_name>{ { .Asked = "q1", .Name = "n1" }, { .Asked = "q2", .Name = "newer" } }, "adopting keeps what was remembered since, and drops invalid rows");
		for (auto index = 0U; index < maximum_prompt_turn_names + 3U; ++index) {
			restarted.remember({ .Asked = "q" + std::to_string(index + 10U), .Name = "n" });
		}
		expect(restarted.records().size() == maximum_prompt_turn_names && !restarted.recall("q1") && restarted.recall("q" + std::to_string(maximum_prompt_turn_names + 12U)), "the least recently sent go first");
	}
} // namespace

auto main() -> int
{
	test_shipped_equals_today();
	test_scan_and_expansion();
	test_conditions();
	test_order_and_limit();
	test_validation_messages();
	test_merge();
	test_skill_text();
	test_turn_names();
	std::cout << "prompt-macro-tests:pass\n";
	return EXIT_SUCCESS;
}
