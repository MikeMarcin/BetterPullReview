// [[BPR-1212]]. Suggest change on a pull request line: the draft starts as a
// suggestion block holding the selected lines exactly, and the draft shows
// what the block changes as the reviewer edits it.

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/review_publication_policy.hpp"
#include "bpr/core/suggestion_draft.hpp"

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
	fence_body();
	change();
	std::cout << "suggestion draft: ok\n";
	return EXIT_SUCCESS;
}
