// RFC-BPR-0128, review of 2026-11-14. A report's dressed page never loads a
// resource: agent-written text with an image, raw HTML and resource links is
// drawn with no image, the page's document denies a load even when asked
// directly for a file that exists, and a link is followed as its text.
#include "../support/test_expectation.hpp"
#include "bpr/qt/agent_report_view.hpp"

#include "bpr/core/agent_report.hpp"
#include "bpr/core/agent_report_keep.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/report_page.hpp"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextTable>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
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

	/// [[BPR-2238]]. How far each code box stands inside the left and the
	/// right of its finding's box, as the page laid that box out.
	struct inset
	{
		int Left  = 0;
		int Right = 0;
	};

	[[nodiscard]] auto code_insets(bpr::qt::agent_report_view& aView) -> std::vector<inset>
	{
		auto  found = std::vector<inset>{ };
		auto* page  = aView.dressed_view();
		for (auto* const code : aView.code_views()) {
			if (!code->isVisible()) {
				continue;
			}
			const auto  box    = code->geometry();
			const auto  cursor = page->cursorForPosition(box.topLeft() + QPoint{ 2, 2 });
			auto* const table  = cursor.currentTable();
			if (table == nullptr) {
				found.push_back({ .Left = -1000, .Right = -1000 });
				continue;
			}
			const auto frame = page->document()->documentLayout()->frameBoundingRect(table).translated(-page->horizontalScrollBar()->value(), -page->verticalScrollBar()->value());
			found.push_back({ .Left = box.left() - static_cast<int>(frame.left()), .Right = static_cast<int>(frame.right()) - (box.left() + box.width()) });
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
	QApplication application{ aArgc, aArgv };
	// A folder made without a mode inherits the temporary folder's entries:
	// QTemporaryDir's owner-only mode gives a Windows AppContainer a folder
	// it may not write in ("Access is denied").
	struct scratch_folder
	{
		QString Path = QDir::temp().filePath(QStringLiteral("bpr-agent-report-view-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
		scratch_folder() { static_cast<void>(QDir{ }.mkpath(Path)); }
		~scratch_folder() { static_cast<void>(QDir{ Path }.removeRecursively()); }
		[[nodiscard]] auto isValid() const -> bool { return QDir{ Path }.exists(); }
		[[nodiscard]] auto path() const -> const QString& { return Path; }
		[[nodiscard]] auto filePath(const QString& aName) const -> QString { return QDir{ Path }.filePath(aName); }
	} folder;
	expect(folder.isValid(), "a scratch folder");
	const auto picture = folder.filePath(QStringLiteral("secret.png"));
	{
		QFile      file{ picture };
		const auto written = file.open(QIODevice::WriteOnly) && file.write("PICTURE-BYTES") == 13;
		if (!written) {
			std::cerr << "agent-report-view: " << folder.path().toStdString() << " valid=" << folder.isValid() << " error=" << file.errorString().toStdString() << '\n';
		}
		expect(written, "a file a resource could name");
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
		// The design round's renders: which look, and the dark theme's
		// verdict colours when the palette is dark.
		const auto look = qEnvironmentVariable("BPR_REPORT_RENDER_LOOK");
		view.set_look(look == QStringLiteral("band") ? bpr::qt::report_look::band : look == QStringLiteral("plain") ? bpr::qt::report_look::plain
																													: bpr::qt::report_look::cards);
		if (view.palette().color(QPalette::Base).lightness() < 128) {
			view.set_tone_colours(bpr::qt::builtin_tone_colours(true));
		}
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
	// [[BPR-2238]]. In the cards look a finding's code box and suggested
	// change stand inside the finding's box, its padding at both sides, at
	// every pane width.
	for (const auto width : { 360, 520, 700, 900, 1400 }) {
		view.resize(width, 700);
		QApplication::processEvents();
		const auto insets = code_insets(view);
		expect(insets.size() == 2U, "the finding's code and its suggested change are shown at " + std::to_string(width));
		for (const auto& at : insets) {
			std::cout << "BPR-2238 width " << width << ": left " << at.Left << ", right " << at.Right << "\n";
			expect(at.Right >= 14 && at.Right <= 16 && at.Left >= 3 + 14 && at.Left <= 3 + 16, "a code box keeps the box's padding at both sides at " + std::to_string(width) + ": left " + std::to_string(at.Left) + ", right " + std::to_string(at.Right));
		}
	}
	view.resize(900, 700);
	QApplication::processEvents();
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

	// [[BPR-2231]]. With the host's records, a finding's state is the turn's
	// one state: a finding at a card's place is that card (its key and
	// delivery go with the keep), one with no card is kept under the
	// report's own digest; what the card did reads on the report.
	auto records = std::map<std::pair<bpr::core::agent_findings_digest, std::uint32_t>, bpr::core::agent_finding_card_record>{ };
	view.set_keep_records([&records](const bpr::core::report_finding_key& aKey) {
		const auto found = records.find({ aKey.Findings, aKey.Ordinal });
		return found == records.end() ? bpr::core::agent_finding_card_record{ } : found->second;
	});
	auto       batches = std::vector<std::vector<bpr::qt::report_draft>>{ };
	auto       finish  = bpr::qt::report_draft_all_done{ };
	const auto block   = bpr::core::digest_agent_findings("[]");
	view.set_draft_all_dispatch([&batches, &finish](std::vector<bpr::qt::report_draft> aDrafts, const bpr::core::draft_all_outcome&, bpr::qt::report_draft_all_done aDone) {
		batches.push_back(std::move(aDrafts));
		finish = std::move(aDone);
	});
	view.set_placeable([](const bpr::qt::report_draft& aDraft) { return aDraft.Path != QStringLiteral("src/gone.cpp"); });
	const auto three  = QStringLiteral("# Review of #30\n\n**Verdict:** Needs changes\n\nThree things.\n\n## Must fix\n\n### issue: A\n\n`src/a.cpp:3`\n\nWhy A.\n\n### issue: B\n\n`src/b.cpp:7`\n\nWhy B.\n\n"
									   "### issue: C\n\n`src/c.cpp:2`\n\nWhy C.\n\n### issue: D\n\n`src/gone.cpp:4`\n\nWhy D.\n");
	auto       card   = bpr::qt::agent_report_card{ .Session = 77U, .Ordinal = 1U, .Finding = { }, .Key = { .Findings = block, .Ordinal = 1U } };
	card.Finding.Path = "src/b.cpp";
	view.present({ .Turn      = 9U,
				   .Title     = QStringLiteral("Review of #30"),
				   .Markdown  = three,
				   .Question  = QStringLiteral("Review this pull request"),
				   .Byline    = { },
				   .Suggested = { { .Path = "src/b.cpp", .First = 7U, .Last = 7U, .Comment = "" } },
				   .Cards     = { card },
				   .Answer    = three });
	QApplication::processEvents();
	expect(view.findings().size() == 4U && view.findings()[1].Card == std::optional<std::size_t>{ 0U } && !view.findings()[0].Card, "B is its card; A, C and D are no card's");
	const auto own         = bpr::core::digest_report_findings(three.toStdString());
	records[{ block, 1U }] = { .Findings = block, .Ordinal = 1U, .State = bpr::core::agent_finding_card_disposition::drafted };
	view.settle_keep({ .Findings = block, .Ordinal = 1U, .Card = true });
	expect(view.keep_state(view.findings()[1].Outline).Phase == bpr::qt::report_keep_phase::kept, "drafted on its card, the finding reads Drafted on the report");
	// [[BPR-2232]]. Draft all: the findings not yet drafted, in the report's
	// order (B is drafted; D is not in the diff and is counted so).
	expect(view.draft_all_offered() == 2U && view.dressed_view()->toPlainText().contains(QStringLiteral("Draft all (2)")), "Draft all offers the two left to draft: " + std::to_string(view.draft_all_offered()));
	expect(view.press_draft_all() && batches.size() == 1U && batches[0].size() == 2U && batches[0][0].Path == QStringLiteral("src/a.cpp") && batches[0][1].Path == QStringLiteral("src/c.cpp")
			   && !batches[0][0].Card && batches[0][0].Key == bpr::core::report_finding_key{ .Findings = own, .Ordinal = view.findings()[0].Outline, .Card = false },
		   "Draft all drafts A then C, each under the report's own key");
	expect(view.keep_state(view.findings()[0].Outline).Phase == bpr::qt::report_keep_phase::working && view.draft_all_said().contains(QStringLiteral("Drafting 2 findings")) && !view.press_draft_all(),
		   "under way: each says so, and Draft all is not pressed twice");
	records[{ own, view.findings()[0].Outline }] = { .Findings = own, .Ordinal = view.findings()[0].Outline, .State = bpr::core::agent_finding_card_disposition::drafted };
	view.settle_keep({ .Findings = own, .Ordinal = view.findings()[0].Outline, .Card = false });
	expect(view.keep_state(view.findings()[0].Outline).Phase == bpr::qt::report_keep_phase::kept && view.keep_state(view.findings()[2].Outline).Phase == bpr::qt::report_keep_phase::working,
		   "each finding settles as its draft is made");
	records[{ own, view.findings()[2].Outline }] = { .Findings = own, .Ordinal = view.findings()[2].Outline, .State = bpr::core::agent_finding_card_disposition::refused, .Why = QStringLiteral("Not drafted: resolve the draft open at that line first.").toStdString() };
	finish({ .Drafted = 1U, .AlreadyDrafted = 1U, .NotInDiff = 1U, .Refused = 1U });
	QApplication::processEvents();
	const auto said = view.dressed_view()->toPlainText();
	expect(view.draft_all_said() == QStringLiteral("Drafted 1 private draft. Skipped 3: 1 already drafted, 1 no longer in the diff, 1 could not be drafted; its finding says why.") && said.contains(view.draft_all_said())
			   && view.keep_state(view.findings()[2].Outline).Phase == bpr::qt::report_keep_phase::refused && said.contains(QStringLiteral("resolve the draft open")),
		   "it says how many were drafted and which were not, and the refused one says why: " + view.draft_all_said().toStdString());
	expect(view.draft_all_offered() == 1U && said.contains(QStringLiteral("Draft all (1)")), "the refused one is still offered");
	std::cout << "AGENT_REPORT_VIEW_OK\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << aError.what() << '\n';
	return EXIT_FAILURE;
}
