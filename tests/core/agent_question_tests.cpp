/// \file
/// \brief [[BPR-2002]], RFC-BPR-0102. A question says what it points at, and
/// a suggestion always sends.

#include "bpr/core/agent_question.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using core::agent_entry_point;
	using core::agent_question_subject;
	using core::agent_suggested_subject;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_review_this_pull_request_never_needs_a_selection()
	{
		// AC01, AC02: with nothing selected, and with lines selected and
		// attached, the review suggestion is the same request.
		for (const auto lines : { false, true }) {
			const auto question = core::decide_agent_question({ .LinesAttached = lines, .LinesSelected = lines, .Suggested = agent_suggested_subject::pull_request, .Review = true, .OpenFile = "src/a.cpp" });
			expect(question && question->Subject == agent_question_subject::pull_request && question->EntryPoint == agent_entry_point::run_ai_review && question->File.empty(),
				   "Review this pull request is about the pull request, with the review entry point, selected or not");
			expect(!core::acts_on_selection(question->EntryPoint), "and its entry point needs no selection");
		}
	}

	void test_a_suggestion_carries_its_subject()
	{
		const auto file = core::decide_agent_question({ .Suggested = agent_suggested_subject::open_file, .Review = true, .OpenFile = "src/render.cpp" });
		expect(file && file->Subject == agent_question_subject::file && file->File == "src/render.cpp" && file->EntryPoint == agent_entry_point::run_ai_review,
			   "Review <file> names that file");
		expect(!core::decide_agent_question({ .Suggested = agent_suggested_subject::open_file, .Review = true }), "a file suggestion with no file open is not offered");
		const auto broke = core::decide_agent_question({ .LinesAttached = true, .LinesSelected = true, .Suggested = agent_suggested_subject::pull_request });
		expect(broke && broke->Subject == agent_question_subject::pull_request && broke->EntryPoint == agent_entry_point::open_full_agent_session,
			   "a question suggestion about the pull request ignores the attachment");
		const auto explain = core::decide_agent_question({ .LinesSelected = true, .Suggested = agent_suggested_subject::selected_lines });
		expect(explain && explain->Subject == agent_question_subject::lines && explain->EntryPoint == agent_entry_point::ask_about_selection,
			   "Explain lines sends the selected lines, even detached");
		expect(!core::decide_agent_question({ .Suggested = agent_suggested_subject::selected_lines }), "Explain lines is offered only while lines are selected");
	}

	void test_a_typed_question_is_about_the_attachment_or_the_pull_request()
	{
		// AC04, AC07.
		const auto attached = core::decide_agent_question({ .LinesAttached = true, .LinesSelected = true });
		expect(attached && attached->Subject == agent_question_subject::lines && attached->EntryPoint == agent_entry_point::ask_about_selection, "attached lines are the subject");
		const auto detached = core::decide_agent_question({ .LinesAttached = false, .LinesSelected = true });
		expect(detached && detached->Subject == agent_question_subject::pull_request && !core::acts_on_selection(detached->EntryPoint),
			   "detached lines leave the pull request as the subject, and no selection is required");
		const auto nothing = core::decide_agent_question({ });
		expect(nothing && nothing->Subject == agent_question_subject::pull_request && !core::acts_on_selection(nothing->EntryPoint), "a typed question with nothing selected is sent");
		const auto conversation = core::decide_agent_question({ .LinesAttached = true, .LinesSelected = true, .ConversationAttached = true });
		expect(conversation && conversation->Subject == agent_question_subject::conversation && conversation->EntryPoint == agent_entry_point::ask_about_conversation,
			   "a conversation chip keeps its own subject");
	}

	void test_a_subject_engages_exactly_what_its_kind_means()
	{
		using core::supplement_subject;
		expect(core::valid(supplement_subject{ .Subject = agent_question_subject::pull_request, .InScope = { "a.cpp", "b.cpp" } }), "pull request with files in scope");
		expect(!core::valid(supplement_subject{ .Subject = agent_question_subject::pull_request, .Path = "a.cpp" }), "a pull request subject names no one file");
		expect(core::valid(supplement_subject{ .Subject = agent_question_subject::file, .Path = "a.cpp" }), "a file subject names its file");
		expect(!core::valid(supplement_subject{ .Subject = agent_question_subject::file }), "a file subject without a file");
		expect(core::valid(supplement_subject{ .Subject = agent_question_subject::lines, .Path = "a.cpp", .FirstLine = 5U, .LastLine = 8U, .Text = "x\n" }), "lines with their text");
		expect(!core::valid(supplement_subject{ .Subject = agent_question_subject::lines, .Path = "a.cpp", .FirstLine = 8U, .LastLine = 5U }), "lines out of order");
		expect(!core::valid(supplement_subject{ .Subject = agent_question_subject::lines, .Path = "a.cpp", .FirstLine = 1U, .LastLine = 1U, .Text = std::string(core::maximum_subject_text_bytes + 1U, 'x') }),
			   "text past its bound");
		expect(core::valid(core::supplement_looking_at{ .Path = "a.cpp", .Line = 3U }) && !core::valid(core::supplement_looking_at{ }), "where the reviewer is looking names a file");
		for (const auto kind : { agent_question_subject::pull_request, agent_question_subject::file, agent_question_subject::lines, agent_question_subject::conversation }) {
			expect(!core::stable_name(kind).empty() && !core::agent_question_statement(kind).empty(), "every subject has a name and a statement");
		}
	}
} // namespace

auto main() -> int
{
	test_review_this_pull_request_never_needs_a_selection();
	test_a_suggestion_carries_its_subject();
	test_a_typed_question_is_about_the_attachment_or_the_pull_request();
	test_a_subject_engages_exactly_what_its_kind_means();
	std::cout << "agent-question-tests:pass\n";
	return EXIT_SUCCESS;
}
