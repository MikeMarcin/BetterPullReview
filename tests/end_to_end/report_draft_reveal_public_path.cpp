/// \file
/// \brief The owner: "Draft comment on a report barely puts the draft on
/// screen in the comments drawer". Through the window, with the fixture
/// agent (no real agent, no network, no cost): Review this pull request is
/// answered with a report; its first finding's Draft comment makes a draft
/// behind the report and writes it in the Comments drawer beside the report.
/// The whole composer, its editor and its controls, is in the drawer's view;
/// after Go to draft the draft's note is in view at its line. Nothing is
/// published.
///
///     report_draft_reveal_public_path
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
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

		// Draft comment, behind the report; then Go to draft.
		expect(report->press(issue.Outline, qt::report_link::keep), "Draft comment is pressed");
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
		// Go to draft: the file, with the draft's note in view at its line.
		expect(report->press(issue.Outline, qt::report_link::go), "Go to draft is pressed");
		wait([&] { return window.active_document_path() == path && !window.report_shown(); }, "Go to draft shows the file");
		settle(1'000);
		render(window, QStringLiteral("report-draft-go-to-draft"));
		const auto note = std::ranges::find_if(cards->cards(), [](const qt::thread_card& aCard) { return aCard.Draft; });
		expect(note != cards->cards().end(), "the draft is a note in the drawer");
		const auto shownAt = cards->note_rect(note->Id);
		expect(cards->open_card() != nullptr ? cards->open_card()->geometry().bottom() < cards->viewport()->height() : (shownAt.top() >= 0 && shownAt.bottom() < cards->viewport()->height()),
			   "after Go to draft the draft's note is in view too");
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
