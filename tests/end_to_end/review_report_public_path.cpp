/// \file
/// \brief [[BPR-2219]] (owner, 2026-11-16: the review "reads much more like
/// an agent transcript than an actual report", and "it no longer gives the
/// ability to draft a comment from the agent response"). Through the window,
/// with the fixture agent, which answers a review as a real agent does: a
/// few words between its tool calls, then the report. Review this pull
/// request, from the review bar's Ask, is a report card whose summary is the
/// report's; the opened report has its verdict, its sections and no word of
/// what the agent said while it worked; every finding has the keep action
/// the core chose; Draft comment makes a private draft at the finding's
/// lines and becomes Go to draft, and stays so when the report is opened
/// again; a finding that names only a file, and the summary, are kept local.
/// No real agent, no network, no cost; nothing is published.
///
///     review_report_public_path
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/agent_report.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/prompts_page.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QToolButton>

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

	/// With `BPR_REVIEW_REPORT_RENDER_ROOT` set, a widget is saved there as
	/// a PNG (the owner's looks).
	void render(QWidget& aWidget, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_REVIEW_REPORT_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		QApplication::processEvents();
		static_cast<void>(aWidget.grab().save(root + QLatin1Char('/') + aName + QStringLiteral(".png")));
	}

	/// With `BPR_REVIEW_REPORT_LOOK` set to a folder, the review's turn in
	/// the Agent tab is saved there as the application draws it, at each
	/// drawer width from the narrowest: the reply, the drawer and the window.
	///
	/// \details This is the application's own window, composed as it ships,
	/// in the theme its appearance setting follows, answered by the fixture
	/// agent. The renders go to a directory named for the platform and style
	/// that drew them; only `cocoa-macos`, made with the native look
	/// (`native_look_startup.cpp`), is what the owner's window paints.
	/// `QT_SCALE_FACTOR` is the scale in each file's name; the screen's own
	/// ratio multiplies it.
	void render_report_turn_look(qt::workbench_window& aWindow, const std::size_t aTurn, const QString& aRoot)
	{
		auto* const tab    = aWindow.agent_tab();
		const auto  folder = QStringLiteral("%1/%2-%3").arg(aRoot, QGuiApplication::platformName(), QApplication::style()->name().toLower());
		expect(QDir{ }.mkpath(folder), "the look's directory exists");
		const auto scale  = QString::number(qRound(qEnvironmentVariable("QT_SCALE_FACTOR", QStringLiteral("1")).toDouble() * 100.0));
		const auto scheme = aWindow.palette().color(QPalette::Window).lightness() < 128 ? QStringLiteral("dark") : QStringLiteral("light");
		wait([&] { return tab->turn_report_tally(aTurn) != nullptr && tab->turn_report_tally(aTurn)->isVisible(); }, "the review shows its summary");
		for (const auto width : { 280U, 320U, 360U, 420U, 640U }) {
			// The drawer's width, as the host restores a stored one.
			auto layout   = application::workbench_projection{ };
			layout.Layout = application::workbench_layout_delta{ .FilesWidth = 260U, .ConversationsWidth = width };
			aWindow.publish(layout);
			static_cast<void>(e2e::wait_until_plain([] { return false; }, std::chrono::milliseconds{ 250 }));
			tab->reveal_turn(aTurn);
			QApplication::processEvents();
			const auto stem = QStringLiteral("%1/%2-%3-%4-").arg(folder, scheme, scale).arg(width);
			expect(tab->turn_reply(aTurn)->grab().save(stem + QStringLiteral("reply.png")), "the reply's render is saved");
			expect(aWindow.conversations_drawer()->grab().save(stem + QStringLiteral("drawer.png")), "the drawer's render is saved");
			if (width == 360U) {
				expect(aWindow.grab().save(stem + QStringLiteral("window.png")), "the window's render is saved");
			}
		}
		std::cout << "review-report-look: " << folder.toStdString() << ", " << scheme.toStdString() << ", device pixel ratio " << qApp->devicePixelRatio() << '\n';
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication  qtApplication{ argc, argv };
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		std::cerr << "review-report: no private data root\n";
		return EXIT_FAILURE;
	}
	std::unique_ptr<app::github_application> application;
	try {
		auto options                = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = *dataRoot };
		options.FixtureAgentSession = true;
		auto terminal               = false;
		auto startup                = access::start(app::detail::github_public_path_fixture::nested_folders_v1, std::move(options), [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait([&] { return terminal && startup && startup->is_quiescent(); }, "startup terminal");
		expect(application != nullptr, "startup succeeds");
		application->show();
		auto& window = application->window();
		window.resize(1440, 960);
		window.github_connect_action()->trigger();
		wait([&] { return window.github_open_action()->isEnabled(); }, "connected");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
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
		const auto answer = tab->turn_answer(before).toStdString();
		const auto note   = std::string{ "I'll start with the brief and the diff" };
		// The cause, as a real agent shows it: the turn's answer is every
		// word the agent said, its working notes before the report.
		expect(answer.find(note) != std::string::npos && answer.find(note) < answer.find("# Review of #21") && answer.find("Now I'll write up the review.") != std::string::npos,
			   "the fixture agent's answer holds what it said while it worked, before its report:\n" + answer.substr(0U, 400U));

		// The card: the report's title and the report's summary, not the notes.
		auto* const card = tab->turn_report_card(before);
		expect(card != nullptr && card->isVisibleTo(tab), "the review is a report card");
		auto* const title = card->findChild<QLabel*>(QStringLiteral("agentTurnReportTitle"));
		expect(title != nullptr && title->text() == QStringLiteral("Review of #21"), "the card names the report by its title: " + (title != nullptr ? title->text().toStdString() : std::string{ }));
		const auto summary = tab->turn_answer_view(before)->original_body();
		expect(summary.find("The move is clean") != std::string::npos && summary.find(note) == std::string::npos && summary.find("## Must fix") == std::string::npos && summary.find("Verdict") == std::string::npos,
			   "the card's summary is the report's summary, with no word of the notes: " + summary);
		// [[BPR-2236]]. The verdict is said over it, in its tone, as the page says it.
		const auto* const verdict = tab->turn_verdict(before);
		expect(verdict != nullptr && verdict->isVisibleTo(tab) && verdict->text().contains(QStringLiteral("Needs changes")), "the verdict stands over the summary");
		render(window, QStringLiteral("review-report-card"));
		// A look's renders alone, for the owner: the rest of the course is
		// not run.
		if (const auto look = qEnvironmentVariable("BPR_REVIEW_REPORT_LOOK"); !look.isEmpty()) {
			render_report_turn_look(window, before, look);
			application->begin_shutdown();
			wait([&] { return application->status().ShutdownComplete; }, "shutdown after the look");
			return EXIT_SUCCESS;
		}

		// [[BPR-2258]]. Another file is on screen, so the issue's own file is
		// not and Draft comment has to open it.
		const auto other = QStringLiteral("src/engine/renderer.h");
		expect(window.go_to_file_line(other, 1U, false), "another file is asked for");
		wait([&] {
			const auto* const comparison = window.comparison_surface();
			const auto        context    = comparison != nullptr ? comparison->current_context() : std::nullopt;
			return window.active_document_path() == other && context.has_value() && context->Key.File == core::file_identity_of(other.toStdString()) && comparison->current_result() != nullptr;
		},
			 "another file is on screen");

		// The opened report: a report, not a transcript.
		auto* const row = card->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"));
		expect(row != nullptr, "the card has its Open report row");
		row->click();
		auto* const report = window.report_view();
		wait([&] { return window.report_shown() && report->isVisible(); }, "the report opens");
		QApplication::processEvents();
		expect(report->dressed_shown() && report->title() == QStringLiteral("Review of #21"), "the report keeps to its template and is drawn as a page");
		expect(tab->turn_name(before) == QStringLiteral("Review this pull request") && tab->turn_question(before).startsWith(QStringLiteral("Review this pull request as a careful colleague would"))
				   && report->document().Question == QStringLiteral("Review this pull request"),
			   "the turn was sent under the prompt's name with its body, and the report says the name, not the body: " + report->document().Question.left(80).toStdString());
		const auto& dressed = report->dressed();
		expect(dressed.Verdict == "Needs changes" && dressed.VerdictTone == core::report_tone::open && dressed.Summary.starts_with("The move is clean") && dressed.Summary.find(note) == std::string::npos,
			   "its verdict stands over its summary: " + dressed.Verdict + " / " + dressed.Summary);
		auto names = std::string{ };
		for (const auto& section : dressed.Sections) {
			names += section.Name + "|";
		}
		expect(names == "Must fix|Worth a look|Good|Questions for the author|Not reviewed|", "its sections are the review's, in order: " + names);
		auto page = report->dressed_view()->toPlainText();
		expect(!page.contains(QString::fromStdString(note)) && !page.contains(QStringLiteral("write up the review")) && !page.contains(QStringLiteral("bpr-findings")),
			   "no word the agent said while it worked is in the report, nor its findings block");
		expect(page.contains(QStringLiteral("Verdict")) && page.contains(QStringLiteral("Needs changes")) && page.contains(QStringLiteral("Keep summary")), "the page shows the verdict and offers Keep summary");

		// Every finding has the keep action the core chose.
		const auto& findings = report->findings();
		expect(findings.size() == 5U, "five findings: " + std::to_string(findings.size()));
		const auto& issue = findings[0];
		expect(issue.Keep == core::report_keep::draft_comment && issue.Path == "src/engine/renderer.cpp" && issue.First == 12U && issue.Last == 13U, "the issue drafts at its lines");
		expect(issue.Comment.starts_with("issue (blocking): ") && issue.Comment.find("```suggestion\n") != std::string::npos, "its comment leads with its label and carries the suggestion:\n" + issue.Comment);
		expect(findings[1].Keep == core::report_keep::draft_comment && findings[1].Path == "src/engine/shaders/post/tonemap.frag" && findings[1].First == 5U && findings[2].Keep == core::report_keep::draft_comment,
			   "a finding without a suggestion drafts too");
		expect(findings[3].Keep == core::report_keep::keep_local && findings[3].Path == "README.md" && findings[3].First == 0U, "a finding that names only a file is kept local");
		expect(findings[4].Keep == core::report_keep::draft_comment && findings[4].Comment.starts_with("praise: "), "praise can be drafted");
		expect(page.count(QStringLiteral("Draft comment")) == 4 && page.count(QStringLiteral("Keep local")) == 1, "the page offers each: " + std::to_string(page.count(QStringLiteral("Draft comment"))) + " Draft comment, " + std::to_string(page.count(QStringLiteral("Keep local"))) + " Keep local");
		expect(report->code_views().size() == 2U && std::ranges::all_of(report->code_views(), [](const qt::markdown_body_view* aView) { return aView->width() > 200; }), "opened at once, the report's code boxes are as wide as its column");
		render(*report, QStringLiteral("review-report-offered"));

		// Draft comment: a private draft at the finding's lines.
		// [[BPR-2258]], owner 2026-11-18: "I'd rather stay on the report".
		// The draft is made behind the report: the report stays the active
		// tab and on screen throughout, the file tabs keep their files, and
		// focus stays on the report.
		const auto strip = [&window] {
			auto said = std::string{ };
			for (const auto& each : window.document_tabs()->tabs()) {
				said += (each.Report ? std::string{ "report" } : each.Path.toStdString()) + (each.Active ? "*" : "") + ";";
			}
			return said;
		};
		auto       stayed   = true;
		const auto onReport = [&] {
			stayed = stayed && window.report_shown() && report->isVisible() && window.active_document_path().isEmpty();
			return stayed;
		};
		const auto tabsBefore = strip();
		expect(onReport() && tabsBefore.find("report*") != std::string::npos && tabsBefore.find(other.toStdString() + ";") != std::string::npos, "the report is the active tab beside another file's: " + tabsBefore);
		report->dressed_view()->setFocus(Qt::OtherFocusReason);
		QApplication::processEvents();
		const auto mutation = access::review_mutation_evidence(*application);
		expect(report->press(issue.Outline, qt::report_link::keep), "Draft comment is pressed");
		wait([&] {
			static_cast<void>(onReport());
			const auto phase = report->keep_state(issue.Outline).Phase;
			return phase == qt::report_keep_phase::kept || phase == qt::report_keep_phase::refused;
		},
			 "Draft comment is answered");
		expect(report->keep_state(issue.Outline).Phase == qt::report_keep_phase::kept, "the finding is drafted: " + report->keep_state(issue.Outline).Why.toStdString());
		QApplication::processEvents();
		expect(onReport(), "the report stayed in front while the draft was made");
		expect(strip() == tabsBefore, "the tabs and their files are as they were: " + strip() + " (was " + tabsBefore + ")");
		expect(report->isAncestorOf(QApplication::focusWidget()), "focus stays on the report");
		expect(report->last_announcement() == QStringLiteral("Draft comment made on src/engine/renderer.cpp, lines 12 to 13. Go to draft opens it."),
			   "a screen reader hears the draft was made: " + report->last_announcement().toStdString());
		// The draft is one of my conversations: a draft on the file, at the
		// finding's first line.
		auto* const cards   = window.conversation_panel()->cards();
		const auto  drafted = [&] {
			return std::ranges::any_of(cards->cards(), [&path](const qt::thread_card& aCard) {
				return aCard.Draft && (aCard.Path == path || aCard.Path.isEmpty()) && (aCard.Line == std::optional<std::uint32_t>{ 12U } || aCard.StartLine == std::optional<std::uint32_t>{ 12U } || aCard.FirstLine == std::optional<std::uint32_t>{ 12U });
			});
		};
		if (!e2e::wait_until_plain(drafted, e2e::extended_agent_budget)) {
			auto       listed = std::string{ };
			const auto number = [](const std::optional<std::uint32_t>& aLine) { return aLine ? std::to_string(*aLine) : std::string{ "-" }; };
			for (const auto& each : cards->cards()) {
				listed += each.Path.toStdString() + ":" + number(each.Line) + "/" + number(each.StartLine) + "/" + number(each.FirstLine) + (each.Draft ? " draft" : "") + (each.Local ? " local" : "") + "; ";
			}
			const auto& shown = window.review_editor()->shown_draft();
			throw std::runtime_error{ "a draft stands at the finding's first line: conversations " + listed + "editor draft " + (shown ? "line " + std::to_string(shown->Anchor.Line) + " body " + shown->Body.substr(0U, 120U) : std::string{ "none" }) };
		}
		// Its words are the finding's comment: the host answers Drafted only
		// once the draft at that line holds exactly the text it was given
		// (`promote_private_item`), so the kept state above says so.
		const auto unpublished = [&] {
			const auto after = access::review_mutation_evidence(*application);
			return after.Submissions == mutation.Submissions && after.StoredComments == mutation.StoredComments && after.NativePosts == mutation.NativePosts && after.FilePosts == mutation.FilePosts;
		};
		expect(unpublished(), "nothing is published");
		page = report->dressed_view()->toPlainText();
		expect(page.contains(QStringLiteral("Drafted")) && page.contains(QStringLiteral("Go to draft")) && page.count(QStringLiteral("Draft comment")) == 3, "the finding now reads Drafted and Go to draft");
		expect(!report->press(issue.Outline, qt::report_link::keep), "a drafted finding is not drafted twice");
		render(*report, QStringLiteral("review-report-drafted"));

		// [[BPR-2231]] AC-b, from the report: the issue is the answer's first
		// finding card, which now reads Drafted too and offers no Draft.
		const auto cardState = [&](const std::size_t aCard) {
			const auto  shown = tab->turn_cards(before);
			auto* const said  = aCard < shown.size() ? shown[aCard]->findChild<QLabel*>(QStringLiteral("agentFindingState")) : nullptr;
			return said != nullptr ? said->text() : QString{ };
		};
		expect(issue.Card == std::optional<std::size_t>{ 0U } && findings[1].Card == std::optional<std::size_t>{ 1U } && tab->turn_cards(before).size() == 2U, "the issue and the tonemap question are the answer's two finding cards");
		// [[BPR-2331]]. The report holds both cards: the Agent tab shows its
		// summary over the attachment and neither card. The cards still
		// carry each finding's one state.
		expect(std::ranges::all_of(tab->turn_cards(before), [](const QWidget* aCard) { return aCard->isHidden(); }) && tab->turn_report_tally(before) != nullptr && !tab->turn_report_tally(before)->isHidden() && !tab->turn_report_tally(before)->counts().empty(),
			   "the Agent tab shows the report's summary, not its finding cards");
		{
			// Owner, 2026-11-25: "I'm thinking that I want to move the report
			// findings donut & counts above the attachment."
			const auto* const counts = tab->turn_report_tally(before);
			const auto        end    = counts->mapTo(card, QPoint{ 0, counts->height() }).y();
			const auto        top    = row->mapTo(card, QPoint{ }).y();
			expect(row->isVisibleTo(tab) && end <= top, "the summary stands over the attachment: it ends at " + std::to_string(end) + ", the attachment starts at " + std::to_string(top));
		}
		wait([&] { return cardState(0U).startsWith(QStringLiteral("Drafted")); }, "the issue's card reads Drafted: " + cardState(0U).toStdString());
		// And the other way round: the tonemap card's own Draft (the path
		// the host's Draft all takes), and the report reads Drafted at that
		// finding.
		auto* const tonemapDraft = tab->turn_cards(before)[1]->findChild<QPushButton*>(QStringLiteral("agentFindingDraftComment"));
		expect(tonemapDraft != nullptr && tonemapDraft->isEnabled(), "the tonemap card offers Draft");
		tonemapDraft->click();
		wait([&] { return cardState(1U).startsWith(QStringLiteral("Drafted")); }, "the tonemap card is drafted: " + cardState(1U).toStdString());
		window.open_agent_question();
		tab->reveal_turn(before);
		render(window, QStringLiteral("review-report-cards-drafted"));
		row->click();
		wait([&] { return window.report_shown() && report->isVisible(); }, "the report opens again");
		expect(report->keep_state(findings[1].Outline).Phase == qt::report_keep_phase::kept && !report->press(findings[1].Outline, qt::report_link::keep), "drafted on its card, the finding reads Drafted on the report and is not drafted twice");

		// [[BPR-2232]]. Draft all from the report: the two findings not yet
		// drafted, in the report's order, by the Agent tab's Draft all path;
		// it says how many and which were passed over; the report stays.
		page = report->dressed_view()->toPlainText();
		expect(report->draft_all_offered() == 2U && page.contains(QStringLiteral("Draft all (2)")), "the report offers Draft all for the two left: " + std::to_string(report->draft_all_offered()));
		render(*report, QStringLiteral("review-report-draft-all-offered"));
		const auto tabsBeforeAll = strip();
		stayed                   = true;
		expect(report->press_draft_all(), "Draft all is pressed");
		wait([&] {
			static_cast<void>(onReport());
			return !report->draft_all_said().startsWith(QStringLiteral("Drafting")) && !report->draft_all_said().isEmpty();
		},
			 "Draft all ends");
		QApplication::processEvents();
		expect(onReport() && strip() == tabsBeforeAll, "Draft all drafts behind the report too, the tabs as they were: " + strip() + " (was " + tabsBeforeAll + ")");
		expect(report->draft_all_said() == QStringLiteral("Drafted 2 private drafts. Skipped 2: 2 already drafted."), "it says how many were drafted and which were not: " + report->draft_all_said().toStdString());
		expect(report->keep_state(findings[2].Outline).Phase == qt::report_keep_phase::kept && report->keep_state(findings[4].Outline).Phase == qt::report_keep_phase::kept && report->draft_all_offered() == 0U,
			   "both read Drafted, and nothing is left to draft");
		wait([&] { return window.report_shown() && report->isVisible(); }, "the report is in front when Draft all ends");
		expect(unpublished(), "Draft all publishes nothing");
		page = report->dressed_view()->toPlainText();
		expect(page.contains(report->draft_all_said()) && !page.contains(QStringLiteral("Draft all (")), "the page says it, and offers Draft all no more");
		render(*report, QStringLiteral("review-report-draft-all-done"));

		// Keep local, for the finding that names only a file, and Keep summary.
		for (const auto outline : { findings[3].Outline, std::uint32_t{ 0U } }) {
			expect(report->press(outline, qt::report_link::keep), "Keep is pressed");
			wait([&] {
				const auto phase = report->keep_state(outline).Phase;
				return phase == qt::report_keep_phase::kept || phase == qt::report_keep_phase::refused;
			},
				 "Keep is answered");
			expect(report->keep_state(outline).Phase == qt::report_keep_phase::kept, "kept as a local comment: " + report->keep_state(outline).Why.toStdString());
		}
		expect(unpublished(), "nothing is published by keeping");

		// Go to draft: the file, at the lines; the report opened again still
		// says what was kept.
		expect(report->press(issue.Outline, qt::report_link::go), "Go to draft is pressed");
		wait([&] { return window.active_document_path() == path && !window.report_shown(); }, "Go to draft shows the file");
		// [[BPR-2233]]. The report's row pressed once, straight after, while
		// the file's own open may still be settling: the later gesture wins.
		row->click();
		QApplication::processEvents();
		wait([&] { return window.report_shown() && report->isVisible(); }, "the report opens again");
		{
			auto settle = QElapsedTimer{ };
			settle.start();
			while (settle.elapsed() < 3000) {
				QApplication::processEvents(QEventLoop::AllEvents, 50);
				expect(window.report_shown() && report->isVisible(), "the file's settling open does not take the front back from the report");
			}
		}
		page = report->dressed_view()->toPlainText();
		expect(report->keep_state(issue.Outline).Phase == qt::report_keep_phase::kept && report->keep_state(0U).Phase == qt::report_keep_phase::kept && page.contains(QStringLiteral("Go to draft"))
				   && !page.contains(QStringLiteral("Keep summary")),
			   "opened again, the report still says what was drafted and kept");
		render(window, QStringLiteral("review-report-window"));

		// AC-f. Settings › Prompts: the review prompt's body is its
		// instructions, and under its editor the format that is always added.
		window.open_settings(qt::settings_page::prompts);
		window.prompts()->select("agent-tab.review-pull-request");
		QApplication::processEvents();
		auto* const body   = window.prompts()->findChild<QPlainTextEdit*>(QStringLiteral("promptsText"));
		auto* const format = window.prompts()->findChild<QPlainTextEdit*>(QStringLiteral("promptsResultText"));
		expect(body != nullptr && body->toPlainText().startsWith(QStringLiteral("Review this pull request as a careful colleague would")) && body->toPlainText().contains(QStringLiteral("Sections, in this order")),
			   "Settings › Prompts shows the review prompt's instructions as its body");
		expect(window.prompts()->review_result_box()->isVisibleTo(window.prompts()) && format != nullptr && format->isReadOnly() && format->toPlainText().contains(QStringLiteral("# Review of #N"))
				   && format->toPlainText().contains(QStringLiteral("**Verdict:**")),
			   "and, read-only under it, the report format that is always added");
		// For the look: the editor and the section under it, in view.
		for (auto* parent = window.prompts()->review_result_box()->parentWidget(); parent != nullptr; parent = parent->parentWidget()) {
			if (auto* const scroll = qobject_cast<QScrollArea*>(parent); scroll != nullptr) {
				scroll->ensureWidgetVisible(body, 0, 40);
				scroll->ensureWidgetVisible(window.prompts()->review_result_box(), 0, 40);
				break;
			}
		}
		render(*window.settings(), QStringLiteral("review-report-prompts"));
		// Its format shows only on a click of its disclosure.
		auto* const toggle = window.prompts()->findChild<QToolButton*>(QStringLiteral("promptsResultToggle"));
		expect(toggle != nullptr && !toggle->isChecked() && !format->isVisibleTo(window.prompts()), "the Result's format is closed by default");
		toggle->click();
		QApplication::processEvents();
		expect(format->isVisibleTo(window.prompts()), "and opens on a click");
		render(*window.settings(), QStringLiteral("review-report-prompts-open"));
		window.settings()->hide();

		application->begin_shutdown();
		wait([&] { return application->status().ShutdownComplete; }, "shutdown");
		application.reset();

		// [[BPR-2231]] AC-a. After a restart the report and the cards still
		// say what was drafted and kept.
		{
			auto again                = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = *dataRoot };
			again.FixtureAgentSession = true;
			auto restarted            = false;
			auto second               = access::start(app::detail::github_public_path_fixture::nested_folders_v1, std::move(again), [&](auto aResult) {
				restarted = true;
				if (aResult) {
					application = std::move(*aResult);
				}
			});
			wait([&] { return restarted && second && second->is_quiescent(); }, "restart terminal");
			expect(application != nullptr, "the restart succeeds");
			application->show();
			auto& reopened = application->window();
			reopened.resize(1440, 960);
			reopened.github_connect_action()->trigger();
			wait([&] { return reopened.github_open_action()->isEnabled(); }, "connected again");
			reopened.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
			reopened.home_open_action()->trigger();
			wait([&] { return reopened.go_to_file_line(path, 3U, false) && reopened.active_document_path() == path; }, "the file opens again");
			reopened.open_agent_question();
			auto* const kept = reopened.agent_tab();
			auto        at   = std::size_t{ 0U };
			wait([&] {
				for (at = 0U; at < kept->turn_count(); ++at) {
					if (kept->turn_report_card(at) != nullptr) {
						return true;
					}
				}
				return false;
			},
				 "the review's turn is back");
			wait([&] {
				const auto shown = kept->turn_cards(at);
				return shown.size() == 2U && std::ranges::all_of(shown, [](QWidget* aCard) {
						   auto* const said = aCard->findChild<QLabel*>(QStringLiteral("agentFindingState"));
						   return said != nullptr && said->text().startsWith(QStringLiteral("Drafted"));
					   });
			},
				 "both finding cards read Drafted after the restart");
			auto* const open = kept->turn_report_card(at)->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"));
			open->click();
			auto* const back = reopened.report_view();
			wait([&] { return reopened.report_shown() && back->isVisible() && back->findings().size() == 5U; }, "the report opens after the restart");
			wait([&] { return std::ranges::all_of(back->findings(), [&](const core::report_finding& aFinding) { return back->keep_state(aFinding.Outline).Phase == qt::report_keep_phase::kept; }); },
				 "every finding still reads Drafted or Kept after the restart");
			expect(back->keep_state(0U).Phase == qt::report_keep_phase::kept && back->draft_all_offered() == 0U && !back->dressed_view()->toPlainText().contains(QStringLiteral("Keep summary")),
				   "and the summary still reads Kept");
			{
				auto settle = QElapsedTimer{ };
				settle.start();
				while (settle.elapsed() < 1500) {
					QApplication::processEvents(QEventLoop::AllEvents, 50);
				}
			}
			// A report opened before its column had its width: the code
			// boxes take the column's width when it comes.
			expect(back->code_views().size() == 2U && std::ranges::all_of(back->code_views(), [](const qt::markdown_body_view* aView) { return aView->width() > 200; }), "the report's code boxes are as wide as its column");
			render(*back, QStringLiteral("review-report-after-restart"));
			application->begin_shutdown();
			wait([&] { return application->status().ShutdownComplete; }, "shutdown after the restart");
		}
		std::cout << "review-report:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "review-report: " << aError.what() << '\n';
		if (application) {
			const auto* const tab = application->window().agent_tab();
			std::cerr << "agent-panel: " << application->window().agent_panel()->status_text().toStdString() << "\nturns: " << tab->turn_count() << '\n';
			application->begin_shutdown();
		}
		return EXIT_FAILURE;
	}
}
