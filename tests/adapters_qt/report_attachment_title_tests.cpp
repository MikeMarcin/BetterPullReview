/// \file
/// \brief [[BPR-2297]]. A report attachment's title is whole: either all of
/// it shows, or it is shortened on whole lines and ends in "…". No line is
/// cut through its glyphs.
///
/// \details A report without a heading takes its answer's first line as its
/// title, and an answer's first line can be a paragraph. The row once capped
/// its title at two lines' height and let the label wrap it to three into
/// that box, which cut the first and last line through. These checks restore
/// such a turn into the Agent tab of a real window and read the title at a
/// narrow and a wide drawer and with a larger application font. Registered at
/// 1.0, 1.25 and 2.0 scale; run it with `BPR_NATIVE_LOOK=1` for macOS's own
/// style and fonts.

#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/line_bounded_label.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QFontMetrics>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>

#include <cstdlib>
#include <iostream>
#include <string>
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

	void open_review(qt::workbench_window& aWindow)
	{
		aWindow.resize(1'440, 900);
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

	/// An answer long enough to be a report, with no heading: its first line
	/// is its title.
	[[nodiscard]] auto answer_opening_with(const QString& aFirstLine) -> QString
	{
		auto answer = aFirstLine + QStringLiteral("\n\n");
		for (int line = 0; line < 48; ++line) {
			answer += QStringLiteral("- Placeholder point %1 of the sample report.\n").arg(line + 1);
		}
		return answer;
	}

	/// Either the whole title shows, or the lines drawn are whole, at most
	/// the maximum, and the last ends in "…".
	void expect_whole_title(QPushButton& aRow, const QString& aWhere)
	{
		auto* const title = aRow.findChild<QLabel*>(QStringLiteral("agentTurnReportTitle"));
		auto* const meta  = aRow.findChild<QLabel*>(QStringLiteral("agentTurnReportMeta"));
		expect(title != nullptr && meta != nullptr, aWhere.toStdString() + ": the row has its title and its kind");
		const auto where   = aWhere.toStdString();
		const auto box     = title->contentsRect();
		const auto metrics = QFontMetrics{ title->font() };
		const auto spacing = metrics.lineSpacing();
		const auto whole   = title->text().simplified();
		expect(title->isVisible() && box.width() > 0, where + ": the title shows");
		expect(title->geometry().bottom() < meta->geometry().top(), where + ": the title ends above its kind");
		expect(aRow.rect().contains(title->geometry()), where + ": the title is inside its row");
		auto* const bounded = dynamic_cast<qt::line_bounded_label*>(title);
		if (bounded == nullptr) {
			// A plain wrapping label draws every line; its box must hold them.
			const auto needed = metrics.boundingRect(QRect{ 0, 0, box.width(), 100'000 }, Qt::TextWordWrap, title->text()).height();
			expect(box.height() >= needed, where + ": every line of the title is whole, " + std::to_string(box.height()) + " px for " + std::to_string(needed) + " px of wrapped lines (" + std::to_string(needed / spacing) + " lines of " + std::to_string(spacing) + " px)");
			return;
		}
		const auto lines = bounded->shown_lines(box.width());
		expect(!lines.isEmpty() && lines.size() <= bounded->maximum_lines(), where + ": between one line and the most, " + std::to_string(lines.size()));
		expect(box.height() >= static_cast<int>(lines.size()) * spacing,
			   where + ": every line drawn is whole, " + std::to_string(box.height()) + " px for " + std::to_string(lines.size()) + " lines of " + std::to_string(spacing) + " px");
		for (const auto& line : lines) {
			expect(metrics.horizontalAdvance(line) <= box.width(), where + ": a line fits the title's width: " + line.toStdString());
		}
		auto joined = lines.join(QLatin1Char(' '));
		if (bounded->elided(box.width())) {
			expect(joined.endsWith(QChar{ 0x2026 }), where + ": a shortened title ends in an ellipsis: " + joined.toStdString());
			joined.chop(1);
			expect(whole.startsWith(joined.trimmed()), where + ": what shows is how the title begins: " + joined.toStdString());
			expect(joined.trimmed().size() < whole.size(), where + ": shortened, it shows less than the whole");
		} else {
			expect(joined == whole, where + ": the whole title shows: " + joined.toStdString());
		}
	}

	void test_a_long_title_is_whole_or_shortened_on_whole_lines()
	{
		qt::workbench_window window;
		open_review(window);
		auto* const tab = window.agent_tab();
		// Invented placeholder text: a title that wraps to three lines and
		// more in a drawer, and a short one.
		const auto longTitle  = QStringLiteral("Placeholder answer opening sentence that runs on well past a single line, so that the attachment card has to wrap its title onto three or more lines in any drawer the reviewer is likely to use, and then keeps going for good measure.");
		const auto shortTitle = QStringLiteral("Placeholder short title");
		tab->present_thread({ { .Question = QStringLiteral("Placeholder question?"), .Answer = answer_opening_with(longTitle), .Outcome = core::agent_turn_outcome::answered },
							  { .Question = QStringLiteral("Another placeholder?"), .Answer = answer_opening_with(shortTitle), .Outcome = core::agent_turn_outcome::answered } });
		settle();
		expect(tab->turn_count() == 2U, "the two turns are restored");
		auto* const longCard  = tab->turn_report_card(0U);
		auto* const shortCard = tab->turn_report_card(1U);
		expect(longCard != nullptr && longCard->isVisible() && shortCard != nullptr && shortCard->isVisible(), "each answer shows as a report attachment");
		auto* const longRow  = longCard->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"));
		auto* const shortRow = shortCard->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"));
		expect(longRow != nullptr && shortRow != nullptr, "each card has its row");
		expect(longRow->findChild<QLabel*>(QStringLiteral("agentTurnReportTitle"))->text() == longTitle, "the long title is the answer's first line, kept whole");

		const auto checkBoth = [&](const QString& aWhere) {
			expect_whole_title(*longRow, QStringLiteral("long title, ") + aWhere);
			expect_whole_title(*shortRow, QStringLiteral("short title, ") + aWhere);
			if (auto* const bounded = dynamic_cast<qt::line_bounded_label*>(shortRow->findChild<QLabel*>(QStringLiteral("agentTurnReportTitle")))) {
				expect(!bounded->elided(bounded->contentsRect().width()), aWhere.toStdString() + ": a short title is not shortened");
			}
		};
		checkBoth(QStringLiteral("narrow drawer"));
		if (auto* const bounded = dynamic_cast<qt::line_bounded_label*>(longRow->findChild<QLabel*>(QStringLiteral("agentTurnReportTitle")))) {
			expect(bounded->elided(bounded->contentsRect().width()) && bounded->shown_lines(bounded->contentsRect().width()).size() == bounded->maximum_lines(),
				   "in a narrow drawer the long title is shortened on its last whole line");
		}
		publish_drawer(window, 640U);
		checkBoth(QStringLiteral("wide drawer"));
		// A larger application font: the title's lines follow its metrics.
		const auto original = QApplication::font();
		auto       larger   = original;
		larger.setPointSizeF(original.pointSizeF() * 1.6);
		QApplication::setFont(larger);
		settle();
		checkBoth(QStringLiteral("wide drawer, larger font"));
		publish_drawer(window, 360U);
		checkBoth(QStringLiteral("narrow drawer, larger font"));
		QApplication::setFont(original);
		settle();
		window.hide();
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_a_long_title_is_whole_or_shortened_on_whole_lines();
	std::cout << "report-attachment-title:pass\n";
	return EXIT_SUCCESS;
}
