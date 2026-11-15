// [[BPR-2171]]. A closed note's preview: the first paragraph as words, one
// word per kind of block that is not words, an ellipsis only when words were
// left out. The bodies are the design's (docs/design/discuss-busy-pr-v1).
#include "../../src/adapters/qt/src/conversation_preview.hpp"
#include "../support/test_expectation.hpp"

#include <QApplication>

#include <cstdlib>
#include <iostream>

namespace {
	using bpr::qt::detail::note_preview;
	using bpr::qt::detail::preview_of;
	using bpr::tests::expect;

	[[nodiscard]] auto ranges_text(const note_preview& aPreview, const std::vector<std::pair<int, int>>& aRanges) -> QStringList
	{
		auto result = QStringList{ };
		for (const auto& [start, length] : aRanges) {
			result.push_back(aPreview.Text.mid(start, length));
		}
		return result;
	}
}

int main(int aArgc, char** aArgv)
try {
	QApplication application{ aArgc, aArgv };
	{
		const auto preview = preview_of(QStringLiteral("Thanks for taking this on. The selection survives a refresh now, but two things block it.\n\n## Blocking\n\n"
													   "1. `set_current_thread()` keeps a reply's id when no card holds it, so Current can name nothing.\n"
													   "2. The refresh drops Open **before** the model is rebuilt.\n\n"
													   "```cpp\nif (const auto root = root_of(aId)) {\n}\n```\n\n> Keep the root.\n\n![refresh](https://example.invalid/a.png)\n"),
										0);
		expect(preview.Text == QStringLiteral("Thanks for taking this on. The selection survives a refresh now, but two things block it."), "a review summary shows its first paragraph as words");
		expect(preview.Omitted, "the heading's words were left out, so it ends with an ellipsis");
		expect(preview.Tokens == QStringList{ QStringLiteral("List"), QStringLiteral("C++"), QStringLiteral("Quote"), QStringLiteral("Image") }, "each block is named once, in order");
	}
	{
		const auto preview = preview_of(QStringLiteral("## Build report\n\n| Target | Result | Time |\n|---|---|---|\n| macos-arm64 | passed | 6m 12s |\n| linux-x64 | passed | 7m 03s |\n\n"
													   "<details><summary>Warnings (3)</summary>\n\n- -Wshadow in conversation_cards_view.cpp:612\n\n</details>\n"),
										0);
		expect(preview.Text == QStringLiteral("Build report") && ranges_text(preview, preview.Strong) == QStringList{ QStringLiteral("Build report") }, "a leading heading shows in semibold");
		expect(preview.Tokens.contains(QStringLiteral("Table")) && preview.Tokens.contains(QStringLiteral("Details")) && !preview.Text.contains(QLatin1Char{ '|' }), "a table and a details block are named, not printed");
	}
	{
		const auto preview = preview_of(QStringLiteral("> Both are covered by `conversation_read_public_path` now.\n\nIt does not cover a refresh that *removes* the thread. Worth a case?"), 0);
		expect(preview.Tokens == QStringList{ QStringLiteral("Quote") } && preview.Text == QStringLiteral("It does not cover a refresh that removes the thread. Worth a case?"), "a reply that opens with a quote names it and reads its own words, emphasis dropped");
		expect(!preview.Omitted, "nothing was left out");
	}
	{
		const auto body    = QStringLiteral("issue (blocking): `set_current_thread()` keeps a reply's id when no card holds it.\n\n1. Open a thread with replies.\n2. Refresh.");
		const auto preview = preview_of(body, 17);
		expect(ranges_text(preview, preview.Strong) == QStringList{ QStringLiteral("issue (blocking):") }, "the Conventional Comments header stays semibold");
		expect(ranges_text(preview, preview.Code) == QStringList{ QStringLiteral("set_current_thread()") }, "inline code keeps the code font");
		expect(preview.Tokens == QStringList{ QStringLiteral("List") } && !preview.Omitted, "the list after it is a token, not words left out");
	}
	{
		const auto preview = preview_of(QStringLiteral("suggestion: Take the span, not the vector.\n\n```suggestion\nauto roots_of(std::span<const node> aNodes) -> std::vector<std::string>\n```\n"), 11);
		expect(preview.Text == QStringLiteral("suggestion: Take the span, not the vector.") && preview.Tokens == QStringList{ QStringLiteral("Suggestion") }, "a suggested change is named Suggestion");
	}
	{
		const auto preview = preview_of(QStringLiteral("question: Who still calls `set_feed()` once Discuss has its own list?"), 9);
		expect(preview.Text == QStringLiteral("question: Who still calls set_feed() once Discuss has its own list?") && preview.Tokens.isEmpty() && !preview.Omitted, "a plain one-line comment reads as before");
		expect(ranges_text(preview, preview.Code) == QStringList{ QStringLiteral("set_feed()") }, "its inline code keeps the code font");
	}
	{
		const auto preview = preview_of(QStringLiteral("See [the design](https://example.invalid/design) and the **new** column.\n\n- one\n- two"), 0);
		expect(preview.Text == QStringLiteral("See the design and the new column.") && preview.Tokens == QStringList{ QStringLiteral("List") }, "link and emphasis marks go, their words stay");
	}
	{
		const auto preview = preview_of(QStringLiteral("- first item\n- second item\n\nAfter the list."), 0);
		expect(preview.Text == QStringLiteral("first item") && preview.Tokens == QStringList{ QStringLiteral("List") } && preview.Omitted, "a body that opens with a list shows its first item");
	}
	{
		const auto preview = preview_of(QStringLiteral("```\nplain\n```"), 0);
		expect(preview.Text.isEmpty() && preview.Tokens == QStringList{ QStringLiteral("Code") }, "a fence without a language is Code");
	}
	std::cout << "CONVERSATION_PREVIEW_OK\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
