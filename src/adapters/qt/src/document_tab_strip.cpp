#include "bpr/qt/document_tab_strip.hpp"

#include <QAction>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>
#include <QTabBar>
#include <QToolButton>
#include <QWheelEvent>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		class equal_tab_bar final : public QTabBar
		{
		public:
			using QTabBar::QTabBar;
			int  Room = 0;
			auto tabSizeHint(int aIndex) const -> QSize override
			{
				const auto total = std::max(1, count());
				return { std::max(document_tab_strip::minimum_tab_width, Room / total + (aIndex < Room % total ? 1 : 0)), document_tab_strip::height_pixels };
			}
			auto minimumTabSizeHint(int aIndex) const -> QSize override { return tabSizeHint(aIndex); }
		};
		constexpr int pill_padding = 6;
		constexpr int pill_height  = 16;

		/// \brief A tab's own pair, in a pill beside its name.
		class pair_pill final : public QWidget
		{
		public:
			pair_pill(QString aText, QWidget* aParent)
				: QWidget{ aParent }
				, Text{ std::move(aText) }
			{
				setObjectName(QStringLiteral("documentTabPairPill"));
				setAttribute(Qt::WA_TransparentForMouseEvents, true);
				setFocusPolicy(Qt::NoFocus);
				setFixedSize(sizeHint());
			}

			[[nodiscard]] auto text() const -> const QString&
			{
				return Text;
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				auto small = font();
				small.setPointSizeF(small.pointSizeF() * 0.85);
				small.setBold(true);
				return QSize{ QFontMetrics{ small }.horizontalAdvance(Text) + (2 * pill_padding) + 4, pill_height };
			}

		protected:
			void paintEvent(QPaintEvent* aEvent) override
			{
				static_cast<void>(aEvent);
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setPen(Qt::NoPen);
				painter.setBrush(palette().color(QPalette::Active, QPalette::Highlight));
				const auto box = QRectF{ 2.0, 0.5, static_cast<qreal>(width()) - 2.5, static_cast<qreal>(height()) - 1.0 };
				painter.drawRoundedRect(box, box.height() / 2.0, box.height() / 2.0);
				auto small = font();
				small.setPointSizeF(small.pointSizeF() * 0.85);
				small.setBold(true);
				painter.setFont(small);
				painter.setPen(palette().color(QPalette::Active, QPalette::HighlightedText));
				painter.drawText(box, Qt::AlignCenter | Qt::TextSingleLine, Text);
			}

		private:
			QString Text;
		};

		/// \brief One line of status, elided at the right to the room it has;
		/// the whole sentence is its tooltip and description.
		class status_line final : public QLabel
		{
		public:
			explicit status_line(QWidget* aParent)
				: QLabel{ aParent }
			{
				setObjectName(QStringLiteral("documentTabStatus"));
				setAccessibleName(QStringLiteral("Comparison status"));
				setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
				setForegroundRole(QPalette::PlaceholderText);
				setTextFormat(Qt::PlainText);
			}

			[[nodiscard]] auto minimumSizeHint() const -> QSize override
			{
				// [[BPR-1197]]. Even beside a collapsed tab with its own pair,
				// leave room for an ellipsis and a target for the full tooltip.
				return { fontMetrics().horizontalAdvance(QStringLiteral("…")) + 14, fontMetrics().height() };
			}

		protected:
			void paintEvent(QPaintEvent* aEvent) override
			{
				static_cast<void>(aEvent);
				auto painter = QPainter{ this };
				painter.setPen(palette().color(QPalette::Active, foregroundRole()));
				const auto room = contentsRect().adjusted(6, 0, -8, 0);
				painter.drawText(room, Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine,
								 fontMetrics().elidedText(text(), Qt::ElideRight, std::max(1, room.width())));
			}
		};

		[[nodiscard]] auto state_words(const core::document_tab_view_state aState) -> QString
		{
			switch (aState) {
			case core::document_tab_view_state::loading:
				return QStringLiteral("Loading");
			case core::document_tab_view_state::unavailable:
				return QStringLiteral("Unavailable: its file or its comparison is gone");
			case core::document_tab_view_state::idle:
			case core::document_tab_view_state::ready:
				break;
			}
			return { };
		}
	} // namespace

	document_tab_strip::document_tab_strip(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("documentTabStrip"));
		setAccessibleName(QStringLiteral("Document tabs"));
		setAccessibleDescription(QStringLiteral(
			"One tab per file, comparison and reading position. Ctrl+Tab and "
			"Ctrl+Shift+Tab move between them; the close key closes the active one"));
		setFixedHeight(height_pixels);
		setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		setBackgroundRole(QPalette::Window);
		setAutoFillBackground(true);

		auto* const layout = new QHBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);

		Scroll = new QScrollArea{ this };
		Scroll->setObjectName(QStringLiteral("documentTabScroll"));
		Scroll->setFrameShape(QFrame::NoFrame);
		Scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Scroll->setWidgetResizable(false);
		Scroll->setFocusPolicy(Qt::NoFocus);
		Scroll->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		Scroll->setFixedHeight(height_pixels);
		Scroll->viewport()->installEventFilter(this);
		connect(Scroll->horizontalScrollBar(), &QScrollBar::rangeChanged, this, [this] { if (Bar) { reveal_active(); } });
		Bar = new equal_tab_bar{ Scroll };
		Bar->installEventFilter(this);
		Bar->setObjectName(QStringLiteral("documentTabBar"));
		Bar->setAccessibleName(QStringLiteral("Document tabs"));
		Bar->setAccessibleDescription(QStringLiteral(
			"This window's tabs: each a file, a comparison and a reading position. "
			"The arrow keys choose one; its menu duplicates, closes or changes its comparison"));
		Bar->setDocumentMode(true);
		Bar->setTabsClosable(true);
		Bar->setExpanding(false);
		Bar->setUsesScrollButtons(false);
		Bar->setElideMode(Qt::ElideLeft);
		Bar->setDrawBase(false);
		Bar->setMovable(false);
		Bar->setFocusPolicy(Qt::StrongFocus);
		Bar->setContextMenuPolicy(Qt::CustomContextMenu);
		Scroll->setWidget(Bar);
		layout->addWidget(Scroll, 1);

		NewTab = new QToolButton{ this };
		NewTab->setObjectName(QStringLiteral("documentTabNew"));
		NewTab->setAutoRaise(true);
		NewTab->setText(QStringLiteral("+"));
		NewTab->setFocusPolicy(Qt::StrongFocus);
		NewTab->setAccessibleName(QStringLiteral("New tab"));
		NewTab->setAccessibleDescription(QStringLiteral("Opens the active tab's file and comparison in a tab of its own"));
		NewTab->setToolTip(QStringLiteral("New tab: this file and comparison, in a tab of its own"));
		NewTab->setEnabled(false);

		Status = new status_line{ this };
		Status->hide();
		layout->addWidget(Status);
		layout->addWidget(NewTab);
		setMinimumWidth(minimumSizeHint().width());

		connect(Bar, &QTabBar::currentChanged, this, [this](const int aIndex) {
			if (Rebuilding) {
				return;
			}
			if (const auto tab = tab_at(aIndex); tab.has_value()) {
				send(core::activate_tab_intent{ .Tab = *tab });
				reveal_active();
			}
		});
		connect(Bar, &QTabBar::tabCloseRequested, this, [this](const int aIndex) {
			if (const auto tab = tab_at(aIndex); tab.has_value()) {
				send(core::close_tab_intent{ .Tab = *tab });
			}
		});
		connect(Bar, &QWidget::customContextMenuRequested, this, [this](const QPoint& aAt) { open_tab_menu(aAt); });
		connect(NewTab, &QToolButton::clicked, this, [this] {
			if (const auto* const current = active(); current != nullptr) {
				send(core::duplicate_tab_intent{ .Tab = current->Id });
				Bar->setFocus(Qt::OtherFocusReason);
			}
		});
	}

	void document_tab_strip::set_dispatch(document_tab_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void document_tab_strip::present(std::vector<document_tab_presentation> aTabs)
	{
		if (aTabs == Tabs) {
			return;
		}
		Tabs = std::move(aTabs);
		rebuild();
	}

	void document_tab_strip::set_status(const QString& aText, const bool aNotable)
	{
		Status->setText(aText);
		Status->setToolTip(aText);
		Status->setAccessibleDescription(aText);
		Status->setVisible(aNotable && !aText.isEmpty());
		Status->update();
		layout_tabs();
	}

	auto document_tab_strip::tabs() const noexcept -> const std::vector<document_tab_presentation>&
	{
		return Tabs;
	}

	auto document_tab_strip::tab_bar() const noexcept -> QTabBar*
	{
		return Bar;
	}

	auto document_tab_strip::scroll_area() const noexcept -> QScrollArea* { return Scroll; }

	auto document_tab_strip::new_tab_button() const noexcept -> QToolButton*
	{
		return NewTab;
	}

	auto document_tab_strip::status_label() const noexcept -> QLabel*
	{
		return Status;
	}

	auto document_tab_strip::tab_at(const int aIndex) const -> std::optional<core::document_tab_id>
	{
		if (aIndex < 0 || static_cast<std::size_t>(aIndex) >= Shown.size()) {
			return std::nullopt;
		}
		return Shown[static_cast<std::size_t>(aIndex)];
	}

	auto document_tab_strip::index_of(const core::document_tab_id aTab) const -> std::optional<int>
	{
		const auto found = std::ranges::find(Shown, aTab);
		return found == Shown.end() ? std::nullopt : std::optional{ static_cast<int>(std::distance(Shown.begin(), found)) };
	}

	auto document_tab_strip::make_tab_menu(const core::document_tab_id aTab, QWidget* const aParent) -> QMenu*
	{
		const auto found = std::ranges::find(Tabs, aTab, &document_tab_presentation::Id);
		if (found == Tabs.end()) {
			return nullptr;
		}
		const auto& tab  = *found;
		auto*       menu = new QMenu{ aParent };
		menu->setObjectName(QStringLiteral("documentTabMenu"));
		const auto add = [this, menu](const QString& aText, const char* aName, core::document_tab_intent aIntent, const bool aEnabled) {
			auto* const action = menu->addAction(aText);
			action->setObjectName(QString::fromLatin1(aName));
			action->setEnabled(aEnabled);
			connect(action, &QAction::triggered, this, [this, intent = std::move(aIntent)] { send(intent); });
			return action;
		};
		add(QStringLiteral("Duplicate Tab"), "documentTabDuplicate", core::duplicate_tab_intent{ .Tab = tab.Id }, true);
		add(QStringLiteral("Close Tab"), "documentTabClose", core::close_tab_intent{ .Tab = tab.Id }, true);
		add(QStringLiteral("Close Other Tabs"), "documentTabCloseOthers", core::close_other_tabs_intent{ .Tab = tab.Id }, Tabs.size() > 1U);
		menu->addSeparator();
		add(QStringLiteral("Use the Window's Comparison"), "documentTabReturnToWindow",
			core::return_tab_to_window_comparison_intent{ .Tab = tab.Id }, tab.OwnComparison);
		add(QStringLiteral("Make New Tabs Use This Comparison"), "documentTabMakeWindow",
			core::make_tab_comparison_window_intent{ .Tab = tab.Id }, tab.OwnComparison);
		return menu;
	}

	auto document_tab_strip::sizeHint() const -> QSize
	{
		return QSize{ 240, height_pixels };
	}

	auto document_tab_strip::minimumSizeHint() const -> QSize
	{
		// [[BPR-1251]]. Activating a tab brings all of it into view, so the
		// strip keeps room for one tab at its minimum beside the plus. The
		// strip's horizontal policy is Ignored, which makes a layout read the
		// explicit minimum width, so this is also what the strip sets.
		return QSize{ minimum_tab_width + (NewTab != nullptr ? NewTab->sizeHint().width() : 0), height_pixels };
	}

	void document_tab_strip::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		layout_tabs();
	}

	void document_tab_strip::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (Bar != nullptr && (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::ApplicationFontChange || aEvent->type() == QEvent::StyleChange)) {
			setMinimumWidth(minimumSizeHint().width());
			rebuild();
		}
	}

	void document_tab_strip::layout_tabs()
	{
		if (!Scroll || !Bar || Rebuilding) {
			return;
		}
		Status->setMaximumWidth(std::max(24, width() / 3));
		auto*      bar  = static_cast<equal_tab_bar*>(Bar);
		const auto room = std::max(0, Scroll->viewport()->width());
		bar->Room       = room;
		bar->setFixedSize(std::max(room, bar->count() * minimum_tab_width), height_pixels);
		bar->setExpanding(true);
		bar->updateGeometry();
		reveal_active();
	}
	void document_tab_strip::reveal_active()
	{
		const auto rect = Bar->tabRect(Bar->currentIndex());
		if (!rect.isValid()) {
			return;
		}
		auto* scroll = Scroll->horizontalScrollBar();
		if (rect.left() < scroll->value()) {
			scroll->setValue(rect.left());
		} else if (rect.right() >= scroll->value() + Scroll->viewport()->width()) {
			scroll->setValue(rect.right() + 1 - Scroll->viewport()->width());
		}
	}
	bool document_tab_strip::eventFilter(QObject* aObject, QEvent* aEvent)
	{
		if (aEvent->type() == QEvent::Wheel && (aObject == Bar || aObject == Scroll->viewport())) {
			auto*      wheel  = static_cast<QWheelEvent*>(aEvent);
			const auto pixels = wheel->pixelDelta();
			const auto angle  = wheel->angleDelta();
			const auto delta  = !pixels.isNull() ? (pixels.x() != 0 ? pixels.x() : pixels.y())
												 : (angle.x() != 0 ? angle.x() : angle.y());
			auto*      scroll = Scroll->horizontalScrollBar();
			scroll->setValue(scroll->value() - delta);
			wheel->accept();
			return true;
		}
		if (aObject == Scroll->viewport() && aEvent->type() == QEvent::Resize) {
			layout_tabs();
		}
		return QWidget::eventFilter(aObject, aEvent);
	}

	auto document_tab_strip::active() const -> const document_tab_presentation*
	{
		const auto found = std::ranges::find(Tabs, true, &document_tab_presentation::Active);
		return found == Tabs.end() ? nullptr : &*found;
	}

	void document_tab_strip::send(const core::document_tab_intent& aIntent) const
	{
		if (Dispatch) {
			Dispatch(aIntent);
		}
	}

	void document_tab_strip::rebuild()
	{
		Rebuilding = true;
		{
			const QSignalBlocker blocker{ Bar };
			while (Bar->count() > 0) {
				Bar->removeTab(0);
			}
			Shown.clear();
			const auto pillSide = static_cast<QTabBar::ButtonPosition>(
									  Bar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, Bar))
					== QTabBar::LeftSide
				? QTabBar::RightSide
				: QTabBar::LeftSide;
			auto       current  = -1;
			for (const auto& tab : Tabs) {
				const auto state = state_words(tab.State);
				const auto label = tab.State == core::document_tab_view_state::unavailable
					? QStringLiteral("%1 (unavailable)").arg(tab.Name)
					: tab.Name;
				const auto index = Bar->addTab(label);
				auto       tip   = tab.Path;
				if (!tab.Pair.isEmpty()) {
					tip.append(QStringLiteral("\n%1 %2").arg(tab.OwnComparison ? QStringLiteral("Its own comparison:") : QStringLiteral("The window's comparison:"), tab.Pair));
				}
				if (!state.isEmpty()) {
					tip.append(QStringLiteral("\n%1").arg(state));
				}
				Bar->setTabToolTip(index, tip);
				auto accessible = tab.Name;
				if (tab.OwnComparison && !tab.Pair.isEmpty()) {
					accessible.append(QStringLiteral(", comparing %1").arg(tab.Pair));
				}
				if (!state.isEmpty()) {
					accessible.append(QStringLiteral(", %1").arg(state.toLower()));
				}
				Bar->setAccessibleTabName(index, accessible);
				if (tab.OwnComparison && !tab.OwnPair.isEmpty()) {
					Bar->setTabButton(index, pillSide, new pair_pill{ tab.OwnPair, Bar });
				}
				Shown.push_back(tab.Id);
				if (tab.Active) {
					current = index;
				}
			}
			if (current >= 0) {
				Bar->setCurrentIndex(current);
			}
		}
		NewTab->setEnabled(active() != nullptr);
		Rebuilding = false;
		layout_tabs();
		updateGeometry();
	}

	void document_tab_strip::open_tab_menu(const QPoint& aAt)
	{
		const auto tab = tab_at(Bar->tabAt(aAt));
		if (!tab.has_value()) {
			return;
		}
		// [[BPR-1034]]. A heap menu that deletes itself on close, opened with
		// popup(): anything that destroys the window while it is open takes
		// the menu with it, and no frame here waits for it.
		auto* const menu = make_tab_menu(*tab, this);
		if (menu == nullptr) {
			return;
		}
		menu->setAttribute(Qt::WA_DeleteOnClose);
		menu->popup(Bar->mapToGlobal(aAt));
	}

} // namespace bpr::qt
