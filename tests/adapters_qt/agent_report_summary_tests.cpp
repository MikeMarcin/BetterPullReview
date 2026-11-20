/// \file
/// \brief [[BPR-2331]] (owner, 2026-11-20: "let's just keep in the report when
/// they're part of the report and remove them from the agent's tab. We can
/// put a brief summary in the agents tab below the report."). A report turn
/// in the Agent tab shows its attachment and, under it, the report's tally
/// on its side, and no finding card the report holds.
///
/// \details In a real window's Agent tab: a restored review turn whose report
/// holds two of its three cards and a short turn that is no report; the
/// summary's rows read whole at the narrowest and a wide drawer and with a
/// larger font; a row opens the report at its label's first finding; and a
/// live report turn that ends at the conversation's end leaves its
/// attachment in view. Registered at 1.0, 1.25 and 2.0 scale.

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/document_tabs_projection.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/report_tally_view.hpp"
#include "bpr/qt/stick_to_bottom.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QEventLoop>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabWidget>
#include <QTimer>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (int pass = 0; pass < 6; ++pass) {
			QApplication::processEvents();
		}
	}

	void wait(const int aMilliseconds)
	{
		auto loop = QEventLoop{ };
		QTimer::singleShot(aMilliseconds, &loop, &QEventLoop::quit);
		loop.exec();
		settle();
	}

	void publish_drawer(qt::workbench_window& aWindow, const std::uint32_t aConversationsWidth)
	{
		auto opened   = application::workbench_projection{ };
		auto commands = core::workbench_command_delta{ };
		for (std::size_t index = 0U; index < core::workbench_command_count; ++index) {
			commands.Commands[index] = { .Id = static_cast<core::workbench_command_id>(index), .Enabled = true, .DisabledReason = core::workbench_reason_code::none };
		}
		opened.Commands = commands;
		opened.Drawers  = core::workbench_drawer_delta{ .Files = core::workbench_drawer_state::expanded, .Conversations = core::workbench_drawer_state::expanded };
		opened.Layout   = application::workbench_layout_delta{ .FilesWidth = 260U, .ConversationsWidth = aConversationsWidth };
		aWindow.publish(opened);
		settle();
	}

	void open_review(qt::workbench_window& aWindow, const int aHeight)
	{
		aWindow.resize(1'440, aHeight);
		aWindow.show();
		publish_drawer(aWindow, 360U);
		aWindow.choose_modality(qt::review_modality::review);
		settle();
		aWindow.tray_tabs()->setCurrentIndex(2);
		auto profile        = core::agent_profile{ };
		profile.Id          = core::claude_stream_json_profile_id;
		profile.DisplayName = "Claude Code";
		profile.Executable  = "/usr/bin/true";
		profile.Protocol    = core::agent_protocol::claude_stream_json;
		aWindow.agent_panel()->offer_profiles(std::vector{ profile });
		settle();
		auto* const selector = aWindow.agent_panel()->profile_selector();
		selector->setCurrentIndex(selector->findData(QVariant::fromValue<qulonglong>(profile.Id.value())));
		settle();
		aWindow.agent_tab()->refresh();
		settle();
	}

	/// A review report in the shared convention, invented placeholder text:
	/// two issues, a question, a nitpick, a suggestion and a praise.
	[[nodiscard]] auto review_report() -> QString
	{
		return QStringLiteral("# Review of #21\n\n**Verdict:** Needs changes\n\nPlaceholder summary of the review.\n\n"
							  "## Must fix\n\n### issue: Placeholder zero height\n\n`src/a.cpp:12-13`\n\nPlaceholder reason.\n\n"
							  "### issue: Placeholder second issue\n\n`src/a.cpp:30`\n\nPlaceholder reason.\n\n"
							  "## Questions\n\n### question: Placeholder question\n\n`src/b.cpp:4`\n\nPlaceholder question?\n\n"
							  "## Small things\n\n### nitpick: Placeholder name\n\n`src/b.cpp:9`\n\nPlaceholder rename.\n\n"
							  "### suggestion: Placeholder helper\n\n`src/c.cpp:2`\n\nPlaceholder extraction.\n\n"
							  "## Good\n\n### praise: Placeholder tests\n\n`tests/a.cpp:1`\n\nPlaceholder praise.\n");
	}

	[[nodiscard]] auto card_at(const std::string& aPath, const std::uint32_t aFirst, const std::uint32_t aLast, const std::size_t aOrdinal) -> qt::agent_restored_finding_card
	{
		auto card                     = qt::agent_restored_finding_card{ };
		card.Session                  = (std::uint64_t{ 1U } << 63U) | 5U;
		card.Ordinal                  = aOrdinal;
		card.Finding.Path             = aPath;
		card.Finding.Lines.Begin      = aFirst - 1U;
		card.Finding.Lines.End        = aLast;
		card.Finding.Claim            = "Placeholder claim.";
		card.Finding.SuggestedComment = "issue: placeholder suggested comment.";
		return card;
	}

	/// [[BPR-2336]]. The summary is one bar: each count's segment in its
	/// tone, one under the other with only the meter's gap between them, as
	/// tall as its share; beside each its word and count, whole and apart.
	/// The summary never needs more room than the drawer gives, and each
	/// count is in view.
	void expect_whole(qt::report_tally_view& aTally, QScrollArea& aScroll, const std::string& aWhere)
	{
		// A drawer's new width settles over several layout passes.
		wait(150);
		expect(aTally.isVisible(), aWhere + ": the summary shows");
		if (const auto pans = aScroll.horizontalScrollBar()->maximum(); pans != 0) {
			std::cout << "BPR-2336 " << aWhere << ": the conversation pans " << pans << " px, as it does without the summary\n";
		}
		// The summary never asks for more than the drawer gives, so it is
		// never why the conversation pans sideways.
		const auto needs = aTally.mapTo(aScroll.widget(), QPoint{ }).x() + aTally.minimumSizeHint().width();
		expect(needs <= aScroll.viewport()->width(), aWhere + ": the summary fits the drawer, " + std::to_string(needs) + " of " + std::to_string(aScroll.viewport()->width()));
		const auto rows = aTally.rows();
		expect(!rows.empty(), aWhere + ": the summary has its counts");
		auto previous = std::optional<QRect>{ };
		auto bands    = 0;
		for (std::size_t index = 0U; index < rows.size(); ++index) {
			auto* const row     = rows[index];
			const auto  word    = QString::fromStdString(row->count().Word);
			const auto  said    = aWhere + ", " + word.toStdString();
			const auto  metrics = QFontMetrics{ row->font() };
			expect(row->isVisible() && aTally.rect().contains(row->geometry()), said + ": the band shows inside the summary");
			expect(row->width() >= row->minimumSizeHint().width() && row->height() == aTally.band_height(index),
				   said + ": the band has the room it needs, " + std::to_string(row->width()) + "x" + std::to_string(row->height()) + " for " + std::to_string(row->minimumSizeHint().width()) + "x" + std::to_string(aTally.band_height(index)));
			bands += row->height();
			const auto box = row->rect();
			expect(box.contains(row->segment_rect()) && box.contains(row->word_rect()) && box.contains(row->count_rect()), said + ": segment, word and count are inside the band");
			expect(row->word_rect().width() >= metrics.horizontalAdvance(word) && row->word_rect().height() >= metrics.height(), said + ": the word is whole");
			expect(row->segment_rect().right() < row->word_rect().left() && row->word_rect().right() < row->count_rect().left(), said + ": the label stands beside its segment, its word before its count");
			const auto segment = QRect{ row->mapTo(&aTally, row->segment_rect().topLeft()), row->segment_rect().size() };
			const auto middle  = row->word_rect().center().y();
			expect(middle >= row->segment_rect().top() && middle <= row->segment_rect().bottom(), said + ": the label is beside its own segment");
			expect(segment.width() == qt::report_tally_view::bar_width && segment.height() >= metrics.height(), said + ": the segment is the bar's width and at least a line tall");
			if (previous) {
				expect(segment.left() == previous->left(), said + ": the segment is in the same bar as the one above");
				const auto gap = segment.top() - previous->bottom() - 1;
				expect(gap >= 1 && gap <= 3, said + ": the segment follows the one above with only the meter's gap, " + std::to_string(gap));
			}
			previous = segment;
			// Where the rest of the conversation already makes it pan (its
			// turns need more than the narrowest drawer at a large font), the
			// summary is measured by what it needs, above.
			if (aScroll.horizontalScrollBar()->maximum() == 0) {
				const auto shownRight = row->mapTo(aScroll.viewport(), row->count_rect().topRight()).x();
				expect(shownRight < aScroll.viewport()->width(), said + ": the count is in view, its right at " + std::to_string(shownRight) + " of " + std::to_string(aScroll.viewport()->width()));
			}
		}
		expect(bands == aTally.bar_height(), aWhere + ": the bands make the whole bar");
		// Sized by count: a larger count stands taller, equal counts equally.
		for (std::size_t one = 0U; one < rows.size(); ++one) {
			for (std::size_t other = 0U; other < rows.size(); ++other) {
				const auto ones   = rows[one]->count().Count;
				const auto others = rows[other]->count().Count;
				if (ones > others) {
					expect(rows[one]->height() > rows[other]->height(), aWhere + ": " + rows[one]->count().Word + " stands taller than " + rows[other]->count().Word);
				} else if (ones == others) {
					expect(std::abs(rows[one]->height() - rows[other]->height()) <= 1, aWhere + ": " + rows[one]->count().Word + " stands as tall as " + rows[other]->count().Word);
				}
			}
		}
	}

	void test_a_report_turn_shows_its_summary_not_its_cards()
	{
		qt::workbench_window window;
		open_review(window, 900);
		auto* const tab    = window.agent_tab();
		auto* const scroll = tab->findChild<QScrollArea*>(QStringLiteral("agentConversationScroll"));
		expect(scroll != nullptr, "the conversation scrolls");
		// Draft all is offered where a reply has several suggested comments.
		tab->set_draft_all_dispatch([](const qt::agent_draft_all_request&, const qt::agent_draft_all_done&) { }, [](std::uint64_t, const qt::agent_draft_all_undo_done&) { });
		// Two cards at the report's findings' places; one at a place the
		// report does not name. A short answer, no report, with its card.
		tab->present_thread({ { .Question = QStringLiteral("Review this pull request"),
								.Answer   = review_report(),
								.Outcome  = core::agent_turn_outcome::answered,
								.Cards    = { card_at("src/a.cpp", 12U, 13U, 0U), card_at("src/b.cpp", 4U, 4U, 1U), card_at("src/elsewhere.cpp", 50U, 50U, 2U) } },
							  { .Question = QStringLiteral("Placeholder short question?"),
								.Answer   = QStringLiteral("Placeholder short answer."),
								.Outcome  = core::agent_turn_outcome::answered,
								.Cards    = { card_at("src/short.cpp", 3U, 3U, 0U) } } });
		settle();
		expect(tab->turn_count() == 2U, "the two turns are restored");
		auto* const card  = tab->turn_report_card(0U);
		auto* const tally = tab->turn_report_tally(0U);
		expect(card != nullptr && card->isVisible() && tally != nullptr && tally->isVisible(), "the review shows its attachment and its summary");
		auto words = std::vector<std::string>{ };
		for (const auto& count : tally->counts()) {
			words.push_back(count.Word + " " + std::to_string(count.Count));
		}
		expect(words == std::vector<std::string>{ "issue 2", "question 1", "nitpick 1", "suggestion 1", "praise 1" }, "the summary is the report's tally, in its order");
		// The cards: the report holds the two at its places.
		const auto cards = tab->finding_cards();
		expect(cards.size() == 4U, "four cards were made: " + std::to_string(cards.size()));
		expect(tab->finding_card_held(0U) && tab->finding_card_held(1U) && !tab->finding_card_held(2U) && !tab->finding_card_held(3U), "the report holds the two cards at its findings' places");
		expect(cards[0]->isHidden() && cards[1]->isHidden(), "a card the report holds is not shown in the Agent tab");
		expect(cards[2]->isVisible(), "a card the report does not hold is still shown");
		expect(cards[3]->isVisible() && (tab->turn_report_tally(1U) == nullptr || !tab->turn_report_tally(1U)->isVisible()), "a turn that is not a report keeps its card and shows no summary");
		const auto draftAll = tab->findChildren<QWidget*>(QStringLiteral("agentTurnDraftAllRow"));
		expect(!draftAll.isEmpty() && draftAll.front()->isHidden(), "a report turn whose report holds its cards offers Draft all on the report, not here");

		// With BPR_REPORT_SUMMARY_RENDER set to a folder, the reply with its
		// attachment and summary is saved there at the narrowest and a wide
		// drawer: the renders a look carries.
		const auto renderRoot = qEnvironmentVariable("BPR_REPORT_SUMMARY_RENDER");
		const auto render     = [&](const QString& aName) {
			if (!renderRoot.isEmpty()) {
				settle();
				static_cast<void>(tab->turn_reply(0U)->grab().save(renderRoot + QStringLiteral("/") + aName + QStringLiteral(".png")));
			}
		};
		// Whole at the narrowest drawer and a wide one, and with a larger font.
		publish_drawer(window, 280U);
		render(QStringLiteral("summary-narrow"));
		expect_whole(*tally, *scroll, "narrowest drawer");
		publish_drawer(window, 640U);
		render(QStringLiteral("summary-wide"));
		expect_whole(*tally, *scroll, "wide drawer");
		const auto original = QApplication::font();
		auto       larger   = original;
		larger.setPointSizeF(original.pointSizeF() * 1.6);
		QApplication::setFont(larger);
		settle();
		expect_whole(*tally, *scroll, "wide drawer, larger font");
		publish_drawer(window, 280U);
		expect_whole(*tally, *scroll, "narrowest drawer, larger font");
		QApplication::setFont(original);
		publish_drawer(window, 360U);

		// A row opens the report at its label's first finding. The host's
		// part, a report's tab made active, as the window's tab host does.
		window.set_document_tab_dispatch([&window](const core::document_tab_intent& aIntent) {
			if (const auto* const report = std::get_if<core::open_report_tab_intent>(&aIntent)) {
				auto tabs    = application::document_tabs_projection{ };
				auto entry   = application::document_tab_entry{ };
				entry.Id     = core::document_tab_id{ 1U };
				entry.Active = true;
				entry.Report = report->Report;
				tabs.Tabs    = { entry };
				window.publish(tabs);
			}
		});
		auto opened = std::vector<qt::agent_report_document>{ };
		tab->set_report_dispatch([&window, &opened](const qt::agent_report_document& aReport) {
			opened.push_back(aReport);
			window.open_report("turn:" + std::to_string(aReport.Turn), aReport);
		});
		auto* const question = tally->rows().at(1U);
		expect(question->count().Word == "question" && question->count().First != 0U, "the question row names its first finding's row");
		question->click();
		settle();
		expect(opened.size() == 1U && opened.back().Heading == question->count().First, "pressing a row opens the report at that row");
		auto* const report = window.report_view();
		expect(report != nullptr && report->isVisible(), "the report is open");
		const auto top = report->section_top(static_cast<int>(question->count().First));
		expect(top >= 0 && top <= 24, "the report stands at the question's finding: " + std::to_string(top));
		tally->rows().front()->click();
		settle();
		expect(opened.size() == 2U && opened.back().Heading == tally->rows().front()->count().First && report->section_top(static_cast<int>(opened.back().Heading)) <= 24,
			   "pressing another row moves the open report to its first finding");
		// [[BPR-2336]]. Pressing a count's segment of the bar opens the report
		// at it, as pressing its label does.
		{
			auto* const praise = tally->rows().back();
			const auto  at     = QPointF{ praise->segment_rect().center() };
			const auto  global = QPointF{ praise->mapToGlobal(at.toPoint()) };
			auto        press  = QMouseEvent{ QEvent::MouseButtonPress, at, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			auto        lift   = QMouseEvent{ QEvent::MouseButtonRelease, at, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(praise, &press);
			QApplication::sendEvent(praise, &lift);
			settle();
			expect(opened.size() == 3U && opened.back().Heading == praise->count().First, "pressing a count's segment opens the report at its first finding");
			opened.pop_back();
		}
		// And from the keyboard: a band takes focus, says what it counts, and
		// Space opens the report at it.
		{
			auto* const nitpick = tally->rows().at(2U);
			expect(nitpick->focusPolicy() == Qt::StrongFocus && nitpick->accessibleName() == QStringLiteral("nitpick: 1"), "a band takes focus and names its count");
			nitpick->setFocus(Qt::TabFocusReason);
			auto down = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
			auto up   = QKeyEvent{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
			QApplication::sendEvent(nitpick, &down);
			QApplication::sendEvent(nitpick, &up);
			settle();
			expect(opened.size() == 3U && opened.back().Heading == nitpick->count().First, "Space on a focused band opens the report at its first finding");
			opened.pop_back();
		}
		// The attachment's own Open opens it where it opens.
		qobject_cast<QPushButton*>(card->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen")))->click();
		settle();
		expect(opened.size() == 3U && opened.back().Heading == 0U, "the attachment opens the report at no row");
		window.hide();
	}

	void test_a_report_turn_ends_with_its_attachment_in_view()
	{
		qt::workbench_window window;
		// A short window: the turn's end is taller than the conversation.
		open_review(window, 620);
		auto* const tab    = window.agent_tab();
		auto* const scroll = tab->findChild<QScrollArea*>(QStringLiteral("agentConversationScroll"));
		expect(scroll != nullptr && qt::stick_to_bottom::of(*scroll) != nullptr, "the conversation follows its end");
		auto identity                 = application::agent_invocation_projection_identity{ };
		identity.ApplicationSession   = core::application_session_id{ 1U };
		identity.Session.Id           = core::agent_session_id{ 9U };
		identity.InvocationGeneration = 1U;
		window.agent_panel()->admit(identity);
		window.agent_panel()->start(identity, application::prepared_workspace{ .Path = "/tmp/workspace" });
		settle();
		expect(tab->state() == qt::agent_tab_state::working, "the turn runs");
		window.agent_panel()->stream(identity, application::agent_stream_event{ .Session = identity.Session.Id, .Sequence = 1U, .Text = review_report().toStdString() });
		wait(400);
		// Whatever stands under the reply at its end (the cards once did),
		// taller than the conversation.
		auto* const thread = tab->findChild<QWidget*>(QStringLiteral("agentThread"));
		expect(thread != nullptr && thread->layout() != nullptr, "the thread is there");
		auto* const below = new QLabel{ QStringLiteral("Placeholder content under the reply"), thread };
		below->setFixedHeight(900);
		thread->layout()->addWidget(below);
		settle();
		window.agent_panel()->finish(identity, application::agent_run_result{ .Session = identity.Session.Id, .Outcome = application::agent_run_outcome::success });
		wait(200);
		expect(tab->state() != qt::agent_tab_state::working, "the turn ended");
		const auto  at   = tab->turn_count() - 1U;
		auto* const card = tab->turn_report_card(at);
		expect(card != nullptr && card->isVisible(), "the turn ended as a report");
		const auto top = card->mapTo(scroll->viewport(), QPoint{ }).y();
		expect(top >= 0 && top + 24 <= scroll->viewport()->height(), "the attachment is in view at the turn's end: its top at " + std::to_string(top) + " of " + std::to_string(scroll->viewport()->height()));
		window.hide();
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_a_report_turn_shows_its_summary_not_its_cards();
	test_a_report_turn_ends_with_its_attachment_in_view();
	std::cout << "agent-report-summary:pass\n";
	return EXIT_SUCCESS;
}
