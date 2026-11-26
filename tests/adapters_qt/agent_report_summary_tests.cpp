/// \file
/// \brief [[BPR-2331]] (owner, 2026-11-20: "let's just keep in the report when
/// they're part of the report and remove them from the agent's tab. We can
/// put a brief summary in the agents tab below the report."). A report turn
/// in the Agent tab shows the report's tally as a donut and its attachment,
/// and no finding card the report holds. The tally stands over the
/// attachment (owner, 2026-11-25: "I'm thinking that I want to move the
/// report findings donut & counts above the attachment.").
///
/// \details In a real window's Agent tab: a restored review turn whose report
/// holds two of its three cards and a short turn that is no report; the
/// summary's rows read whole at the narrowest and a wide drawer and with a
/// larger font; a row opens the report at its label's first finding; and a
/// live report turn that ends at the conversation's end leaves its summary
/// and attachment in view. [[BPR-2344]]: the summary is a donut, each slice
/// exactly its count's share, with a legend beside it; a slice or a row
/// opens the report. The reply reads answer, summary, attachment, byline at
/// every drawer width, and Tab goes through the counts to the attachment.
/// The legend's rows stand one under the other as lines of text, none over
/// the next, whatever layout rect the style gives a push button, and a
/// row's wash and focus ring stay in the row. Registered at 1.0, 1.25 and
/// 2.0 scale and, on macOS, under the native look
/// (`qt_agent_report_summary_native_look`: Cocoa and the macOS style, where
/// the rows once overlapped). A look of the turn is drawn from the
/// application's own window, by `review_report_public_path.cpp` under
/// `BPR_REVIEW_REPORT_LOOK`, not from this course's window.

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/document_tabs_projection.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/report_tally_view.hpp"
#include "bpr/qt/stick_to_bottom.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QComboBox>
#include <QEnterEvent>
#include <QEventLoop>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QListWidget>
#include <QMouseEvent>
#include <QProxyStyle>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFrame>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <numbers>
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

	/// A window kept off screen is never made active; Qt is told it is, so
	/// a row that is given the keyboard has it.
	void make_active(QWidget& aWindow)
	{
		QT_WARNING_PUSH
		QT_WARNING_DISABLE_DEPRECATED
		QApplication::setActiveWindow(&aWindow);
		QT_WARNING_POP
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

	/// [[BPR-2344]]. Each slice is exactly its count's share of the circle,
	/// in the report's order from twelve o'clock, and the slices make the
	/// whole circle.
	void expect_exact_slices(const qt::report_tally_view& aTally, const std::string& aWhere)
	{
		const auto rows = aTally.rows();
		auto       next = 0.0;
		for (const auto* const row : rows) {
			const auto said  = aWhere + ", " + row->count().Word;
			const auto share = 360.0 * static_cast<double>(row->count().Count) / static_cast<double>(aTally.total());
			expect(std::abs(aTally.slice_start(row->index()) - next) < 1e-9, said + ": the slice starts where the one before ends, " + std::to_string(aTally.slice_start(row->index())) + " for " + std::to_string(next));
			expect(std::abs(aTally.slice_span(row->index()) - share) < 1e-9, said + ": the slice is its count's share, " + std::to_string(aTally.slice_span(row->index())) + " degrees for " + std::to_string(share));
			next += aTally.slice_span(row->index());
		}
		expect(std::abs(next - 360.0) < 1e-9, aWhere + ": the slices make the whole circle, " + std::to_string(next));
	}

	/// [[BPR-2344]]. What the donut draws: at the middle of each slice its
	/// colour, and under every angle the slice the angle falls in.
	void expect_drawn_slices(const qt::report_tally_view& aTally, const std::string& aWhere)
	{
		auto* const pie   = aTally.pie();
		const auto  image = pie->grab().toImage();
		const auto  scale = image.devicePixelRatio();
		for (const auto* const row : aTally.rows()) {
			const auto said   = aWhere + ", " + row->count().Word;
			const auto middle = pie->slice_middle(row->index());
			expect(pie->slice_at(middle) == row->index(), said + ": the slice's middle is in its slice");
			const auto fill = aTally.fill_of(row->count());
			if (fill.alpha() == 255) {
				const auto drawn = image.pixelColor(static_cast<int>(middle.x() * scale), static_cast<int>(middle.y() * scale));
				const auto near  = std::abs(drawn.red() - fill.red()) <= 6 && std::abs(drawn.green() - fill.green()) <= 6 && std::abs(drawn.blue() - fill.blue()) <= 6;
				expect(near, said + ": the slice wears its tone, " + drawn.name().toStdString() + " for " + fill.name().toStdString());
			}
		}
		// Every angle, on the ring, belongs to the slice it falls in.
		const auto ring = (pie->outer_radius() + pie->inner_radius()) / 2.0;
		for (auto degree = 0; degree < 360; ++degree) {
			const auto angle   = degree + 0.5;
			const auto radians = angle * std::numbers::pi / 180.0;
			const auto at      = pie->centre() + QPointF{ ring * std::sin(radians), -ring * std::cos(radians) };
			const auto rows    = aTally.rows();
			const auto owner   = std::ranges::find_if(rows, [&](const qt::report_tally_row* const aRow) { return angle >= aTally.slice_start(aRow->index()) && angle < aTally.slice_start(aRow->index()) + aTally.slice_span(aRow->index()); });
			expect(owner != rows.end() && pie->slice_at(at) == (*owner)->index(), aWhere + ": the angle " + std::to_string(angle) + " is in its slice");
		}
		expect(!pie->slice_at(pie->centre()).has_value(), aWhere + ": the hole is no slice");
	}

	/// The legend's rows stand as lines of text do: each one line of its
	/// font tall and holding that line whole, each exactly under the one
	/// before, no row over any other, and the legend as tall as its rows.
	/// \details Under the macOS style each row was 22 points tall and stood
	/// 14 under the one before: the style gives a push button a layout rect
	/// 8 points shorter than the button, and the legend laid its rows out by
	/// that. The rows are laid out by the rect they paint.
	void expect_rows_tile(const qt::report_tally_view& aTally, const std::string& aWhere)
	{
		const auto        rows   = aTally.rows();
		const auto* const legend = aTally.findChild<QWidget*>(QStringLiteral("agentTurnReportTallyLegend"));
		expect(legend != nullptr && !rows.empty(), aWhere + ": the legend has its rows");
		const auto line = aTally.line_height();
		for (std::size_t index = 0U; index < rows.size(); ++index) {
			const auto* const row  = rows[index];
			const auto        said = aWhere + ", " + row->count().Word;
			const auto        box  = row->geometry();
			const auto        font = QFontMetrics{ row->font() }.height();
			expect(row->parentWidget() == legend && legend->rect().contains(box), said + ": the row is inside the legend");
			expect(box.height() == line && line >= font, said + ": the row is one whole line of its font, " + std::to_string(box.height()) + " for a line of " + std::to_string(font));
			expect(box.top() == static_cast<int>(index) * line, said + ": the row stands exactly under the one before, at " + std::to_string(box.top()) + " for " + std::to_string(static_cast<int>(index) * line));
			for (std::size_t other = 0U; other < index; ++other) {
				expect(!box.intersects(rows[other]->geometry()), said + ": the row is over no part of " + rows[other]->count().Word);
			}
		}
		expect(legend->height() == static_cast<int>(rows.size()) * line, aWhere + ": the legend is as tall as its rows, " + std::to_string(legend->height()) + " for " + std::to_string(rows.size()) + " of " + std::to_string(line));
	}

	/// What the pointer and the keyboard draw on a row (its wash, its focus
	/// ring) is drawn in that row: no pixel of another row's line changes.
	/// Before, a row's wash reached into the words above it and below it.
	void expect_states_keep_to_their_row(qt::report_tally_view& aTally, const std::string& aWhere)
	{
		auto* const legend = aTally.findChild<QWidget*>(QStringLiteral("agentTurnReportTallyLegend"));
		expect(legend != nullptr, aWhere + ": the summary has its legend");
		const auto rows = aTally.rows();
		if (auto* const focused = QApplication::focusWidget(); focused != nullptr && legend->isAncestorOf(focused)) {
			focused->clearFocus();
		}
		aTally.set_hovered(std::nullopt);
		settle();
		const auto rest  = legend->grab().toImage();
		const auto scale = rest.devicePixelRatio();
		// A row's pixels are the device pixels wholly inside it: at a scale
		// of 1.25 the pixel row on the edge between two rows is both rows'.
		const auto changed = [&](const QImage& aDrawn, const QRect& aBox) {
			const auto top   = static_cast<int>(std::ceil(aBox.top() * scale));
			const auto end   = std::min(aDrawn.height(), static_cast<int>(std::floor((aBox.bottom() + 1) * scale)));
			const auto left  = static_cast<int>(std::ceil(aBox.left() * scale));
			const auto wide  = std::min(aDrawn.width(), static_cast<int>(std::floor((aBox.right() + 1) * scale)));
			auto       count = 0;
			for (auto y = top; y < end; ++y) {
				for (auto x = left; x < wide; ++x) {
					count += aDrawn.pixel(x, y) != rest.pixel(x, y) ? 1 : 0;
				}
			}
			return count;
		};
		const auto judge = [&](const qt::report_tally_row& aLit, const std::string& aState) {
			settle();
			const auto drawn = legend->grab().toImage();
			expect(drawn.size() == rest.size(), aWhere + ": the legend keeps its size " + aState);
			for (const auto* const row : rows) {
				const auto differs = changed(drawn, row->geometry());
				if (row == &aLit) {
					// NOLINTNEXTLINE(performance-inefficient-string-concatenation): a check's message.
					expect(differs > 0, aWhere + ", " + row->count().Word + ": the row shows it is " + aState);
				} else {
					// NOLINTNEXTLINE(performance-inefficient-string-concatenation): a check's message.
					expect(differs == 0, aWhere + ", " + row->count().Word + ": " + std::to_string(differs) + " pixels of the row change while " + aLit.count().Word + " is " + aState);
				}
			}
		};
		for (auto* const row : rows) {
			aTally.set_hovered(row->index());
			judge(*row, "under the pointer");
			aTally.set_hovered(std::nullopt);
			row->setFocus(Qt::TabFocusReason);
			settle();
			expect(row->hasFocus(), aWhere + ", " + row->count().Word + ": the row takes the keyboard");
			judge(*row, "holding the keyboard");
			row->clearFocus();
			settle();
		}
	}

	/// [[BPR-2344]]. The summary is a donut and its legend: a row a count in
	/// the report's order, its swatch, word and count whole and apart, the
	/// legend beside the donut. The summary never needs more room than the
	/// drawer gives, and each count is in view.
	void expect_whole(qt::report_tally_view& aTally, QScrollArea& aScroll, const std::string& aWhere)
	{
		// A drawer's new width settles over several layout passes.
		wait(150);
		expect(aTally.isVisible() && aTally.pie()->isVisible(), aWhere + ": the summary shows its donut");
		if (const auto pans = aScroll.horizontalScrollBar()->maximum(); pans != 0) {
			std::cout << "BPR-2344 " << aWhere << ": the conversation pans " << pans << " px, as it does without the summary\n";
		}
		// The summary never asks for more than the drawer gives, so it is
		// never why the conversation pans sideways.
		const auto needs = aTally.mapTo(aScroll.widget(), QPoint{ }).x() + aTally.minimumSizeHint().width();
		std::cout << "BPR-2344 " << aWhere << ": the summary needs " << needs << " of " << aScroll.viewport()->width() << '\n';
		expect(needs <= aScroll.viewport()->width(), aWhere + ": the summary fits the drawer, " + std::to_string(needs) + " of " + std::to_string(aScroll.viewport()->width()));
		const auto rows = aTally.rows();
		expect(!rows.empty(), aWhere + ": the summary has its counts");
		const auto pie = QRect{ aTally.pie()->mapTo(&aTally, QPoint{ }), aTally.pie()->size() };
		expect(aTally.rect().contains(pie), aWhere + ": the donut is inside the summary");
		auto previous = std::optional<QRect>{ };
		for (std::size_t index = 0U; index < rows.size(); ++index) {
			auto* const row     = rows[index];
			const auto  word    = QString::fromStdString(row->count().Word);
			const auto  said    = aWhere + ", " + word.toStdString();
			const auto  metrics = QFontMetrics{ row->font() };
			const auto  placed  = QRect{ row->mapTo(&aTally, QPoint{ }), row->size() };
			expect(row->index() == index, said + ": the row is in the report's order");
			expect(row->isVisible() && aTally.rect().contains(placed), said + ": the row shows inside the summary");
			expect(placed.left() > pie.right(), said + ": the row stands beside the donut");
			expect(row->width() >= row->minimumSizeHint().width() && row->height() == aTally.line_height(),
				   said + ": the row has the room it needs, " + std::to_string(row->width()) + "x" + std::to_string(row->height()) + " for " + std::to_string(row->minimumSizeHint().width()) + "x" + std::to_string(aTally.line_height()));
			const auto box = row->rect();
			expect(box.contains(row->swatch_rect()) && box.contains(row->word_rect()) && box.contains(row->count_rect()), said + ": swatch, word and count are inside the row");
			expect(row->word_rect().width() >= metrics.horizontalAdvance(word) && row->word_rect().height() >= metrics.height(), said + ": the word is whole");
			expect(row->swatch_rect().right() < row->word_rect().left() && row->word_rect().right() < row->count_rect().left(), said + ": swatch, word and count stand apart, in that order");
			if (previous) {
				expect(placed.left() == previous->left() && placed.width() == previous->width() && placed.top() == previous->bottom() + 1, said + ": the row is under the one before, as wide");
				expect(row->count_rect().right() == rows[index - 1U]->count_rect().right(), said + ": the counts make one column");
			}
			previous = placed;
			// Where the rest of the conversation already makes it pan (its
			// turns need more than the narrowest drawer at a large font), the
			// summary is measured by what it needs, above.
			if (aScroll.horizontalScrollBar()->maximum() == 0) {
				const auto shownRight = row->mapTo(aScroll.viewport(), row->count_rect().topRight()).x();
				expect(shownRight < aScroll.viewport()->width(), said + ": the count is in view, its right at " + std::to_string(shownRight) + " of " + std::to_string(aScroll.viewport()->width()));
			}
		}
		expect_rows_tile(aTally, aWhere);
		expect_exact_slices(aTally, aWhere);
		expect_drawn_slices(aTally, aWhere);
	}

	/// [[BPR-2344]]. A summary on its own: a report of one label is one whole
	/// ring; three to one is three quarters and a quarter.
	void test_the_donut_is_exact()
	{
		auto       one   = qt::report_tally_view{ };
		const auto issue = core::dressed_count{ .Word = "issue", .Count = 3U, .Tone = core::report_tone::open, .First = 2U };
		one.set_counts({ issue });
		one.resize(one.sizeHint());
		one.show();
		settle();
		expect(one.rows().size() == 1U && one.slice_start(0U) == 0.0 && one.slice_span(0U) == 360.0, "a single label is the whole circle");
		expect_drawn_slices(one, "a single label");
		if (const auto renderRoot = qEnvironmentVariable("BPR_REPORT_SUMMARY_RENDER"); !renderRoot.isEmpty()) {
			static_cast<void>(one.grab().save(renderRoot + QStringLiteral("/summary-single-label.png")));
		}
		// The ring is whole: no gap at twelve o'clock or anywhere.
		{
			auto* const pie   = one.pie();
			const auto  image = pie->grab().toImage();
			const auto  scale = image.devicePixelRatio();
			const auto  fill  = one.fill_of(issue);
			const auto  ring  = (pie->outer_radius() + pie->inner_radius()) / 2.0;
			for (auto degree = 0; degree < 360; degree += 3) {
				const auto radians = degree * std::numbers::pi / 180.0;
				const auto at      = pie->centre() + QPointF{ ring * std::sin(radians), -ring * std::cos(radians) };
				const auto drawn   = image.pixelColor(static_cast<int>(at.x() * scale), static_cast<int>(at.y() * scale));
				expect(std::abs(drawn.red() - fill.red()) <= 6 && std::abs(drawn.green() - fill.green()) <= 6 && std::abs(drawn.blue() - fill.blue()) <= 6,
					   "a single label's ring is whole at " + std::to_string(degree) + " degrees: " + drawn.name().toStdString());
			}
		}
		auto       two    = qt::report_tally_view{ };
		const auto praise = core::dressed_count{ .Word = "praise", .Count = 1U, .Tone = core::report_tone::done, .First = 9U };
		auto       opened = std::vector<std::uint32_t>{ };
		two.set_open([&opened](const std::uint32_t aFirst) { opened.push_back(aFirst); });
		two.set_counts({ issue, praise });
		two.resize(two.sizeHint());
		two.show();
		settle();
		expect(two.slice_start(1U) == 270.0 && two.slice_span(0U) == 270.0 && two.slice_span(1U) == 90.0, "three to one is three quarters and a quarter");
		expect_exact_slices(two, "three to one");
		expect_drawn_slices(two, "three to one");
		// Hovering a slice lights its row; hovering a row lights its slice.
		// No row has focus, which lights its slice too.
		if (auto* const focused = QApplication::focusWidget()) {
			focused->clearFocus();
		}
		settle();
		expect(!two.highlighted().has_value(), "with no pointer and no focus nothing is lit");
		auto* const pie  = two.pie();
		const auto  at   = pie->slice_middle(1U);
		auto        move = QMouseEvent{ QEvent::MouseMove, at, pie->mapToGlobal(at), Qt::NoButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(pie, &move);
		expect(two.highlighted() == std::optional<std::size_t>{ 1U }, "hovering the praise slice lights the praise row");
		const auto lifted = pie->grab().toImage();
		{
			// The lit slice stands out of the ring: just past its edge it is drawn.
			const auto radians = (two.slice_start(1U) + 45.0) * std::numbers::pi / 180.0;
			const auto reach   = pie->outer_radius() + 1.5;
			const auto out     = pie->centre() + QPointF{ reach * std::sin(radians), -reach * std::cos(radians) };
			const auto scale   = lifted.devicePixelRatio();
			expect(lifted.pixelColor(static_cast<int>(out.x() * scale), static_cast<int>(out.y() * scale)).alpha() > 128, "the hovered slice stands out of the ring");
		}
		auto leave = QEvent{ QEvent::Leave };
		QApplication::sendEvent(pie, &leave);
		expect(!two.highlighted().has_value(), "leaving the donut lights nothing");
		auto* const first = two.rows().front();
		auto        enter = QEnterEvent{ QPointF{ 4.0, 4.0 }, QPointF{ 4.0, 4.0 }, first->mapToGlobal(QPointF{ 4.0, 4.0 }) };
		QApplication::sendEvent(first, &enter);
		expect(two.highlighted() == std::optional<std::size_t>{ 0U }, "hovering the issue row lights the issue slice");
		QApplication::sendEvent(first, &leave);
		expect(!two.highlighted().has_value(), "leaving the row lights nothing");
		// A slice's tooltip is its row's, beside the slice; the hole has none.
		{
			const auto middle = pie->slice_middle(1U);
			auto       help   = QHelpEvent{ QEvent::ToolTip, middle.toPoint(), pie->mapToGlobal(middle.toPoint()) };
			QApplication::sendEvent(pie, &help);
			const auto* const tip   = qobject_cast<QLabel*>(qt::anchored_tooltip());
			const auto        shown = tip != nullptr && tip->text() == two.rows().back()->toolTip();
			expect(shown, "a slice's tooltip is its row's");
			qt::hide_anchored_tooltip();
		}
		// A press on a slice opens the report at it; on the hole, nothing.
		const auto pressAt = [&](const QPointF aAt) {
			auto press = QMouseEvent{ QEvent::MouseButtonPress, aAt, pie->mapToGlobal(aAt), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			auto lift  = QMouseEvent{ QEvent::MouseButtonRelease, aAt, pie->mapToGlobal(aAt), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(pie, &press);
			QApplication::sendEvent(pie, &lift);
		};
		pressAt(pie->slice_middle(0U));
		pressAt(pie->centre());
		pressAt(pie->slice_middle(1U));
		expect(opened == std::vector<std::uint32_t>{ 2U, 9U }, "a press on a slice opens the report at its first item, a press in the hole does nothing");
	}

	/// Build 76 (Ubuntu's larger font in the narrowest drawer). Given less
	/// than the full donut and its legend need, the summary draws the
	/// smaller donut with the legend beside it, every word whole; given the
	/// room again, the full one.
	void test_a_narrow_summary_draws_the_smaller_donut()
	{
		auto tally = qt::report_tally_view{ };
		tally.set_counts({
			core::dressed_count{ .Word = "issue", .Count = 3U, .Tone = core::report_tone::open, .First = 2U },
			core::dressed_count{ .Word = "suggestion", .Count = 2U, .Tone = core::report_tone::neutral, .First = 5U },
			core::dressed_count{ .Word = "praise", .Count = 1U, .Tone = core::report_tone::done, .First = 9U },
		});
		const auto least = tally.minimumSizeHint().width();
		const auto full  = least + qt::report_tally_pie::diameter - qt::report_tally_pie::compact_diameter;
		tally.resize(least, tally.sizeHint().height());
		tally.show();
		settle();
		auto* const pie = tally.pie();
		expect(pie->ring_diameter() == qt::report_tally_pie::compact_diameter && pie->width() == qt::report_tally_pie::compact_diameter + (2 * qt::report_tally_pie::lift),
			   "at its least width the summary draws the smaller donut");
		const auto rows = tally.rows();
		for (auto* const row : rows) {
			const auto placed = QRect{ row->mapTo(&tally, QPoint{ }), row->size() };
			const auto word   = QString::fromStdString(row->count().Word);
			expect(placed.left() > pie->geometry().right() && tally.rect().contains(placed) && row->width() >= row->minimumSizeHint().width()
					   && row->word_rect().width() >= QFontMetrics{ row->font() }.horizontalAdvance(word),
				   word.toStdString() + ": beside the smaller donut, inside the summary and whole");
		}
		expect_exact_slices(tally, "the smaller donut");
		expect_drawn_slices(tally, "the smaller donut");
		tally.resize(full - 1, tally.height());
		settle();
		expect(pie->ring_diameter() == qt::report_tally_pie::compact_diameter, "a pixel short of the full donut's room, it stays smaller");
		tally.resize(full, tally.height());
		settle();
		expect(pie->ring_diameter() == qt::report_tally_pie::diameter, "with the room, the donut is full size again");
		expect_drawn_slices(tally, "the full donut again");
	}

	/// A style that gives a push button a layout rect smaller than the
	/// button, as the macOS style does (8 points off its foot); this one
	/// takes some off every side.
	class bezel_style final : public QProxyStyle
	{
	public:
		[[nodiscard]] auto subElementRect(const SubElement aElement, const QStyleOption* const aOption, const QWidget* const aWidget) const -> QRect override
		{
			if (aElement == SE_PushButtonLayoutItem && aOption != nullptr) {
				return aOption->rect.adjusted(3, 4, -3, -8);
			}
			return QProxyStyle::subElementRect(aElement, aOption, aWidget);
		}
	};

	/// The legend's rows stand apart whatever rect the style gives a push
	/// button, on every platform this course runs on, and what the pointer
	/// and the keyboard draw on a row stays in it.
	/// \details The style is shown to be one that does it: two plain push
	/// buttons one under the other, laid out by it, stand over each other,
	/// which is what the legend's rows did under the macOS style.
	void test_the_rows_stand_apart_whatever_rect_the_style_gives_a_button()
	{
		auto style = bezel_style{ };
		{
			auto        plain  = QWidget{ };
			auto* const column = new QVBoxLayout{ &plain };
			column->setContentsMargins(0, 0, 0, 0);
			column->setSpacing(0);
			auto* const first  = new QPushButton{ QStringLiteral("Placeholder"), &plain };
			auto* const second = new QPushButton{ QStringLiteral("Placeholder"), &plain };
			for (auto* const button : { first, second }) {
				button->setStyle(&style);
				button->setFixedHeight(22);
				column->addWidget(button);
			}
			plain.show();
			settle();
			expect(first->geometry().intersects(second->geometry()), "under the style, two plain push buttons stand over each other: the second at " + std::to_string(second->geometry().top()) + ", the first ending at " + std::to_string(first->geometry().bottom() + 1));
		}
		auto tally = qt::report_tally_view{ };
		tally.set_counts({
			core::dressed_count{ .Word = "issue", .Count = 2U, .Tone = core::report_tone::open, .First = 2U },
			core::dressed_count{ .Word = "question", .Count = 1U, .Tone = core::report_tone::neutral, .First = 4U },
			core::dressed_count{ .Word = "suggestion", .Count = 1U, .Tone = core::report_tone::neutral, .First = 6U },
			core::dressed_count{ .Word = "praise", .Count = 1U, .Tone = core::report_tone::done, .First = 9U },
		});
		auto* const legend = tally.findChild<QWidget*>(QStringLiteral("agentTurnReportTallyLegend"));
		expect(legend != nullptr && legend->layout() != nullptr, "the summary has its legend");
		for (auto* const row : tally.rows()) {
			row->setStyle(&style);
			auto option = QStyleOptionButton{ };
			option.initFrom(row);
			expect(row->style()->subElementRect(QStyle::SE_PushButtonLayoutItem, &option, row) != row->rect(), row->count().Word + ": the style gives the row a smaller layout rect than the row");
		}
		legend->layout()->invalidate();
		tally.resize(tally.sizeHint());
		tally.show();
		settle();
		expect_rows_tile(tally, "a style with a smaller button rect");
		const auto pie = tally.pie()->geometry();
		for (const auto* const row : tally.rows()) {
			const auto placed = QRect{ row->mapTo(&tally, QPoint{ }), row->size() };
			expect(tally.rect().contains(placed) && placed.left() == pie.right() + 1 + qt::report_tally_view::legend_gap, row->count().Word + ": the row stands its gap beside the donut, inside the summary, at " + std::to_string(placed.left()));
		}
		make_active(tally);
		expect_states_keep_to_their_row(tally, "a style with a smaller button rect");
		// In the font a person may choose, too.
		auto larger = tally.font();
		larger.setPointSizeF(larger.pointSizeF() * 1.6);
		tally.setFont(larger);
		settle();
		tally.resize(tally.sizeHint());
		settle();
		expect_rows_tile(tally, "a style with a smaller button rect, larger font");
		expect_states_keep_to_their_row(tally, "a style with a smaller button rect, larger font");
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
		// summary and attachment is saved there at the narrowest and a wide
		// drawer: layout evidence, not a look, which the application draws.
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
		// Build 76 (Ubuntu's larger font in the narrowest drawer). The donut
		// keeps its full size wherever it fits beside the legend.
		const auto fullRing = [&tally] { return tally->pie()->ring_diameter() == qt::report_tally_pie::diameter; };
		expect(fullRing(), "narrowest drawer: the donut keeps its full size");
		publish_drawer(window, 640U);
		render(QStringLiteral("summary-wide"));
		expect_whole(*tally, *scroll, "wide drawer");
		expect(fullRing(), "wide drawer: the donut keeps its full size");
		const auto original = QApplication::font();
		auto       larger   = original;
		larger.setPointSizeF(original.pointSizeF() * 1.6);
		QApplication::setFont(larger);
		settle();
		expect_whole(*tally, *scroll, "wide drawer, larger font");
		expect(fullRing(), "wide drawer, larger font: the donut keeps its full size");
		publish_drawer(window, 280U);
		render(QStringLiteral("summary-narrow-larger-font"));
		expect_whole(*tally, *scroll, "narrowest drawer, larger font");
		{
			// Smaller only where the full donut beside the legend would not
			// fit in the width the summary is given. That width stops short
			// of the drawer's edge by the reply's own margins. Measured
			// against the drawer's edge it once passed only because finding
			// cards whose rows could not wrap made the conversation 69 px
			// wider than the drawer, and the summary with it.
			const auto full = tally->minimumSizeHint().width() + qt::report_tally_pie::diameter - qt::report_tally_pie::compact_diameter;
			std::cout << "BPR-2344 narrowest drawer, larger font: the donut is " << tally->pie()->ring_diameter() << " px; the full one needs " << full << " of the summary's " << tally->width() << " in a " << scroll->viewport()->width()
					  << " px drawer\n";
			expect(fullRing() || full > tally->width(), "narrowest drawer, larger font: the donut is smaller only where the full one would not fit");
		}
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
		// [[BPR-2344]]. Pressing a count's slice of the donut opens the report
		// at it, as pressing its legend row does.
		{
			auto* const praise = tally->rows().back();
			auto* const pie    = tally->pie();
			const auto  at     = pie->slice_middle(praise->index());
			const auto  global = pie->mapToGlobal(at);
			auto        press  = QMouseEvent{ QEvent::MouseButtonPress, at, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			auto        lift   = QMouseEvent{ QEvent::MouseButtonRelease, at, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(pie, &press);
			QApplication::sendEvent(pie, &lift);
			settle();
			expect(opened.size() == 3U && opened.back().Heading == praise->count().First, "pressing a count's slice opens the report at its first finding");
			opened.pop_back();
		}
		// And from the keyboard: a row takes focus, says what it counts, and
		// Space opens the report at it; its slice stands out while it has
		// focus.
		{
			auto* const nitpick = tally->rows().at(2U);
			expect(nitpick->focusPolicy() == Qt::StrongFocus && nitpick->accessibleName() == QStringLiteral("nitpick: 1") && nitpick->toolTip() == QStringLiteral("Open the report at its first nitpick"),
				   "a row takes focus, names its count and says what it opens");
			make_active(window);
			nitpick->setFocus(Qt::TabFocusReason);
			settle();
			expect(tally->highlighted() == std::optional<std::size_t>{ nitpick->index() }, "a focused row lights its slice");
			auto down = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
			auto up   = QKeyEvent{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
			QApplication::sendEvent(nitpick, &down);
			QApplication::sendEvent(nitpick, &up);
			settle();
			expect(opened.size() == 3U && opened.back().Heading == nitpick->count().First, "Space on a focused row opens the report at its first finding");
			opened.pop_back();
		}
		// The attachment's own Open opens it where it opens.
		qobject_cast<QPushButton*>(card->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen")))->click();
		settle();
		expect(opened.size() == 3U && opened.back().Heading == 0U, "the attachment opens the report at no row");
		window.hide();
	}

	/// A report of a report's shape that labels no finding: it counts nothing.
	[[nodiscard]] auto plain_report() -> QString
	{
		return QStringLiteral("# Placeholder walk-through\n\nPlaceholder summary of the walk-through.\n\n"
							  "## Placeholder first part\n\nPlaceholder text.\n\n"
							  "## Placeholder second part\n\nPlaceholder text.\n");
	}

	/// \p aWidget's place in \p aReply; its bottom is one past its last row.
	[[nodiscard]] auto placed_in(const QWidget& aReply, const QWidget& aWidget) -> QRect { return QRect{ aWidget.mapTo(&aReply, QPoint{ }), aWidget.size() }; }

	/// \p aWidget's place in \p aReply as its layout gave it: the widget's
	/// own rect, or the smaller one a style lays a push button out by.
	[[nodiscard]] auto laid_out_in(const QWidget& aReply, QWidget& aWidget) -> QRect
	{
		const auto item = QWidgetItem{ &aWidget }.geometry();
		return QRect{ aWidget.parentWidget()->mapTo(&aReply, item.topLeft()), item.size() };
	}

	/// The next widget Tab stops at after \p aFrom, as Qt's focus order has it.
	[[nodiscard]] auto next_tab_stop(const QWidget& aFrom) -> QWidget*
	{
		for (auto* next = aFrom.nextInFocusChain(); next != nullptr && next != &aFrom; next = next->nextInFocusChain()) {
			if ((next->focusPolicy() & Qt::TabFocus) != 0 && next->isVisible() && next->isEnabled() && next->focusProxy() == nullptr) {
				return next;
			}
		}
		return nullptr;
	}

	/// Owner, 2026-11-25: "I'm thinking that I want to move the report
	/// findings donut & counts above the attachment." Down the reply: the
	/// answer, the summary, the attachment, the byline, on one left edge.
	void expect_summary_over_attachment(const qt::agent_tab_view& aTab, const std::size_t aTurn, const std::string& aWhere)
	{
		auto* const reply  = aTab.turn_reply(aTurn);
		auto* const card   = aTab.turn_report_card(aTurn);
		auto* const tally  = aTab.turn_report_tally(aTurn);
		auto* const answer = aTab.turn_answer_view(aTurn);
		expect(reply != nullptr && card != nullptr && tally != nullptr && answer != nullptr && card->isVisible() && tally->isVisible() && answer->isVisible(), aWhere + ": the reply shows its answer and its report");
		auto* const open   = card->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"));
		auto* const legend = tally->findChild<QWidget*>(QStringLiteral("agentTurnReportTallyLegend"));
		auto* const byline = reply->findChild<QWidget*>(QStringLiteral("agentTurnFooter"));
		expect(open != nullptr && open->isVisible() && legend != nullptr && byline != nullptr && byline->isVisible(), aWhere + ": the report has its attachment and the reply its byline");
		const auto answerBox = placed_in(*reply, *answer);
		const auto tallyBox  = placed_in(*reply, *tally);
		const auto openBox   = placed_in(*reply, *open);
		const auto bylineBox = placed_in(*reply, *byline);
		// What the summary draws ends with the taller of its donut and legend.
		const auto drawnEnd  = std::max(placed_in(*reply, *tally->pie()).bottom(), placed_in(*reply, *legend).bottom()) + 1;
		const auto positions = ": answer " + std::to_string(answerBox.top()) + ".." + std::to_string(answerBox.bottom() + 1) + ", summary " + std::to_string(tallyBox.top()) + ".." + std::to_string(drawnEnd) + ", attachment "
			+ std::to_string(openBox.top()) + ".." + std::to_string(openBox.bottom() + 1) + ", byline " + std::to_string(bylineBox.top());
		expect(answerBox.bottom() < tallyBox.top(), aWhere + ": the summary stands under the answer" + positions);
		expect(drawnEnd <= openBox.top(), aWhere + ": the attachment stands under the summary" + positions);
		expect(openBox.top() - drawnEnd == qt::report_tally_view::attachment_gap, aWhere + ": the attachment is its gap under the donut and legend" + positions);
		expect(openBox.bottom() < bylineBox.top() && bylineBox.top() - openBox.bottom() - 1 == reply->layout()->spacing(), aWhere + ": the byline follows the attachment as it follows any reply's last part" + positions);
		expect(tallyBox.left() == answerBox.left() && openBox.left() == answerBox.left() && openBox.right() == answerBox.right(), aWhere + ": answer, summary and attachment share the reply's edges" + positions);
		// Tab goes through the counts in the report's order, then to the
		// attachment they stand over.
		const auto rows = tally->rows();
		for (std::size_t index = 0U; index + 1U < rows.size(); ++index) {
			expect(next_tab_stop(*rows[index]) == rows[index + 1U], aWhere + ": Tab goes from " + rows[index]->count().Word + " to " + rows[index + 1U]->count().Word);
		}
		expect(!rows.empty() && next_tab_stop(*rows.back()) == open, aWhere + ": Tab goes from the last count to the attachment");
	}

	void test_the_summary_stands_over_its_attachment()
	{
		qt::workbench_window window;
		open_review(window, 900);
		auto* const tab = window.agent_tab();
		tab->present_thread({ { .Question = QStringLiteral("Review this pull request"), .Answer = review_report(), .Footer = QStringLiteral("Placeholder agent answered in 0:04"), .Outcome = core::agent_turn_outcome::answered },
							  { .Question = QStringLiteral("Placeholder walk-through?"), .Answer = plain_report(), .Footer = QStringLiteral("Placeholder agent answered in 0:02"), .Outcome = core::agent_turn_outcome::answered } });
		settle();
		expect(tab->turn_count() == 2U, "the two report turns are restored");
		// A report that counts nothing: its attachment alone, where an
		// attachment has always stood under its answer.
		auto* const plainCard = tab->turn_report_card(1U);
		expect(plainCard != nullptr && plainCard->isVisible() && (tab->turn_report_tally(1U) == nullptr || tab->turn_report_tally(1U)->isHidden()), "a report that labels no finding shows its attachment and no summary");
		// The attachment row is a push button too, and the macOS style lays
		// one out by a rect smaller than the button: alone under its answer
		// the row is drawn that much above where its card put it. That is
		// the row's own to settle, not the legend's, so the place judged
		// here is the one the layout gave, and the course says where the
		// drawn row is not at it.
		auto* const plainOpen = plainCard->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"));
		expect(plainOpen != nullptr, "the attachment alone has its row");
		const auto alone = [&] {
			const auto answerBox = placed_in(*tab->turn_reply(1U), *tab->turn_answer_view(1U));
			const auto laidOut   = laid_out_in(*tab->turn_reply(1U), *plainOpen).top() - answerBox.bottom() - 1;
			const auto drawn     = placed_in(*tab->turn_reply(1U), *plainOpen).top() - answerBox.bottom() - 1;
			if (drawn != laidOut) {
				std::cout << "report-attachment-alone: drawn " << drawn << " under its answer, laid out " << laidOut << " (the " << QApplication::style()->name().toStdString() << " style's push button rect)\n";
			}
			return laidOut;
		};
		const auto summaryGap = [&] { return placed_in(*tab->turn_reply(0U), *tab->turn_report_tally(0U)).top() - placed_in(*tab->turn_reply(0U), *tab->turn_answer_view(0U)).bottom() - 1; };
		for (const auto width : { 280U, 320U, 360U, 420U, 640U }) {
			publish_drawer(window, width);
			wait(150);
			const auto where = "a " + std::to_string(width) + " drawer";
			expect_summary_over_attachment(*tab, 0U, where);
			expect(alone() == tab->turn_reply(1U)->layout()->spacing() + 9, where + ": an attachment alone stands nine under its answer, " + std::to_string(alone()));
			// The summary reads on from the answer: it stands nearer to it
			// than the bordered row does alone.
			expect(summaryGap() > tab->turn_reply(0U)->layout()->spacing() && summaryGap() < alone(),
				   where + ": the summary stands nearer the answer than an attachment alone does, " + std::to_string(summaryGap()) + " for " + std::to_string(alone()));
		}
		const auto original = QApplication::font();
		auto       larger   = original;
		larger.setPointSizeF(original.pointSizeF() * 1.6);
		QApplication::setFont(larger);
		settle();
		expect_summary_over_attachment(*tab, 0U, "a 640 drawer, larger font");
		publish_drawer(window, 280U);
		wait(150);
		expect_summary_over_attachment(*tab, 0U, "a 280 drawer, larger font");
		QApplication::setFont(original);
		settle();
		// From the keyboard, in fact: Tab on the last count reaches the
		// attachment, and Shift+Tab goes back.
		publish_drawer(window, 360U);
		wait(150);
		auto* const tally = tab->turn_report_tally(0U);
		auto* const open  = tab->turn_report_card(0U)->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"));
		make_active(window);
		tally->rows().back()->setFocus(Qt::TabFocusReason);
		settle();
		expect(tally->rows().back()->hasFocus(), "the last count takes focus");
		auto forward = QKeyEvent{ QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier };
		QApplication::sendEvent(tally->rows().back(), &forward);
		settle();
		expect(open->hasFocus(), "Tab on the last count reaches the attachment");
		auto back = QKeyEvent{ QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier };
		QApplication::sendEvent(open, &back);
		settle();
		expect(tally->rows().back()->hasFocus(), "Shift+Tab on the attachment goes back to the last count");
		window.hide();
	}

	/// [[BPR-2338]] (owner, 2026-11-20: "the scroll when clicking on
	/// something to jump to it isn't aligned properly, the thing we're
	/// jumping to is cut off by the tab header"). After a jump the item's
	/// box (its rule, label line and place, above its heading) stands whole
	/// in the report, a line below the document tabs, which the report
	/// stands under and never under-runs.
	void expect_item_whole(qt::workbench_window& aWindow, const int aHeading, const std::string& aWhere)
	{
		auto* const report = aWindow.report_view();
		auto* const scroll = report->findChild<QScrollArea*>(QStringLiteral("agentReportScroll"));
		auto* const page   = report->dressed_view();
		expect(scroll != nullptr && report->dressed_shown() && page->isVisible(), aWhere + ": the report shows its page");
		const auto name  = QString::fromLatin1(qt::report_heading_anchor) + QString::number(aHeading);
		auto       block = QTextBlock{ };
		for (auto each = page->document()->begin(); !block.isValid() && each.isValid(); each = each.next()) {
			for (auto fragment = each.begin(); !fragment.atEnd(); ++fragment) {
				if (fragment.fragment().charFormat().anchorNames().contains(name)) {
					block = each;
				}
			}
		}
		expect(block.isValid(), aWhere + ": the page holds the item's heading");
		auto* const root = page->document()->rootFrame();
		auto*       box  = QTextCursor{ block }.currentFrame();
		while (box != nullptr && box != root && box->parentFrame() != root) {
			box = box->parentFrame();
		}
		expect(box != nullptr && box != root, aWhere + ": the item is a box");
		const auto drawn    = page->document()->documentLayout()->frameBoundingRect(box);
		const auto inWindow = [&](QWidget* const aWidget, const int aY) { return aWidget->mapTo(&aWindow, QPoint{ 0, aY }).y(); };
		// The box's first words: a finding's label, a section's name.
		auto label = box->firstCursorPosition().block();
		while (label.isValid() && label.text().trimmed().isEmpty()) {
			label = label.next();
		}
		expect(label.isValid(), aWhere + ": the box has words");
		const auto boxTop    = inWindow(page->viewport(), static_cast<int>(std::floor(drawn.top())));
		const auto labelTop  = inWindow(page->viewport(), page->cursorRect(QTextCursor{ label }).top());
		const auto headTop   = inWindow(page->viewport(), page->cursorRect(QTextCursor{ block }).top());
		const auto header    = inWindow(aWindow.document_tabs(), aWindow.document_tabs()->height());
		const auto shownTop  = inWindow(scroll->viewport(), 0);
		const auto shownEnd  = inWindow(scroll->viewport(), scroll->viewport()->height());
		const auto margin    = QFontMetrics{ page->font() }.lineSpacing();
		const auto positions = ": box " + std::to_string(boxTop) + ", label " + std::to_string(labelTop) + ", heading " + std::to_string(headTop) + ", header's bottom " + std::to_string(header)
			+ ", report's top " + std::to_string(shownTop) + ", margin " + std::to_string(margin);
		expect(shownTop >= header, aWhere + ": the document tabs do not overlay the report" + positions);
		expect(labelTop >= boxTop && headTop >= labelTop, aWhere + ": the box's first line is in it, at or above the heading" + positions);
		expect(boxTop >= header + margin && boxTop >= shownTop + margin, aWhere + ": the item's top stands a line below the header" + positions);
		expect(boxTop <= shownTop + margin + 2 && headTop < shownEnd, aWhere + ": the item is brought to the report's top" + positions);
		expect(report->section_top(aHeading) == boxTop - shownTop, aWhere + ": the section's top is the item's" + positions);
	}

	void test_a_jump_shows_its_item_whole_under_the_tab_header()
	{
		qt::workbench_window window;
		open_review(window, 900);
		auto* const tab = window.agent_tab();
		tab->present_thread({ { .Question = QStringLiteral("Review this pull request"), .Answer = review_report(), .Outcome = core::agent_turn_outcome::answered } });
		settle();
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
		tab->set_report_dispatch([&window](const qt::agent_report_document& aReport) { window.open_report("turn:" + std::to_string(aReport.Turn), aReport); });
		auto* const tally = tab->turn_report_tally(0U);
		expect(tally != nullptr && tally->isVisible(), "the review shows its summary");
		// Each count of the summary, the suggestion (the owner's) first.
		auto rows = tally->rows();
		std::ranges::stable_partition(rows, [](const qt::report_tally_row* const aRow) { return aRow->count().Word == "suggestion"; });
		expect(rows.front()->count().Word == "suggestion", "the summary counts a suggestion");
		// [[BPR-2344]]. The suggestion through its slice of the donut, the
		// rest through their legend rows.
		{
			auto* const pie   = tally->pie();
			const auto  at    = pie->slice_middle(rows.front()->index());
			auto        press = QMouseEvent{ QEvent::MouseButtonPress, at, pie->mapToGlobal(at), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			auto        lift  = QMouseEvent{ QEvent::MouseButtonRelease, at, pie->mapToGlobal(at), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(pie, &press);
			QApplication::sendEvent(pie, &lift);
			settle();
			expect_item_whole(window, static_cast<int>(rows.front()->count().First), "the summary's suggestion slice");
		}
		for (auto* const row : rows) {
			row->click();
			settle();
			expect_item_whole(window, static_cast<int>(row->count().First), "the summary's " + row->count().Word);
		}
		// Each section of the report's menu, from the last up.
		auto* const report   = window.report_view();
		auto        sections = 0;
		while (sections < report->outline()->count() && !report->outline()->item(sections)->data(Qt::AccessibleTextRole).toString().isEmpty()) {
			++sections;
		}
		expect(sections > 6, "the menu lists the report's sections: " + std::to_string(sections));
		for (auto section = sections - 1; section > 0; --section) {
			report->open_menu();
			settle();
			report->outline()->setCurrentRow(section);
			auto press = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
			QApplication::sendEvent(report->outline(), &press);
			settle();
			expect_item_whole(window, section, "the menu's section " + std::to_string(section));
			expect(report->current_section() == section, "the menu marks section " + std::to_string(section) + " after going to it");
		}
		window.hide();
	}

	/// The least height the window takes.
	constexpr int shortest_window = 360;

	/// A live report turn ends at the end of a conversation shorter than the
	/// turn, in a window `aHeight` tall. Whether the conversation had the
	/// room for the summary and the attachment both.
	[[nodiscard]] auto report_turn_ends_in_view(const int aHeight) -> bool
	{
		qt::workbench_window window;
		// A short window: the turn's end is taller than the conversation.
		open_review(window, aHeight);
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
		const auto  at    = tab->turn_count() - 1U;
		auto* const card  = tab->turn_report_card(at);
		auto* const tally = tab->turn_report_tally(at);
		expect(card != nullptr && card->isVisible() && tally != nullptr && tally->isVisible(), "the turn ended as a report with its summary");
		auto* const open = card->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"));
		expect(open != nullptr && open->isVisible(), "the report has its attachment");
		const auto shown = scroll->viewport()->height();
		const auto top   = open->mapTo(scroll->viewport(), QPoint{ }).y();
		const auto said  = ": the attachment at " + std::to_string(top) + ".." + std::to_string(top + open->height()) + " of " + std::to_string(shown);
		// The attachment opens the report: it is whole in view whatever the
		// conversation's height.
		expect(top >= 0 && top + open->height() <= shown, "the attachment is whole in view at the turn's end" + said);
		const auto summary = tally->mapTo(scroll->viewport(), QPoint{ }).y();
		const auto both    = open->mapTo(card, QPoint{ 0, open->height() }).y() + 24;
		std::cout << "report-in-view " << aHeight << said << ", the summary's top at " << summary << ", both need " << both << '\n';
		if (both <= shown) {
			expect(summary >= 0, "with the room for both, the summary over the attachment is in view too" + said + ", the summary's top at " + std::to_string(summary));
		} else {
			// Too short for both: the attachment is the one kept whole, at
			// the conversation's foot, with as much of the summary as fits.
			expect(summary < 0 && shown - (top + open->height()) <= 24, "too short for both, the attachment stands whole at the foot" + said);
		}
		window.hide();
		return both <= shown;
	}

	/// The attachment's keeping in view, in a conversation that holds the
	/// summary over it too and in one too short for both.
	void test_a_report_turn_ends_with_its_attachment_in_view()
	{
		expect(report_turn_ends_in_view(620), "a short window's conversation holds the summary and its attachment");
		expect(!report_turn_ends_in_view(shortest_window), "the shortest window's conversation is too short for both");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_the_donut_is_exact();
	test_a_narrow_summary_draws_the_smaller_donut();
	test_the_rows_stand_apart_whatever_rect_the_style_gives_a_button();
	test_a_report_turn_shows_its_summary_not_its_cards();
	test_the_summary_stands_over_its_attachment();
	test_a_jump_shows_its_item_whole_under_the_tab_header();
	test_a_report_turn_ends_with_its_attachment_in_view();
	std::cout << "agent-report-summary:pass\n";
	return EXIT_SUCCESS;
}
