/// \file
/// \brief The owner: "Draft comment on a report barely puts the draft on
/// screen in the comments drawer". Through the window, with the fixture
/// agent (no real agent, no network, no cost): Review this pull request is
/// answered with a report; its first finding's Draft comment makes a draft
/// behind the report and writes it in the Comments drawer beside the report.
/// The whole composer, its editor and its controls, is in the drawer's view.
///
/// The owner, on the look that followed: "How come when I clicked goto draft
/// it didnt switch to the comments tab? Then goto draft went to the comment
/// but didn't select it, it was still collapsed so it was hard to even tell
/// it was the draft." Go to draft, pressed with the draft closed and the
/// right tray on the Agent tab, from the draft's own line, from another line
/// of its file and from another file: each time the file's tab comes in
/// front, the tray turns to Comments, and the draft is open in its composer,
/// whole in the drawer, holding the keyboard. Nothing is published.
///
/// The owner, on the look after that: "When I click draft comment it sends me
/// to go to draft with this button text selected and then I cant actually
/// click go to draft properly until I deselect it and then click it again."
/// Draft comment and every Go to draft here are one click of the pointer on
/// the report's own link, through the window system: after Draft comment
/// nothing on the page is selected, and the one click on Go to draft goes.
/// The owner again: "It's worse than that actually, clicking on it does
/// nothing, you had to click away first before you were allowed to click."
/// No click is made anywhere else between the two. Then the keyboard: Return
/// on another finding's Draft comment hands the keyboard to its Go to draft,
/// which wears the focus ring and is not selected, and Return goes.
///
///     report_draft_reveal_public_path
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/window_system_keys.hpp"
#include "../support/window_system_mouse.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	template <typename Predicate>
	void wait(Predicate aPredicate, const std::string& aMessage)
	{
		if (!e2e::wait_until_plain(aPredicate, e2e::extended_agent_budget)) {
			throw std::runtime_error{ aMessage };
		}
	}

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			throw std::runtime_error{ aMessage };
		}
	}

	/// Runs the event loop for `aMilliseconds`, as a person waits for the
	/// window to settle.
	void settle(const int aMilliseconds)
	{
		auto clock = QElapsedTimer{ };
		clock.start();
		while (clock.elapsed() < aMilliseconds) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
		}
	}

	/// With `BPR_REPORT_DRAFT_REVEAL_RENDER_ROOT` set, the window is saved
	/// there as a PNG.
	void render(QWidget& aWidget, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_REPORT_DRAFT_REVEAL_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		QApplication::processEvents();
		static_cast<void>(aWidget.grab().save(root + QLatin1Char('/') + aName + QStringLiteral(".png")));
	}

	/// The middle of the report's first link whose words are `aWords`, on
	/// the page's viewport; none when the page has no such link.
	[[nodiscard]] auto link_point(QTextEdit& aPage, const QString& aWords) -> std::optional<QPoint>
	{
		for (auto block = aPage.document()->begin(); block.isValid(); block = block.next()) {
			for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
				const auto part = fragment.fragment();
				// A chip's words are kept on one line with no-break spaces.
				if (part.charFormat().anchorHref().isEmpty() || part.text().replace(QChar::Nbsp, QLatin1Char{ ' ' }) != aWords) {
					continue;
				}
				auto cursor = QTextCursor{ aPage.document() };
				cursor.setPosition(part.position());
				const auto first = aPage.cursorRect(cursor);
				cursor.setPosition(part.position() + part.length());
				return QPoint{ (first.left() + aPage.cursorRect(cursor).left()) / 2, first.center().y() };
			}
		}
		return std::nullopt;
	}

	/// One click of the pointer on the report's link `aWords`, as a hand
	/// makes it: the link is brought into the pane, then pressed and released
	/// where it is drawn, through the window system.
	void click_link(qt::agent_report_view& aReport, const QString& aWords, const std::string& aFrom)
	{
		auto* const page = aReport.dressed_view();
		auto        at   = link_point(*page, aWords);
		expect(at.has_value(), aFrom + ": the report offers " + aWords.toStdString());
		if (auto* const pane = aReport.findChild<QScrollArea*>(QStringLiteral("agentReportScroll")); pane != nullptr && pane->widget() != nullptr) {
			const auto in = page->viewport()->mapTo(pane->widget(), *at);
			pane->ensureVisible(in.x(), in.y(), 0, 80);
			QApplication::processEvents();
			at = link_point(*page, aWords);
		}
		expect(at.has_value() && page->viewport()->visibleRegion().contains(*at), aFrom + ": " + aWords.toStdString() + " is in the pane");
		tests::window_system_clicks(*page->viewport(), QPointF{ *at }, 1);
	}

	/// Where the open card stands in the drawer, in words.
	[[nodiscard]] auto said(const qt::conversation_cards_view& aCards) -> std::string
	{
		const auto* const open = aCards.open_card();
		if (open == nullptr) {
			return "no card open";
		}
		return "card top " + std::to_string(open->geometry().top()) + ", foot " + std::to_string(open->geometry().bottom() + 1) + " of a " + std::to_string(aCards.viewport()->height()) + " px drawer, the notes' own scroll " + std::to_string(aCards.overflow());
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication  qtApplication{ argc, argv };
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		std::cerr << "report-draft-reveal: no private data root\n";
		return EXIT_FAILURE;
	}
	std::unique_ptr<app::github_application> application;
	try {
		auto options                = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot };
		options.FixtureAgentSession = true;
		auto terminal               = false;
		auto startup                = access::start(app::detail::github_public_path_fixture::inline_drawer_v1, std::move(options), [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait([&] { return terminal && startup && startup->is_quiescent(); }, "startup terminal");
		expect(application != nullptr, "startup succeeds");
		application->show();
		auto& window = application->window();
		if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
			// The native look keeps the window off screen, and the platform
			// never makes such a window active: Qt is told it is, so a click
			// gives the keyboard as it does on screen.
			QT_WARNING_PUSH
			QT_WARNING_DISABLE_DEPRECATED
			QApplication::setActiveWindow(&window);
			QT_WARNING_POP
		}
		// A laptop's window: the drawer holds the whole composer, but the
		// finding's composer, hanging at its line, ran past the drawer's foot.
		window.resize(1440, 720);
		window.github_connect_action()->trigger();
		wait([&] { return window.github_open_action()->isEnabled(); }, "connected");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/inline-drawer#22"));
		window.home_open_action()->trigger();
		const auto path = QStringLiteral("src/engine/renderer.cpp");
		wait([&] { return window.go_to_file_line(path, 3U, false) && window.active_document_path() == path; }, "the file opens");
		wait([&] {
			const auto* const comparison = window.comparison_surface();
			return comparison != nullptr && comparison->current_result() != nullptr;
		},
			 "the comparison is in");
		window.open_agent_question();
		auto* const tab = window.agent_tab();
		wait([&] { return tab->setup_readiness() == qt::agent_setup_readiness::ready; }, "the fixture agent is set up");

		// Review this pull request, from the review bar's Ask.
		auto* const ask    = window.review_ask_button();
		auto*       review = static_cast<QAction*>(nullptr);
		wait([&] {
			ask->refresh_menu();
			review = ask->menu()->findChild<QAction*>(QStringLiteral("quickAsk_agent-tab.review-pull-request_pull-request"));
			return review != nullptr && review->isEnabled();
		},
			 "Review this pull request is offered in the review bar's Ask");
		const auto before = tab->turn_count();
		review->trigger();
		wait([&] { return tab->turn_count() == before + 1U && tab->state() != qt::agent_tab_state::working && tab->turn_outcome(before).has_value() && access::agent_invocation_evidence(*application).Quiescent; },
			 "the review is answered");
		auto* const card = tab->turn_report_card(before);
		expect(card != nullptr, "the review is a report card");
		auto* const row = card->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"));
		expect(row != nullptr, "the card has its Open report row");
		row->click();
		auto* const report = window.report_view();
		wait([&] { return window.report_shown() && report->isVisible(); }, "the report opens");
		const auto& findings = report->findings();
		expect(!findings.empty() && findings[0].Keep == core::report_keep::draft_comment && findings[0].Path == path.toStdString(), "the report's first finding drafts on " + path.toStdString());
		const auto& issue    = findings[0];
		const auto  mutation = access::review_mutation_evidence(*application);

		// Draft comment, behind the report; then Go to draft. Each is one
		// click of the pointer on the report's own link.
		auto* const page = report->dressed_view();
		render(window, QStringLiteral("report-draft-offered"));
		click_link(*report, QStringLiteral("Draft comment"), "Draft comment");
		wait([&] {
			const auto phase = report->keep_state(issue.Outline).Phase;
			return phase == qt::report_keep_phase::kept || phase == qt::report_keep_phase::refused;
		},
			 "Draft comment is answered");
		expect(report->keep_state(issue.Outline).Phase == qt::report_keep_phase::kept, "the finding is drafted: " + report->keep_state(issue.Outline).Why.toStdString());
		// The owner's screen: the report in front, and beside it the draft
		// written in the Comments drawer, hanging at its line in the file
		// behind the report.
		auto* const cards = window.conversation_panel()->cards();
		settle(1'000);
		render(window, QStringLiteral("report-draft-drafted"));
		std::cout << "report-draft-reveal: drafted, " << said(*cards) << '\n';
		expect(window.report_shown() && cards->isVisible() && cards->writing_draft() && cards->open_card() != nullptr && cards->draft_editor() != nullptr, "the draft is written in the Comments drawer beside the report: " + said(*cards));
		const auto* const open   = cards->open_card();
		const auto        height = cards->viewport()->height();
		expect(open->geometry().top() >= 0 && open->geometry().bottom() < height, "the whole draft, its editor and its controls, is in the drawer's view: " + said(*cards));
		expect(cards->draft_editor()->toPlainText().startsWith(QStringLiteral("issue: ")) && cards->draft_editor()->visibleRegion().boundingRect() == cards->draft_editor()->rect(), "its editor holds the finding's words, all of it in view");
		const auto words   = cards->draft_editor()->toPlainText();
		const auto onNotes = [&window] { return window.tray_tabs()->tabText(window.tray_tabs()->currentIndex()).startsWith(QStringLiteral("Comments")); };
		// The owner's two clicks. Go to draft stands where Draft comment was
		// with nothing on the page selected, and one click on it goes: the
		// file comes in front with the draft still open in Comments.
		const auto selected = page->textCursor().selectedText();
		std::cout << "report-draft-reveal: after Draft comment the page's selection is \"" << selected.toStdString() << "\"\n";
		click_link(*report, QStringLiteral("Go to draft"), "straight after Draft comment");
		const auto went = e2e::wait_until_plain([&] { return window.active_document_path() == path && !window.report_shown(); }, std::chrono::seconds{ 5 });
		settle(500);
		render(window, QStringLiteral("report-go-to-draft-one-click"));
		std::cout << "report-draft-reveal: one click on Go to draft " << (went ? "goes to the draft" : "goes nowhere") << '\n';
		expect(selected.isEmpty(), "after Draft comment nothing on the report is selected; \"" + selected.toStdString() + "\" is");
		expect(went, "straight after Draft comment: one click on Go to draft goes to the draft");
		expect(onNotes() && cards->writing_draft() && cards->draft_editor() != nullptr && cards->draft_editor()->toPlainText() == words, "straight after Draft comment: the draft is open in Comments: " + said(*cards));
		// Go to draft. The reviewer left the draft as a closed note and turned
		// the right tray to the Agent tab; the report is still in front.
		const auto onAgent = [&window] { return window.tray_tabs()->tabText(window.tray_tabs()->currentIndex()).startsWith(QStringLiteral("Agent")); };
		const auto isDraft = [](const qt::thread_card& aCard) { return aCard.Draft; };
		const auto leave   = [&](const std::string& aFrom) {
			if (cards->writing_draft()) {
				auto* const close = cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadClose"));
				expect(close != nullptr && close->isEnabled(), aFrom + ": the open draft offers Close");
				close->click();
			}
			wait([&] { return !cards->writing_draft() && std::ranges::any_of(cards->cards(), isDraft); }, aFrom + ": the draft is left a closed note");
			window.open_agent_question();
			wait(onAgent, aFrom + ": the right tray shows the Agent tab");
		};
		const auto goes = [&](const std::string& aFrom, const QString& aRender) {
			wait([&] { return window.report_shown() && report->isVisible(); }, aFrom + ": the report is in front");
			expect(onAgent() && !cards->writing_draft(), aFrom + ": the draft is closed and the tray is on the Agent tab before the press");
			render(window, aRender + QStringLiteral("-before"));
			click_link(*report, QStringLiteral("Go to draft"), aFrom);
			wait([&] { return window.active_document_path() == path && !window.report_shown(); }, aFrom + ": Go to draft shows the file");
			static_cast<void>(e2e::wait_until_plain([&] { return onNotes() && cards->writing_draft() && cards->draft_editor() != nullptr; }, e2e::extended_agent_budget));
			settle(1'000);
			render(window, aRender);
			std::cout << "report-draft-reveal: " << aFrom << ", " << said(*cards) << '\n';
			expect(onNotes() && cards->isVisible(), aFrom + ": Go to draft turns the right tray to Comments; it shows " + window.tray_tabs()->tabText(window.tray_tabs()->currentIndex()).toStdString());
			expect(cards->writing_draft() && cards->open_card() != nullptr && cards->draft_editor() != nullptr, aFrom + ": the draft is open in its composer, not a closed note: " + said(*cards));
			const auto* const opened = cards->open_card();
			expect(opened->geometry().top() >= 0 && opened->geometry().bottom() < cards->viewport()->height(), aFrom + ": the whole draft is in the drawer's view: " + said(*cards));
			expect(cards->draft_editor()->toPlainText() == words && cards->draft_editor()->visibleRegion().boundingRect() == cards->draft_editor()->rect(), aFrom + ": its editor holds the draft's words, all of it in view");
			const auto* const focus = QApplication::focusWidget();
			expect(focus != nullptr && (focus == opened || opened->isAncestorOf(focus)), aFrom + ": the draft holds the keyboard; it is with " + (focus != nullptr ? focus->objectName().toStdString() : std::string{ "nothing" }));
		};
		// From the draft's own line: the line's draft is a closed note.
		leave("its own line");
		row->click();
		goes("its own line", QStringLiteral("report-go-to-draft"));
		// From another line of its file: the draft is another line's saved one.
		leave("another line");
		expect(window.go_to_file_line(path, 3U, false), "another line is gone to");
		wait([&] { const auto& shown = window.review_editor()->shown_draft(); return !window.report_shown() && shown && shown->Anchor.Line == 2U; }, "line 3 is the selected line, and the draft another line's");
		window.open_agent_question();
		row->click();
		goes("another line", QStringLiteral("report-go-to-draft-other-line"));
		// From another file, behind the report.
		leave("another file");
		const auto other = QStringLiteral("src/ui/widgets/button.cpp");
		wait([&] { return window.open_file_in_review(other) && window.active_document_path() == other; }, "another file opens");
		wait([&] { return window.comparison_surface()->current_result() != nullptr && !window.report_shown(); }, "its comparison is in");
		settle(500);
		window.open_agent_question();
		row->click();
		goes("another file", QStringLiteral("report-go-to-draft-other-file"));
		// The keyboard. With the report in front again, Tab reaches another
		// finding's Draft comment; Return drafts it behind the report and
		// the keyboard goes on to its Go to draft, in the window's focus
		// ring and not selected; Return on that goes to the draft.
		leave("the keyboard");
		row->click();
		wait([&] { return window.report_shown() && report->isVisible(); }, "the keyboard: the report is in front");
		const auto kept = [&report] {
			return std::ranges::count_if(report->findings(), [&report](const core::report_finding& aFinding) { return report->keep_state(aFinding.Outline).Phase == qt::report_keep_phase::kept; });
		};
		page->setFocus(Qt::TabFocusReason);
		page->moveCursor(QTextCursor::Start);
		for (auto presses = 0; presses < 60 && report->keyboard_link() != QStringLiteral("Draft comment"); ++presses) {
			tests::window_system_key(*page, Qt::Key_Tab);
		}
		render(window, QStringLiteral("report-keyboard-on-draft-comment"));
		expect(report->keyboard_link() == QStringLiteral("Draft comment") && page->hasFocus() && !page->textCursor().hasSelection(),
			   "the keyboard: Tab reaches a Draft comment and selects nothing; it is on \"" + report->keyboard_link().toStdString() + "\"");
		const auto keptBefore = kept();
		tests::window_system_key(*page, Qt::Key_Return);
		wait([&] { return kept() == keptBefore + 1; }, "the keyboard: Return on Draft comment drafts its finding");
		settle(1'000);
		render(window, QStringLiteral("report-keyboard-go-to-draft"));
		std::cout << "report-draft-reveal: after Return on Draft comment the keyboard is on \"" << report->keyboard_link().toStdString() << "\" and the page's selection is \""
				  << page->textCursor().selectedText().toStdString() << "\"\n";
		expect(window.report_shown() && page->hasFocus(), "the keyboard: the report stays in front and keeps the keyboard");
		expect(report->keyboard_link() == QStringLiteral("Go to draft") && !page->textCursor().hasSelection(), "the keyboard: it goes on to Go to draft, which is not selected");
		tests::window_system_key(*page, Qt::Key_Return);
		wait([&] { return !window.report_shown() && cards->writing_draft(); }, "the keyboard: Return on Go to draft goes to the draft");
		const auto after = access::review_mutation_evidence(*application);
		expect(after.Submissions == mutation.Submissions && after.StoredComments == mutation.StoredComments && after.NativePosts == mutation.NativePosts && after.FilePosts == mutation.FilePosts, "nothing is published");

		application->begin_shutdown();
		wait([&] { return application->status().ShutdownComplete; }, "shutdown");
		std::cout << "report-draft-reveal:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "report-draft-reveal: " << aError.what() << '\n';
		if (application) {
			application->begin_shutdown();
		}
		return EXIT_FAILURE;
	}
}
