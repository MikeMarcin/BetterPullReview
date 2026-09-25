#include "bpr/qt/document_tab_strip.hpp"

#include <QAction>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QTabBar>
#include <QToolButton>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		/// \brief What a tab takes beside its name: the style's padding, its
		/// close button and the gaps around them. An estimate, used only to
		/// decide whether every tab fits; the bar lays itself out.
		constexpr int tab_overhead = 48;
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
				setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
				setForegroundRole(QPalette::PlaceholderText);
				setTextFormat(Qt::PlainText);
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

		Bar = new QTabBar{ this };
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
		layout->addWidget(Bar);

		Overflow = new QToolButton{ this };
		Overflow->setObjectName(QStringLiteral("documentTabOverflow"));
		Overflow->setAutoRaise(true);
		Overflow->setPopupMode(QToolButton::InstantPopup);
		Overflow->setFocusPolicy(Qt::StrongFocus);
		Overflow->setAccessibleName(QStringLiteral("More tabs"));
		Overflow->setAccessibleDescription(QStringLiteral("The tabs the strip has no room for"));
		Overflow->setMenu(new QMenu{ Overflow });
		Overflow->menu()->setObjectName(QStringLiteral("documentTabOverflowMenu"));
		Overflow->hide();
		layout->addWidget(Overflow);

		NewTab = new QToolButton{ this };
		NewTab->setObjectName(QStringLiteral("documentTabNew"));
		NewTab->setAutoRaise(true);
		NewTab->setText(QStringLiteral("+"));
		NewTab->setFocusPolicy(Qt::StrongFocus);
		NewTab->setAccessibleName(QStringLiteral("New tab"));
		NewTab->setAccessibleDescription(QStringLiteral("Opens the active tab's file and comparison in a tab of its own"));
		NewTab->setToolTip(QStringLiteral("New tab: this file and comparison, in a tab of its own"));
		NewTab->setEnabled(false);
		layout->addWidget(NewTab);

		Status = new status_line{ this };
		Status->hide();
		layout->addWidget(Status, 1);

		connect(Bar, &QTabBar::currentChanged, this, [this](const int aIndex) {
			if (Rebuilding) {
				return;
			}
			if (const auto tab = tab_at(aIndex); tab.has_value()) {
				send(core::activate_tab_intent{ .Tab = *tab });
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
			}
		});
		connect(Overflow->menu(), &QMenu::aboutToShow, this, [this] { populate_overflow(); });
	}

	void document_tab_strip::set_dispatch(document_tab_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void document_tab_strip::present(std::vector<document_tab_presentation> aTabs)
	{
		if (aTabs == Tabs && Collapsed == !fits_whole()) {
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
	}

	auto document_tab_strip::tabs() const noexcept -> const std::vector<document_tab_presentation>&
	{
		return Tabs;
	}

	auto document_tab_strip::tab_bar() const noexcept -> QTabBar*
	{
		return Bar;
	}

	auto document_tab_strip::overflow_button() const noexcept -> QToolButton*
	{
		return Overflow;
	}

	auto document_tab_strip::new_tab_button() const noexcept -> QToolButton*
	{
		return NewTab;
	}

	auto document_tab_strip::status_label() const noexcept -> QLabel*
	{
		return Status;
	}

	auto document_tab_strip::collapsed() const noexcept -> bool
	{
		return Collapsed;
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
		return QSize{ 0, height_pixels };
	}

	void document_tab_strip::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		if (Collapsed == fits_whole()) {
			rebuild();
		}
	}

	void document_tab_strip::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (Bar != nullptr && (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::ApplicationFontChange || aEvent->type() == QEvent::StyleChange)) {
			rebuild();
		}
	}

	auto document_tab_strip::fits_whole() const -> bool
	{
		if (Tabs.size() <= 1U || width() <= 0) {
			return true;
		}
		const auto metrics = Bar->fontMetrics();
		auto       total   = NewTab->sizeHint().width();
		for (const auto& tab : Tabs) {
			total += std::min(metrics.horizontalAdvance(tab.Name), 240) + tab_overhead;
			if (tab.OwnComparison) {
				total += metrics.horizontalAdvance(tab.OwnPair) + (2 * pill_padding) + 8;
			}
		}
		return total <= width();
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
			Collapsed           = !fits_whole();
			const auto pillSide = static_cast<QTabBar::ButtonPosition>(
									  Bar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, Bar))
					== QTabBar::LeftSide
				? QTabBar::RightSide
				: QTabBar::LeftSide;
			auto       current  = -1;
			for (const auto& tab : Tabs) {
				if (Collapsed && !tab.Active) {
					continue;
				}
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
		const auto hidden = Collapsed ? Tabs.size() - Shown.size() : 0U;
		Overflow->setVisible(hidden > 0U);
		Overflow->setText(QStringLiteral("+%1").arg(hidden));
		Overflow->setToolTip(hidden == 1U ? QStringLiteral("One more tab") : QStringLiteral("%1 more tabs").arg(hidden));
		Overflow->setAccessibleDescription(Overflow->toolTip());
		NewTab->setEnabled(active() != nullptr);
		Rebuilding = false;
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

	void document_tab_strip::populate_overflow()
	{
		auto* const menu = Overflow->menu();
		menu->clear();
		for (const auto& tab : Tabs) {
			if (tab.Active) {
				continue;
			}
			auto* const action = menu->addAction(tab.OwnComparison && !tab.OwnPair.isEmpty()
													 ? QStringLiteral("%1 · %2").arg(tab.Name, tab.OwnPair)
													 : tab.Name);
			action->setToolTip(tab.Path);
			connect(action, &QAction::triggered, this, [this, id = tab.Id] { send(core::activate_tab_intent{ .Tab = id }); });
		}
	}
} // namespace bpr::qt
