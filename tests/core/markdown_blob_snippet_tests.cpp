#include "bpr/core/markdown_blob_snippet.hpp"
#include "bpr/core/markdown_code.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	constexpr auto commit_text  = std::string_view{ "0123456789abcdef0123456789abcdef01234567" };
	constexpr auto address_text = std::string_view{ "https://github.com/octo-org/widgets/blob/0123456789abcdef0123456789abcdef01234567/src/frame.cpp#L3-L5" };

	auto permalink_of(const std::string& aAddress) -> markdown_blob_permalink
	{
		const auto found = recognize_markdown_blob_permalink(aAddress);
		expect(found.has_value(), "the fixture address is a permalink");
		return *found;
	}

	void standalone()
	{
		const auto commit  = std::string{ commit_text };
		const auto address = std::string{ address_text };
		// A bare permalink on its own line, then prose.
		{
			const auto body  = address + "\n\nThis clamps the width.";
			const auto found = markdown_standalone_permalinks(body);
			expect(found.size() == 1U && found[0].Begin == 0U && found[0].End == address.size() && found[0].Address == address, "a leading permalink line stands alone");
			expect(found[0].Permalink.FirstLine == 3U && found[0].Permalink.LastLine == 5U, "its range is read");
		}
		// `<permalink>`, CRLF, trailing spaces, between two paragraph lines.
		{
			const auto body  = "See\r\n<" + address + ">  \r\nfor the clamp.";
			const auto found = markdown_standalone_permalinks(body);
			expect(found.size() == 1U && found[0].Address == address && body.substr(found[0].Begin, found[0].End - found[0].Begin) == "<" + address + ">  ", "an angled permalink between paragraph lines stands alone");
		}
		// Places a box cannot go.
		const auto refused = std::vector<std::string>{
			std::string{ "See " } + address,
			address + " is where",
			"- " + address,
			"> " + address,
			"  " + address,
			"    " + address,
			"```\n" + address + "\n```",
			"~~~~\n" + address + "\n~~~\n" + address + "\n~~~~",
			"<!--\n" + address + "\n-->",
			"<div>\n" + address + "\n</div>",
			"<pre>\n\n" + address + "\n</pre>",
			address + "\n---",
			address + "\n===",
			"[" + address + "](" + address + ")",
			"https://github.com/octo-org/widgets/blob/main/src/frame.cpp#L3",
			"https://github.com/octo-org/widgets/blob/" + commit + "/src/frame.cpp",
			address + std::string{ markdown_blob_snippet_mark },
		};
		for (const auto& body : refused) {
			expect(markdown_standalone_permalinks(body).empty(), body.c_str());
		}
		// After a closed fence and a closed comment, lines stand again.
		{
			const auto body = "```\ncode\n```\n<!-- note -->\n" + address + "\n<!--\nhidden\n-->\n" + address;
			expect(markdown_standalone_permalinks(body).size() == 2U, "closed fences and comments end where they close");
		}
		// At most the per-body bound, in document order.
		{
			auto body = std::string{ };
			for (std::size_t index = 0U; index < markdown_blob_snippets_per_body + 3U; ++index) {
				body += address;
				body += "\n\n";
			}
			const auto found = markdown_standalone_permalinks(body);
			expect(found.size() == markdown_blob_snippets_per_body && found[1].Begin > found[0].Begin, "boxes per body are bounded");
		}
	}

	void lines()
	{
		const auto commit    = std::string{ commit_text };
		const auto address   = std::string{ address_text };
		const auto permalink = permalink_of(address);
		const auto file      = std::string{ "one\r\ntwo\nthree\n\tfour\n\nsix" };
		const auto read      = markdown_blob_snippet_lines(file, permalink);
		expect(read && *read == std::vector<std::string>{ "three", "\tfour", "" }, "lines three to five are read, CR dropped, tabs and empty lines kept");
		expect(markdown_blob_snippet_lines(file, permalink_of("https://github.com/octo-org/widgets/blob/" + commit + "/src/frame.cpp#L6")) == std::vector<std::string>{ "six" }, "a final line without a newline counts");
		expect(!markdown_blob_snippet_lines("a\nb\nc\nd\n", permalink_of("https://github.com/octo-org/widgets/blob/" + commit + "/src/frame.cpp#L4-L5")), "a range past the file's end shows no box, as GitHub keeps the link");
		expect(!markdown_blob_snippet_lines(std::string{ "a\nb\nc\0d\ne\n", 10U }, permalink), "a binary file shows no box");
		expect(!markdown_blob_snippet_lines("a\nb\n\xff\xfe\nd\ne\n", permalink), "ill-formed UTF-8 lines show no box");
		expect(!markdown_blob_snippet_lines(std::string(markdown_blob_snippet_file_bytes + 1U, 'a'), permalink), "an oversized file shows no box");
		expect(!markdown_blob_snippet_lines("a\nb\n" + std::string(markdown_blob_snippet_bytes, 'x') + "\nd\ne\n", permalink), "oversized lines show no box");
		auto longest     = permalink;
		longest.LastLine = longest.FirstLine + markdown_blob_snippet_maximum_lines - 1U;
		expect(markdown_blob_snippet_range_shown(longest), "the line bound is inclusive");
		++longest.LastLine;
		expect(!markdown_blob_snippet_range_shown(longest) && !markdown_blob_snippet_lines(std::string(1000U, '\n'), longest), "an over-bound range keeps the compact link");
	}

	void header()
	{
		const auto commit    = std::string{ commit_text };
		const auto address   = std::string{ address_text };
		const auto permalink = permalink_of(address);
		expect(markdown_blob_snippet_title(permalink) == "widgets/src/frame.cpp", "the title is repo/path, as GitHub's header");
		expect(markdown_blob_snippet_range(permalink) == "Lines 3 to 5", "a range reads Lines A to B");
		expect(markdown_blob_snippet_range(permalink_of("https://github.com/octo-org/widgets/blob/" + commit + "/src/frame.cpp#L9")) == "Line 9", "one line reads Line N");
		expect(markdown_blob_snippet_commit(permalink) == "0123456", "the commit is seven digits");
		expect(markdown_blob_permalink_commit_address(address) == "https://github.com/octo-org/widgets/commit/" + commit, "the commit page is on the permalink's host");
		expect(markdown_blob_permalink_commit_address("http://ghe.example.invalid:8080/team/tools/blob/" + commit + "/a.md#L1") == "http://ghe.example.invalid:8080/team/tools/commit/" + commit, "an Enterprise host keeps its scheme and port");
		expect(markdown_blob_permalink_commit_address("https://example.invalid/a").empty(), "a non-permalink has no commit page");
		expect(markdown_blob_permalink_label(permalink, true) == "widgets/src/frame.cpp#L3-L5 at 0123456", "the own repository's label drops the owner");
		expect(markdown_blob_permalink_label(permalink, false) == "octo-org/widgets/src/frame.cpp#L3-L5 at 0123456", "another repository's label keeps it");
		auto shouted       = permalink;
		shouted.Owner      = "OCTO-ORG";
		shouted.Repository = "Widgets";
		expect(markdown_blob_permalink_same_lines(permalink, shouted) && markdown_blob_permalink_source_key(permalink) == markdown_blob_permalink_source_key(shouted), "owner and repository compare without case");
		auto other     = permalink;
		other.LastLine = 6U;
		expect(!markdown_blob_permalink_same_lines(permalink, other) && markdown_blob_permalink_source_key(permalink) == markdown_blob_permalink_source_key(other), "another range is the same file, not the same lines");
	}

	void embedding()
	{
		const auto address = std::string{ address_text };
		const auto body    = "See\n" + address + "\nfor the clamp.";
		const auto found   = markdown_standalone_permalinks(body);
		expect(found.size() == 1U, "the fixture has one box");
		const auto shownLines = std::vector<std::string>{ "int w = 0;", "w = std::clamp(w, 0, 9); // ``` kept", "" };
		const auto placements = std::vector<markdown_blob_snippet_placement>{ { .Begin = found[0].Begin, .End = found[0].End, .Ordinal = 4U, .Lines = shownLines } };
		const auto embedded   = markdown_with_blob_snippets(body, placements);
		const auto fence      = std::string{ "````" } + std::string{ markdown_blob_snippet_mark } + "4";
		expect(embedded == "See\n\n" + fence + "\nint w = 0;\nw = std::clamp(w, 0, 9); // ``` kept\n\n````\n\nfor the clamp.", embedded.c_str());
		const auto fences = scan_markdown_fenced_code(embedded);
		expect(fences.size() == 1U && fences[0].Closed && fences[0].Lines == shownLines, "the box's lines are one closed fence");
		expect(markdown_blob_snippet_ordinal(fences[0].Info) == 4U, "the fence names its box");
		expect(!markdown_blob_snippet_ordinal("cpp") && !markdown_blob_snippet_ordinal(std::string{ markdown_blob_snippet_mark } + "x") && !markdown_blob_snippet_ordinal(std::string{ markdown_blob_snippet_mark } + "01"), "other infos name no box");
		expect(markdown_with_blob_snippets(body, { }) == body, "no placements leave the body");
		const auto backwards = std::vector<markdown_blob_snippet_placement>{ placements[0], placements[0] };
		expect(markdown_with_blob_snippets(body, backwards) == embedded, "an overlapping placement is skipped");
	}
} // namespace

int main()
{
	standalone();
	lines();
	header();
	embedding();
	return EXIT_SUCCESS;
}
