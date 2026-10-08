// [[BPR-1091]]. RFC-BPR-0051 R37: a comment-workshop candidate is compared
// with the original it would replace, by the shared Conventional Comments
// recognizer for the label, by the bytes of every suggestion block for the
// patch, and by the prose for whether it still asks a question. A prose-only
// enhancement keeps all three; anything else is named.

#include "bpr/core/feedback_workshop_candidate.hpp"
#include "bpr/core/suggestion_draft.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "feedback workshop candidate failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_suggestion_blocks_are_exact_bytes()
	{
		expect(suggestion_blocks("Plain prose.").empty(), "a body with no fence has no block");
		expect(suggestion_blocks("```cpp\nint x;\n```").empty(), "an ordinary code fence is not a suggestion");
		const auto crlf = suggestion_blocks("Use this:\r\n```suggestion\r\n\tlock();\r\n  x = 1; \r\n```\r\nThanks");
		expect(crlf.size() == 1U && crlf.front() == "\tlock();\r\n  x = 1; \r\n", "the block keeps its tabs, trailing spaces and CRLF byte for byte");
		const auto empty = suggestion_blocks("```suggestion\n```");
		expect(empty.size() == 1U && empty.front().empty(), "an empty block (a deletion) is a block with no bytes");
		const auto two = suggestion_blocks("~~~~ suggestion\nfirst\n~~~~\nand\n```suggestion\nsecond\n```");
		expect(two.size() == 2U && two[0] == "first\n" && two[1] == "second\n", "every block, tilde and longer fences included, in order");
		const auto open = suggestion_blocks("```suggestion\nunclosed");
		expect(open.size() == 1U && open.front() == "unclosed", "an unclosed block runs to the end, as the suggestion editor reads it");
		const auto nested = suggestion_blocks("````suggestion\n```\ncode\n```\n````");
		expect(nested.size() == 1U && nested.front() == "```\ncode\n```\n", "a shorter fence inside does not close a longer one");
	}

	void test_questions_are_read_from_prose_only()
	{
		expect(workshop_asks_question("Could this race?"), "a sentence ending in a question mark asks");
		expect(workshop_asks_question("question: is this safe? I think so."), "a question mid-body asks");
		expect(workshop_asks_question("(really?)"), "closing punctuation after the mark still ends the question");
		expect(!workshop_asks_question("This races."), "a statement asks nothing");
		expect(!workshop_asks_question("See https://example.test/?page=2 for details."), "a query string asks nothing");
		expect(!workshop_asks_question("Use `a ? b : c` here."), "inline code asks nothing");
		expect(!workshop_asks_question("Replace with:\n```suggestion\nx = ready ? 1 : 0;\nwhy?\n```"), "fenced code asks nothing");
		expect(workshop_asks_question("Is this right?\r\nThanks."), "CRLF after the mark ends the question");
	}

	void test_a_prose_only_candidate_keeps_everything()
	{
		const auto change = compare_workshop_candidate(
			"question (non-blocking, security): could this race?\n\n```suggestion\n\tlock();\r\n```",
			"question (non-blocking, security): could this race when two readers refresh at once?\n\n```suggestion\n\tlock();\r\n```");
		expect(change.Label == workshop_label_change::kept && change.Question == workshop_question_change::kept && change.Patch == workshop_patch_change::kept, "label, question and patch are kept");
		expect(change.prose_only() && change.candidate_readable(), "so the candidate is prose only and readable");
		expect(change.After.Label == feedback_label::question && change.After.Expectation == feedback_expectation::non_blocking && change.After.Decorations == std::vector<std::string>{ "non-blocking", "security" },
			   "the facts are the shared recognizer's, custom decoration included");
		const auto plain = compare_workshop_candidate("Needs a test.", "This needs a test that fails without the check.");
		expect(plain.Label == workshop_label_change::none && plain.Question == workshop_question_change::none && plain.Patch == workshop_patch_change::none && plain.prose_only(), "plain prose reworded changes nothing but the words");
		const auto relettered = compare_workshop_candidate("Question: is this needed?", "question: is this needed at all?");
		expect(relettered.Label == workshop_label_change::kept, "the same catalog label in another case is the same label");
	}

	void test_semantic_and_patch_changes_are_named()
	{
		const auto stronger = compare_workshop_candidate("question: could this race?", "issue (blocking): this races.");
		expect(stronger.Label == workshop_label_change::changed && stronger.Question == workshop_question_change::removed && !stronger.prose_only(), "strengthening a question into a blocking issue is a label change and a dropped question");
		const auto decorated = compare_workshop_candidate("suggestion: rename it.", "suggestion (blocking): rename it.");
		expect(decorated.Label == workshop_label_change::changed && decorated.After.Expectation == feedback_expectation::blocking, "adding a decoration is a label change");
		const auto custom = compare_workshop_candidate("nitpick (ux): spacing.", "nitpick (a11y): spacing.");
		expect(custom.Label == workshop_label_change::changed, "replacing a custom decoration is a label change");
		const auto added = compare_workshop_candidate("Rename it.", "suggestion: rename it.");
		expect(added.Label == workshop_label_change::added && added.Before.Status == feedback_prefix_status::plain, "a label on a plain comment is added");
		const auto removed = compare_workshop_candidate("praise: nice.", "Nice.");
		expect(removed.Label == workshop_label_change::removed, "a label dropped is removed");
		const auto unknown = compare_workshop_candidate("issue: this races.", "isue: this races.");
		expect(unknown.Label == workshop_label_change::changed && !unknown.candidate_readable() && unknown.After.Status == feedback_prefix_status::unknown_label && unknown.After.RawLabel == "isue", "a misspelled label is a change the recognizer cannot read");
		const auto malformed = compare_workshop_candidate("issue: this races.", "issue (blocking: this races.");
		expect(malformed.Label == workshop_label_change::changed && malformed.After.Status == feedback_prefix_status::malformed, "a broken header is a change to an unreadable header");
		const auto unknownKept = compare_workshop_candidate("risk: this races.", "risk: this races under load.");
		expect(unknownKept.Label == workshop_label_change::kept, "the same unknown token is kept");
		const auto unknownChanged = compare_workshop_candidate("risk: this races.", "hazard: this races.");
		expect(unknownChanged.Label == workshop_label_change::changed, "a different unknown token is a change");
		const auto asked = compare_workshop_candidate("This races.", "Does this race?");
		expect(asked.Question == workshop_question_change::added, "a statement turned into a question is named");

		const auto rewritten = compare_workshop_candidate("Use:\n```suggestion\nlock();\n```", "Use:\n```suggestion\nlock_shared();\n```");
		expect(rewritten.Patch == workshop_patch_change::changed, "a rewritten patch is changed");
		const auto lineEnding = compare_workshop_candidate("```suggestion\nlock();\r\n```", "```suggestion\nlock();\n```");
		expect(lineEnding.Patch == workshop_patch_change::changed, "a patch whose line ending changed is changed: the bytes are the patch");
		const auto patchAdded = compare_workshop_candidate("Lock it.", "Lock it.\n```suggestion\nlock();\n```");
		expect(patchAdded.Patch == workshop_patch_change::added, "a patch the original did not have is added");
		const auto patchRemoved = compare_workshop_candidate("Lock it.\n```suggestion\nlock();\n```", "Lock it.");
		expect(patchRemoved.Patch == workshop_patch_change::removed, "a patch dropped is removed");
		const auto fenceRestyled = compare_workshop_candidate("```suggestion\nlock();\n```", "~~~suggestion\nlock();\n~~~");
		expect(fenceRestyled.Patch == workshop_patch_change::kept, "the same replacement bytes in another fence style is the same patch");
	}

	void test_names_are_stable()
	{
		expect(stable_name(workshop_label_change::changed) == "changed" && stable_name(workshop_label_change::kept) == "kept", "label change names");
		expect(stable_name(workshop_question_change::removed) == "removed", "question change names");
		expect(stable_name(workshop_patch_change::none) == "none" && stable_name(workshop_patch_change::added) == "added", "patch change names");
	}
} // namespace

auto main() -> int
{
	test_suggestion_blocks_are_exact_bytes();
	test_questions_are_read_from_prose_only();
	test_a_prose_only_candidate_keeps_everything();
	test_semantic_and_patch_changes_are_named();
	test_names_are_stable();
	std::cout << "feedback-workshop-candidate-tests:pass\n";
	return EXIT_SUCCESS;
}
