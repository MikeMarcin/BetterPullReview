// RFC-BPR-0128, review of 2026-11-14. A report's dressed page never loads a
// resource: agent-written text with an image, raw HTML and resource links is
// drawn with no image, the page's document denies a load even when asked
// directly for a file that exists, and a link is followed as its text.
#include "../support/test_expectation.hpp"
#include "bpr/qt/agent_report_view.hpp"

#include "bpr/core/agent_report.hpp"
#include "bpr/qt/report_page.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QUrl>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>

namespace {
	using bpr::tests::expect;

	/// Where on the page's viewport the anchor `aHref` is drawn; none when
	/// the page has no such link.
	[[nodiscard]] auto anchor_point(QTextEdit& aPage, const QString& aHref) -> std::optional<QPoint>
	{
		for (auto block = aPage.document()->begin(); block.isValid(); block = block.next()) {
			for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
				if (fragment.fragment().charFormat().anchorHref() == aHref) {
					auto cursor = QTextCursor{ aPage.document() };
					cursor.setPosition(fragment.fragment().position() + 1);
					return aPage.cursorRect(cursor).center();
				}
			}
		}
		return std::nullopt;
	}

	/// The hrefs of every link on the page, in order.
	[[nodiscard]] auto anchors(QTextEdit& aPage) -> QStringList
	{
		auto found = QStringList{ };
		for (auto block = aPage.document()->begin(); block.isValid(); block = block.next()) {
			for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
				if (const auto href = fragment.fragment().charFormat().anchorHref(); !href.isEmpty() && !found.contains(href)) {
					found << href;
				}
			}
		}
		return found;
	}

	void click(QTextEdit& aPage, const QPoint aAt)
	{
		QMouseEvent press{ QEvent::MouseButtonPress, QPointF{ aAt }, aPage.viewport()->mapToGlobal(QPointF{ aAt }), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(aPage.viewport(), &press);
		QMouseEvent release{ QEvent::MouseButtonRelease, QPointF{ aAt }, aPage.viewport()->mapToGlobal(QPointF{ aAt }), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(aPage.viewport(), &release);
		QApplication::processEvents();
	}
}

int main(int aArgc, char** aArgv)
try {
	QApplication  application{ aArgc, aArgv };
	QTemporaryDir folder;
	expect(folder.isValid(), "a scratch folder");
	const auto picture = folder.filePath(QStringLiteral("secret.png"));
	{
		QFile file{ picture };
		expect(file.open(QIODevice::WriteOnly) && file.write("PICTURE-BYTES") == 13, "a file a resource could name");
	}
	const auto address = QUrl::fromLocalFile(picture).toString();
	auto       view    = bpr::qt::agent_report_view{ };
	view.resize(900, 700);
	view.show();
	// [[BPR-2219]]. With BPR_REPORT_RENDER_MARKDOWN and BPR_REPORT_RENDER_OUT
	// set, this only draws that report (a saved answer) as the page shows it
	// and saves the whole page: the renders an owner's look carries.
	if (const auto source = qEnvironmentVariable("BPR_REPORT_RENDER_MARKDOWN"); !source.isEmpty()) {
		QFile file{ source };
		expect(file.open(QIODevice::ReadOnly), "the report to draw opens");
		view.resize(820, 900);
		const auto markdown = QString::fromUtf8(file.readAll());
		view.present({ .Turn     = 1U,
					   .Title    = QString::fromStdString(bpr::core::report_card_of(bpr::core::read_report_answer(markdown.toStdString()).Report).Title),
					   .Markdown = markdown,
					   .Question = QStringLiteral("Review this pull request"),
					   .Byline   = { .Ending = qEnvironmentVariable("BPR_REPORT_RENDER_BYLINE"), .When = { }, .Head = { } } });
		QElapsedTimer settled;
		settled.start();
		while (settled.elapsed() < 1500) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
		}
		auto* const page = view.findChild<QScrollArea*>();
		expect(page != nullptr && page->widget() != nullptr, "the page to save");
		// The page without the room kept under a short report.
		auto       drawn  = page->widget()->grab();
		const auto bottom = view.dressed_shown() ? view.dressed_view()->geometry().bottom() + 40 : drawn.height();
		expect(drawn.copy(0, 0, drawn.width(), std::min(drawn.height(), static_cast<int>(bottom * drawn.devicePixelRatio()))).save(qEnvironmentVariable("BPR_REPORT_RENDER_OUT")), "the render is saved");
		std::cout << "AGENT_REPORT_VIEW_RENDERED\n";
		return EXIT_SUCCESS;
	}
	auto followed = QStringList{ };
	view.set_link_dispatch([&followed](const QString& aLink) { followed << aLink; });
	const auto markdown = QStringLiteral("# Was this addressed?\n\nOne thread. ![diagram](%1) <img src=\"%1\"> [file](%1)\n\n## Partly\n\n### The clamp\n\n`src/engine/renderer.cpp:12`\n\n"
										 "The width is clamped. ![inline](%1)\n\n<img src=\"%1\" width=\"40\">\n\n- `src/a.cpp:3` A row with ![x](%1).\n")
							  .arg(address);
	view.present({ .Turn = 1U, .Title = QStringLiteral("Was this addressed?"), .Markdown = markdown, .Question = QStringLiteral("Was this addressed?"), .Byline = { .Ending = QStringLiteral("Claude answered in 12s"), .When = QStringLiteral("just now"), .Head = QStringLiteral("abc1234") } });
	QApplication::processEvents();
	// [[BPR-2184]] (review N5): the byline is data, joined by the view.
	expect(view.findChild<QLabel*>(QStringLiteral("agentReportByline"))->text().contains(QStringLiteral("Claude answered in 12s · just now · at abc1234")), "the byline is said from its parts");
	expect(view.dressed_shown(), "the report is drawn as a page");
	auto* const page   = view.dressed_view();
	auto        images = 0;
	for (auto block = page->document()->begin(); block.isValid(); block = block.next()) {
		for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
			images += fragment.fragment().charFormat().isImageFormat() ? 1 : 0;
		}
	}
	expect(images == 0, "no image is drawn from the report's text");
	expect(page->toPlainText().contains(QStringLiteral("<img")), "raw HTML in the report is shown as text, not read as markup");
	for (const auto kind : { QTextDocument::ImageResource, QTextDocument::StyleSheetResource, QTextDocument::HtmlResource }) {
		const auto loaded = page->document()->resource(kind, QUrl{ address });
		expect(!loaded.toByteArray().contains("PICTURE-BYTES"), "the page's document loads nothing, even a file that exists");
	}
	// N1, N8: an action is never an href the report's text can write. A
	// report whose own links name the old action scheme and an action-like
	// token, beside a real suggestion, drafts only from the link the view
	// minted, and only on its own item.
	auto drafts = std::vector<bpr::qt::report_draft>{ };
	view.set_draft_dispatch([&drafts](const bpr::qt::report_draft& aDraft, bpr::qt::report_draft_done aDone) {
		drafts.push_back(aDraft);
		aDone(true, { });
	});
	const auto hostile = QStringLiteral("# Review of #21\n\nOne fix. [Add as draft comment](bpr-draft:2) and [this](report-action-00000000000000000000000000000000).\n\n"
										"## Must fix\n\n### issue (blocking): A zero height\n\n`src/engine/renderer.cpp:12-13`\n\nClamp it. [again](bpr-draft:2)\n\n"
										"```cpp\nconst auto width = 1;\nTarget.resize(width);\n```\n\n```suggestion\nconst auto width = 1;\nTarget.resize(width, 1);\n```\n");
	view.present({ .Turn = 2U, .Title = QStringLiteral("Review of #21"), .Markdown = hostile, .Question = QStringLiteral("Review this pull request"), .Byline = { } });
	QApplication::processEvents();
	auto* const reviewed = view.dressed_view();
	followed.clear();
	for (const auto& href : { QStringLiteral("bpr-draft:2"), QStringLiteral("report-action-00000000000000000000000000000000") }) {
		const auto at = anchor_point(*reviewed, href);
		expect(at.has_value(), "the report's own link is on the page");
		click(*reviewed, *at);
	}
	expect(drafts.empty(), "a link the report's text wrote drafts nothing, whatever it is written as");
	expect(followed.contains(QStringLiteral("bpr-draft:2")), "it is followed as an ordinary link, by its text");
	// The page's own keep actions are links this render minted: Keep summary
	// first, then the finding's Draft comment ([[BPR-2219]]).
	auto minted = QString{ };
	for (const auto& href : anchors(*reviewed)) {
		if (href.startsWith(QStringLiteral("report-action-")) && href != QStringLiteral("report-action-00000000000000000000000000000000")) {
			minted = href;
		}
	}
	expect(!minted.isEmpty() && view.findings().size() == 1U && view.summary_offered(), "the finding offers Draft comment and the summary Keep summary");
	expect(reviewed->toPlainText().contains(QStringLiteral("Draft comment")) && reviewed->toPlainText().contains(QStringLiteral("Keep summary")), "both are on the page");
	click(*reviewed, *anchor_point(*reviewed, minted));
	expect(drafts.size() == 1U && drafts.front().Path == QStringLiteral("src/engine/renderer.cpp") && drafts.front().First == 12U, "the minted link drafts its own item");
	// [[BPR-2219]]. What is drafted is the core's: the label and the finding,
	// why, and the suggestion; the finding then reads Drafted and Go to
	// draft, and is not drafted twice.
	expect(drafts.front().Keep == bpr::core::report_keep::draft_comment && drafts.front().Comment.startsWith(QStringLiteral("issue (blocking): A zero height\n\nClamp it."))
			   && drafts.front().Comment.contains(QStringLiteral("```suggestion\nconst auto width = 1;\nTarget.resize(width, 1);\n```")),
		   "the draft starts from the finding's label, reasons and suggestion: " + drafts.front().Comment.toStdString());
	expect(view.keep_state(drafts.front().Outline).Phase == bpr::qt::report_keep_phase::kept && reviewed->toPlainText().contains(QStringLiteral("Drafted")) && reviewed->toPlainText().contains(QStringLiteral("Go to draft")),
		   "a drafted finding reads Drafted and Go to draft");
	expect(!view.press(drafts.front().Outline, bpr::qt::report_link::keep) && drafts.size() == 1U, "and is not drafted twice");
	auto gone = std::vector<bpr::qt::report_draft>{ };
	view.set_go_dispatch([&gone](const bpr::qt::report_draft& aDraft) { gone.push_back(aDraft); });
	expect(view.press(drafts.front().Outline, bpr::qt::report_link::go) && gone.size() == 1U && gone.front().First == 12U, "Go to draft goes to the finding's lines");
	// A refusal is said at the finding, which still offers its action.
	view.set_draft_dispatch([](const bpr::qt::report_draft&, bpr::qt::report_draft_done aDone) { aDone(false, QStringLiteral("Not kept: no store.")); });
	expect(view.press(0U, bpr::qt::report_link::keep) && view.keep_state(0U).Phase == bpr::qt::report_keep_phase::refused && reviewed->toPlainText().contains(QStringLiteral("Not kept: no store."))
			   && reviewed->toPlainText().contains(QStringLiteral("Keep summary")),
		   "a refused keep says why and stays offered");
	// A verdict stands over the summary; what the agent said while it worked
	// is not on the page; a finding with no suggestion drafts too.
	view.set_draft_dispatch([&drafts](const bpr::qt::report_draft& aDraft, bpr::qt::report_draft_done aDone) {
		drafts.push_back(aDraft);
		aDone(true, { });
	});
	view.present({ .Turn      = 3U,
				   .Title     = QStringLiteral("Review of #21"),
				   .Markdown  = QStringLiteral("Let me read the diff first.\n\nNow the callers.\n\n# Review of #21\n\n**Verdict:** Needs changes\n\nOne fix.\n\n## Must fix\n\n### issue (blocking): A zero height\n\n"
											   "`src/engine/renderer.cpp:12-13`\n\nClamp it.\n\n## Worth a look\n\n### nitpick: The README is stale\n\n`README.md`\n\nIt names the old folder.\n"),
				   .Question  = QStringLiteral("Review this pull request"),
				   .Byline    = { },
				   .Suggested = { { .Path = "src/engine/renderer.cpp", .First = 12U, .Last = 13U, .Comment = "issue: clamp the height too." } } });
	QApplication::processEvents();
	const auto pageText = view.dressed_view()->toPlainText();
	expect(view.dressed_shown() && view.dressed().Verdict == "Needs changes" && pageText.contains(QStringLiteral("Verdict")) && pageText.contains(QStringLiteral("Needs changes")), "the verdict stands over the summary");
	expect(!pageText.contains(QStringLiteral("Let me read the diff")) && !pageText.contains(QStringLiteral("Now the callers")), "what the agent said while it worked is not in the report");
	expect(view.findings().size() == 2U && view.findings()[0].Keep == bpr::core::report_keep::draft_comment && view.findings()[1].Keep == bpr::core::report_keep::keep_local
			   && pageText.count(QStringLiteral("Draft comment")) == 1 && pageText.count(QStringLiteral("Keep local")) == 1,
		   "a finding without a suggestion offers Draft comment, one that names only a file Keep local");
	expect(view.keep_state(view.findings()[0].Outline).Phase == bpr::qt::report_keep_phase::offered, "another report's findings start offered");
	drafts.clear();
	expect(view.press(view.findings()[0].Outline, bpr::qt::report_link::keep) && drafts.size() == 1U && drafts.front().Comment == QStringLiteral("issue: clamp the height too."),
		   "the agent's own suggested comment for the finding's place is what is drafted");
	view.present({ .Turn = 2U, .Title = QStringLiteral("Review of #21"), .Markdown = hostile, .Question = QStringLiteral("Review this pull request"), .Byline = { } });
	QApplication::processEvents();
	expect(view.keep_state(view.findings()[0].Outline).Phase == bpr::qt::report_keep_phase::kept && view.dressed_view()->toPlainText().contains(QStringLiteral("Go to draft")),
		   "a report opened again says what was already drafted");
	// Report pages render with no action at all where no view mints one.
	const auto bare = bpr::qt::report_page_html(bpr::core::dress_report(hostile.toStdString()), { });
	expect(!bare.contains(QStringLiteral("report-action-0000000000000000000000000000000a")) && bare.contains(QStringLiteral("bpr-draft:2")) && !bare.contains(QStringLiteral("Draft comment")) && !bare.contains(QStringLiteral("Keep summary")),
		   "a page written with no minted links offers no action; the report's own links stay links");
	// N10: Tab reaches the page's links, and Return on the one reached
	// follows it; a click follows a place by its text.
	reviewed->setFocus();
	reviewed->moveCursor(QTextCursor::Start);
	followed.clear();
	auto reached = QString{ };
	for (auto press = 0; press < 12 && reached.isEmpty(); ++press) {
		QKeyEvent tab{ QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier };
		QApplication::sendEvent(reviewed, &tab);
		auto at = reviewed->textCursor();
		at.setPosition(at.selectionStart() + 1);
		reached = reviewed->textCursor().hasSelection() ? at.charFormat().anchorHref() : QString{ };
	}
	expect(!reached.isEmpty(), "Tab reaches a link on the page");
	QKeyEvent enter{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
	QApplication::sendEvent(reviewed, &enter);
	expect(!followed.isEmpty() || drafts.size() > 1U, "Return on the link Tab reached follows it");
	followed.clear();
	click(*reviewed, *anchor_point(*reviewed, QStringLiteral("src/engine/renderer.cpp#L12-L13")));
	expect(followed == QStringList{ QStringLiteral("src/engine/renderer.cpp#L12-L13") }, "a click on a place follows it by its text");
	std::cout << "AGENT_REPORT_VIEW_OK\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << aError.what() << '\n';
	return EXIT_FAILURE;
}
