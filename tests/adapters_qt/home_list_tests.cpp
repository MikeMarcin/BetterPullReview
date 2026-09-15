#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_panel.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/review_landing_model.hpp"

#include <QAccessible>
#include <QApplication>
#include <QElapsedTimer>
#include <QFont>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPalette>
#include <QPixmap>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyleOptionViewItem>
#include <QThread>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using bucket = core::attention_bucket;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// \brief Lets Qt lay out, including a list view's delayed relayout.
	void settle(const int aMilliseconds = 300)
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		do {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
			QThread::msleep(5);
		} while (timer.elapsed() < aMilliseconds);
	}

	[[nodiscard]] auto item(
		const std::int64_t  aNumber,
		const bucket        aBucket,
		const std::uint32_t aUnseen = 0U,
		std::string         aTitle  = "A title") -> core::queue_item
	{
		auto result = core::queue_item{
			.Ref        = { .Host = "github.com", .Account = "reviewer", .Repository = "octo/app", .Number = aNumber },
			.Membership = { .Role = core::github_role::reviewer },
			.Bucket     = aBucket,
		};
		result.Facts = core::queue_item_facts{ .Title = std::move(aTitle), .Author = "octocat", .UnseenComments = aUnseen };
		return result;
	}

	[[nodiscard]] auto recent_item(const QString& aIdentity, const QString& aTitle, const bool aResumable, const QString& aTime)
		-> QListWidgetItem*
	{
		auto* const row = new QListWidgetItem{ aIdentity };
		row->setData(qt::home_row_role::identity, aIdentity);
		row->setData(qt::home_row_role::title, aTitle);
		row->setData(qt::home_row_role::why, aResumable ? QStringLiteral("My reading is saved.") : QStringLiteral("Viewed."));
		row->setData(qt::home_row_role::state, -1);
		row->setData(qt::home_row_role::time, aTime);
		row->setData(qt::home_row_role::row_action, aResumable ? QStringLiteral("Resume") : QString{ });
		return row;
	}

	[[nodiscard]] auto option_for(const QAbstractItemView& aView, const QModelIndex& aIndex) -> QStyleOptionViewItem
	{
		auto option = QStyleOptionViewItem{ };
		option.initFrom(&aView);
		option.rect   = aView.visualRect(aIndex);
		option.font   = aView.font();
		option.widget = &aView;
		return option;
	}

	[[nodiscard]] auto chroma(const QColor& aColour) -> int
	{
		return std::max({ aColour.red(), aColour.green(), aColour.blue() })
			- std::min({ aColour.red(), aColour.green(), aColour.blue() });
	}

	[[nodiscard]] auto close_to(const QColor& aActual, const QColor& aExpected, const int aTolerance) -> bool
	{
		return std::abs(aActual.red() - aExpected.red()) <= aTolerance
			&& std::abs(aActual.green() - aExpected.green()) <= aTolerance
			&& std::abs(aActual.blue() - aExpected.blue()) <= aTolerance;
	}

	[[nodiscard]] auto grey_palette() -> QPalette
	{
		auto       palette = QPalette{ };
		const auto set     = [&palette](const QPalette::ColorRole aRole, const int aGrey) {
			for (const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
				palette.setColor(group, aRole, QColor{ aGrey, aGrey, aGrey });
			}
		};
		set(QPalette::Window, 0xF2);
		set(QPalette::WindowText, 0x20);
		set(QPalette::Base, 0xFF);
		set(QPalette::AlternateBase, 0xF6);
		set(QPalette::Text, 0x1B);
		set(QPalette::PlaceholderText, 0x6B);
		set(QPalette::Mid, 0xB0);
		set(QPalette::Button, 0xF2);
		set(QPalette::ButtonText, 0x20);
		set(QPalette::Light, 0xFF);
		set(QPalette::Midlight, 0xF8);
		set(QPalette::Dark, 0x80);
		set(QPalette::Shadow, 0x40);
		set(QPalette::Highlight, 0x50);
		set(QPalette::HighlightedText, 0xFF);
		set(QPalette::ToolTipBase, 0xFF);
		set(QPalette::ToolTipText, 0x20);
		set(QPalette::Link, 0x20);
		set(QPalette::LinkVisited, 0x20);
		return palette;
	}

	/// \brief One list over one queue, as the window composes it.
	struct home_fixture
	{
		home_fixture()
		{
			List.set_state_colours(qt::home_state_colours_for(qt::appearance_profile{ }));
			List.resize(720, 900);
		}

		qt::review_landing_model        Source;
		qt::review_landing_filter_model Filter{ Source };
		qt::home_list                   List{ Filter };
	};

	[[nodiscard]] auto buckets_in(const home_fixture& aHome, const qt::home_group aGroup) -> std::vector<bucket>
	{
		auto        result = std::vector<bucket>{ };
		auto* const model  = aHome.List.state_model(aGroup);
		for (auto row = 0; row < model->rowCount(); ++row) {
			result.push_back(aHome.Filter.item_at(model->mapToSource(model->index(row, 0)))->Bucket);
		}
		return result;
	}

	void send_key(QWidget& aWidget, const Qt::Key aKey)
	{
		auto press   = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier };
		auto release = QKeyEvent{ QEvent::KeyRelease, aKey, Qt::NoModifier };
		QApplication::sendEvent(&aWidget, &press);
		QApplication::sendEvent(&aWidget, &release);
		QApplication::processEvents();
	}

	void click(QWidget& aWidget, const QPoint& aAt)
	{
		const auto at      = QPointF{ aAt };
		const auto global  = aWidget.mapToGlobal(at);
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, at, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, at, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(&aWidget, &press);
		QApplication::sendEvent(&aWidget, &release);
		QApplication::processEvents();
	}

	int            StateChanges = 0;
	const QObject* StateTarget  = nullptr;

	void capture_state_change(QAccessibleEvent* const aEvent)
	{
		if (aEvent != nullptr && aEvent->type() == QAccessible::StateChanged && aEvent->object() == StateTarget) {
			++StateChanges;
		}
	}

	// A6: rows in state order, one anatomy for the queue and Recently viewed.
	void test_rows_follow_state_order_with_one_anatomy(const QString& aRenderPath)
	{
		home_fixture home;
		const auto   items = std::vector{
			item(7, bucket::quiet),
			item(6, bucket::observed_with_new_activity),
			item(5, bucket::authored_awaiting_reviewer_action),
			item(4, bucket::fixes_awaiting_verification),
			item(3, bucket::new_comments_or_author_replies, 2U),
			item(2, bucket::changed_since_my_review),
			item(1, bucket::initial_review_requested, 1U),
		};
		home.Source.apply(core::ordered_queue(items));
		auto* const recent = new qt::home_recent_view{ };
		recent->addItem(recent_item(QStringLiteral("microsoft/terminal#20607"),
									QStringLiteral("Preserve the DRCS font when performing a soft reset"), false, QStringLiteral("Tue")));
		home.List.adopt_recent(*recent);
		home.List.set_recent(1, QStringLiteral("1 viewed"));
		home.List.set_queue_status(true, true);
		home.List.show();
		settle();

		auto* const needs     = home.List.state_panel(qt::home_group::needs_you);
		auto* const waiting   = home.List.state_panel(qt::home_group::waiting_on_others);
		auto* const following = home.List.state_panel(qt::home_group::following);
		auto* const recents   = home.List.recent_panel();
		expect(needs->isVisible() && waiting->isVisible() && following->isVisible() && recents->isVisible(),
			   "each state with rows is a panel, and Recently viewed is one");
		expect(needs->y() < waiting->y() && waiting->y() < following->y() && following->y() < recents->y(),
			   "the panels are Needs me, Waiting on others, Following, then Recently viewed");
		expect(buckets_in(home, qt::home_group::needs_you)
					   == std::vector{ bucket::initial_review_requested, bucket::changed_since_my_review,
									   bucket::new_comments_or_author_replies, bucket::fixes_awaiting_verification }
				   && buckets_in(home, qt::home_group::waiting_on_others) == std::vector{ bucket::authored_awaiting_reviewer_action }
				   && buckets_in(home, qt::home_group::following) == std::vector{ bucket::observed_with_new_activity, bucket::quiet },
			   "every row is in the core's state order, inside its state's panel");
		expect(needs->count() == 4 && waiting->count() == 1 && following->count() == 2 && recents->count() == 1
				   && needs->title() == QStringLiteral("Needs me") && recents->title() == QStringLiteral("Recently viewed"),
			   "each header names its state and counts its rows");

		auto* const delegate = home.List.row_delegate();
		for (const auto group : { qt::home_group::needs_you, qt::home_group::waiting_on_others, qt::home_group::following }) {
			expect(home.List.state_view(group)->itemDelegate() == delegate, "every state draws through the one row delegate");
		}
		expect(recent->itemDelegate() == delegate, "Recently viewed draws through the same delegate");

		auto* const needsView = home.List.state_view(qt::home_group::needs_you);
		const auto  first     = needsView->model()->index(0, 0);
		expect(first.data(qt::home_row_role::identity).toString() == QStringLiteral("octo/app#1")
				   && first.data(qt::home_row_role::title).toString() == QStringLiteral("A title")
				   && first.data(qt::home_row_role::reason_word).toString() == QStringLiteral("Requested")
				   && first.data(qt::home_row_role::why).toString() == QStringLiteral("Opened by octocat; my review is requested; 1 new comment.")
				   && first.data(qt::home_row_role::state).toInt() == static_cast<int>(qt::home_group::needs_you)
				   && first.data(qt::home_row_role::time).toString().isEmpty()
				   && first.data(qt::home_row_role::progress).toString().isEmpty(),
			   "a queue row answers the anatomy's roles, with no time or progress it was not given");
		const auto recentRow = recent->model()->index(0, 0);
		expect(recentRow.data(qt::home_row_role::identity).toString() == QStringLiteral("microsoft/terminal#20607")
				   && recentRow.data(qt::home_row_role::reason_word).toString().isEmpty()
				   && recentRow.data(qt::home_row_role::state).toInt() == -1
				   && recentRow.data(qt::home_row_role::time).toString() == QStringLiteral("Tue"),
			   "a recent row answers the same roles, without a state");
		expect(delegate->sizeHint(option_for(*needsView, first), first).height() >= qt::home_row_delegate::minimum_row_height
				   && delegate->sizeHint(option_for(*recent, recentRow), recentRow).height() >= qt::home_row_delegate::minimum_row_height,
			   "every row is at least 44 points tall");
		if (!aRenderPath.isEmpty()) {
			expect(home.List.grab().save(aRenderPath), "the default list renders");
		}
	}

	// A6: colour means state, on the reason word and the rail, and nowhere else.
	void test_colour_is_only_the_reason_word_and_the_rail()
	{
		home_fixture home;
		home.List.setPalette(grey_palette());
		auto font = home.List.font();
		font.setStyleStrategy(QFont::NoSubpixelAntialias);
		home.List.setFont(font);
		home.Source.apply(std::array{ item(1, bucket::initial_review_requested), item(2, bucket::changed_since_my_review) });
		home.List.set_queue_status(true, true);
		home.List.show();
		settle();

		auto* const panel    = home.List.state_panel(qt::home_group::needs_you);
		auto* const view     = home.List.state_view(qt::home_group::needs_you);
		auto* const delegate = home.List.row_delegate();
		const auto  measure  = [&] {
			const auto image = panel->grab().toImage();
			const auto ratio = image.devicePixelRatio();
			auto       words = std::vector<QRect>{ };
			for (auto row = 0; row < view->model()->rowCount(); ++row) {
				const auto index  = view->model()->index(row, 0);
				const auto reason = delegate->reason_rect(option_for(*view, index), index);
				words.push_back(QRect{ view->viewport()->mapTo(panel, reason.topLeft()), reason.size() }.adjusted(-2, -2, 2, 2));
			}
			const auto header = panel->header()->geometry();
			const auto rail   = QRect{ 0, 0, qt::home_panel::rail_width + 1, panel->height() };
			auto       inWord = 0;
			auto       stray  = 0;
			for (auto y = 0; y < image.height(); ++y) {
				for (auto x = 0; x < image.width(); ++x) {
					if (chroma(image.pixelColor(x, y)) <= 40) {
						continue;
					}
					const auto logical = QPoint{ static_cast<int>(x / ratio), static_cast<int>(y / ratio) };
					if (std::ranges::any_of(words, [&logical](const QRect& aWord) { return aWord.contains(logical); })) {
						++inWord;
					} else if (!header.contains(logical) && !rail.contains(logical)) {
						++stray;
					}
				}
			}
			return std::pair{ inWord, stray };
		};
		const auto [inWord, stray] = measure();
		expect(inWord > 0, "the reason word is drawn in its state's colour");
		expect(stray == 0, "no identity, title, sentence, hairline or ground carries colour");

		// Selected, the row keeps the style's own selection and its words its ink.
		view->setCurrentIndex(view->model()->index(0, 0));
		QApplication::processEvents();
		const auto rowRect = view->visualRect(view->model()->index(0, 0));
		const auto image   = panel->grab().toImage();
		const auto ratio   = image.devicePixelRatio();
		const auto sample  = view->viewport()->mapTo(panel, QPoint{ rowRect.left() + 6, rowRect.top() + (rowRect.height() / 2) });
		expect(close_to(image.pixelColor(static_cast<int>(sample.x() * ratio), static_cast<int>(sample.y() * ratio)),
						grey_palette().color(QPalette::Highlight), 24),
			   "the selected row is filled with the palette's selection by the style");
		const auto [selectedInWord, selectedStray] = measure();
		static_cast<void>(selectedInWord);
		expect(selectedStray == 0, "nothing else takes colour when a row is selected");
	}

	// Remote literal text wraps and is never elided; a row is 44 points and one hairline.
	void test_rows_wrap_without_elision()
	{
		home_fixture home;
		const auto   title = std::string{
			"A title long enough to wrap across several lines of a narrow row, with an unbroken token "
			"Supercalifragilistic_identifier_without_any_spaces_whatsoever_in_it and no ellipsis anywhere"
		};
		home.Source.apply(std::array{ item(1, bucket::changed_since_my_review, 0U, title),
									  item(2, bucket::changed_since_my_review, 0U, "Short"),
									  item(3, bucket::changed_since_my_review, 0U, "Short") });
		home.List.set_queue_status(true, true);
		home.List.resize(360, 900);
		home.List.show();
		settle();

		auto* const view     = home.List.state_view(qt::home_group::needs_you);
		auto* const delegate = home.List.row_delegate();
		const auto  index    = view->model()->index(0, 0);
		const auto  option   = option_for(*view, index);
		const auto  lines    = delegate->line_texts(option, index);
		const auto  expected = QStringLiteral("octo/app#1  ") + QString::fromStdString(title)
			+ index.data(qt::home_row_role::why).toString() + QStringLiteral(" Changed");
		expect(lines.join(QString{ }) == expected, "the laid-out lines are the whole literal text");
		expect(lines.size() > 3, "a long title wraps onto more lines");
		expect(std::ranges::none_of(lines, [](const QString& aLine) { return aLine.contains(QChar{ 0x2026 }); }),
			   "no line is elided");
		expect(delegate->sizeHint(option, index).height() > qt::home_row_delegate::minimum_row_height + qt::home_row_delegate::hairline,
			   "a wrapped row grows rather than clipping");
		const auto middle       = view->model()->index(1, 0);
		const auto last         = view->model()->index(2, 0);
		const auto middleHeight = delegate->sizeHint(option_for(*view, middle), middle).height();
		const auto lastHeight   = delegate->sizeHint(option_for(*view, last), last).height();
		expect(lastHeight >= qt::home_row_delegate::minimum_row_height && middleHeight == lastHeight + qt::home_row_delegate::hairline,
			   "a row is at least 44 points, with one hairline between rows");
	}

	// Each state panel collapses by the chevron, Space, Return, Enter, Left and Right.
	void test_panels_collapse_by_pointer_and_keyboard()
	{
		home_fixture home;
		home.Source.apply(std::array{ item(1, bucket::initial_review_requested, 1U), item(2, bucket::changed_since_my_review, 3U) });
		home.List.set_queue_status(true, true);
		home.List.show();
		home.List.activateWindow();
		settle();

		auto* const panel   = home.List.state_panel(qt::home_group::needs_you);
		auto* const header  = panel->header();
		auto* const view    = home.List.state_view(qt::home_group::needs_you);
		StateTarget         = header;
		StateChanges        = 0;
		const auto previous = QAccessible::installUpdateHandler(capture_state_change);

		click(*header, header->chevron_rect().center());
		settle(50);
		const auto summary = QStringLiteral("1 requested, 1 changed; 4 new comments");
		expect(panel->collapsed() && !view->isVisibleTo(panel) && panel->count() == 2 && panel->summary() == summary,
			   "the chevron collapses the panel to its header, which keeps its count and summary");
		expect(panel->height() == panel->header_height(), "collapsed, the panel is its header and nothing else");
		auto* const accessible = QAccessible::queryAccessibleInterface(header);
		expect(accessible != nullptr && accessible->role() == QAccessible::Button && accessible->state().expandable
				   && accessible->state().collapsed && !accessible->state().expanded
				   && accessible->text(QAccessible::Name) == QStringLiteral("Needs me, 2")
				   && accessible->text(QAccessible::Description) == summary,
			   "the header is announced as a collapsed, expandable button with its count and summary");
		expect(StateChanges == 1, "collapsing announces one state change");
		click(*header, header->chevron_rect().center());
		settle(50);
		expect(!panel->collapsed() && view->isVisibleTo(panel) && accessible->state().expanded
				   && accessible->text(QAccessible::Description).isEmpty() && StateChanges == 2,
			   "the chevron expands it again, and says so");

		header->setFocus(Qt::OtherFocusReason);
		send_key(*header, Qt::Key_Space);
		expect(panel->collapsed(), "Space toggles");
		send_key(*header, Qt::Key_Return);
		expect(!panel->collapsed(), "Return toggles");
		send_key(*header, Qt::Key_Enter);
		expect(panel->collapsed(), "Enter toggles");
		send_key(*header, Qt::Key_Left);
		expect(panel->collapsed(), "Left keeps a collapsed panel collapsed");
		send_key(*header, Qt::Key_Right);
		expect(!panel->collapsed(), "Right expands");
		send_key(*header, Qt::Key_Right);
		expect(!panel->collapsed(), "Right keeps an expanded panel expanded");
		send_key(*header, Qt::Key_Left);
		expect(panel->collapsed(), "Left collapses");
		static_cast<void>(QAccessible::installUpdateHandler(previous));
		StateTarget = nullptr;
	}

	// A panel's header sticks to the top of the scroll area while the panel is in view.
	void test_headers_stick_while_their_panel_is_in_view(const QString& aRenderPath)
	{
		home_fixture home;
		auto         items = std::vector<core::queue_item>{ };
		for (auto number = std::int64_t{ 1 }; number <= 40; ++number) {
			items.push_back(item(number, bucket::initial_review_requested));
		}
		for (auto number = std::int64_t{ 41 }; number <= 52; ++number) {
			items.push_back(item(number, bucket::quiet));
		}
		home.Source.apply(items);
		home.List.set_queue_status(true, true);
		home.List.resize(600, 400);
		home.List.show();
		settle(400);

		auto* const bar          = home.List.verticalScrollBar();
		auto* const viewport     = home.List.viewport();
		auto* const needs        = home.List.state_panel(qt::home_group::needs_you);
		auto* const following    = home.List.state_panel(qt::home_group::following);
		const auto  top          = needs->y();
		const auto  headerHeight = needs->header()->height();
		expect(bar->maximum() >= top + needs->height(), "the list scrolls past the end of Needs me");

		bar->setValue(top + 250);
		settle(50);
		expect(needs->header()->mapTo(viewport, QPoint{ }).y() == 0 && needs->header_offset() == 250,
			   "scrolled into a panel, its header sits at the top of the scroll area");
		expect(following->header_offset() == 0, "the next panel's header stays at its panel's top");
		const auto image = viewport->grab().toImage();
		const auto ratio = image.devicePixelRatio();
		const auto tint  = qt::mix_colour(needs->tone(), home.List.palette().color(QPalette::Active, QPalette::Base), qt::home_strength::header);
		// The header's empty right end: no title, count or summary is drawn there.
		const auto probe  = needs->header()->mapTo(viewport, QPoint{ needs->header()->width() - 24, needs->header()->height() / 2 });
		const auto actual = image.pixelColor(static_cast<int>(probe.x() * ratio), static_cast<int>(probe.y() * ratio));
		if (!close_to(actual, tint, 10)) {
			std::cerr << "stuck header probe " << probe.x() << ',' << probe.y() << " actual=" << actual.name().toStdString()
					  << " expected=" << tint.name().toStdString() << '\n';
		}
		expect(close_to(actual, tint, 10), "the stuck header is drawn in its tint at the top of the list");
		if (!aRenderPath.isEmpty()) {
			expect(image.save(aRenderPath), "the scrolled list renders");
		}

		bar->setValue(top + needs->height() - (headerHeight / 2));
		settle(50);
		expect(bar->value() == top + needs->height() - (headerHeight / 2)
				   && needs->header_offset() == needs->height() - headerHeight
				   && needs->header()->geometry().bottom() == needs->height() - 1,
			   "at the end of its panel the header stops with the panel rather than covering the next");

		bar->setValue(0);
		settle(50);
		expect(needs->header_offset() == 0, "scrolled back, the header is at its panel's top again");

		needs->set_collapsed(true);
		settle(100);
		bar->setValue(std::min(bar->maximum(), top + 100));
		settle(50);
		expect(needs->header_offset() == 0, "a collapsed panel's header never moves");
		needs->set_collapsed(false);
	}

	// Slim state bands nested in a group panel stick under the group's header.
	void test_bands_stick_under_their_group_header()
	{
		auto area = QScrollArea{ };
		area.setWidgetResizable(true);
		area.resize(500, 300);
		auto* const column = new QWidget{ };
		auto* const layout = new QVBoxLayout{ column };
		layout->setContentsMargins(0, 0, 0, 0);
		auto* const group = new qt::home_panel{ std::nullopt, qt::home_panel_style::panel, column };
		group->set_title(QStringLiteral("octo"));
		group->set_count(2);
		auto* const bands      = new QWidget{ };
		auto* const bandColumn = new QVBoxLayout{ bands };
		bandColumn->setContentsMargins(0, 0, 0, 0);
		bandColumn->setSpacing(0);
		auto nested = std::vector<qt::home_panel*>{ };
		for (const auto state : { qt::home_group::needs_you, qt::home_group::following }) {
			auto* const band = new qt::home_panel{ state, qt::home_panel_style::band, bands };
			band->set_title(qt::home_group_label(state));
			auto* const body = new QWidget{ };
			body->setFixedHeight(600);
			band->set_content(body);
			bandColumn->addWidget(band);
			nested.push_back(band);
		}
		group->set_content(bands);
		layout->addWidget(group);
		layout->addStretch(1);
		area.setWidget(column);
		area.show();
		settle(200);

		const auto bandTop = nested.front()->mapTo(column, QPoint{ }).y();
		area.verticalScrollBar()->setValue(bandTop + 200);
		settle(50);
		qt::stick_home_header(*group, *area.viewport(), 0);
		expect(group->header()->mapTo(area.viewport(), QPoint{ }).y() == 0, "the group's header sticks at the top");
		expect(nested.front()->header()->mapTo(area.viewport(), QPoint{ }).y() == group->header()->height(),
			   "the band in view sticks under the group's header");
		expect(nested.back()->header_offset() == 0, "the band below keeps its header at its own top");
	}

	// Empty states are absent; an empty ready queue is one sentence linking to Reviews & queue.
	void test_empty_states_and_the_all_empty_sentence()
	{
		home_fixture home;
		auto         opened = 0;
		home.List.set_settings_dispatch([&opened] { ++opened; });
		home.List.show();
		home.List.activateWindow();
		auto* const needs     = home.List.state_panel(qt::home_group::needs_you);
		auto* const waiting   = home.List.state_panel(qt::home_group::waiting_on_others);
		auto* const following = home.List.state_panel(qt::home_group::following);
		auto* const sentence  = home.List.empty_sentence();
		const auto  noState   = [&] { return !needs->isVisible() && !waiting->isVisible() && !following->isVisible(); };

		home.List.set_queue_status(false, true);
		settle(50);
		expect(noState() && !sentence->isVisible(), "while the queue restores there is no panel and no sentence");

		home.Source.apply(std::array{ item(1, bucket::quiet) });
		home.List.set_queue_status(true, true);
		home.List.refresh();
		settle(100);
		const auto chain = home.List.focus_chain();
		expect(!needs->isVisible() && !waiting->isVisible() && following->isVisible() && !sentence->isVisible(),
			   "a state with no row is not shown");
		expect(std::ranges::find(chain, needs->header()) == chain.end() && std::ranges::find(chain, waiting->header()) == chain.end()
				   && std::ranges::find(chain, following->header()) != chain.end(),
			   "and is no keyboard stop");

		home.Source.apply(std::span<const core::queue_item>{ });
		home.List.refresh();
		settle(100);
		expect(needs->isVisible() && needs->count() == 0 && sentence->isVisible()
				   && !home.List.state_view(qt::home_group::needs_you)->isVisible() && !waiting->isVisible() && !following->isVisible(),
			   "an empty ready queue is Needs me holding one sentence, and nothing else");
		expect(sentence->accessibleName()
					   == QStringLiteral("Nothing needs me. Pull requests from saved repositories and observer rules appear here.")
				   && sentence->text().contains(QStringLiteral("href=")),
			   "the sentence is the RFC's, with its link");
		expect(sentence->palette().color(QPalette::Active, QPalette::Link) == sentence->palette().color(QPalette::Active, QPalette::Text),
			   "the link is drawn in the text's colour, not a state's");
		const auto emptyChain = home.List.focus_chain();
		expect(std::ranges::find(emptyChain, sentence) != emptyChain.end(), "the link is a keyboard stop");
		sentence->setFocus(Qt::TabFocusReason);
		QApplication::processEvents();
		send_key(*sentence, Qt::Key_Return);
		expect(opened == 1, "Return on the link opens Reviews & queue");

		for (auto* const panel : { needs, waiting, following }) {
			if (!panel->isVisible() || panel->collapsed()) {
				continue;
			}
			const auto* const rows = home.List.state_view(*panel->state());
			expect((rows->isVisible() && rows->model()->rowCount() > 0) || (panel == needs && sentence->isVisible()),
				   "no shown panel is an empty box");
		}

		home.Source.apply(std::array{ item(2, bucket::initial_review_requested) });
		home.List.set_queue_status(true, false);
		home.List.refresh();
		settle(100);
		expect(noState() && !sentence->isVisible(), "without an account there is no state panel at all");

		home.List.set_queue_status(true, true);
		home.Filter.apply(application::queue_presentation_preferences{ .Bucket = bucket::quiet });
		home.List.refresh();
		settle(100);
		expect(noState() && !sentence->isVisible(), "a filter that hides every row draws no sentence and no box");
	}

	// Resume is a row action on hover and on focus.
	void test_resume_is_a_row_action_on_hover_and_focus()
	{
		home_fixture home;
		auto* const  recent = new qt::home_recent_view{ };
		recent->addItem(recent_item(QStringLiteral("microsoft/terminal#42"), QStringLiteral("Fix the conpty resize race"), false, QStringLiteral("Tue")));
		recent->addItem(recent_item(QStringLiteral("microsoft/terminal#77"), QStringLiteral("Teach the pane splitter about tabs"), true, QStringLiteral("Mon")));
		home.List.adopt_recent(*recent);
		home.List.set_recent(2, QStringLiteral("2 viewed; 1 saved reading"));
		auto resumed = std::vector<int>{ };
		home.List.set_recent_action_dispatch([&resumed](const int aRow) { resumed.push_back(aRow); });
		home.List.show();
		home.List.activateWindow();
		settle();

		auto* const action = home.List.recent_action();
		expect(action != nullptr && !action->isVisible() && action->focusPolicy() == Qt::NoFocus,
			   "Resume is not shown until a row is hovered or focused, and takes no focus");
		const auto hover = [recent](const int aRow) {
			const auto at     = QPointF{ recent->visualRect(recent->model()->index(aRow, 0)).center() };
			auto       motion = QMouseEvent{ QEvent::MouseMove, at, recent->viewport()->mapToGlobal(at), Qt::NoButton, Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(recent->viewport(), &motion);
			QApplication::processEvents();
		};
		hover(1);
		const auto saved = recent->visualRect(recent->model()->index(1, 0));
		expect(action->isVisible() && action->row() == 1 && saved.contains(action->geometry()) && action->text() == QStringLiteral("Resume"),
			   "hovering a row with a saved reading shows Resume inside that row");
		click(*action, action->rect().center());
		expect(resumed == std::vector{ 1 }, "Resume resumes the row it is on");
		hover(0);
		expect(!action->isVisible(), "a row that kept no reading offers no Resume");
		auto leave = QEvent{ QEvent::Leave };
		QApplication::sendEvent(recent->viewport(), &leave);
		QApplication::processEvents();

		recent->setCurrentRow(1);
		recent->setFocus(Qt::OtherFocusReason);
		settle(50);
		expect(recent->hasFocus() && action->isVisible() && action->row() == 1, "with the list focused, Resume sits on the current row");
		recent->setCurrentRow(0);
		QApplication::processEvents();
		expect(!action->isVisible(), "and leaves with a row that has no reading");
	}

	// A row the core moves to another state moves panels without a reset and keeps its place.
	void test_a_row_that_changes_state_moves_panels_and_keeps_its_place()
	{
		home_fixture home;
		auto         resets  = 0;
		auto         context = QObject{ };
		QObject::connect(&home.Source, &QAbstractItemModel::modelReset, &context, [&resets] { ++resets; });
		auto moving = item(1, bucket::initial_review_requested);
		home.Source.apply(core::ordered_queue(std::vector{ moving, item(2, bucket::initial_review_requested), item(3, bucket::quiet) }));
		home.List.set_queue_status(true, true);
		home.List.show();
		settle();

		auto* const needsView     = home.List.state_view(qt::home_group::needs_you);
		auto* const followingView = home.List.state_view(qt::home_group::following);
		expect(home.List.select(moving.Ref, false) && home.List.current_view() == needsView
				   && home.List.current_queue_item()->Ref == moving.Ref,
			   "an identity is selected in the state that holds it");
		followingView->setCurrentIndex(followingView->model()->index(0, 0));
		expect(home.List.current_view() == followingView && !needsView->currentIndex().isValid(),
			   "there is one current row on the page");
		static_cast<void>(home.List.select(moving.Ref, false));

		moving.Bucket = bucket::quiet;
		home.Source.apply(core::ordered_queue(std::vector{ item(2, bucket::initial_review_requested), item(3, bucket::quiet), moving }));
		home.List.refresh();
		settle(100);
		expect(resets == 0, "the move is not a reset");
		expect(home.List.state_panel(qt::home_group::needs_you)->count() == 1 && home.List.state_panel(qt::home_group::following)->count() == 2,
			   "the row left Needs me for Following");
		expect(home.List.select(moving.Ref, false) && home.List.current_view() == followingView,
			   "the same identity is found again in its new state");

		auto* const following = home.List.state_panel(qt::home_group::following);
		following->set_collapsed(true);
		expect(home.List.select(moving.Ref, false) && following->collapsed(),
			   "restoring a selection leaves a collapsed panel collapsed");
		expect(home.List.select(moving.Ref, true) && !following->collapsed(),
			   "an activation expands the panel that holds its row");
	}

	// A panel's rows do not scroll on their own: Home scrolls as one page.
	void test_a_wheel_over_rows_scrolls_the_page()
	{
		home_fixture home;
		auto         items = std::vector<core::queue_item>{ };
		for (auto number = std::int64_t{ 1 }; number <= 30; ++number) {
			items.push_back(item(number, bucket::initial_review_requested));
		}
		home.Source.apply(items);
		home.List.set_queue_status(true, true);
		home.List.resize(600, 300);
		home.List.show();
		settle(400);

		auto* const view   = home.List.state_view(qt::home_group::needs_you);
		auto* const bar    = home.List.verticalScrollBar();
		const auto  before = bar->value();
		expect(view->verticalScrollBar()->maximum() == 0, "a panel's rows are as tall as they are, with nothing to scroll");
		const auto at    = QPointF{ view->visualRect(view->model()->index(1, 0)).center() };
		auto       wheel = QWheelEvent{ at, view->viewport()->mapToGlobal(at), QPoint{ }, QPoint{ 0, -240 }, Qt::NoButton,
										Qt::NoModifier, Qt::NoScrollPhase, false };
		QApplication::sendEvent(view->viewport(), &wheel);
		settle(50);
		expect(bar->value() > before, "a wheel over a panel's rows scrolls Home");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   renderPath = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString{ };
	if (const auto expected = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE"); !expected.isEmpty()) {
		auto probe = QWidget{ };
		probe.show();
		QApplication::processEvents();
		expect(std::abs(probe.devicePixelRatio() - expected.toDouble()) < 0.01, "the course runs at the scale it was registered for");
	}
	const auto scrolledPath = renderPath.isEmpty() ? QString{ } : QString{ renderPath }.replace(QStringLiteral(".png"), QStringLiteral("-scrolled.png"));

	test_rows_follow_state_order_with_one_anatomy(renderPath);
	test_colour_is_only_the_reason_word_and_the_rail();
	test_rows_wrap_without_elision();
	test_panels_collapse_by_pointer_and_keyboard();
	test_headers_stick_while_their_panel_is_in_view(scrolledPath);
	test_bands_stick_under_their_group_header();
	test_empty_states_and_the_all_empty_sentence();
	test_resume_is_a_row_action_on_hover_and_focus();
	test_a_row_that_changes_state_moves_panels_and_keeps_its_place();
	test_a_wheel_over_rows_scrolls_the_page();

	std::cout << "home-list-tests:pass\n";
	return EXIT_SUCCESS;
}
