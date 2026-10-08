// [[BPR-1212]]. Suggest change on a pull request line: the draft starts as a
// suggestion block holding the selected lines exactly, and the draft shows
// what the block changes as the reviewer edits it.

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/review_publication_policy.hpp"
#include "bpr/core/suggestion_draft.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "suggestion draft failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// A document whose lines end as written: "\n", "\r\n" or nothing.
	auto document(const std::vector<std::pair<std::string, std::string>>& aLines) -> decoded_text_document
	{
		auto result = decoded_text_document{ };
		for (const auto& [content, ending] : aLines) {
			const auto begin = static_cast<std::uint64_t>(result.Bytes.size());
			result.Bytes.insert(result.Bytes.end(), content.begin(), content.end());
			const auto end = static_cast<std::uint64_t>(result.Bytes.size());
			result.Bytes.insert(result.Bytes.end(), ending.begin(), ending.end());
			auto line       = decoded_text_line{ };
			line.Content    = byte_span{ .Begin = begin, .End = end };
			line.Terminator = byte_span{ .Begin = end, .End = static_cast<std::uint64_t>(result.Bytes.size()) };
			line.Ending     = ending == "\r\n" ? text_line_ending::crlf : ending == "\n" ? text_line_ending::lf
																						 : text_line_ending::none;
			result.Lines.push_back(line);
		}
		result.HasFinalNewline = !aLines.empty() && !aLines.back().second.empty();
		return result;
	}

	/// [[BPR-1212]] slice 1. A document split from text as the decoder splits it.
	auto document(const std::string_view aText) -> decoded_text_document
	{
		auto result = decoded_text_document{ };
		result.Bytes.assign(aText.begin(), aText.end());
		auto begin = std::uint64_t{ 0U };
		for (std::uint64_t index = 0U; index < aText.size(); ++index) {
			if (aText[index] != '\n') {
				continue;
			}
			const auto crlf = index > begin && aText[index - 1U] == '\r';
			auto       line = decoded_text_line{ };
			line.Content    = { .Begin = begin, .End = crlf ? index - 1U : index };
			line.Terminator = { .Begin = crlf ? index - 1U : index, .End = index + 1U };
			line.Ending     = crlf ? text_line_ending::crlf : text_line_ending::lf;
			result.Lines.push_back(line);
			begin = index + 1U;
		}
		if (begin < aText.size()) {
			auto line    = decoded_text_line{ };
			line.Content = { .Begin = begin, .End = aText.size() };
			result.Lines.push_back(line);
		} else {
			result.HasFinalNewline = true;
		}
		return result;
	}

	/// The block Suggest change starts on lines `aFirst` to `aLast`, as the
	/// window makes it from a draft's range.
	auto block(const decoded_text_document& aDocument, const std::uint32_t aFirst, const std::uint32_t aLast) -> std::optional<std::string>
	{
		const auto lines = suggestion_source_lines(aDocument, aFirst, aLast);
		return lines.has_value() ? std::optional{ suggestion_fence_body(*lines) } : std::nullopt;
	}

	/// [[BPR-1212]] slice 1, moved here with the one fence builder: a range's
	/// block holds the selected lines exactly.
	void range_block()
	{
		const auto text = document("zero\n\tone  \r\ntwo \xC3\xA9\n\nfour ``` and ````\nlast");
		expect(block(text, 1U, 2U) == std::optional<std::string>{ "```suggestion\n\tone  \ntwo \xC3\xA9\n```" },
			   "the lines' bytes without their endings, tabs and trailing spaces kept, inside a fence");
		expect(block(text, 0U, 0U) == std::optional<std::string>{ "```suggestion\nzero\n```" }, "one line is a one-line block");
		expect(block(text, 3U, 3U) == std::optional<std::string>{ "```suggestion\n\n```" }, "an empty line is kept as an empty line");
		expect(block(text, 4U, 5U) == std::optional<std::string>{ "`````suggestion\nfour ``` and ````\nlast\n`````" },
			   "a line holding backtick runs gets a longer fence, and a last line with no newline is whole");
		expect(!block(text, 5U, 6U) && !block(text, 2U, 1U), "a line out of range or a reversed range starts nothing");
		expect(!block(document("bad \xC3\x28\n"), 0U, 0U), "text that is not UTF-8 starts nothing");
		const auto three = block(text, 0U, 2U);
		expect(three && classify_review_body(*three) == review_body_compatibility::requires_line_subject,
			   "the block is a suggestion the publication policy recognises");
		// The caret goes to the end of the block's last line: just before the
		// body's last line break.
		expect(three && three->rfind('\n') == three->size() - 4U && three->substr(three->rfind('\n') - 6U, 6U) == "two \xC3\xA9",
			   "the caret's place is the end of the range's last line inside the block");
		expect(suggested_change_of(*three, *suggestion_source_lines(text, 0U, 2U)).unchanged(), "an unedited range suggestion changes nothing");
	}

	void source_lines()
	{
		const auto text = document({ { "int a;", "\n" }, { "\tint b = 1;  ", "\r\n" }, { "int c;", "" } });
		const auto one  = suggestion_source_lines(text, 1U, 1U);
		expect(one.has_value() && *one == std::vector<std::string>{ "\tint b = 1;  " }, "a line is taken exactly, its tab and trailing blanks kept and its CRLF left out");
		const auto all = suggestion_source_lines(text, 0U, 2U);
		expect(all.has_value() && all->size() == 3U && all->back() == "int c;", "a range runs to its last line, with no final newline invented");
		expect(!suggestion_source_lines(text, 2U, 3U).has_value(), "a range past the document is refused, not truncated");
		expect(!suggestion_source_lines(text, 2U, 1U).has_value(), "a reversed range is refused");
	}

	void fence_body()
	{
		const auto lines = std::vector<std::string>{ "\tconst auto width = 1;", "" };
		const auto body  = suggestion_fence_body(lines);
		expect(body == "```suggestion\n\tconst auto width = 1;\n\n```", "the block holds the lines exactly, an empty line included");
		expect(classify_review_body(body) == review_body_compatibility::requires_line_subject, "the publication policy reads the body as a suggestion");
		expect(body.rfind('\n') == body.size() - 4U, "the caret's place, the end of the last line inside the block, is just before the last line break");
		const auto quoting = std::vector<std::string>{ "  ````markdown", "text" };
		expect(suggestion_fence_body(quoting).starts_with("`````suggestion\n") && suggestion_fence_body(quoting).ends_with("\n`````"),
			   "a quoted fence is outrun by one backtick, so the block cannot close early");
		expect(suggested_change_of(suggestion_fence_body(quoting), quoting).unchanged(), "a longer fence still reads back as the same lines");
	}

	void change()
	{
		const auto original = std::vector<std::string>{ "one", "two", "three" };
		const auto same     = suggested_change_of(suggestion_fence_body(original), original);
		expect(same.Fenced && same.unchanged(), "an unedited suggestion changes nothing");
		const auto edited = suggested_change_of("Why:\n```suggestion\none\n2\nthree\n```\nafter", original);
		expect(edited.Fenced && edited.Removed == std::vector<std::string>{ "two" } && edited.Added == std::vector<std::string>{ "2" },
			   "the shared first and last lines are left out of what it changes");
		const auto grown = suggested_change_of("```suggestion\none\ntwo\ntwo and a half\nthree\n```", original);
		expect(grown.Removed.empty() && grown.Added == std::vector<std::string>{ "two and a half" }, "an inserted line is added and removes nothing");
		const auto open = suggested_change_of("```suggestion\none\nTWO", original);
		expect(open.Fenced && open.Removed == std::vector<std::string>{ "two", "three" } && open.Added == std::vector<std::string>{ "TWO" },
			   "a block still being typed runs to the end of the body");
		const auto crlf = suggested_change_of("```suggestion\r\none\r\ntwo\r\nthree\r\n```", original);
		expect(crlf.Fenced && crlf.unchanged(), "a CR before a line break is not part of a line");
		const auto tilde = suggested_change_of("~~~ suggestion\none\n~~~", original);
		expect(tilde.Fenced && tilde.Removed == std::vector<std::string>{ "two", "three" }, "a tilde fence with a space before its info is a suggestion too");
		expect(!suggested_change_of("suggestion: keep it", original).Fenced, "prose that says suggestion is not a block");
		expect(!suggested_change_of("```suggestions\none\n```", original).Fenced, "another info word is not a suggestion block");
		expect(!suggested_change_of("```cpp\none\n```", original).Fenced, "an ordinary code block is not a suggestion");
		const auto shortClose = suggested_change_of("````suggestion\none\n```\ntwo\n````", original);
		expect(shortClose.Added == std::vector<std::string>{ "```", "two" } && shortClose.Removed == std::vector<std::string>{ "two", "three" },
			   "a shorter fence inside the block does not close it");
	}
} // namespace

auto main() -> int
{
	source_lines();
	range_block();
	fence_body();
	change();
	std::cout << "suggestion draft: ok\n";
	return EXIT_SUCCESS;
}
