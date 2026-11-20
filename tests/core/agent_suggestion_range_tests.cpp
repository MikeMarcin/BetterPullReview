// [[BPR-2298]] (owner, 2026-11-19). The lines an agent's suggested comment
// is anchored to, so that GitHub, applying its suggestion block, replaces
// exactly the lines the block rewrites: the agent's range kept when the block
// fits it, widened when the block repeats the head lines beside it, and
// flagged when nothing explains a longer block. Applying each decided range
// as GitHub does gives the file the suggestion means.

#include "bpr/core/agent_suggestion_range.hpp"
#include "bpr/core/revisioned_review_draft.hpp"

#include <cstdint>
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
			std::cerr << "agent suggestion range failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// The nested-folders fixture's head `src/engine/renderer.cpp`.
	[[nodiscard]] auto renderer_lines() -> std::vector<std::string>
	{
		return {
			"#include \"renderer.h\"",
			"",
			"void Renderer::draw()",
			"{",
			"\tclear();",
			"\tdraw_scene();",
			"\tpresent();",
			"}",
			"",
			"void Renderer::resize(const int aWidth, const int aHeight)",
			"{",
			"\tWidth  = aWidth;",
			"\tHeight = aHeight;",
			"}",
		};
	}

	[[nodiscard]] auto comment(const std::string_view aBlock) -> std::string
	{
		return "issue: `resize` accepts negative sizes.\n\n```suggestion\n" + std::string{ aBlock } + "```\n";
	}

	[[nodiscard]] auto decide(const std::string_view aComment, const std::uint32_t aFirst, const std::uint32_t aLast, const std::vector<std::string>& aHead = renderer_lines()) -> agent_suggestion_range
	{
		return decide_agent_suggestion_range(aComment, aFirst, aLast, aHead, 1U, static_cast<std::uint32_t>(aHead.size()));
	}

	/// What GitHub makes of the file when the suggestion is applied on
	/// one-based lines `aFirst` to `aLast`: those lines replaced by the block's.
	[[nodiscard]] auto apply(const std::vector<std::string>& aHead, const std::uint32_t aFirst, const std::uint32_t aLast, const std::string_view aComment) -> std::vector<std::string>
	{
		auto       result = std::vector<std::string>(aHead.begin(), aHead.begin() + (aFirst - 1U));
		const auto block  = single_suggestion_lines(aComment);
		expect(block.has_value(), "the comment has one suggestion block");
		result.insert(result.end(), block->begin(), block->end());
		result.insert(result.end(), aHead.begin() + aLast, aHead.end());
		return result;
	}

	/// A document split from text as the decoder splits it, `\n` endings.
	[[nodiscard]] auto document(const std::vector<std::string>& aLines) -> decoded_text_document
	{
		auto result = decoded_text_document{ };
		for (const auto& line : aLines) {
			auto record    = decoded_text_line{ };
			record.Content = { .Begin = result.Bytes.size(), .End = result.Bytes.size() + line.size() };
			result.Bytes.insert(result.Bytes.end(), line.begin(), line.end());
			record.Terminator = { .Begin = result.Bytes.size(), .End = result.Bytes.size() + 1U };
			record.Ending     = text_line_ending::lf;
			result.Bytes.push_back('\n');
			result.Lines.push_back(record);
		}
		result.HasFinalNewline = true;
		return result;
	}

	void test_the_block_read()
	{
		expect(!single_suggestion_lines("No block here.").has_value(), "a comment without a block has none");
		expect(!single_suggestion_lines("```suggestion\na\n```\n\n```suggestion\nb\n```\n").has_value(), "a comment with two blocks has none to check");
		expect(single_suggestion_lines("```suggestion\r\na\r\nb\r\n```\r\n") == std::vector<std::string>{ "a", "b" }, "CRLF lines are read without their CR");
		expect(single_suggestion_lines("```suggestion\n```\n") == std::vector<std::string>{ }, "an empty block deletes and has no lines");
		expect(single_suggestion_lines("```suggestion\na\nb") == std::vector<std::string>{ "a", "b" }, "an unclosed block runs to the end");
		const auto none = decide("Clamp the sizes.", 12U, 12U);
		expect(none.Reading == agent_suggestion_range_reading::no_suggestion && none.First == 12U && none.Last == 12U && stable_name(none.Reading) == "no_suggestion",
			   "a comment with no suggestion keeps the agent's lines");
	}

	/// The owner's report: the agent names the line at fault, and its block
	/// also carries the unchanged line after it.
	void test_a_block_that_carries_the_next_line_widens_to_it()
	{
		const auto text  = comment("\tWidth  = std::max(aWidth, 0);\n\tHeight = aHeight;\n");
		const auto range = decide(text, 12U, 12U);
		expect(range.Reading == agent_suggestion_range_reading::widened && range.First == 12U && range.Last == 13U && range.SuggestedLines == 2U && stable_name(range.Reading) == "widened",
			   "the block's unchanged second line widens the range to it");
		const auto renderer = renderer_lines();
		auto       expected = renderer;
		expected[11]        = "\tWidth  = std::max(aWidth, 0);";
		expect(apply(renderer, range.First, range.Last, text) == expected, "applied on the decided lines, the suggestion clamps the width and changes nothing else");
		expect(apply(renderer, 12U, 12U, text).size() == renderer.size() + 1U, "applied on the agent's one line, it writes `Height` twice: the defect");
	}

	void test_a_block_that_carries_the_line_before_widens_back_to_it()
	{
		const auto text  = comment("\tWidth  = aWidth;\n\tHeight = std::max(aHeight, 0);\n");
		const auto range = decide(text, 13U, 13U);
		expect(range.Reading == agent_suggestion_range_reading::widened && range.First == 12U && range.Last == 13U, "the block's unchanged first line widens the range back to it");
		const auto renderer = renderer_lines();
		auto       expected = renderer;
		expected[12]        = "\tHeight = std::max(aHeight, 0);";
		expect(apply(renderer, range.First, range.Last, text) == expected, "applied, it clamps the height alone");
	}

	void test_a_whole_function_rewritten_from_its_first_line()
	{
		const auto text  = comment("void Renderer::resize(const int aWidth, const int aHeight)\n{\n\tWidth  = std::max(aWidth, 0);\n\tHeight = std::max(aHeight, 0);\n}\n");
		const auto range = decide(text, 10U, 10U);
		expect(range.Reading == agent_suggestion_range_reading::widened && range.First == 10U && range.Last == 14U, "a block that rewrites the middle of a function widens to the whole function");
		const auto renderer = renderer_lines();
		auto       expected = renderer;
		expected[11]        = "\tWidth  = std::max(aWidth, 0);";
		expected[12]        = "\tHeight = std::max(aHeight, 0);";
		expect(apply(renderer, range.First, range.Last, text) == expected, "applied, only the two assignments change");
		// The same block named at the middle line widens both ways.
		const auto middle = decide(text, 12U, 13U);
		expect(middle.Reading == agent_suggestion_range_reading::widened && middle.First == 10U && middle.Last == 14U, "named at the lines at fault, it widens both ways");
		// Already right: nothing to do.
		const auto right = decide(text, 10U, 14U);
		expect(right.Reading == agent_suggestion_range_reading::kept && right.First == 10U && right.Last == 14U, "a range that already covers the block is kept");
	}

	void test_an_insertion_keeps_the_agents_line()
	{
		// The block keeps the line it is on and adds one after it.
		const auto text  = comment("\tpresent();\n\tlog_frame();\n");
		const auto range = decide(text, 7U, 7U);
		expect(range.Reading == agent_suggestion_range_reading::kept && range.First == 7U && range.Last == 7U && stable_name(range.Reading) == "kept", "a block that keeps its line and adds after it is kept");
		// A new function after `}` whose own closing brace matches the next
		// brace below is not taken as a rewrite of it: a brace alone is not
		// evidence.
		const auto braces = std::vector<std::string>{ "namespace a {", "\tvoid f() {", "\t}", "", "}" };
		const auto added  = comment("\t}\n\n\tvoid g() {\n\t}\n");
		const auto kept   = decide(added, 3U, 3U, braces);
		expect(kept.Reading == agent_suggestion_range_reading::kept && kept.First == 3U && kept.Last == 3U, "a closing brace below never widens a range");
	}

	void test_a_block_no_longer_than_its_lines_is_kept()
	{
		const auto text = comment("\tWidth  = std::max(aWidth, 0);\n");
		expect(decide(text, 12U, 12U).Reading == agent_suggestion_range_reading::kept, "one line for one line is kept");
		expect(decide(text, 12U, 13U).Reading == agent_suggestion_range_reading::kept, "one line for two (a deletion with it) is kept");
		expect(decide(comment(""), 12U, 13U).Reading == agent_suggestion_range_reading::kept, "an empty block deletes its lines and is kept");
	}

	void test_an_unexplained_longer_block_is_flagged()
	{
		const auto text  = comment("\tif (aWidth < 0) {\n\t\treturn;\n\t}\n");
		const auto range = decide(text, 12U, 12U);
		expect(range.Reading == agent_suggestion_range_reading::unverified && range.First == 12U && range.Last == 12U && range.SuggestedLines == 3U && stable_name(range.Reading) == "unverified",
			   "a longer block that keeps no line around it keeps the agent's line, flagged");
	}

	void test_lines_outside_the_document()
	{
		const auto text = comment("a\nb\n");
		expect(decide(text, 40U, 40U).Reading == agent_suggestion_range_reading::outside_document, "a line past the end is outside the document");
		expect(decide(text, 0U, 1U).Reading == agent_suggestion_range_reading::outside_document, "line zero is outside the document");
		expect(decide(text, 3U, 2U).Reading == agent_suggestion_range_reading::outside_document, "a reversed range is outside the document");
	}

	void test_the_document_reads_only_its_window()
	{
		const auto head = document(renderer_lines());
		const auto text = comment("\tWidth  = std::max(aWidth, 0);\n\tHeight = aHeight;\n");
		expect(decide_agent_suggestion_range(text, 12U, 12U, head) == decide(text, 12U, 12U), "the document gives the lines' decision");
		expect(decide_agent_suggestion_range(text, 12U, 12U, head) == decide_agent_suggestion_range(text, 12U, 12U, head), "equal inputs give equal decisions");
		expect(decide_agent_suggestion_range(text, 15U, 15U, head).Reading == agent_suggestion_range_reading::outside_document, "past the document's end");
		// A window at the file's first line.
		const auto first = comment("#include \"renderer.h\"\n#include <algorithm>\n");
		expect(decide_agent_suggestion_range(first, 1U, 1U, head).Reading == agent_suggestion_range_reading::kept, "an include added after the first line is kept");
	}

	void test_the_draft_takes_the_range_in_one_edit()
	{
		auto draft      = review_comment_draft{ };
		draft.Id        = review_draft_id{ 7U };
		draft.Anchor    = source_line_ref{ .Comparison = file_comparison_digest{ }, .Side = diff_side::right, .Line = 12U };
		draft.State     = review_draft_state::drafting;
		const auto held = revisioned_review_draft{ draft, 1U, review_draft_scope_id{ 3U } };
		const auto edit = promote_private_text_on_lines("Clamp it.", held, 11U);
		expect(edit.has_value() && edit->ChangeRange && edit->StartLine == std::optional<std::uint32_t>{ 11U } && edit->Command.Body == "Clamp it.", "the text and the first line go in one edit");
		const auto applied = apply(held, *edit);
		expect(applied.Refusal == revisioned_draft_refusal::none && applied.Accepted.Draft.StartLine == std::optional<std::uint32_t>{ 11U } && applied.Accepted.Draft.Anchor.Line == 12U,
			   "the draft is on lines 12 to 13 (zero-based 11 to 12)");
		expect(!promote_private_text_on_lines("Clamp it.", held, 12U).has_value(), "a first line at the anchor is refused, as GitHub would");
		const auto same = promote_private_text_on_lines("Clamp it.", held, std::nullopt);
		expect(same.has_value() && !same->ChangeRange, "one line on a one-line draft is a plain promotion");
	}
} // namespace

auto main() -> int
{
	test_the_block_read();
	test_a_block_that_carries_the_next_line_widens_to_it();
	test_a_block_that_carries_the_line_before_widens_back_to_it();
	test_a_whole_function_rewritten_from_its_first_line();
	test_an_insertion_keeps_the_agents_line();
	test_a_block_no_longer_than_its_lines_is_kept();
	test_an_unexplained_longer_block_is_flagged();
	test_lines_outside_the_document();
	test_the_document_reads_only_its_window();
	test_the_draft_takes_the_range_in_one_edit();
	std::cout << "agent suggestion range: pass\n";
	return EXIT_SUCCESS;
}
