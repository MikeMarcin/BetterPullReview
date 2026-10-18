// RFC-BPR-0036 owner amendment 2 ([[BPR-1562]], [[BPR-1563]], [[BPR-1564]]):
// fenced code blocks, their languages, diff line marks and suggested-change
// rows. GitHub's own answers are the fixtures under
// tests/fixtures/markdown/code-blocks-v1 (`*.github.html`, Markdown API in gfm
// mode); this course reads them and compares.

#include "bpr/core/markdown_code.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef BPR_MARKDOWN_CODE_FIXTURES
	#error "BPR_MARKDOWN_CODE_FIXTURES must name the code-blocks fixture directory"
#endif

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto read(const std::string& aName) -> std::string
	{
		auto file = std::ifstream{ std::string{ BPR_MARKDOWN_CODE_FIXTURES } + "/" + aName, std::ios::binary };
		expect(file.good(), "fixture opens: " + aName);
		return { std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{ } };
	}

	auto unescape(std::string aText) -> std::string
	{
		for (const auto& [from, to] : std::vector<std::pair<std::string, std::string>>{ { "&lt;", "<" }, { "&gt;", ">" }, { "&quot;", "\"" }, { "&#39;", "'" }, { "&amp;", "&" } }) {
			for (auto at = aText.find(from); at != std::string::npos; at = aText.find(from, at + to.size())) {
				aText.replace(at, from.size(), to);
			}
		}
		return aText;
	}

	void scanning()
	{
		const auto blocks = scan_markdown_fenced_code("text\n\n```cpp\nint a;\n\tint b;\n```\n\n~~~ py title\nx = 1\n~~~~\n\n    ```\n    not a fence\n    ```\n");
		expect(blocks.size() == 2U, "two fences; indented code holds none");
		expect(blocks[0].Info == "cpp" && blocks[0].Lines == std::vector<std::string>{ "int a;", "\tint b;" } && blocks[0].Closed, "backtick block keeps its tab");
		expect(blocks[1].Info == "py title" && blocks[1].text() == "x = 1" && blocks[1].Closed, "tilde block closes on a longer fence");
		expect(blocks[1].Ordinal == 1U, "ordinals follow document order");

		const auto nested = scan_markdown_fenced_code("> ```js\n> a();\n>\n> b();\n> ```\n\n- item\n\n  ```go\n  x := 1\n    y := 2\n  ```\n1. ```rust\n   fn f() {}\n   ```\n- > ```sh\n  > ls\n  > ```\n");
		expect(nested.size() == 4U, "fences in quotes, items and an item's quote");
		expect(nested[0].Lines == std::vector<std::string>{ "a();", "", "b();" }, "quote markers removed, blank quote line kept");
		expect(nested[1].Lines == std::vector<std::string>{ "x := 1", "  y := 2" }, "item indentation removed, deeper kept");
		expect(nested[2].Info == "rust" && nested[2].Lines == std::vector<std::string>{ "fn f() {}" } && nested[2].Closed, "fence on the item's own line");
		expect(nested[3].Info == "sh" && nested[3].Lines == std::vector<std::string>{ "ls" }, "a quote inside an item");

		const auto ends = scan_markdown_fenced_code("> ```\n> inside\noutside\n```c\nopen\n");
		expect(ends.size() == 2U && !ends[0].Closed && ends[0].Lines == std::vector<std::string>{ "inside" }, "a quote that ends ends its fence");
		expect(!ends[1].Closed && ends[1].Lines == std::vector<std::string>{ "open", "" }, "an unclosed fence runs to the end");

		const auto indented = scan_markdown_fenced_code("  ```\n  a\n b\nc\n   ```\n");
		expect(indented.size() == 1U && indented[0].Lines == std::vector<std::string>{ "a", "b", "c" }, "the fence's indentation is removed from its lines");

		const auto shapes = scan_markdown_fenced_code("``` `x\nfoo\n``\nnot\n``\n\n```\r\ncrlf\r\n```\r\n");
		expect(shapes.size() == 1U && shapes[0].Lines == std::vector<std::string>{ "crlf" }, "backtick info with a backtick, short fences, CRLF");

		const auto shorter = scan_markdown_fenced_code("````\n```\nstill code\n````\n");
		expect(shorter.size() == 1U && shorter[0].Lines == std::vector<std::string>{ "```", "still code" }, "a shorter fence does not close");

		auto many = std::string{ };
		for (auto index = 0; index < 600; ++index) {
			many += "```\nx\n```\n";
		}
		expect(scan_markdown_fenced_code(many).size() == markdown_code_blocks_per_body, "blocks per body bounded");
		expect(scan_markdown_fenced_code(std::string(40U, '>') + " ```\n").empty(), "deep nesting holds no fence");
		expect(scan_markdown_fenced_code("").empty() && scan_markdown_fenced_code("\n\n").empty(), "empty bodies");
	}

	void languages()
	{
		expect(markdown_code_language_of("cpp").Selector == "code.cpp", "cpp");
		expect(markdown_code_language_of("C++ {.x}").Selector == "code.cpp", "case and attributes");
		expect(markdown_code_language_of("cpp,python").Selector == "code.cpp", "cut at a comma");
		expect(markdown_code_language_of(".cpp").Selector == "code.cpp", "an extension");
		expect(markdown_code_language_of("cmake").Selector == "CMakeLists.txt", "CMake by its file name");
		expect(markdown_code_language_of("diff").Kind == markdown_code_kind::diff && markdown_code_language_of("PATCH").Kind == markdown_code_kind::diff, "diff kinds");
		expect(markdown_code_language_of("suggestion").Kind == markdown_code_kind::suggestion, "suggestion");
		expect(markdown_code_language_of("Suggestion").Kind == markdown_code_kind::plain, "suggestion is written in lower case");
		for (const auto* info : { "", "text", "plaintext", "nohighlight", "{cpp}", "c++11", "python3.11", "cobol-ish", "console", "txt", "mermaid" }) {
			expect(markdown_code_language_of(info).Kind == markdown_code_kind::plain, std::string{ "plain: " } + info);
		}

		// GitHub's answer for every info string of the fixture.
		const auto html  = read("languages.github.html");
		const auto entry = std::regex{ R"re(<p dir="auto"><code class="notranslate">([^<]*)</code>(?: \(form\))?</p>\s*<(div class="highlight highlight-([^"]+)"|pre|section|math-renderer))re" };
		auto       seen  = 0;
		for (auto it = std::sregex_iterator{ html.begin(), html.end(), entry }; it != std::sregex_iterator{ }; ++it) {
			const auto info     = unescape((*it)[1].str());
			const auto scope    = (*it)[3].str();
			const auto language = markdown_code_language_of(info);
			++seen;
			if (scope.empty()) {
				expect(language.Kind == markdown_code_kind::plain, "GitHub leaves plain: " + info);
			} else if (scope == "source-diff") {
				expect(language.Kind == markdown_code_kind::diff, "GitHub marks as diff: " + info);
			} else if (scope != "text-shell-session" && scope != "text-adblock") {
				// Every other language GitHub colours is in the table; the two
				// above have no packaged definition and stay plain on purpose.
				expect(language.Kind == markdown_code_kind::language, std::string{ "GitHub colours " }.append(info).append(" (").append(scope).append(")"));
			}
		}
		expect(seen == 147, "the fixture's info strings were read: " + std::to_string(seen));
	}

	auto kinds(const std::string_view aLine) -> std::string
	{
		auto result = std::string{ };
		for (const auto& mark : markdown_diff_marks(aLine)) {
			static constexpr std::string_view names[] = { "mi1", "md", "mc", "mdr", "c1", "ms" };
			result += std::string{ names[static_cast<int>(mark.Kind)] } + ":" + std::string{ aLine.substr(mark.Begin, mark.Length) } + ";";
		}
		return result;
	}

	void diff_lines()
	{
		// Every line of every diff block in GitHub's rendering, against the
		// marks: each top-level `pl-*` span is one mark of that class.
		const auto markdown = read("diff.md") + read("diff-probe.md");
		const auto html     = read("diff.github.html") + read("diff-probe.github.html");
		auto       expected = std::vector<std::string>{ };
		const auto blocks   = std::regex{ R"re(<div class="highlight highlight-source-diff"[^>]*><pre class="notranslate">([\s\S]*?)</pre></div>)re" };
		for (auto it = std::sregex_iterator{ html.begin(), html.end(), blocks }; it != std::sregex_iterator{ }; ++it) {
			auto       text  = (*it)[1].str();
			auto       line  = std::string{ };
			const auto flush = [&] {
				expected.push_back(line);
				line.clear();
			};
			auto rest = std::string_view{ text };
			while (!rest.empty()) {
				if (rest.starts_with("<span class=\"pl-")) {
					const auto cls  = rest.substr(16U, rest.find('"', 16U) - 16U);
					auto       body = rest.substr(rest.find('>') + 1U);
					// Inner spans repeat the class over the sign; the outer one names the run.
					auto depth = 1;
					auto inner = std::string{ };
					auto at    = std::size_t{ 0U };
					while (depth > 0) {
						if (body.substr(at).starts_with("<span")) {
							++depth;
							at = body.find('>', at) + 1U;
						} else if (body.substr(at).starts_with("</span>")) {
							--depth;
							at += 7U;
						} else {
							inner += body[at++];
						}
					}
					line += std::string{ cls } + ":" + unescape(inner) + ";";
					rest = body.substr(at);
				} else if (rest.front() == '\n') {
					flush();
					rest.remove_prefix(1U);
				} else if (rest.front() == '<') {
					rest.remove_prefix(rest.find('>') + 1U);
				} else {
					rest.remove_prefix(1U);
				}
			}
			flush();
		}
		auto actual = std::vector<std::string>{ };
		for (const auto& block : scan_markdown_fenced_code(markdown)) {
			if (markdown_code_language_of(block.Info).Kind != markdown_code_kind::diff) {
				continue;
			}
			for (const auto& line : block.Lines) {
				actual.push_back(kinds(line));
			}
		}
		expect(actual.size() == expected.size() && actual.size() >= 40U, "every diff line compared: " + std::to_string(actual.size()) + " of " + std::to_string(expected.size()));
		for (std::size_t index = 0U; index < actual.size(); ++index) {
			expect(actual[index] == expected[index], "diff line " + std::to_string(index) + ": ours " + actual[index] + " GitHub " + expected[index]);
		}
		expect(markdown_diff_marks(" context").empty() && markdown_diff_marks("").empty(), "context and empty lines are unmarked");
	}

	void suggestions()
	{
		const auto suggested = std::vector<std::string>{ "\t\tauto next  = read_snapshot_unlocked();" };
		const auto context   = markdown_suggestion_context{ .FirstLine = 9U, .Lines = { "\t\tauto next  = read_snapshot();" } };
		const auto rows      = markdown_suggestion_rows(suggested, context);
		expect(rows.size() == 2U && !rows[0].Added && rows[1].Added && rows[0].Number == 9U && rows[1].Number == 9U, "one removed then one added, numbered from the line");
		expect(rows[0].Text.substr(rows[0].ChangedBegin, rows[0].ChangedEnd - rows[0].ChangedBegin) == "read_snapshot", "changed word removed, as GitHub marks it");
		expect(rows[1].Text.substr(rows[1].ChangedBegin, rows[1].ChangedEnd - rows[1].ChangedBegin) == "read_snapshot_unlocked", "changed word added, as GitHub marks it");

		const auto three = markdown_suggestion_rows(std::vector<std::string>{ "a;", "Snapshot   = snapshot{ };", "c;" },
													markdown_suggestion_context{ .FirstLine = 23U, .Lines = { "a;", "Snapshot   = { };", "c;" } });
		expect(three.size() == 6U && three[2].Number == 25U && three[5].Number == 25U, "every line removed and added, no context rows");
		expect(three[0].ChangedBegin == three[0].ChangedEnd && three[3].ChangedBegin == three[3].ChangedEnd, "an unchanged pair marks nothing");
		expect(three[1].ChangedBegin == three[1].ChangedEnd, "nothing of the removed line is marked when only words were added");
		expect(three[4].Text.substr(three[4].ChangedBegin, three[4].ChangedEnd - three[4].ChangedBegin) == "snapshot", "only the added word is marked");

		const auto inserted = markdown_suggestion_rows(std::vector<std::string>{ "a", "b", "c" }, markdown_suggestion_context{ .FirstLine = 17U, .Lines = { "a", "c" } });
		expect(inserted.size() == 5U && inserted[4].Number == 19U, "added rows numbered on from the first line");
		for (const auto& row : inserted) {
			expect(row.ChangedBegin == row.ChangedEnd, "unequal counts mark no words");
		}
		const auto deletion = markdown_suggestion_rows({ }, markdown_suggestion_context{ .FirstLine = 4U, .Lines = { "#include <utility>" } });
		expect(deletion.size() == 1U && !deletion[0].Added && deletion[0].Number == 4U, "an empty suggestion removes the line");
		const auto alone = markdown_suggestion_rows(suggested, std::nullopt);
		expect(alone.size() == 1U && alone[0].Added && !alone[0].Number && alone[0].ChangedBegin == alone[0].ChangedEnd, "without context, added rows alone");
	}
} // namespace

int main()
{
	scanning();
	languages();
	diff_lines();
	suggestions();
	std::cout << "markdown-code-tests:pass\n";
	return EXIT_SUCCESS;
}
