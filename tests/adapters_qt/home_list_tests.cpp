#include "../support/window_system_mouse.hpp"
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
#include <QLocale>
#include <QMouseEvent>
#include <QPalette>
#include <QPixmap>
#include <QProxyStyle>
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
#include <tuple>
#include <utility>
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

	/// \brief [[BPR-0783]]. One row of a sanitized two-account fixture.
	[[nodiscard]] auto owned_item(
		const std::int64_t aNumber,
		const bucket       aBucket,
		const std::string& aHost,
		const std::string& aAccount,
		const std::string& aRepository) -> core::queue_item
	{
		auto result = core::queue_item{
			.Ref        = { .Host = aHost, .Account = aAccount, .Repository = aRepository, .Number = aNumber },
			.Membership = { .Role = core::github_role::reviewer },
			.Bucket     = aBucket,
		};
		result.Facts = core::queue_item_facts{ .Title = "A title", .Author = "octocat" };
		return result;
	}

	constexpr auto enterprise_host = "ghe.example.invalid";
	constexpr auto default_host    = "github.com";
	constexpr auto default_login   = "octo-reviewer";
	constexpr auto other_login     = "octo-enterprise";

	/// \brief Two accounts on two hosts, in the shape boards H6 to H8 draw.
	///
	/// \details State order puts the two Needs me rows first, so Organization
	/// groups read octo-org, acme-tools, then octo-reviewer.
	[[nodiscard]] auto two_account_queue() -> std::vector<core::queue_item>
	{
		return core::ordered_queue(std::vector{
			owned_item(101, bucket::initial_review_requested, default_host, default_login, "octo-org/app"),
			owned_item(102, bucket::observed_with_new_activity, default_host, default_login, "octo-org/app"),
			owned_item(201, bucket::changed_since_my_review, enterprise_host, other_login, "acme-tools/build"),
			owned_item(202, bucket::quiet, enterprise_host, other_login, "acme-tools/build"),
			owned_item(301, bucket::authored_awaiting_reviewer_action, default_host, default_login, "octo-reviewer/notes"),
		});
	}

	/// \brief One group holding two accounts, as board H6's mixed group draws it.
	///
	/// \details Both rows are owned by octo-org on github.com, so Organization
	/// puts them in one group whose rows belong to different accounts.
	[[nodiscard]] auto mixed_account_queue() -> std::vector<core::queue_item>
	{
		return core::ordered_queue(std::vector{
			owned_item(401, bucket::initial_review_requested, default_host, default_login, "octo-org/app"),
			owned_item(402, bucket::changed_since_my_review, default_host, "octo-second", "octo-org/tools"),
		});
	}

	/// \brief The same rows on one account, so no chip is drawn anywhere.
	[[nodiscard]] auto one_account_queue() -> std::vector<core::queue_item>
	{
		return core::ordered_queue(std::vector{
			owned_item(101, bucket::initial_review_requested, default_host, default_login, "octo-org/app"),
			owned_item(201, bucket::changed_since_my_review, default_host, default_login, "acme-tools/build"),
			owned_item(301, bucket::authored_awaiting_reviewer_action, default_host, default_login, "octo-reviewer/notes"),
		});
	}

	[[nodiscard]] auto identities_of(const QAbstractItemView& aView) -> QStringList
	{
		auto rows = QStringList{ };
		for (auto row = 0; row < aView.model()->rowCount(); ++row) {
			rows.push_back(aView.model()->index(row, 0).data(qt::home_row_role::identity).toString());
		}
		return rows;
	}

	[[nodiscard]] auto titles_of(const std::vector<qt::home_panel*>& aPanels) -> QStringList
	{
		auto titles = QStringList{ };
		for (const auto* const panel : aPanels) {
			titles.push_back(panel->title());
		}
		return titles;
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

	/// \brief [[BPR-1364]]. `aCount` left clicks on a row, through the window
	/// system as a hand makes them, so the click gives focus and two make a
	/// double-click.
	void press_row(QAbstractItemView& aView, const QModelIndex& aRow, const int aCount)
	{
		bpr::tests::window_system_clicks(*aView.viewport(), QRectF{ aView.visualRect(aRow) }.center(), aCount);
	}

	/// \brief [[BPR-1364]]. A style that activates an item on a single click,
	/// as KDE's does.
	class single_click_style final : public QProxyStyle
	{
	public:
		auto styleHint(const StyleHint aHint, const QStyleOption* const aOption, const QWidget* const aWidget, QStyleHintReturn* const aReturn) const
			-> int override
		{
			return aHint == QStyle::SH_ItemView_ActivateItemOnSingleClick ? 1 : QProxyStyle::styleHint(aHint, aOption, aWidget, aReturn);
		}
	};

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
				   && first.data(qt::home_row_role::why).toString() == QStringLiteral("My review is requested; 1 new comment.")
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
		// The title leads; the repository and number head the second line.
		const auto expected = QString::fromStdString(title) + QStringLiteral("octo/app#1 by octocat  ")
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

	void test_authored_sections_in_every_home_arrangement(const QString& aRenderPath)
	{
		home_fixture home;
		auto         mine           = item(1, bucket::new_comments_or_author_replies, 3U);
		mine.Membership.Authored    = true;
		auto waiting                = item(2, bucket::authored_awaiting_reviewer_action);
		waiting.Membership.Role     = core::github_role::author;
		auto quiet                  = item(3, bucket::quiet);
		quiet.Membership.Role       = core::github_role::author;
		auto unknown                = item(4, bucket::detail_unknown);
		unknown.Membership.Authored = true;
		auto other                  = item(1, bucket::new_comments_or_author_replies, 2U);
		other.Ref.Account           = "another-account";
		const auto queue            = core::ordered_queue(std::array{ mine, waiting, quiet, unknown, other });
		home.Source.apply(queue);
		home.List.set_queue_status(true, true);
		home.List.show();
		home.List.refresh();
		settle(100);
		auto* const authored = home.List.state_panel(qt::home_group::my_pull_requests);
		expect(authored->isVisible() && authored->count() == 3 && authored->tone() == qt::builtin_attention_color(qt::appearance_profile{ }.variant(), qt::attention_tone::my_pull_requests),
			   "mixed-state authored section has three rows and its own header and rail tone [[BPR-1528]]");
		expect(home.List.state_panel(qt::home_group::needs_you)->count() == 1 && home.List.state_panel(qt::home_group::waiting_on_others)->count() == 1, "Needs me counts only the other account and waiting-only keeps its panel");
		expect(authored->summary().contains(QStringLiteral("1 needs action")) && authored->summary().contains(QStringLiteral("3 new comments")) && authored->summary().contains(QStringLiteral("1 unknown")), "collapsed authored summary preserves actionable, unread and unknown facts");
		const auto row = home.List.state_model(qt::home_group::my_pull_requests)->index(0, 0);
		expect(row.data(qt::home_row_role::why).toString().contains(QStringLiteral("My pull request")) && row.data(qt::home_row_role::why).toString().contains(QStringLiteral("3 new comments")), "authored row identifies ownership and unread activity when headers are hidden");
		expect(row.data(qt::home_row_role::state).toInt() == static_cast<int>(qt::home_group::needs_you), "authorship separation keeps the row's amber attention state");
		for (const auto grouping : { application::home_grouping::none, application::home_grouping::organization, application::home_grouping::repository, application::home_grouping::account }) {
			for (const auto headers : { true, false }) {
				home.List.set_grouping(grouping);
				home.List.set_state_headers(headers);
				home.List.refresh();
				settle(50);
				expect(home.List.select(mine.Ref, true) && home.List.current_queue_item()->Ref == mine.Ref, "every grouping/header arrangement keeps exact authored selection reachable");
				auto sections = std::vector<int>{ };
				if (grouping == application::home_grouping::none && !headers) {
					const auto* model = home.List.flat_view()->model();
					for (auto i = 0; i < model->rowCount(); ++i) {
						sections.push_back(model->index(i, 0).data(qt::review_landing_model::group_role).toInt());
					}
					expect(std::ranges::is_sorted(sections), "without headers ownership sections remain contiguous in section order");
				}
				if (!aRenderPath.isEmpty()) {
					const auto path = QString{ aRenderPath }.replace(QStringLiteral(".png"), QStringLiteral("-authored-%1-%2.png").arg(static_cast<int>(grouping)).arg(headers));
					expect(home.List.grab().save(path), "authored arrangements render to inspection artifacts");
				}
			}
		}
		home.List.set_grouping(application::home_grouping::none);
		home.List.set_state_headers(true);
		home.List.set_collapsed_panels(application::home_collapsed_panel::waiting_on_others);
		expect(!authored->collapsed(), "old waiting collapse never hides separated authored activity");
		authored->header()->setFocus();
		send_key(*authored->header(), Qt::Key_Left);
		expect(authored->collapsed() && (home.List.collapsed_panels() & application::home_collapsed_panel::my_pull_requests) != 0U, "keyboard collapse uses its own durable bit");
		send_key(*authored->header(), Qt::Key_Right);
		expect(!authored->collapsed(), "keyboard expands the authored panel");
		auto preferences  = application::queue_presentation_preferences{ };
		preferences.Quiet = application::queue_quiet_filter::attention;
		home.Filter.apply(preferences);
		home.List.refresh();
		expect(authored->count() == 2 && authored->summary().contains(QStringLiteral("1 unknown")), "filtered authored count excludes quiet while preserving unknown");
		home.Filter.apply({ });
		home.Source.apply(std::array{ mine, quiet, unknown });
		home.List.refresh();
		settle(50);
		expect(authored->isVisible() && !home.List.empty_sentence()->isVisible() && !home.List.state_panel(qt::home_group::needs_you)->isVisible(), "authored-only Home has its rows and no empty Needs me panel");
	}

	// [[BPR-1528]]. My pull requests draws its own tinted header and rail, at
	// the strengths every state panel uses, as a panel and as a group's band;
	// its rows keep their own state colours.
	void test_my_pull_requests_has_its_own_header_and_rail()
	{
		home_fixture home;
		auto         mine        = item(1, bucket::new_comments_or_author_replies, 3U);
		mine.Membership.Authored = true;
		home.Source.apply(std::array{ mine, item(2, bucket::initial_review_requested) });
		home.List.set_queue_status(true, true);
		home.List.show();
		home.List.refresh();
		settle(100);

		const auto  colours  = qt::home_state_colours_for(qt::appearance_profile{ });
		auto* const authored = home.List.state_panel(qt::home_group::my_pull_requests);
		auto* const needs    = home.List.state_panel(qt::home_group::needs_you);
		expect(colours.MyPullRequests.isValid() && authored->tone() == colours.MyPullRequests,
			   "My pull requests' panel takes its own tone");
		expect(authored->tone() != needs->tone() && authored->tone() != colours.WaitingOnOthers && authored->tone() != colours.Following,
			   "the tone is none of the three states'");

		const auto base  = authored->palette().color(QPalette::Active, QPalette::Base);
		const auto image = authored->grab().toImage();
		const auto ratio = image.devicePixelRatio();
		const auto pixel = [&image, ratio](const QPoint aPoint) {
			return image.pixelColor(static_cast<int>(aPoint.x() * ratio), static_cast<int>(aPoint.y() * ratio));
		};
		const auto header = authored->header()->geometry();
		const auto tint   = qt::mix_colour(colours.MyPullRequests, base, qt::home_strength::header);
		const auto rail   = qt::mix_colour(colours.MyPullRequests, base, qt::home_strength::rail);
		const auto empty  = QPoint{ header.right() - 24, header.center().y() };
		const auto onRail = QPoint{ 1, header.bottom() + 12 };
		if (!close_to(pixel(empty), tint, 10) || !close_to(pixel(onRail), rail, 10)) {
			std::cerr << "authored header=" << pixel(empty).name().toStdString() << " expected=" << tint.name().toStdString()
					  << " rail=" << pixel(onRail).name().toStdString() << " expected=" << rail.name().toStdString() << '\n';
		}
		expect(close_to(pixel(empty), tint, 10), "the header is tinted in My pull requests' tone");
		expect(close_to(pixel(onRail), rail, 10), "the rail runs down the panel in My pull requests' tone");

		const auto row = home.List.state_model(qt::home_group::my_pull_requests)->index(0, 0);
		expect(row.data(qt::home_row_role::state).toInt() == static_cast<int>(qt::home_group::needs_you)
				   && colours.for_group(static_cast<qt::home_group>(row.data(qt::home_row_role::state).toInt())) == colours.NeedsMe,
			   "the authored row's reason word keeps its own state colour");

		home.List.set_grouping(application::home_grouping::repository);
		home.List.set_state_headers(true);
		home.List.refresh();
		settle(100);
		const auto groups = home.List.group_panels();
		expect(!groups.empty(), "Repository grouping draws a group");
		auto* const band = groups.empty() ? nullptr : home.List.band_panel(*groups.front(), qt::home_group::my_pull_requests);
		expect(band != nullptr && band->tone() == colours.MyPullRequests, "inside a group the My pull requests band carries the same rail");
		home.List.set_grouping(application::home_grouping::none);
		home.List.refresh();
	}

	// Empty states are absent; an empty ready queue is one sentence linking to Repositories.
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
		expect(opened == 1, "Return on the link opens Repositories");
		// RFC-BPR-0062, owner verdict 2026-10-08: with nothing saved, the empty
		// lane could hold nothing but this sentence, so it is not shown.
		home.List.set_onboarding(true);
		home.List.refresh();
		settle(50);
		expect(home.List.onboarding() && noState() && !sentence->isVisible(), "with nothing saved an empty queue shows no Needs me lane");
		home.Source.apply(std::array{ item(1, bucket::quiet) });
		home.List.refresh();
		settle(100);
		expect(following->isVisible() && !needs->isVisible(), "a row enrolled without a saved repository still shows");
		home.Source.apply(std::span<const core::queue_item>{ });
		home.List.set_onboarding(false);
		home.List.refresh();
		settle(100);
		expect(needs->isVisible() && sentence->isVisible(), "a saved repository brings back the empty lane's sentence");

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
		// [[BPR-0783]]. Home filters by role, added by, quiet items and the sort
		// within a group; Repository and Bucket are no longer filters at all.
		home.Filter.apply(application::queue_presentation_preferences{ .Role = core::github_role::author });
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

	// [[BPR-1683]]. Ignored is the last panel in every arrangement, starts
	// collapsed to its count, and its rows carry Unignore; Delete on a queue row
	// asks for Ignore. The list decides nothing: it reports the row.
	void test_ignored_is_last_collapsed_and_offers_unignore(const QString& aRenderPath)
	{
		home_fixture home;
		auto         ignored = item(7, bucket::initial_review_requested, 2U, "Ignored work");
		ignored.Ignored      = true;
		auto other           = item(8, bucket::quiet);
		other.Ignored        = true;
		home.Source.apply(core::ordered_queue(std::vector{ ignored, item(1, bucket::initial_review_requested), item(3, bucket::quiet), other }));
		auto asked = std::vector<std::pair<std::int64_t, bool>>{ };
		home.List.set_ignore_dispatch([&asked](const core::queue_item& aItem) { asked.emplace_back(aItem.Ref.Number, aItem.Ignored); });
		home.List.set_queue_status(true, true);
		home.List.show();
		home.List.activateWindow();
		settle();

		auto* const panel = home.List.ignored_panel();
		auto* const view  = home.List.ignored_view();
		expect(panel != nullptr && panel->isVisible() && panel->collapsed() && !view->isVisibleTo(panel),
			   "Ignored is shown collapsed to its header");
		expect(panel->title() == QStringLiteral("Ignored") && panel->count() == 2 && panel->summary() == QStringLiteral("2 ignored") && !panel->tone().isValid(),
			   "its header names it, counts it and carries no state colour");
		expect(home.List.state_panel(qt::home_group::needs_you)->count() == 1 && home.List.state_panel(qt::home_group::following)->count() == 1,
			   "the ignored rows are in no other panel");
		auto* const layout = home.List.widget()->layout();
		expect(layout->indexOf(panel) > layout->indexOf(home.List.recent_panel()) && layout->indexOf(panel) > layout->indexOf(home.List.state_panel(qt::home_group::following)),
			   "Ignored sorts after every other panel, Recently viewed included");
		const auto chain = home.List.focus_chain();
		expect(std::ranges::find(chain, panel->header()) != chain.end() && std::ranges::find(chain, view) == chain.end(),
			   "collapsed, its header is a keyboard stop and its rows are not");
		if (!aRenderPath.isEmpty()) {
			expect(home.List.grab().save(QString{ aRenderPath }.replace(QStringLiteral(".png"), QStringLiteral("-ignored-collapsed.png"))),
				   "the list with Ignored collapsed renders");
		}

		panel->header()->setFocus(Qt::OtherFocusReason);
		send_key(*panel->header(), Qt::Key_Right);
		settle(50);
		expect(!panel->collapsed() && view->isVisibleTo(panel) && identities_of(*view) == QStringList{ QStringLiteral("octo/app#7"), QStringLiteral("octo/app#8") },
			   "expanding it by keyboard shows the ignored rows in core order");
		if (!aRenderPath.isEmpty()) {
			expect(home.List.grab().save(QString{ aRenderPath }.replace(QStringLiteral(".png"), QStringLiteral("-ignored-expanded.png"))),
				   "the list with Ignored expanded renders");
		}
		const auto first = view->model()->index(0, 0);
		expect(first.data(qt::home_row_role::row_action).toString() == QStringLiteral("Unignore") && !first.data(qt::home_row_role::state).isValid()
				   && first.data(Qt::AccessibleDescriptionRole).toString().contains(QStringLiteral("Ignored;")),
			   "an ignored row offers Unignore, draws no state colour and says it is ignored");

		view->setFocus(Qt::OtherFocusReason);
		view->setCurrentIndex(first);
		settle(50);
		auto* const unignore = home.List.ignored_action();
		expect(unignore != nullptr && unignore->isVisible() && unignore->text() == QStringLiteral("Unignore") && unignore->focusPolicy() == Qt::NoFocus
				   && unignore->accessibleName() == QStringLiteral("Unignore octo/app#7"),
			   "with the list focused, Unignore sits on the current row and names it");
		click(*unignore, unignore->rect().center());
		expect(asked == std::vector<std::pair<std::int64_t, bool>>{ { 7, true } }, "Unignore asks to unignore that row");
		send_key(*view, Qt::Key_Backspace);
		expect(asked.size() == 2U && asked.back() == std::pair<std::int64_t, bool>{ 7, true }, "Backspace on an ignored row asks the same");

		auto* const needs = home.List.state_view(qt::home_group::needs_you);
		needs->setFocus(Qt::OtherFocusReason);
		needs->setCurrentIndex(needs->model()->index(0, 0));
		send_key(*needs, Qt::Key_Delete);
		expect(asked.size() == 3U && asked.back() == std::pair<std::int64_t, bool>{ 1, false }, "Delete on a queue row asks to ignore it");

		home.List.set_grouping(application::home_grouping::repository);
		settle(100);
		const auto groups = home.List.group_panels();
		expect(groups.size() == 1U && groups.front()->count() == 2 && home.List.ignored_panel()->isVisible() && home.List.ignored_panel()->count() == 2,
			   "grouped, the group holds only the active rows and Ignored stays its own panel");
		home.List.set_grouping(application::home_grouping::none);
		home.List.set_state_headers(false);
		settle(100);
		expect(home.List.flat_view()->model()->rowCount() == 2 && home.List.ignored_panel()->isVisible(),
			   "with State headers off, the one card holds only the active rows");
		home.List.set_state_headers(true);

		home.Source.apply(core::ordered_queue(std::vector{ ignored, other }));
		home.List.refresh();
		settle(100);
		expect(home.List.empty_sentence()->isVisibleTo(&home.List) && home.List.ignored_panel()->isVisible(),
			   "a queue of only ignored rows says nothing needs me, above Ignored");
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

	/// \brief Counts saturated pixels down the leading few points of one view.
	///
	/// \details [[BPR-0783]]. With State headers off there is no tinted band to
	/// carry the state, so each row does: a 3-point mark at its left.
	[[nodiscard]] auto marked_pixels(QWidget& aPanel, const QAbstractItemView& aRows) -> int
	{
		const auto image = aPanel.grab().toImage();
		const auto ratio = image.devicePixelRatio();
		const auto top   = aRows.viewport()->mapTo(&aPanel, QPoint{ 0, 0 });
		auto       found = 0;
		for (auto y = top.y() + 2; y < top.y() + aRows.viewport()->height() - 2; ++y) {
			for (auto x = top.x(); x < top.x() + qt::home_row_delegate::mark_width; ++x) {
				const auto point = QPoint{ static_cast<int>(x * ratio), static_cast<int>(y * ratio) };
				if (point.x() < image.width() && point.y() < image.height() && chroma(image.pixelColor(point)) > 40) {
					++found;
				}
			}
		}
		return found;
	}

	// A7: Group None with State headers off is one card of state-ordered rows,
	// each carrying its state's mark, with no state panel at all.
	void test_group_none_without_state_headers_is_one_marked_list()
	{
		home_fixture home;
		home.Source.apply(two_account_queue());
		home.List.set_queue_status(true, true);
		home.List.set_state_headers(false);
		home.List.show();
		settle(300);

		auto* const flat = home.List.flat_panel();
		auto* const rows = home.List.flat_view();
		expect(flat->isVisible() && rows->isVisible() && !flat->header_shown(),
			   "the rows sit in one neutral card with no header of its own");
		for (const auto state : { qt::home_group::needs_you, qt::home_group::waiting_on_others, qt::home_group::following }) {
			expect(!home.List.state_panel(state)->isVisible(), "no state is a panel of its own");
		}
		expect(identities_of(*rows)
				   == QStringList{ QStringLiteral("octo-org/app#101"),
								   QStringLiteral("acme-tools/build#201"),
								   QStringLiteral("octo-reviewer/notes#301"),
								   QStringLiteral("octo-org/app#102"),
								   QStringLiteral("acme-tools/build#202") },
			   "state is still the first key: Needs me, then Waiting on others, then Following");
		expect(marked_pixels(*flat, *rows) > 0, "each row carries its state's mark at its leading edge");
		const auto chain = home.List.focus_chain();
		expect(std::ranges::find(chain, static_cast<QWidget*>(rows)) != chain.end()
				   && std::ranges::find(chain, static_cast<QWidget*>(flat->header())) == chain.end(),
			   "the rows are a keyboard stop and the headerless card is not");
	}

	// A7: one neutral panel per group, with bands inside when State headers is on
	// and plain state-ordered rows when it is off.
	void test_each_grouping_renders_with_state_headers_on_and_off()
	{
		home_fixture home;
		home.Source.apply(two_account_queue());
		home.List.set_queue_status(true, true);
		home.List.show();
		settle(300);

		// The states the first group of each grouping holds: octo-org and its
		// repository hold a Needs me and a Following row, while the github.com
		// account holds those and the authored one as well.
		const auto both     = std::vector{ qt::home_group::needs_you, qt::home_group::following };
		const auto whole    = std::vector{ qt::home_group::needs_you, qt::home_group::waiting_on_others, qt::home_group::following };
		const auto expected = std::vector{
			std::tuple{ application::home_grouping::organization,
						QStringList{ QStringLiteral("octo-org"), QStringLiteral("acme-tools"), QStringLiteral("octo-reviewer") },
						both },
			std::tuple{ application::home_grouping::repository,
						QStringList{ QStringLiteral("octo-org/app"), QStringLiteral("acme-tools/build"), QStringLiteral("octo-reviewer/notes") },
						both },
			std::tuple{ application::home_grouping::account,
						QStringList{ QStringLiteral("octo-reviewer"), QStringLiteral("octo-enterprise") },
						whole },
		};
		for (const auto& [grouping, names, states] : expected) {
			home.List.set_grouping(grouping);
			home.List.set_state_headers(true);
			settle(200);
			const auto panels = home.List.group_panels();
			expect(titles_of(panels) == names,
				   "each group is a neutral panel, in the order its first row in state order gives it");
			expect(std::ranges::all_of(panels, [](const qt::home_panel* const aPanel) { return !aPanel->state().has_value(); }),
				   "a group panel is neutral: it carries no state and no rail");
			auto previous = -1;
			for (const auto* const panel : panels) {
				expect(panel->y() > previous, "the groups are laid out in that order");
				previous = panel->y();
			}
			// State headers on: a slim tinted band per non-empty state inside the group.
			auto* const first = panels.front();
			auto* const band  = home.List.band_panel(*first, qt::home_group::needs_you);
			expect(band != nullptr && band->style_kind() == qt::home_panel_style::band
					   && band->state() == qt::home_group::needs_you && band->isVisible()
					   && band->title() == QStringLiteral("Needs me"),
				   "the group's first state is a band of its own inside it");
			auto shown = std::vector<qt::home_group>{ };
			for (const auto state : { qt::home_group::needs_you, qt::home_group::waiting_on_others, qt::home_group::following }) {
				if (home.List.band_panel(*first, state) != nullptr) {
					shown.push_back(state);
				}
			}
			expect(shown == states,
				   "the group holds one band per state it has rows in, and a state with nothing in it is not shown");
			expect(home.List.rows_view(*first) == nullptr && home.List.rows_view(*band) != nullptr,
				   "with bands the rows belong to the bands, not to the group");
			const auto counted = [&home, first] {
				auto total = 0;
				for (const auto state : { qt::home_group::needs_you, qt::home_group::waiting_on_others, qt::home_group::following }) {
					if (auto* const each = home.List.band_panel(*first, state); each != nullptr) {
						total += each->count();
					}
				}
				return total;
			}();
			expect(first->count() == counted && first->count() > 0,
				   "the group's count is what its bands hold");

			// State headers off: the group's rows directly, in state order, marked.
			home.List.set_state_headers(false);
			settle(200);
			const auto plain = home.List.group_panels();
			expect(titles_of(plain) == names, "the same groups in the same order");
			auto* const rows = home.List.rows_view(*home.List.group_panels().front());
			expect(rows != nullptr && rows->isVisible()
					   && home.List.band_panel(*home.List.group_panels().front(), qt::home_group::needs_you) == nullptr,
				   "the rows sit directly in the group and no band is shown");
			expect(marked_pixels(*home.List.group_panels().front(), *rows) > 0,
				   "those rows carry their state marks instead");
		}

		// Recently viewed is the last neutral panel in every grouping.
		auto* const recent = new qt::home_recent_view{ };
		recent->addItem(recent_item(QStringLiteral("octo-org/app#99"), QStringLiteral("A title"), false, QStringLiteral("Tue")));
		home.List.adopt_recent(*recent);
		home.List.set_recent(1, QStringLiteral("1 viewed"));
		for (const auto grouping : { application::home_grouping::none,
									 application::home_grouping::organization,
									 application::home_grouping::repository,
									 application::home_grouping::account }) {
			home.List.set_grouping(grouping);
			settle(150);
			const auto panels = home.List.group_panels();
			expect(home.List.recent_panel()->isVisible()
					   && (panels.empty() || home.List.recent_panel()->y() > panels.back()->y()),
				   "Recently viewed is the last panel, in every grouping");
		}
	}

	// A7 and Decisions (d): the account chip, the group that carries it instead,
	// and the single-account profile that shows none.
	void test_account_chips_follow_the_account_count_and_the_group()
	{
		home_fixture home;
		home.Source.apply(two_account_queue());
		home.List.set_queue_status(true, true);
		home.List.show();
		settle(300);

		expect(home.List.account_chips(), "the queue names two accounts, so a chip is drawn somewhere");
		const auto chip = [](const QAbstractItemView& aView, const int aRow) {
			return aView.model()->index(aRow, 0).data(qt::home_row_role::account_chip).toString();
		};
		auto* const needs = home.List.state_view(qt::home_group::needs_you);
		expect(chip(*needs, 0) == QStringLiteral("octo-reviewer")
				   && chip(*needs, 1) == QStringLiteral("octo-enterprise on ghe.example.invalid"),
			   "with no grouping each row carries its account, and the host only where it differs");

		home.List.set_grouping(application::home_grouping::organization);
		settle(200);
		auto* const group = home.List.group_panel(QStringLiteral("octo-org"));
		auto* const band  = home.List.band_panel(*group, qt::home_group::needs_you);
		expect(group != nullptr && band != nullptr && chip(*home.List.rows_view(*band), 0).isEmpty()
				   && group->trailing() == QStringLiteral("octo-reviewer") && group->trailing_is_chip(),
			   "a single-account group carries the chip in its header and its rows drop it");
		auto* const enterprise = home.List.group_panel(QStringLiteral("acme-tools"));
		expect(enterprise != nullptr
				   && enterprise->trailing() == QStringLiteral("octo-enterprise on ghe.example.invalid"),
			   "the enterprise group's own chip says the host it is on");
		// Decisions (d): an owner that is one of the accounts' logins is round.
		expect(group->avatar_name() == QStringLiteral("octo-org") && !group->avatar_round(),
			   "an owner that is not one of the logins gets a square tile");
		expect(home.List.group_panel(QStringLiteral("octo-reviewer"))->avatar_round(),
			   "an owner that is one of the logins gets a round tile");

		home.List.set_grouping(application::home_grouping::account);
		settle(200);
		auto* const account = home.List.group_panel(QStringLiteral("octo-reviewer"));
		expect(account != nullptr && account->avatar_round()
				   && account->trailing() == QStringLiteral("github.com") && !account->trailing_is_chip(),
			   "an account group is round and states its host at the header's right");
		expect(chip(*home.List.rows_view(*home.List.band_panel(*account, qt::home_group::needs_you)), 0).isEmpty(),
			   "under Account grouping no row carries a chip");
		expect(account->header()->accessibleName().contains(QStringLiteral("github.com")),
			   "what the header carries at its right is announced with it");

		// Board H6 and [[BPR-0783]] review finding: a group holding two accounts
		// keeps the chip on each row, and carries none in its header.
		home_fixture mixed;
		mixed.Source.apply(mixed_account_queue());
		mixed.List.set_queue_status(true, true);
		mixed.List.set_grouping(application::home_grouping::organization);
		mixed.List.set_state_headers(false);
		mixed.List.show();
		settle(250);
		auto* const mixedGroup = mixed.List.group_panel(QStringLiteral("octo-org"));
		expect(mixedGroup != nullptr && mixedGroup->trailing().isEmpty(),
			   "a group of two accounts carries no chip in its header");
		auto* const mixedRows = mixed.List.rows_view(*mixedGroup);
		expect(mixedRows != nullptr && mixedRows->model()->rowCount() == 2,
			   "both accounts' rows are in the one group");
		expect(!chip(*mixedRows, 0).isEmpty() && !chip(*mixedRows, 1).isEmpty()
				   && chip(*mixedRows, 0) != chip(*mixedRows, 1),
			   "and each row keeps its own account chip");

		// One account: no chip anywhere, whatever the grouping.
		home_fixture single;
		single.Source.apply(one_account_queue());
		single.List.set_queue_status(true, true);
		single.List.show();
		settle(200);
		expect(!single.List.account_chips(), "one account is nothing to tell apart");
		for (const auto grouping : { application::home_grouping::none, application::home_grouping::organization }) {
			single.List.set_grouping(grouping);
			settle(150);
			for (auto* const view : { single.List.state_view(qt::home_group::needs_you) }) {
				if (grouping == application::home_grouping::none) {
					expect(chip(*view, 0).isEmpty(), "a single-account profile shows no chip on its rows");
				}
			}
			if (grouping == application::home_grouping::organization) {
				expect(single.List.group_panel(QStringLiteral("octo-org"))->trailing().isEmpty(),
					   "and none in a group header either");
			}
		}
	}

	// A7: a band collapses on its own and sticks under its group's header.
	void test_a_band_collapses_and_sticks_under_its_group_header()
	{
		home_fixture home;
		auto         items = std::vector<core::queue_item>{ };
		for (auto number = std::int64_t{ 1 }; number <= 24; ++number) {
			items.push_back(owned_item(number, bucket::initial_review_requested, default_host, default_login, "octo-org/app"));
		}
		for (auto number = std::int64_t{ 25 }; number <= 30; ++number) {
			items.push_back(owned_item(number, bucket::quiet, default_host, default_login, "octo-org/app"));
		}
		home.Source.apply(core::ordered_queue(items));
		home.List.set_queue_status(true, true);
		home.List.set_grouping(application::home_grouping::organization);
		home.List.resize(600, 400);
		home.List.show();
		settle(400);

		auto* const group = home.List.group_panel(QStringLiteral("octo-org"));
		auto* const band  = home.List.band_panel(*group, qt::home_group::needs_you);
		expect(group != nullptr && band != nullptr && band->count() == 24, "the band holds its state's rows");
		auto* const bar = home.List.verticalScrollBar();
		bar->setValue(band->mapTo(home.List.widget(), QPoint{ }).y() + 200);
		settle(100);
		expect(group->header()->mapTo(home.List.viewport(), QPoint{ }).y() == 0,
			   "the group's header sticks to the top of the page");
		expect(band->header()->mapTo(home.List.viewport(), QPoint{ }).y() == group->header()->height(),
			   "the band in view sticks directly under its group's header");

		bar->setValue(0);
		settle(100);
		send_key(*band->header(), Qt::Key_Left);
		settle(100);
		expect(band->collapsed() && !home.List.rows_view(*band) && group->count() == 30,
			   "the band collapses on its own and the group still counts every row");
		expect(band->header()->accessibleName() == QStringLiteral("Needs me, 24"),
			   "a collapsed band keeps its name and count");
		send_key(*band->header(), Qt::Key_Right);
		settle(100);
		expect(!band->collapsed(), "Right expands it again");
	}

	// Decisions (a): a group's collapse lives as long as the window and survives a
	// regroup, and is never part of the view the window would have persisted.
	void test_group_collapse_survives_a_regroup_round_trip()
	{
		home_fixture home;
		home.Source.apply(two_account_queue());
		home.List.set_queue_status(true, true);
		home.List.set_grouping(application::home_grouping::organization);
		home.List.show();
		settle(300);

		auto* const group = home.List.group_panel(QStringLiteral("octo-org"));
		auto* const band  = home.List.band_panel(*group, qt::home_group::following);
		expect(group != nullptr && band != nullptr, "the group and its Following band are shown");
		group->set_collapsed(true);
		band->set_collapsed(true);
		settle(100);
		expect(home.List.collapsed_panels() == 0U,
			   "a group's collapse is not one of the panels the window remembers");

		home.List.set_grouping(application::home_grouping::none);
		settle(200);
		expect(home.List.group_panels().empty(), "the groups are retired with the grouping");
		home.List.set_grouping(application::home_grouping::organization);
		settle(200);
		auto* const again = home.List.group_panel(QStringLiteral("octo-org"));
		expect(again != nullptr && again->collapsed(), "the same owner is still collapsed after a regroup");
		again->set_collapsed(false);
		settle(100);
		expect(home.List.band_panel(*again, qt::home_group::following)->collapsed(),
			   "and so is the band inside it");

		// [[BPR-0783]] review finding. An owner and a login can be the same word
		// on one host — "octo-reviewer" is both here — and they are not the same
		// group, so one's collapse is not the other's memory.
		auto* const owner = home.List.group_panel(QStringLiteral("octo-reviewer"));
		expect(owner != nullptr, "the owner group named like the login is shown");
		owner->set_collapsed(true);
		settle(50);
		home.List.set_grouping(application::home_grouping::account);
		settle(200);
		auto* const account = home.List.group_panel(QStringLiteral("octo-reviewer"));
		// Two claims, asserted apart: a bundled one cannot say whether the group
		// was missing or merely collapsed.
		expect(account != nullptr, "the account group named like the owner is shown");
		expect(!account->collapsed(),
			   "the account of the same name is not collapsed by the owner's memory");
		account->set_collapsed(true);
		settle(50);
		home.List.set_grouping(application::home_grouping::organization);
		settle(200);
		expect(home.List.group_panel(QStringLiteral("octo-reviewer"))->collapsed(),
			   "and each keeps its own collapse across the round trip");

		// A publication that keeps the same keys keeps the collapse too. The round
		// trip rebuilt the panels, so the group is found again rather than held.
		auto* const firstOwner = home.List.group_panel(QStringLiteral("octo-org"));
		expect(firstOwner != nullptr, "the first owner is shown again");
		firstOwner->set_collapsed(true);
		settle(50);
		home.Source.apply(two_account_queue());
		home.List.refresh();
		settle(150);
		expect(home.List.group_panel(QStringLiteral("octo-org"))->collapsed(),
			   "a queue publication does not reopen what the reviewer closed");
	}

	// [[BPR-0783]] Decisions (a), settled by the owner on 2026-09-15: a group's
	// collapse is deliberately not remembered across a restart. It must therefore
	// never reach the view the window persists -- a group key names an owner, a
	// repository or an account -- and a window given its remembered view back
	// must show its groups expanded.
	void test_a_group_collapse_never_reaches_the_remembered_view()
	{
		home_fixture home;
		auto         views = 0;
		home.Source.apply(two_account_queue());
		home.List.set_queue_status(true, true);
		home.List.set_view_observer([&views] { ++views; });
		home.List.show();
		settle(300);

		// A panel that carries no name is remembered, and reports itself.
		home.List.state_panel(qt::home_group::following)->set_collapsed(true);
		settle(100);
		expect(views == 1 && home.List.collapsed_panels() == application::home_collapsed_panel::following,
			   "collapsing a state panel reaches the view the window remembers");

		home.List.set_grouping(application::home_grouping::organization);
		settle(200);
		const auto  grouped = views;
		auto* const group   = home.List.group_panel(QStringLiteral("octo-org"));
		expect(group != nullptr, "the owner group is shown");
		group->set_collapsed(true);
		auto* const band = home.List.band_panel(*group, qt::home_group::following);
		expect(band != nullptr, "its Following band is shown");
		band->set_collapsed(true);
		settle(150);
		expect(views == grouped,
			   "collapsing a group or its band dispatches nothing, so no owner, repository or account name travels to the store");
		expect(home.List.collapsed_panels() == application::home_collapsed_panel::following,
			   "and the panels the window remembers are unchanged by it");

		// What a restart restores is those bits alone.
		home_fixture restarted;
		restarted.Source.apply(two_account_queue());
		restarted.List.set_queue_status(true, true);
		restarted.List.set_collapsed_panels(home.List.collapsed_panels());
		restarted.List.set_grouping(application::home_grouping::organization);
		restarted.List.show();
		settle(300);
		auto* const restored = restarted.List.group_panel(QStringLiteral("octo-org"));
		expect(restored != nullptr && !restored->collapsed(),
			   "a restarted window shows its groups expanded, as the owner chose");
		expect(!restarted.List.band_panel(*restored, qt::home_group::following)->collapsed(),
			   "and the bands inside them expanded");
	}

	// A7 and A8: the keyboard walks group headers, band headers and rows in
	// reading order, with one current row across every view.
	void test_the_keyboard_walks_groups_bands_and_rows()
	{
		home_fixture home;
		home.Source.apply(two_account_queue());
		home.List.set_queue_status(true, true);
		home.List.set_grouping(application::home_grouping::organization);
		home.List.show();
		home.List.activateWindow();
		settle(300);

		auto* const group  = home.List.group_panel(QStringLiteral("octo-org"));
		auto* const needs  = home.List.band_panel(*group, qt::home_group::needs_you);
		auto* const follow = home.List.band_panel(*group, qt::home_group::following);
		const auto  chain  = home.List.focus_chain();
		expect(chain.size() > 4U && chain[0] == group->header() && chain[1] == needs->header()
				   && chain[2] == home.List.rows_view(*needs) && chain[3] == follow->header()
				   && chain[4] == home.List.rows_view(*follow),
			   "the chain is the group's header, then each band's header and its rows, in reading order");

		group->header()->setFocus(Qt::OtherFocusReason);
		send_key(*group->header(), Qt::Key_Down);
		expect(QApplication::focusWidget() == needs->header(), "Down from a group header reaches its first band");
		send_key(*needs->header(), Qt::Key_Down);
		expect(QApplication::focusWidget() == home.List.rows_view(*needs), "Down again reaches that band's rows");
		auto* const rows = home.List.rows_view(*needs);
		send_key(*rows, Qt::Key_Up);
		expect(QApplication::focusWidget() == needs->header(), "Up from the first row returns to the band's header");
		send_key(*needs->header(), Qt::Key_Up);
		expect(QApplication::focusWidget() == group->header(), "Up again returns to the group's header");

		// One current row on the page, across groups.
		auto* const other     = home.List.group_panel(QStringLiteral("acme-tools"));
		auto* const otherRows = home.List.rows_view(*home.List.band_panel(*other, qt::home_group::needs_you));
		rows->setCurrentIndex(rows->model()->index(0, 0));
		expect(home.List.current_view() == rows && home.List.current_queue_item()->Ref.Number == 101,
			   "a row made current in one group is the page's current row");
		otherRows->setCurrentIndex(otherRows->model()->index(0, 0));
		QApplication::processEvents();
		expect(home.List.current_view() == otherRows && !rows->currentIndex().isValid(),
			   "making a row current in another group clears the first");

		// Selecting an identity expands the band and the group that hold it.
		group->set_collapsed(true);
		needs->set_collapsed(true);
		settle(100);
		expect(home.List.select(core::pull_request_ref{ .Host = default_host, .Account = default_login, .Repository = "octo-org/app", .Number = 101 }, true)
				   && !group->collapsed() && !needs->collapsed(),
			   "focusing a row expands the group and the band that hold it");
	}

	// [[BPR-1364]]. The owner, 2026-10-09: "I don't like a single click on
	// home card opening the pr." In every arrangement a single click on a row
	// only selects it; a double-click or Return opens it.
	void test_a_single_click_selects_and_a_double_click_opens()
	{
		home_fixture home;
		home.Source.apply(two_account_queue());
		home.List.set_queue_status(true, true);
		auto opened = std::vector<std::int64_t>{ };
		home.List.set_open_dispatch([&opened](const core::queue_item& aItem) { opened.push_back(aItem.Ref.Number); });
		home.List.show();
		home.List.activateWindow();
		settle(300);

		const auto arrangements = std::vector{
			std::tuple{ application::home_grouping::none, true, "state panels" },
			std::tuple{ application::home_grouping::none, false, "the one card" },
			std::tuple{ application::home_grouping::organization, true, "a group's state band" },
			std::tuple{ application::home_grouping::organization, false, "a group's rows" },
		};
		for (const auto& [grouping, headers, name] : arrangements) {
			const auto where = std::string{ " (" } + name + ")";
			home.List.set_grouping(grouping);
			home.List.set_state_headers(headers);
			settle(200);
			auto* view = static_cast<QAbstractItemView*>(nullptr);
			if (grouping == application::home_grouping::none) {
				view = headers ? home.List.state_view(qt::home_group::needs_you) : home.List.flat_view();
			} else {
				auto* const group = home.List.group_panel(QStringLiteral("octo-org"));
				expect(group != nullptr, "the octo-org group is shown" + where);
				view = headers ? home.List.rows_view(*home.List.band_panel(*group, qt::home_group::needs_you)) : home.List.rows_view(*group);
			}
			expect(view != nullptr && view->isVisible(), "the rows are shown" + where);
			expect(view->accessibleDescription() == QStringLiteral("Double-click or press Return to open"),
				   "the rows say how they open, for keyboard and screen-reader users" + where);
			const auto row = view->model()->index(0, 0);
			expect(row.data(qt::home_row_role::identity).toString() == QStringLiteral("octo-org/app#101"), "the first row is #101" + where);
			view->selectionModel()->clear();
			view->clearFocus();
			opened.clear();
			QApplication::processEvents();

			press_row(*view, row, 1);
			expect(opened.empty(), "a single click on a row opens nothing" + where);
			expect(view->currentIndex() == row, "a single click makes the row current" + where);
			expect(view->selectionModel()->isSelected(row), "a single click highlights the row" + where);
			expect(view->hasFocus(), "a single click gives the row's list the keyboard" + where);
			expect(home.List.current_queue_item().has_value() && home.List.current_queue_item()->Ref.Number == 101,
				   "the clicked row is the page's current row" + where);

			press_row(*view, row, 2);
			expect(opened == std::vector<std::int64_t>{ 101 }, "a double-click opens the row once" + where);

			send_key(*view, Qt::Key_Return);
			expect(opened == std::vector<std::int64_t>{ 101, 101 }, "Return opens the current row" + where);
		}

		// A click leaves the keyboard on the row it selected, so changing the
		// arrangement retires the list that holds it. The keyboard stays on
		// that pull request's row, and the open row's Cancel, which lives in
		// that list, survives it.
		home.List.set_open(qt::home_row_open{ .Identity = QStringLiteral("octo-org/app#101"),
											  .Phase    = qt::home_row_open_phase::waiting,
											  .Line     = QStringLiteral("Opening #101: asking GitHub for the pull request…"),
											  .Action   = QStringLiteral("Cancel") });
		settle(50);
		auto* const cancel = home.List.open_action();
		expect(cancel != nullptr && cancel->isVisible() && home.List.current_view()->hasFocus(),
			   "a group's row carries Cancel while the keyboard is on it");
		home.List.set_grouping(application::home_grouping::none);
		settle(200);
		expect(home.List.current_view() == home.List.flat_view() && home.List.current_view()->hasFocus()
				   && home.List.current_queue_item()->Ref.Number == 101,
			   "regrouping keeps the keyboard on the same pull request's row");
		expect(home.List.open_action() == cancel && cancel->isVisible() && cancel->parentWidget() == home.List.flat_view()->viewport(),
			   "the open row's Cancel survives its list and moves to the row's new one");
		home.List.set_state_headers(true);
		settle(200);
		expect(home.List.current_view() == home.List.state_view(qt::home_group::needs_you) && home.List.current_view()->hasFocus(),
			   "turning State headers on keeps the keyboard on the row too");
		home.List.set_open(qt::home_row_open{ });
		settle(50);

		// Where the style asks for single-click activation (KDE), a single
		// click still only selects.
		auto* const view   = home.List.state_view(qt::home_group::needs_you);
		auto        style  = single_click_style{ };
		auto        events = 0;
		view->setStyle(&style);
		const auto activations = QObject::connect(view, &QAbstractItemView::activated, view, [&events] { ++events; });
		opened.clear();
		const auto row = view->model()->index(0, 0);
		press_row(*view, row, 1);
		expect(events == 1, "the style makes a single click an activation");
		expect(opened.empty() && view->currentIndex() == row, "and that single click still only selects the row");
		press_row(*view, row, 2);
		expect(opened == std::vector<std::int64_t>{ 101 }, "a double-click opens it under that style too");
		QObject::disconnect(activations);
		view->setStyle(nullptr);
	}

	/// [[BPR-1407]]. A row's size and history share one right-aligned column
	/// that the hover action, the Reviewing tag and the reason never overlap.
	void test_change_facts_keep_their_own_column()
	{
		constexpr auto now = std::int64_t{ 1'760'000'000 };
		home_fixture   home;
		auto           queue  = one_account_queue();
		queue[0].Facts.Title  = "A title long enough that the row's own text has to wrap before it could reach the column of facts";
		queue[0].Facts.Change = { .OpenedAt = now - (6 * 86'400), .Additions = 1204U, .Deletions = 318U, .Comments = 14U };
		queue[1].Facts.Change = { .Additions = 18U, .Deletions = 3U };
		home.Source.apply(queue);
		home.Source.set_now(now);
		home.Source.set_reviewing(queue[0].Ref);
		home.List.set_queue_status(true, true);
		home.List.show();
		settle(200);

		auto* const view     = home.List.state_view(qt::home_group::needs_you);
		auto* const delegate = home.List.row_delegate();
		const auto  first    = view->model()->index(0, 0);
		const auto  second   = view->model()->index(1, 0);
		const auto  locale   = QLocale{ };
		expect(first.data(qt::home_row_role::size).toString() == QStringLiteral("+%1 / \u2212%2").arg(locale.toString(1204), locale.toString(318))
				   && first.data(qt::home_row_role::age).toString() == QStringLiteral("6d") && first.data(qt::home_row_role::comments).toString() == QStringLiteral("14"),
			   "the first row says its size, age and comments");
		const auto facts = delegate->facts_rect(option_for(*view, first), first);
		expect(!facts.isEmpty() && view->visualRect(first).contains(facts), "the facts are drawn inside their row");
		expect(!facts.intersects(qt::home_row_delegate::action_rect(option_for(*view, first), first)), "the hover action keeps its own place and never covers a figure");
		expect(!facts.intersects(delegate->reviewing_rect(option_for(*view, first), first)) && !facts.intersects(delegate->reason_rect(option_for(*view, first), first)),
			   "neither the Reviewing tag nor the reason word reaches the facts");
		const auto sizeOnly = delegate->facts_rect(option_for(*view, second), second);
		expect(!sizeOnly.isEmpty() && sizeOnly.right() == facts.right(), "rows share the column's right edge, so their figures line up");
		expect(second.data(qt::home_row_role::age).toString().isEmpty() && second.data(qt::home_row_role::comments).toString().isEmpty(), "a row without an age or a count has neither");
		// [[BPR-1505]]. Cancel and Retry belong before the facts, whose
		// horizontal alignment must survive each opening state.
		const auto tag    = delegate->reviewing_rect(option_for(*view, first), first);
		const auto states = std::array{
			std::pair{ qt::home_row_open_phase::opening, QString{ } },
			std::pair{ qt::home_row_open_phase::waiting, QStringLiteral("Cancel") },
			std::pair{ qt::home_row_open_phase::failed, QStringLiteral("Retry") },
		};
		for (const auto& [phase, label] : states) {
			delegate->set_open({ first.data(qt::home_row_role::identity).toString(), phase, QStringLiteral("Opening this pull request"), label });
			const auto during = delegate->facts_rect(option_for(*view, first), first);
			expect(during.left() == facts.left() && during.right() == facts.right(), "opening, Cancel and Retry keep the changes and history aligned");
			expect(delegate->reviewing_rect(option_for(*view, first), first).left() == tag.left(), "opening leaves the Reviewing tag aligned too");
			const auto action = delegate->open_action_rect(option_for(*view, first), first);
			expect(action.isEmpty() == label.isEmpty(), "Cancel and Retry have measured action geometry");
			expect(action.isEmpty() || (action.right() < during.left() && !action.intersects(delegate->reviewing_rect(option_for(*view, first), first))),
				   "Cancel and Retry sit left of the facts and never overlap the Reviewing tag");
		}
		delegate->set_open({ });
		expect(delegate->facts_rect(option_for(*view, first), first) == facts, "ending the opening restores the complete idle facts geometry");
		auto* const mine = home.List.state_view(qt::home_group::my_pull_requests);
		expect(delegate->facts_rect(option_for(*mine, mine->model()->index(0, 0)), mine->model()->index(0, 0)).isEmpty(), "a row whose change was not read draws nothing");
	}

	/// [[BPR-1365]]. The review this window has open is drawn quietly and never
	/// with the selection's fill, in every theme, in the queue and in Recently
	/// viewed; and the queue shows its selection only while it has Home's
	/// keyboard. `aRenderPath` names each theme's render.
	void test_the_open_review_is_quiet_and_never_the_selection(const QString& aRenderPath)
	{
		constexpr auto modes = std::array{
			std::pair{ core::appearance_mode::light, "light" },
			std::pair{ core::appearance_mode::dark, "dark" },
			std::pair{ core::appearance_mode::high_contrast_light, "high-contrast-light" },
			std::pair{ core::appearance_mode::high_contrast_dark, "high-contrast-dark" },
		};
		for (const auto& [mode, name] : modes) {
			const auto where   = std::string{ " (" } + name + ")";
			auto       profile = qt::appearance_profile{ };
			profile.select_mode(mode);
			const auto palette = profile.derive(QApplication::palette());
			expect(!close_to(palette.color(QPalette::Active, QPalette::AlternateBase), palette.color(QPalette::Active, QPalette::Highlight), 24),
				   "the theme's quiet tint is not its selection" + where);

			home_fixture home;
			home.List.setPalette(palette);
			auto queue           = one_account_queue();
			queue[0].Facts.Draft = true;
			queue[0].Facts.Title = "A complete draft title with enough literal text to wrap across the narrow Home row without elision";
			queue[1].Facts.Draft = false;
			queue[1].Ref.Account = "second-account";
			home.Source.apply(queue);
			home.Source.set_reviewing(queue[0].Ref);
			home.List.set_queue_status(true, true);
			auto* const recent = new qt::home_recent_view{ };
			auto* const open   = recent_item(QStringLiteral("octo-org/app#101"), QStringLiteral("A title"), true, QStringLiteral("6:59 AM"));
			open->setData(qt::home_row_role::reviewing, true);
			open->setData(qt::home_row_role::draft, true);
			recent->addItem(open);
			recent->addItem(recent_item(QStringLiteral("microsoft/terminal#42"), QStringLiteral("Fix the conpty resize race"), false, QStringLiteral("Tue")));
			home.List.adopt_recent(*recent);
			home.List.set_recent(2, QStringLiteral("2 viewed; 1 saved reading"));
			home.List.show();
			home.List.activateWindow();
			settle(200);

			auto* const view     = home.List.state_view(qt::home_group::needs_you);
			auto* const delegate = home.List.row_delegate();
			const auto  first    = view->model()->index(0, 0);
			expect(first.data(qt::home_row_role::reviewing).toBool()
					   && first.data(Qt::AccessibleDescriptionRole).toString().startsWith(qt::home_reviewing_description()),
				   "the open pull request's queue row is the one this window is reviewing, and says so" + where);
			auto* const waiting = home.List.state_view(qt::home_group::waiting_on_others);
			expect(!waiting->model()->index(0, 0).data(qt::home_row_role::reviewing).toBool()
					   && delegate->reviewing_rect(option_for(*waiting, waiting->model()->index(0, 0)), waiting->model()->index(0, 0)).isEmpty(),
				   "another row is not, and has no tag" + where);
			const auto tag = delegate->reviewing_rect(option_for(*view, first), first);
			expect(!tag.isEmpty() && view->visualRect(first).contains(tag), "the open review's row carries its tag inside the row" + where);
			expect(first.data(qt::home_row_role::draft).toBool() && first.data(Qt::AccessibleTextRole).toString().startsWith(QStringLiteral("Draft pull request; octo-org/app#101")), "draft status precedes literal identity in accessible text" + where);
			const auto chip = delegate->draft_rect(option_for(*view, first), first);
			expect(view->visualRect(first).contains(chip) && chip.width() >= QFontMetrics{ view->font() }.horizontalAdvance(QStringLiteral("Draft")) + 12 && !chip.intersects(tag), "the complete leading chip and Reviewing tag are distinct and inside the row" + where);
			expect(!first.data(qt::home_row_role::account_chip).toString().isEmpty(), "draft shares its row with the existing account chip" + where);
			for (const auto phase : { qt::home_row_open_phase::opening, qt::home_row_open_phase::waiting, qt::home_row_open_phase::failed }) {
				delegate->set_open({ first.data(qt::home_row_role::identity).toString(), phase, QStringLiteral("Opening this pull request"), QStringLiteral("Cancel") });
				expect(delegate->draft_rect(option_for(*view, first), first).size() == chip.size(), "opening and failure keep the complete leading status" + where);
			}
			delegate->set_open({ });

			const auto recentIndex = recent->model()->index(0, 0);
			expect(!delegate->draft_rect(option_for(*recent, recentIndex), recentIndex).isEmpty(), "recent-only status uses the same leading chip geometry" + where);
			expect(delegate->draft_rect(option_for(*waiting, waiting->model()->index(0, 0)), waiting->model()->index(0, 0)).isEmpty(), "unknown rows do not invent a Draft chip" + where);

			const auto ground = [&home](const QAbstractItemView& aView, const QModelIndex& aIndex) {
				const auto image = home.List.grab().toImage();
				const auto ratio = image.devicePixelRatio();
				const auto rect  = aView.visualRect(aIndex);
				const auto at    = aView.viewport()->mapTo(&home.List, QPoint{ rect.left() + 6, rect.center().y() });
				return image.pixelColor(static_cast<int>(at.x() * ratio), static_cast<int>(at.y() * ratio));
			};
			const auto alternate = palette.color(QPalette::Active, QPalette::AlternateBase);
			const auto highlight = palette.color(QPalette::Active, QPalette::Highlight);
			expect(close_to(ground(*view, first), alternate, 6), "the open review's queue row is on the quiet tint" + where);
			expect(close_to(ground(*recent, recent->model()->index(0, 0)), alternate, 6), "the open review's Recently viewed row is on the quiet tint" + where);
			expect(!close_to(ground(*recent, recent->model()->index(1, 0)), alternate, 2), "a row that is not open is not tinted" + where);

			// The tag's word is drawn in the row's ink: some of its pixels are
			// far from the tint it sits on.
			const auto image  = home.List.grab().toImage();
			const auto ratio  = image.devicePixelRatio();
			const auto origin = view->viewport()->mapTo(&home.List, tag.topLeft());
			auto       inked  = 0;
			for (auto y = static_cast<int>(origin.y() * ratio); y < static_cast<int>((origin.y() + tag.height()) * ratio); ++y) {
				for (auto x = static_cast<int>(origin.x() * ratio); x < static_cast<int>((origin.x() + tag.width()) * ratio); ++x) {
					inked += close_to(image.pixelColor(x, y), alternate, 48) ? 0 : 1;
				}
			}
			expect(inked > 20, "the tag's word and outline are drawn" + where);

			// Selected, the open review is the style's selection, in its colour.
			view->setFocus(Qt::OtherFocusReason);
			view->setCurrentIndex(first);
			settle(30);
			expect(close_to(ground(*view, first), highlight, 24), "a selected row, open or not, is the native selection" + where);
			if (!aRenderPath.isEmpty()) {
				auto path = QString{ aRenderPath }.replace(QStringLiteral(".png"), QStringLiteral("-reviewing-%1.png").arg(QString::fromLatin1(name)));
				expect(home.List.grab().save(path), "the reviewing render is saved" + where);
			}

			// The queue keeps its current row and shows no selection while
			// another of Home's lists has the keyboard, whatever makes a row
			// current meanwhile.
			home.List.set_selection_shown(false);
			expect(view->currentIndex() == first && !view->selectionModel()->hasSelection(), "hidden, the queue keeps its current row and selects nothing" + where);
			expect(home.List.select(queue[1].Ref, false), "a restoring selection finds the row");
			expect(std::ranges::none_of(std::array{ view, waiting }, [](const QAbstractItemView* aView) { return aView->selectionModel()->hasSelection(); }),
				   "a row made current while hidden is not shown selected" + where);
			home.List.set_selection_shown(true);
			expect(home.List.current_queue_item().has_value() && home.List.current_queue_item()->Ref == queue[1].Ref
					   && home.List.current_view()->selectionModel()->isSelected(home.List.current_view()->currentIndex()),
				   "shown again, the current row is selected" + where);
		}
	}

	// A6 and A7: an empty ready queue is one sentence whatever the view asks for.
	void test_an_empty_queue_is_one_sentence_in_every_grouping()
	{
		home_fixture home;
		home.List.set_queue_status(true, true);
		home.List.show();
		settle(200);
		for (const auto grouping : { application::home_grouping::none,
									 application::home_grouping::organization,
									 application::home_grouping::repository,
									 application::home_grouping::account }) {
			for (const auto headers : { true, false }) {
				home.List.set_grouping(grouping);
				home.List.set_state_headers(headers);
				settle(120);
				expect(home.List.state_panel(qt::home_group::needs_you)->isVisible()
						   && home.List.empty_sentence()->isVisible()
						   && !home.List.state_panel(qt::home_group::waiting_on_others)->isVisible()
						   && !home.List.state_panel(qt::home_group::following)->isVisible()
						   && !home.List.flat_panel()->isVisible()
						   && home.List.group_panels().empty(),
					   "an empty queue has nothing to group: one sentence, and no box at all");
			}
		}
	}

	// Incremental: a grouped large queue stays linear and resets nothing.
	void test_a_large_grouped_queue_stays_incremental()
	{
		home_fixture home;
		auto         resets  = 0;
		auto         context = QObject{ };
		QObject::connect(&home.Source, &QAbstractItemModel::modelReset, &context, [&resets] { ++resets; });
		auto items = std::vector<core::queue_item>{ };
		items.reserve(2'048U);
		for (auto number = std::int64_t{ 1 }; number <= 2'048; ++number) {
			items.push_back(owned_item(number,
									   number % 2 == 0 ? bucket::initial_review_requested : bucket::quiet,
									   default_host,
									   default_login,
									   number % 4 == 0 ? "octo-org/app" : "acme-tools/build"));
		}
		home.Source.apply(core::ordered_queue(items));
		home.List.set_queue_status(true, true);
		home.List.set_grouping(application::home_grouping::repository);
		home.List.set_state_headers(false);
		home.List.resize(700, 500);
		home.List.show();
		settle(600);

		const auto panels = home.List.group_panels();
		auto       total  = 0;
		for (const auto* const panel : panels) {
			total += panel->count();
		}
		expect(panels.size() == 2U && total == 2'048, "every row is in exactly one group");
		expect(resets == 0, "the decided queue is never reset to arrange it");
		expect(identities_of(*home.List.rows_view(*panels.front())).size() == panels.front()->count(),
			   "the group's rows are the rows it counted");
	}

	// A row under a group says only what its group header does not: the number
	// under a repository, "name#123" under an owner, the whole identity otherwise.
	void test_a_grouped_row_drops_what_its_group_names()
	{
		home_fixture home;
		home.Source.apply(core::ordered_queue(std::vector{
			owned_item(7, bucket::initial_review_requested, default_host, default_login, "octo-org/app"),
		}));
		home.List.set_queue_status(true, true);
		home.List.set_state_headers(false);
		home.List.resize(700, 500);
		home.List.show();
		settle(300);
		const auto shown = [&home] {
			const auto  panels = home.List.group_panels();
			auto* const view   = panels.empty() ? home.List.flat_view() : home.List.rows_view(*panels.front());
			return view->model()->index(0, 0).data(qt::home_row_role::shown_identity);
		};
		const auto none = shown();
		expect(none.toString() == QStringLiteral("octo-org/app#7"), "ungrouped, a row names its whole identity");
		home.List.set_grouping(application::home_grouping::repository);
		settle(300);
		expect(shown().toString() == QStringLiteral("#7"), "under a repository, a row names only its number");
		home.List.set_grouping(application::home_grouping::organization);
		settle(300);
		expect(shown().toString() == QStringLiteral("app#7"), "under an owner, a row names its repository and number");
		home.List.set_grouping(application::home_grouping::account);
		settle(300);
		expect(shown().toString() == QStringLiteral("octo-org/app#7"), "under an account, a row names its whole identity");
	}

	// A7: the chosen sort orders rows inside a state, never across states.
	void test_a_sort_orders_rows_within_a_state()
	{
		home_fixture home;
		home.Source.apply(core::ordered_queue(std::vector{
			owned_item(1, bucket::initial_review_requested, default_host, default_login, "octo-org/zebra"),
			owned_item(2, bucket::changed_since_my_review, default_host, default_login, "octo-org/alpha"),
			owned_item(3, bucket::quiet, default_host, default_login, "octo-org/middle"),
		}));
		home.Filter.apply(application::queue_presentation_preferences{
			.Sort = application::queue_sort_order::repository,
		});
		home.List.set_queue_status(true, true);
		home.List.set_state_headers(false);
		home.List.show();
		settle(300);

		// Sorted by repository alone the queue would read alpha, middle, zebra.
		// State is always the first key, so Needs me comes first and the sort
		// orders only within it.
		const auto expected = QStringList{ QStringLiteral("octo-org/alpha#2"),
										   QStringLiteral("octo-org/zebra#1"),
										   QStringLiteral("octo-org/middle#3") };
		expect(identities_of(*home.List.flat_view()) == expected,
			   "with State headers off the sort orders rows inside a state, not across states");

		home.List.set_grouping(application::home_grouping::organization);
		settle(250);
		auto* const group = home.List.group_panel(QStringLiteral("octo-org"));
		expect(group != nullptr && home.List.rows_view(*group) != nullptr
				   && identities_of(*home.List.rows_view(*group)) == expected,
			   "and the same inside a group");
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

	test_authored_sections_in_every_home_arrangement(renderPath);
	test_my_pull_requests_has_its_own_header_and_rail();
	test_rows_follow_state_order_with_one_anatomy(renderPath);
	test_colour_is_only_the_reason_word_and_the_rail();
	test_rows_wrap_without_elision();
	test_panels_collapse_by_pointer_and_keyboard();
	test_headers_stick_while_their_panel_is_in_view(scrolledPath);
	test_bands_stick_under_their_group_header();
	test_empty_states_and_the_all_empty_sentence();
	test_resume_is_a_row_action_on_hover_and_focus();
	test_ignored_is_last_collapsed_and_offers_unignore(renderPath);
	test_a_row_that_changes_state_moves_panels_and_keeps_its_place();
	test_a_wheel_over_rows_scrolls_the_page();
	// [[BPR-0783]]. Group, the state bands, the account chips and the keyboard.
	test_group_none_without_state_headers_is_one_marked_list();
	test_each_grouping_renders_with_state_headers_on_and_off();
	test_account_chips_follow_the_account_count_and_the_group();
	test_a_band_collapses_and_sticks_under_its_group_header();
	test_group_collapse_survives_a_regroup_round_trip();
	test_a_group_collapse_never_reaches_the_remembered_view();
	test_the_keyboard_walks_groups_bands_and_rows();
	test_an_empty_queue_is_one_sentence_in_every_grouping();
	test_a_sort_orders_rows_within_a_state();
	test_a_large_grouped_queue_stays_incremental();
	test_a_grouped_row_drops_what_its_group_names();
	// [[BPR-1364]]. A single click selects; a double-click or Return opens.
	test_a_single_click_selects_and_a_double_click_opens();
	// [[BPR-1365]]. The open review is quiet; Home shows one selection.
	test_change_facts_keep_their_own_column();
	test_the_open_review_is_quiet_and_never_the_selection(renderPath);

	std::cout << "home-list-tests:pass\n";
	return EXIT_SUCCESS;
}
