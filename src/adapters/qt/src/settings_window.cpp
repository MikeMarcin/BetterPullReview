#include "bpr/qt/settings_window.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace bpr::qt {
	namespace {
		constexpr auto page_order = std::array{
			settings_page::general,
			settings_page::accounts_and_hosts,
			settings_page::reviews_and_queue,
			settings_page::appearance,
			settings_page::notifications,
			settings_page::agents_and_context,
			settings_page::storage_and_privacy,
		};
		static_assert(page_order.size() == settings_page_count);

		[[nodiscard]] auto page_index(const settings_page aPage) noexcept -> int
		{
			return static_cast<int>(aPage);
		}

		/// Says where a setting lives until its page hosts a surface. Written
		/// for the reviewer looking for the setting, not for the roadmap.
		[[nodiscard]] auto page_notice(const settings_page aPage) -> QString
		{
			switch (aPage) {
			case settings_page::general:
				return QStringLiteral(
					"New windows open Home. Window restoration is not available in this build. "
					"Review navigation defaults are in Reviews & Queue; colours and fonts are in Appearance.");
			case settings_page::accounts_and_hosts:
				return QStringLiteral(
					"Sign in, sign out and enterprise servers are in the review window's "
					"header for now. Remembered servers will move here.");
			case settings_page::reviews_and_queue:
				return QStringLiteral(
					"Observer rules and queue ordering are in the review window's queue "
					"for now.");
			case settings_page::appearance:
				return QStringLiteral("Appearance settings are unavailable in this build.");
			case settings_page::notifications:
				return QStringLiteral("Local notification preferences will live here.");
			case settings_page::agents_and_context:
				return QStringLiteral("Agent profiles and context supplements will live here.");
			case settings_page::storage_and_privacy:
				return QStringLiteral(
					"Storage location, cache limits and diagnostics privacy will live "
					"here.");
			}
			return { };
		}
	} // namespace

	auto settings_page_title(const settings_page aPage) -> QString
	{
		switch (aPage) {
		case settings_page::general:
			return QStringLiteral("General");
		case settings_page::accounts_and_hosts:
			return QStringLiteral("Accounts & hosts");
		case settings_page::reviews_and_queue:
			return QStringLiteral("Reviews & queue");
		case settings_page::appearance:
			return QStringLiteral("Appearance");
		case settings_page::notifications:
			return QStringLiteral("Notifications");
		case settings_page::agents_and_context:
			return QStringLiteral("Agents & context");
		case settings_page::storage_and_privacy:
			return QStringLiteral("Storage & privacy");
		}
		return { };
	}

	void settings_window::set_profile_label(const QString& aLabel)
	{
		const auto title = QStringLiteral("Settings — %1 profile").arg(aLabel);
		setWindowTitle(title);
		setAccessibleName(title);
	}

	settings_window::settings_window(QWidget* const aParent)
		: QWidget{ aParent, Qt::Window }
	{
		setObjectName(QStringLiteral("settingsWindow"));
		setWindowTitle(QStringLiteral("Settings — Default profile"));
		setAccessibleName(QStringLiteral("Settings — Default profile"));
		setAccessibleDescription(QStringLiteral(
			"Application settings grouped by page; changes apply as they are made"));
		// A modeless window that a reviewer opens beside the comparison, so
		// closing it must not destroy the panels it adopted: hide, never delete.
		setAttribute(Qt::WA_DeleteOnClose, false);
		setMinimumSize(760, 480);
		resize(960, 640);

		Sidebar = new QListWidget{ this };
		Sidebar->setObjectName(QStringLiteral("settingsSidebar"));
		Sidebar->setAccessibleName(QStringLiteral("Settings pages"));
		Sidebar->setFixedWidth(196);
		Sidebar->setFrameShape(QFrame::NoFrame);
		Sidebar->setSelectionMode(QAbstractItemView::SingleSelection);

		Pages = new QStackedWidget{ this };
		Pages->setObjectName(QStringLiteral("settingsPages"));
		Pages->setAccessibleName(QStringLiteral("Settings page"));

		for (const auto page : page_order) {
			Sidebar->addItem(settings_page_title(page));
			auto* const scroll = new QScrollArea{ Pages };
			scroll->setObjectName(QStringLiteral("settingsPageScroll-%1").arg(page_index(page)));
			scroll->setAccessibleName(settings_page_title(page));
			scroll->setWidgetResizable(true);
			scroll->setFrameShape(QFrame::NoFrame);
			auto* const surface = new QWidget;
			surface->setObjectName(
				QStringLiteral("settingsPage-%1").arg(page_index(page)));
			auto* const column = new QVBoxLayout{ surface };
			column->setContentsMargins(20, 16, 20, 16);
			auto* const title = new QLabel{ settings_page_title(page), surface };
			title->setObjectName(QStringLiteral("settingsPageTitle"));
			auto titleFont = title->font();
			titleFont.setPointSizeF(titleFont.pointSizeF() * 1.25);
			titleFont.setWeight(QFont::DemiBold);
			title->setFont(titleFont);
			column->addWidget(title);
			auto* const notice = new QLabel{ page_notice(page), surface };
			notice->setObjectName(QStringLiteral("settingsPageNotice"));
			notice->setWordWrap(true);
			column->addWidget(notice);
			column->addStretch(1);
			scroll->setWidget(surface);
			Pages->addWidget(scroll);
		}

		auto* const row = new QHBoxLayout{ this };
		row->setContentsMargins(0, 0, 0, 0);
		row->setSpacing(0);
		row->addWidget(Sidebar);
		row->addWidget(Pages, 1);

		connect(Sidebar, &QListWidget::currentRowChanged, this, [this](const int aRow) {
			select_page(aRow);
		});
		Sidebar->setCurrentRow(page_index(settings_page::appearance));
	}

	settings_window::~settings_window() = default;

	void settings_window::adopt_panel(const settings_page aPage, QWidget* const aPanel)
	{
		auto* const surface = page_widget(aPage);
		if (surface == nullptr || aPanel == nullptr) {
			return;
		}
		auto* const column = qobject_cast<QVBoxLayout*>(surface->layout());
		if (column == nullptr) {
			return;
		}
		if (auto* const notice = surface->findChild<QLabel*>(
				QStringLiteral("settingsPageNotice"), Qt::FindDirectChildrenOnly);
			notice != nullptr) {
			notice->hide();
		}
		aPanel->setParent(surface);
		// Between the title and the trailing stretch.
		column->insertWidget(1, aPanel);
		aPanel->show();
	}

	void settings_window::withdraw_page(const settings_page aPage)
	{
		auto* const item = Sidebar->item(page_index(aPage));
		if (item == nullptr) {
			return;
		}
		item->setHidden(true);
		if (Sidebar->currentRow() == page_index(aPage)) {
			Sidebar->setCurrentRow(page_index(settings_page::general));
		}
	}

	void settings_window::present(const settings_page aPage)
	{
		if (!isVisible()) {
			ReturnFocus = QApplication::focusWidget();
		}
		const auto* const item = Sidebar->item(page_index(aPage));
		Sidebar->setCurrentRow(page_index(item != nullptr && item->isHidden() ? settings_page::general : aPage));
		show();
		raise();
		activateWindow();
	}

	auto settings_window::current_page() const noexcept -> settings_page
	{
		const auto row = Pages->currentIndex();
		if (row < 0 || row >= static_cast<int>(settings_page_count)) {
			return settings_page::general;
		}
		return page_order[static_cast<std::size_t>(row)];
	}

	auto settings_window::sidebar() const noexcept -> QListWidget*
	{
		return Sidebar;
	}

	auto settings_window::pages() const noexcept -> QStackedWidget*
	{
		return Pages;
	}

	auto settings_window::page_widget(const settings_page aPage) const noexcept -> QWidget*
	{
		auto* const scroll = qobject_cast<QScrollArea*>(Pages->widget(page_index(aPage)));
		return scroll != nullptr ? scroll->widget() : nullptr;
	}

	void settings_window::set_close_handler(std::function<void()> aHandler)
	{
		CloseHandler = std::move(aHandler);
	}

	void settings_window::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		auto labelWidth = 0;
		for (auto index = 0; index < Sidebar->count(); ++index) {
			labelWidth = std::max(labelWidth, Sidebar->fontMetrics().horizontalAdvance(Sidebar->item(index)->text()));
		}
		// Keep category names visible while returning unused sidebar width to
		// the page in a small window. Native font metrics own the minimum.
		Sidebar->setFixedWidth(std::max(labelWidth + 20, width() < 900 ? 0 : 196));
	}

	void settings_window::closeEvent(QCloseEvent* const aEvent)
	{
		QWidget::closeEvent(aEvent);
		if (aEvent->isAccepted() && CloseHandler) {
			CloseHandler();
		}
		if (aEvent->isAccepted() && ReturnFocus != nullptr && ReturnFocus->isVisible()) {
			ReturnFocus->window()->activateWindow();
			ReturnFocus->setFocus(Qt::OtherFocusReason);
		}
	}

	void settings_window::keyPressEvent(QKeyEvent* const aEvent)
	{
		// Child pickers and editors receive Escape first. Only an unhandled
		// key reaches the window, so closing cannot steal their cancellation.
		if (aEvent->key() == Qt::Key_Escape && aEvent->modifiers() == Qt::NoModifier) {
			close();
			aEvent->accept();
			return;
		}
		QWidget::keyPressEvent(aEvent);
	}

	void settings_window::select_page(const int aRow)
	{
		if (aRow < 0 || aRow >= Pages->count()) {
			return;
		}
		Pages->setCurrentIndex(aRow);
	}
} // namespace bpr::qt
