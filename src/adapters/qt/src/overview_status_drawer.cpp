#include "bpr/qt/overview_status_drawer.hpp"

#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/overview_tour_summary.hpp"
#include "bpr/qt/resize_handle_cursor.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_strip.hpp"

#include "conversation_glyphs.hpp"
#include "drawer_tab_bar.hpp"

#include <QAction>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto clamped(const int aWidth) -> std::uint32_t
		{
			return static_cast<std::uint32_t>(std::clamp(aWidth, static_cast<int>(application::minimum_status_drawer_width),
														 static_cast<int>(application::maximum_status_drawer_width)));
		}

		void follow(QToolButton& aButton, QAction* const aAction)
		{
			aButton.setEnabled(aAction->isEnabled());
			QObject::connect(&aButton, &QToolButton::clicked, aAction, &QAction::trigger);
			QObject::connect(aAction, &QAction::changed, &aButton, [&aButton, aAction] { aButton.setEnabled(aAction->isEnabled()); });
		}

		[[nodiscard]] auto index_of(const overview_drawer_tab aTab) -> int
		{
			return static_cast<int>(aTab);
		}

		[[nodiscard]] auto make_holder(QWidget* const aParent, const QString& aName) -> QWidget*
		{
			auto* const holder = new QWidget{ aParent };
			holder->setObjectName(aName);
			auto* const column = new QVBoxLayout{ holder };
			column->setContentsMargins(0, 0, 0, 0);
			column->setSpacing(0);
			return holder;
		}

		/// [[BPR-1781]]. Each tab's name and what it holds, in tab order.
		[[nodiscard]] auto tab_name(const int aIndex) -> QString
		{
			switch (static_cast<overview_drawer_tab>(aIndex)) {
			case overview_drawer_tab::status:   return QObject::tr("Status");
			case overview_drawer_tab::commits:  return QObject::tr("Commits");
			case overview_drawer_tab::comments: return QObject::tr("Comments");
			case overview_drawer_tab::notes:    return QObject::tr("Notes");
			case overview_drawer_tab::tour:     return QObject::tr("Tour");
			case overview_drawer_tab::agent:    return QObject::tr("Agent");
			}
			return { };
		}

		[[nodiscard]] auto tab_about(const int aIndex) -> QString
		{
			switch (static_cast<overview_drawer_tab>(aIndex)) {
			case overview_drawer_tab::status:   return QObject::tr("Where this pull request stands and your part in its review");
			case overview_drawer_tab::commits:  return QObject::tr("The pull request's commits; show or hide them to filter the files");
			case overview_drawer_tab::comments: return QObject::tr("The pull request's conversation: its comments and reviews, newest last, and a new comment");
			case overview_drawer_tab::notes:    return QObject::tr("Your private notes on this pull request. Nothing here is published.");
			case overview_drawer_tab::tour:     return QObject::tr("The pull request's review tour; write and follow it in Review");
			case overview_drawer_tab::agent:    return QObject::tr("Agent conversations about this pull request. Nothing here is published.");
			}
			return { };
		}
	}

	overview_status_drawer::overview_status_drawer(QWidget* const aContent, QWidget* const aParent)
		: QWidget{ aParent }
		, Content{ aContent }
	{
		setObjectName(QStringLiteral("overviewStatusDrawer"));
		setAccessibleName(QStringLiteral("Status drawer"));
		setAccessibleDescription(QStringLiteral("Where this pull request stands and your part in its review"));
		auto* const row = new QHBoxLayout{ this };
		row->setContentsMargins(0, 0, 0, 0);
		row->setSpacing(0);
		Rail = new workbench_drawer_rail{ QStringLiteral("Status"), QStringLiteral("overviewStatusRail"), this };
		connect(Rail, &QAbstractButton::clicked, this, [this] { toggle(); });
		row->addWidget(Rail);

		Expanded = new QWidget{ this };
		Expanded->setObjectName(QStringLiteral("overviewStatusExpanded"));
		auto* const column = new QVBoxLayout{ Expanded };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(0);
		Header = new drawer_header{ Expanded };
		Header->setObjectName(QStringLiteral("overviewStatusHeader"));
		Header->setAutoFillBackground(true);
		Header->setBackgroundRole(QPalette::Base);
		Tabs = new detail::drawer_tab_bar{ Header };
		Tabs->setObjectName(QStringLiteral("overviewStatusTabs"));
		Tabs->setAccessibleName(QStringLiteral("Status drawer"));
		configure_drawer_tabs(*Tabs);
		Tabs->setUsesScrollButtons(false);
		// RFC-BPR-0089: Commits beside Status. [[BPR-1781]]: then the
		// conversation tabs Review's tray has, in its order.
		for (auto index = 0; index < overview_drawer_tab_count; ++index) {
			Tabs->addTab(tab_name(index));
			Tabs->setAccessibleTabName(index, tab_name(index));
			Tabs->setTabToolTip(index, tab_about(index));
		}
		Tools = new QWidget{ Header };
		Tools->setObjectName(QStringLiteral("overviewStatusTools"));
		auto* const tools = new QHBoxLayout{ Tools };
		tools->setContentsMargins(0, 0, 0, 0);
		tools->setSpacing(1);
		Github = make_drawer_tool(Tools, { }, QStringLiteral("Open on GitHub"), QStringLiteral("overviewStatusOpenOnGithub"));
		Github->hide();
		Hide = make_drawer_tool(Tools, { }, QStringLiteral("Hide the Status drawer"), QStringLiteral("overviewStatusHide"));
		connect(Hide, &QToolButton::clicked, this, [this] { toggle(); });
		tools->addWidget(Github);
		tools->addWidget(Hide);
		Header->set_parts(Tabs, Tools);
		column->addWidget(Header);
		Pages = new QStackedWidget{ Expanded };
		Pages->setObjectName(QStringLiteral("overviewStatusPages"));
		Content->setParent(Pages);
		if (auto* const scroll = qobject_cast<QScrollArea*>(Content); scroll != nullptr && scroll->widget() != nullptr) {
			scroll->widget()->installEventFilter(this);
		}
		Pages->addWidget(Content);
		Commits = new commit_filter_tab{ Pages };
		Pages->addWidget(Commits);
		CommentsHolder = make_holder(Pages, QStringLiteral("overviewCommentsHolder"));
		Pages->addWidget(CommentsHolder);
		NotesHolder = make_holder(Pages, QStringLiteral("overviewNotesHolder"));
		Pages->addWidget(NotesHolder);
		TourSummary = new overview_tour_summary{ Pages };
		Pages->addWidget(TourSummary);
		AgentHolder = make_holder(Pages, QStringLiteral("overviewAgentHolder"));
		Pages->addWidget(AgentHolder);
		set_tour_tab_shown(false);
		connect(Tabs, &QTabBar::currentChanged, this, [this](const int aIndex) {
			Pages->setCurrentIndex(aIndex);
			fit_tabs();
			if (TabObserver && aIndex >= 0) {
				TabObserver(static_cast<overview_drawer_tab>(aIndex));
			}
		});
		Header->installEventFilter(this);
		column->addWidget(Pages, 1);
		row->addWidget(Expanded, 1);
		refresh_icons();
		present();
	}

	void overview_status_drawer::set_tools(QAction* const aOpenOnGithub)
	{
		if (aOpenOnGithub != nullptr) {
			follow(*Github, aOpenOnGithub);
			const auto key = aOpenOnGithub->shortcut().toString(QKeySequence::NativeText);
			Github->setToolTip(key.isEmpty() ? QStringLiteral("Open on GitHub") : QStringLiteral("Open on GitHub (%1)").arg(key));
			Github->show();
		}
	}

	void overview_status_drawer::show_tab(const overview_drawer_tab aTab)
	{
		if (aTab == overview_drawer_tab::tour && !tour_tab_shown()) {
			return;
		}
		if (Drawer.State != core::workbench_drawer_state::expanded) {
			toggle();
		}
		Tabs->setCurrentIndex(index_of(aTab));
	}

	auto overview_status_drawer::current_tab() const -> overview_drawer_tab
	{
		return static_cast<overview_drawer_tab>(std::max(0, Tabs->currentIndex()));
	}

	auto overview_status_drawer::holder(const overview_drawer_tab aTab) const -> QWidget*
	{
		switch (aTab) {
		case overview_drawer_tab::comments: return CommentsHolder;
		case overview_drawer_tab::notes:    return NotesHolder;
		case overview_drawer_tab::agent:    return AgentHolder;
		case overview_drawer_tab::status:
		case overview_drawer_tab::commits:
		case overview_drawer_tab::tour:     return nullptr;
		}
		return nullptr;
	}

	auto overview_status_drawer::tour_summary() const noexcept -> overview_tour_summary*
	{
		return TourSummary;
	}

	void overview_status_drawer::set_tour_tab_shown(const bool aShown)
	{
		const auto tour = index_of(overview_drawer_tab::tour);
		if (Tabs->isTabVisible(tour) == aShown) {
			return;
		}
		if (!aShown && Tabs->currentIndex() == tour) {
			Tabs->setCurrentIndex(index_of(overview_drawer_tab::status));
		}
		Tabs->setTabVisible(tour, aShown);
		fit_tabs();
	}

	auto overview_status_drawer::tour_tab_shown() const -> bool
	{
		return Tabs->isTabVisible(index_of(overview_drawer_tab::tour));
	}

	void overview_status_drawer::set_tab_observer(std::function<void(overview_drawer_tab)> aObserver)
	{
		TabObserver = std::move(aObserver);
	}

	void overview_status_drawer::fit_tabs(const bool aRepaint)
	{
		// [[BPR-1781]]. Six tabs, fitted as Review's tray fits its four
		// ([[BPR-1684]]): every name; the open tab's glyph and name with the
		// others' glyphs; glyphs only. The row takes the first that fits.
		auto* const tabs = static_cast<detail::drawer_tab_bar*>(Tabs);
		enum class face : std::uint8_t { names,
										 open_named,
										 glyphs };
		const auto current = tabs->currentIndex();
		const auto textOf  = [current](const face aFace, const int aIndex) {
			return aFace == face::names || (aFace == face::open_named && aIndex == current) ? tab_name(aIndex) : QString{ };
		};
		auto room = Header->width();
		if (const auto* const grid = qobject_cast<const QGridLayout*>(Header->layout()); grid != nullptr && room > 0) {
			room -= grid->contentsMargins().left() + 2 * std::max(0, grid->horizontalSpacing());
		}
		auto chosen = face::names;
		if (room > 0) {
			for (const auto candidate : { face::names, face::open_named, face::glyphs }) {
				chosen     = candidate;
				auto width = 0;
				for (auto index = 0; index < overview_drawer_tab_count; ++index) {
					if (tabs->isTabVisible(index)) {
						width += tabs->face_width(index, textOf(candidate, index), candidate != face::names);
					}
				}
				if (width <= room) {
					break;
				}
			}
		}
		const auto glyph = [this](const int aIndex) {
			switch (static_cast<overview_drawer_tab>(aIndex)) {
			case overview_drawer_tab::status:   return strip_icon(strip_glyph::review_details, palette());
			case overview_drawer_tab::commits:  return drawer_icon(drawer_glyph::pull_request, palette());
			case overview_drawer_tab::comments: return conversation_icon(conversation_glyph::comment, palette());
			case overview_drawer_tab::notes:    return drawer_icon(drawer_glyph::page, palette());
			case overview_drawer_tab::tour:     return drawer_icon(drawer_glyph::tour, palette());
			case overview_drawer_tab::agent:    return drawer_icon(drawer_glyph::agent, palette());
			}
			return QIcon{ };
		};
		auto reshaped = false;
		for (auto index = 0; index < overview_drawer_tab_count; ++index) {
			const auto text  = textOf(chosen, index);
			const auto shown = chosen != face::names;
			if (aRepaint || tabs->tabIcon(index).isNull() == shown) {
				reshaped = reshaped || tabs->tabIcon(index).isNull() == shown;
				tabs->setTabIcon(index, shown ? glyph(index) : QIcon{ });
			}
			if (tabs->tabText(index) != text) {
				reshaped = true;
				tabs->setTabText(index, text);
			}
			// A tab showing only its glyph says its name first.
			const auto tip = text.isEmpty() ? QStringLiteral("%1\n%2").arg(tab_name(index), tab_about(index)) : tab_about(index);
			if (tabs->tabToolTip(index) != tip) {
				tabs->setTabToolTip(index, tip);
			}
		}
		if (reshaped) {
			tabs->updateGeometry();
		}
	}

	void overview_status_drawer::adopt(const application::workbench_status_drawer& aDrawer)
	{
		if (!application::valid(aDrawer) || Drawer == aDrawer) {
			return;
		}
		Drawer = aDrawer;
		present();
	}

	auto overview_status_drawer::drawer() const noexcept -> const application::workbench_status_drawer&
	{
		return Drawer;
	}

	void overview_status_drawer::set_change_dispatch(change_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void overview_status_drawer::toggle()
	{
		auto next  = Drawer;
		next.State = Drawer.State == core::workbench_drawer_state::expanded ? core::workbench_drawer_state::collapsed
																			: core::workbench_drawer_state::expanded;
		// The gesture shows at once; the host's echo of the same value changes nothing.
		Drawer = next;
		present();
		if (next.State == core::workbench_drawer_state::collapsed) {
			Rail->setFocus(Qt::OtherFocusReason);
		} else {
			Tabs->setFocus(Qt::OtherFocusReason);
		}
		report(next);
	}

	void overview_status_drawer::observe_drag(const int aWidth)
	{
		if (Drawer.State != core::workbench_drawer_state::expanded) {
			return;
		}
		auto next  = Drawer;
		next.Width = clamped(aWidth);
		if (next == Drawer) {
			return;
		}
		Drawer = next;
		report(next);
	}

	auto overview_status_drawer::expanded_width() const noexcept -> int
	{
		return static_cast<int>(Drawer.Width.value_or(application::default_status_drawer_width));
	}

	auto overview_status_drawer::tabs() const noexcept -> QTabBar*
	{
		return Tabs;
	}

	auto overview_status_drawer::header() const noexcept -> drawer_header*
	{
		return Header;
	}

	auto overview_status_drawer::hide_button() const noexcept -> QToolButton*
	{
		return Hide;
	}

	auto overview_status_drawer::rail() const noexcept -> workbench_drawer_rail*
	{
		return Rail;
	}

	auto overview_status_drawer::content() const noexcept -> QWidget*
	{
		return Content;
	}

	auto overview_status_drawer::github_button() const noexcept -> QToolButton*
	{
		return Github;
	}

	auto overview_status_drawer::commits() const noexcept -> commit_filter_tab*
	{
		return Commits;
	}

	void overview_status_drawer::show_commits()
	{
		if (Drawer.State != core::workbench_drawer_state::expanded) {
			toggle();
		}
		Tabs->setCurrentIndex(1);
		Commits->list()->setFocus(Qt::OtherFocusReason);
	}

	auto overview_status_drawer::floor_width() const -> int
	{
		// The narrowest the drawer goes is its bound, or what its content needs
		// beside a native scroll bar if that is wider, so a larger system font
		// never clips a control (what the fixed column used to grow for).
		auto floor = static_cast<int>(application::minimum_status_drawer_width);
		if (const auto* const scroll = qobject_cast<const QScrollArea*>(Content); scroll != nullptr && scroll->widget() != nullptr) {
			const auto bar = std::max(0, scroll->verticalScrollBar()->sizeHint().width() - scroll->style()->pixelMetric(QStyle::PM_ScrollView_ScrollBarOverlap, nullptr, scroll));
			floor          = std::max(floor, scroll->widget()->minimumSizeHint().width() + bar + 2 * scroll->frameWidth());
		}
		return std::min(floor, static_cast<int>(application::maximum_status_drawer_width));
	}

	auto overview_status_drawer::eventFilter(QObject* const aObject, QEvent* const aEvent) -> bool
	{
		if (aObject == Header && aEvent->type() == QEvent::Resize) {
			fit_tabs();
		}
		if (aEvent->type() == QEvent::LayoutRequest && Drawer.State == core::workbench_drawer_state::expanded && minimumWidth() != floor_width()) {
			setMinimumWidth(floor_width());
		}
		return QWidget::eventFilter(aObject, aEvent);
	}

	void overview_status_drawer::showEvent(QShowEvent* const aEvent)
	{
		QWidget::showEvent(aEvent);
		// Placed in its splitter after construction, the drawer takes its
		// width the first time it shows.
		present();
	}

	void overview_status_drawer::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange) {
			refresh_icons();
			fit_tabs(true);
		}
	}

	void overview_status_drawer::present()
	{
		const auto expanded = Drawer.State == core::workbench_drawer_state::expanded;
		Expanded->setVisible(expanded);
		Rail->setVisible(!expanded);
		if (expanded) {
			setMinimumWidth(floor_width());
			setMaximumWidth(static_cast<int>(application::maximum_status_drawer_width));
		} else {
			setFixedWidth(workbench_drawer_rail::width_pixels);
		}
		if (auto* const split = qobject_cast<QSplitter*>(parentWidget()); split != nullptr) {
			const auto index = split->indexOf(this);
			if (index > 0 && split->handle(index) != nullptr) {
				split->handle(index)->setEnabled(expanded);
			}
			if (index > 0) {
				const auto mine  = expanded ? expanded_width() : workbench_drawer_rail::width_pixels;
				auto       sizes = split->sizes();
				const auto total = std::max(0, sizes.value(0) + sizes.value(index));
				if (total > 0 && sizes.value(index) != mine) {
					sizes[0]     = std::max(0, total - mine);
					sizes[index] = mine;
					split->setSizes(sizes);
				}
			}
		}
		Hide->setToolTip(QStringLiteral("Hide the Status drawer"));
		updateGeometry();
	}

	void overview_status_drawer::report(const application::workbench_status_drawer& aDrawer)
	{
		if (Dispatch) {
			Dispatch(aDrawer);
		}
	}

	void overview_status_drawer::refresh_icons()
	{
		Github->setIcon(strip_icon(strip_glyph::open_on_github, palette()));
		Hide->setIcon(drawer_icon(drawer_glyph::hide_right, palette()));
	}

	overview_status_splitter::overview_status_splitter(QWidget* const aParent)
		: QSplitter{ Qt::Horizontal, aParent }
	{
		setObjectName(QStringLiteral("overviewStatusSplitter"));
		setChildrenCollapsible(false);
		setHandleWidth(5);
	}

	auto overview_status_splitter::createHandle() -> QSplitterHandle*
	{
		return new overview_status_handle{ orientation(), this };
	}

	overview_status_handle::overview_status_handle(const Qt::Orientation aOrientation, QSplitter* const aParent)
		: QSplitterHandle{ aOrientation, aParent }
	{
		setObjectName(QStringLiteral("overviewStatusSeam"));
		setAccessibleName(QStringLiteral("Resize the Status drawer"));
		hold_resize_cursor(*this);
	}

	void overview_status_handle::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.fillRect(rect(), palette().color(QPalette::Active, QPalette::Window));
		// One hairline in Mid, as the workbench seam draws it.
		painter.setPen(palette().color(QPalette::Active, QPalette::Mid));
		const auto edge = (width() - 1) / 2;
		painter.drawLine(edge, 0, edge, height());
	}
} // namespace bpr::qt
