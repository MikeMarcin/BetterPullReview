// [[BPR-1861]]. An agent's answer split into its prose and its
// `bpr-findings` block. Every body here is synthetic.

#include "bpr/core/agent_answer_findings.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "agent answer findings failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// A suggestion whose body holds a fenced C++ block, written as a model
	/// writes it inside a JSON string: newlines escaped, backticks literal.
	constexpr std::string_view fenced_payload
		= "{\"findings\": [{\"path\": \"src/widget.cpp\", \"startLine\": 12, \"claim\": \"The widget leaks.\", "
		  "\"suggestedComment\": \"suggestion: release it\\n\\n```cpp\\nWidget* widget = make_widget();\\nreturn widget;\\n```\\n\\nThe test is no less strict: which assertion failed?\"}]}";

	void test_no_block_is_all_prose()
	{
		for (const std::string_view answer : { "", "  Just words.\n", "We mention ```bpr-findings inline, not as a fence.", "```json\n{}\n```" }) {
			const auto split = split_agent_answer(answer);
			expect(split.Block == agent_findings_block::absent && split.Prose == answer && split.Payload.empty(), "an answer without an opening line is kept exactly");
		}
	}

	void test_a_fenced_code_suggestion_is_one_block()
	{
		const auto answer = "Summary: one finding.\n\n```bpr-findings\n" + std::string{ fenced_payload } + "\n```\n";
		const auto split  = split_agent_answer(answer);
		expect(split.Block == agent_findings_block::complete, "the block is complete");
		expect(split.Payload == fenced_payload, "the payload is the whole JSON value, through its closing ]}");
		expect(split.Prose == "Summary: one finding.", "the reply shows only the prose");
		for (const std::string_view fragment : { "cpp", "\\n", "```", "]}", "\"}", "Widget" }) {
			expect(split.Prose.find(fragment) == std::string::npos, "no fragment of the payload reaches the prose");
		}
	}

	void test_strings_hold_quotes_braces_and_brackets()
	{
		const auto payload = std::string{ "[{\"claim\": \"a \\\"quoted\\\" } ] { [ word\", \"evidence\": \"C:\\\\dir\\\\\", \"suggestedComment\": \"note: `x[0]` and {y}\"}]" };
		const auto split   = split_agent_answer("Words.\n```bpr-findings\n" + payload + "\n```\nAfter the block.");
		expect(split.Block == agent_findings_block::complete && split.Payload == payload, "escaped quotes, a trailing escaped backslash, braces and brackets in strings are text");
		expect(split.Prose == "Words.\n\nAfter the block.", "prose before and after the block is kept");
	}

	void test_closing_forms()
	{
		const auto sameLine = split_agent_answer("P.\n```bpr-findings\n{\"findings\": []}```");
		expect(sameLine.Block == agent_findings_block::complete && sameLine.Payload == "{\"findings\": []}" && sameLine.Prose == "P.", "the fence may close on the value's last line");
		const auto unclosed = split_agent_answer("P.\n```bpr-findings\n[]\n");
		expect(unclosed.Block == agent_findings_block::complete && unclosed.Payload == "[]" && unclosed.Prose == "P.", "a complete value at the end of the answer needs no closing fence");
		const auto crlf = split_agent_answer("P.\r\n   ```bpr-findings\r\n[\r\n{\"a\": \"b\"}\r\n]\r\n````\r\nQ.\r\n");
		expect(crlf.Block == agent_findings_block::complete && crlf.Payload == "[\r\n{\"a\": \"b\"}\r\n]" && crlf.Prose == "P.\n\nQ.", "CRLF, an indented opener and a longer closing fence");
		const auto twice = split_agent_answer("```bpr-findings\n[1]\n```\nMiddle.\n```bpr-findings\n[2]\n```");
		expect(twice.Block == agent_findings_block::complete && twice.Payload == "[2]" && twice.Prose.starts_with("```bpr-findings\n[1]"), "the last opening line is the block");
	}

	void expect_shown_whole(const agent_answer_split& aSplit, const std::string_view aBefore, const std::string_view aWhy)
	{
		expect(aSplit.Block == agent_findings_block::unreadable, std::string{ aWhy } + ": unreadable");
		expect(aSplit.Prose.starts_with(std::string{ aBefore } + "\n\n```"), std::string{ aWhy } + ": the prose comes first, then a fence");
		// The block is one fenced code block whose fence is longer than any
		// backtick run inside it, so no part of it is read as Markdown.
		const auto open   = aSplit.Prose.find("```", aBefore.size());
		const auto end    = aSplit.Prose.find('\n', open);
		const auto fence  = aSplit.Prose.substr(open, aSplit.Prose.find_first_not_of('`', open) - open);
		const auto inside = aSplit.Prose.substr(end + 1U, aSplit.Prose.size() - fence.size() - end - 2U);
		expect(aSplit.Prose.substr(open, end - open) == fence + "json", std::string{ aWhy } + ": the opening fence names json");
		expect(aSplit.Prose.ends_with("\n" + fence) && inside == aSplit.Payload, std::string{ aWhy } + ": the payload is shown whole and the fence closes it");
		expect(inside.find(fence) == std::string::npos, std::string{ aWhy } + ": nothing inside closes the fence");
	}

	void test_unreadable_blocks_are_shown_whole()
	{
		// Truncated in the middle of the suggestion's code.
		const auto cut = std::string{ fenced_payload.substr(0U, fenced_payload.find("return widget")) };
		expect_shown_whole(split_agent_answer("Summary.\n```bpr-findings\n" + cut), "Summary.", "truncated");
		expect(split_agent_answer("Summary.\n```bpr-findings\n" + cut).Payload == cut, "the truncated payload is everything after the opening line");
		expect_shown_whole(split_agent_answer("S.\n```bpr-findings\n{\"a\": [1}\n```"), "S.", "mismatched brackets");
		const auto trailing = split_agent_answer("S.\n```bpr-findings\n[] and more\n```\nAfter.");
		expect(trailing.Block == agent_findings_block::unreadable && trailing.Payload == "[] and more" && trailing.Prose == "S.\n\n```json\n[] and more\n```\n\nAfter.",
			   "text after the value inside the fence: the block is shown whole as code and the prose after it is kept");
		expect_shown_whole(split_agent_answer("S.\n```bpr-findings\nnot json\n```"), "S.", "not a JSON value");
		const auto longer = split_agent_answer("S.\n```bpr-findings\n{\"a\": \"````\n");
		expect_shown_whole(longer, "S.", "a four-backtick run inside");
		expect(longer.Prose.find("`````json") != std::string::npos, "the fence is longer than the run inside");
		const auto empty = split_agent_answer("S.\n```bpr-findings\n");
		expect(empty.Block == agent_findings_block::unreadable && empty.Prose == "S." && empty.Payload.empty(), "an empty block shows nothing of itself");
	}

	void test_names_and_determinism()
	{
		expect(stable_name(agent_findings_block::absent) == "absent" && stable_name(agent_findings_block::complete) == "complete" && stable_name(agent_findings_block::unreadable) == "unreadable",
			   "stable names");
		const auto answer = "A.\n```bpr-findings\n" + std::string{ fenced_payload } + "\n```";
		expect(split_agent_answer(answer) == split_agent_answer(answer), "equal answers split equally");
	}
} // namespace

auto main() -> int
{
	test_no_block_is_all_prose();
	test_a_fenced_code_suggestion_is_one_block();
	test_strings_hold_quotes_braces_and_brackets();
	test_closing_forms();
	test_unreadable_blocks_are_shown_whole();
	test_names_and_determinism();
	std::cout << "agent-answer-findings-tests:pass\n";
	return EXIT_SUCCESS;
}
