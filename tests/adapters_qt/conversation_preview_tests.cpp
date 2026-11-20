// [[BPR-2171]]. A closed note's preview: the first paragraph as words, one
// word per kind of block that is not words, an ellipsis only when words were
// left out. The bodies are the design's (docs/design/discuss-busy-pr-v1).
#include "../../src/adapters/qt/src/conversation_preview.hpp"
#include "../support/test_expectation.hpp"

#include <QApplication>
#include <QFontMetricsF>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
	using bpr::qt::detail::fit_preview_line;
	using bpr::qt::detail::note_preview;
	using bpr::qt::detail::preview_of;
	using bpr::qt::detail::preview_run;
	using bpr::qt::detail::preview_runs;
	using bpr::tests::expect;

	[[nodiscard]] auto ranges_text(const note_preview& aPreview, const std::vector<std::pair<int, int>>& aRanges) -> QStringList
	{
		auto result = QStringList{ };
		for (const auto& [start, length] : aRanges) {
			result.push_back(aPreview.Text.mid(start, length));
		}
		return result;
	}

	[[nodiscard]] auto joined(const std::vector<preview_run>& aRuns) -> QString
	{
		auto text = QString{ };
		for (const auto& run : aRuns) {
			text += run.Text;
		}
		return text;
	}

	/// [[BPR-2300]]. Threads' excerpts: the rows the owner saw squashed.
	void test_threads_excerpts()
	{
		const auto expectPreview = [](const QString& aBody, const QString& aText, const QStringList& aTokens, const char* aWhat) {
			const auto preview = preview_of(aBody, 0);
			if (preview.Text != aText || preview.Tokens != aTokens) {
				std::cerr << "got [" << preview.Text.toStdString() << "] tokens [" << preview.Tokens.join(QLatin1Char{ ',' }).toStdString() << "]\n";
			}
			expect(preview.Text == aText && preview.Tokens == aTokens, aWhat);
		};
		expectPreview(QStringLiteral("# Retry policy\n\nThe backoff doubles\neach time it fails."), QStringLiteral("Retry policy The backoff doubles each time it fails."), { },
					  "a heading loses its marks and its paragraph's lines join");
		expectPreview(QStringLiteral("### Why\n"), QStringLiteral("Why"), { }, "a heading alone is its words");
		expectPreview(QStringLiteral("```suggestion\nconst auto total = sum(aItems);\n```\n"), QString{ }, { QStringLiteral("Suggestion") }, "a suggestion alone is named, its fence not printed");
		expectPreview(QStringLiteral("```python\nprint(1)\n```\nThat prints once."), QStringLiteral("That prints once."), { QStringLiteral("Python") }, "a leading fence is its language and the prose follows");
		expectPreview(QStringLiteral("* first point\n* second point"), QStringLiteral("first point"), { QStringLiteral("List") }, "a bullet's marker goes");
		expectPreview(QStringLiteral("1. Open the file.\n2. Save it."), QStringLiteral("Open the file."), { QStringLiteral("List") }, "a number's marker goes");
		expectPreview(QStringLiteral("> Quoted from the review.\n> Second line."), QString{ }, { QStringLiteral("Quote") }, "a quote alone is named, its prefix not printed");
		expectPreview(QStringLiteral("See [the guide](https://example.invalid/guide) and <https://example.invalid/faq>."),
					  QStringLiteral("See the guide and https://example.invalid/faq."), { }, "a link is its text, an autolink its address");
		expectPreview(QStringLiteral("<!-- generated: keep -->\nLooks right to me."), QStringLiteral("Looks right to me."), { }, "an HTML comment is not shown");
		expectPreview(QStringLiteral("Ship it :tada:"), QStringLiteral("Ship it \U0001F389"), { }, "an emoji name on one plain line is its emoji");
		expectPreview(QStringLiteral("![screenshot](https://example.invalid/s.png)"), QString{ }, { QStringLiteral("Image") }, "an image alone is named, its markup not printed");
		expectPreview(QStringLiteral("Wrapped   with\ttabs and\n\n\nblank lines"), QStringLiteral("Wrapped with tabs and"), { }, "white space collapses");
		{
			const auto preview = preview_of(QStringLiteral("Prefer `std::span` over **raw** pointers, as in `Target.resize(n)`."), 0);
			expect(preview.Text == QStringLiteral("Prefer std::span over raw pointers, as in Target.resize(n).") && ranges_text(preview, preview.Code) == QStringList{ QStringLiteral("std::span"), QStringLiteral("Target.resize(n)") },
				   "inline code keeps its code font, emphasis loses its marks");
			const auto runs = preview_runs(preview);
			expect(runs.size() == 5U && runs[1].Code && runs[3].Code && !runs[0].Code && joined(runs) == preview.Text, "the runs split at the code");
		}
	}

	/// [[BPR-2300]]. One line, cut at a whole word with an ellipsis.
	void test_fit_one_line()
	{
		const auto  font     = QApplication::font();
		const auto& code     = font;
		const auto  metrics  = QFontMetricsF{ font };
		const auto  ellipsis = QString{ QChar{ 0x2026 } };
		const auto  runs     = std::vector<preview_run>{ { .Text = QStringLiteral("alpha beta gamma delta") } };
		const auto  wide     = static_cast<int>(std::ceil(metrics.horizontalAdvance(QStringLiteral("alpha beta gamma delta")))) + 2;
		expect(joined(fit_preview_line(runs, false, font, code, wide)) == QStringLiteral("alpha beta gamma delta"), "words that fit are kept whole");
		expect(joined(fit_preview_line(runs, true, font, code, wide + 40)) == QStringLiteral("alpha beta gamma delta") + ellipsis, "words left out end with an ellipsis");
		const auto cut = static_cast<int>(std::ceil(metrics.horizontalAdvance(QStringLiteral("alpha beta gam") + ellipsis)));
		expect(joined(fit_preview_line(runs, false, font, code, cut)) == QStringLiteral("alpha beta") + ellipsis, "a cut goes back to a whole word");
		const auto stop = std::vector<preview_run>{ { .Text = QStringLiteral("Done. Then more words here") } };
		expect(joined(fit_preview_line(stop, false, font, code, static_cast<int>(std::ceil(metrics.horizontalAdvance(QStringLiteral("Done. Then m") + ellipsis))))) == QStringLiteral("Done. Then") + ellipsis,
			   "the ellipsis follows the last whole word");
		const auto coded  = std::vector<preview_run>{ { .Text = QStringLiteral("Call ") }, { .Text = QStringLiteral("refresh_all_the_rows()"), .Code = true }, { .Text = QStringLiteral(" now") } };
		const auto inside = fit_preview_line(coded, false, font, code, static_cast<int>(std::ceil(metrics.horizontalAdvance(QStringLiteral("Call refresh_all") + ellipsis))));
		expect(joined(inside) == QStringLiteral("Call") + ellipsis && inside.size() == 2U && !inside.back().Code, "inline code is never cut inside");
		const auto opening = std::vector<preview_run>{ { .Text = QStringLiteral("refresh_all_the_rows_now()"), .Code = true } };
		const auto opened  = fit_preview_line(opening, false, font, code, static_cast<int>(std::ceil(metrics.horizontalAdvance(QStringLiteral("refresh_all") + ellipsis))));
		expect(opened.size() == 2U && opened.front().Code && !opened.front().Text.isEmpty(), "inline code that opens the line is cut rather than lost");
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
	test_threads_excerpts();
	test_fit_one_line();
	std::cout << "CONVERSATION_PREVIEW_OK\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
