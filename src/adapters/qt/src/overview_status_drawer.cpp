#include "bpr/qt/overview_status_drawer.hpp"

#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/resize_handle_cursor.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_strip.hpp"

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
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
		Tabs = new QTabBar{ Header };
		Tabs->setObjectName(QStringLiteral("overviewStatusTabs"));
		Tabs->setAccessibleName(QStringLiteral("Status drawer"));
		configure_drawer_tabs(*Tabs);
		Tabs->addTab(QStringLiteral("Status"));
		Tabs->setTabToolTip(0, QStringLiteral("Where this pull request stands and your part in its review"));
		Tools = new QWidget{ Header };
		Tools->setObjectName(QStringLiteral("overviewStatusTools"));
		auto* const tools = new QHBoxLayout{ Tools };
		tools->setContentsMargins(0, 0, 0, 0);
		tools->setSpacing(1);
		Tour = make_drawer_tool(Tools, { }, QStringLiteral("Prepare review tour"), QStringLiteral("overviewPrepareTour"));
		Tour->setAccessibleDescription(QStringLiteral("Write a private problem statement, solution and source tour."));
		Tour->hide();
		Github = make_drawer_tool(Tools, { }, QStringLiteral("Open on GitHub"), QStringLiteral("overviewStatusOpenOnGithub"));
		Github->hide();
		Hide = make_drawer_tool(Tools, { }, QStringLiteral("Hide the Status drawer"), QStringLiteral("overviewStatusHide"));
		connect(Hide, &QToolButton::clicked, this, [this] { toggle(); });
		tools->addWidget(Tour);
		tools->addWidget(Github);
		tools->addWidget(Hide);
		Header->set_parts(Tabs, Tools);
		column->addWidget(Header);
		Content->setParent(Expanded);
		if (auto* const scroll = qobject_cast<QScrollArea*>(Content); scroll != nullptr && scroll->widget() != nullptr) {
			scroll->widget()->installEventFilter(this);
		}
		column->addWidget(Content, 1);
		row->addWidget(Expanded, 1);
		refresh_icons();
		present();
	}

	void overview_status_drawer::set_tools(QAction* const aPrepareTour, QAction* const aOpenOnGithub)
	{
		if (aPrepareTour != nullptr) {
			follow(*Tour, aPrepareTour);
			const auto key = aPrepareTour->shortcut().toString(QKeySequence::NativeText);
			Tour->setToolTip(key.isEmpty() ? QStringLiteral("Prepare review tour") : QStringLiteral("Prepare review tour (%1)").arg(key));
			Tour->show();
		}
		if (aOpenOnGithub != nullptr) {
			follow(*Github, aOpenOnGithub);
			const auto key = aOpenOnGithub->shortcut().toString(QKeySequence::NativeText);
			Github->setToolTip(key.isEmpty() ? QStringLiteral("Open on GitHub") : QStringLiteral("Open on GitHub (%1)").arg(key));
			Github->show();
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

	auto overview_status_drawer::tour_button() const noexcept -> QToolButton*
	{
		return Tour;
	}

	auto overview_status_drawer::github_button() const noexcept -> QToolButton*
	{
		return Github;
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
		Tour->setIcon(drawer_icon(drawer_glyph::tour, palette()));
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
