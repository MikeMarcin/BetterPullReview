#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SETTINGS_WINDOW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SETTINGS_WINDOW_HPP

#include <QPointer>
#include <QString>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>

class QLabel;
class QKeyEvent;
class QCloseEvent;
class QListWidget;
class QStackedWidget;

namespace bpr::qt {
	/// \brief Names one page of the Settings window, in sidebar order.
	///
	/// \details The order is RFC-BPR-0018's: the pages a reviewer reaches first
	/// come first. Accounts, reviews and appearance host live surfaces; other pages name
	/// where a setting will live so a reviewer looking for it is told, rather
	/// than shown an empty page.
	enum class settings_page : std::uint8_t {
		general,
		accounts_and_hosts,
		reviews_and_queue,
		appearance,
		notifications,
		agents_and_context,
		storage_and_privacy,
	};

	inline constexpr std::size_t settings_page_count = 7U;

	/// \brief Modeless singleton Settings window with a stable sidebar.
	///
	/// \details Presentation only. The window owns no setting: a page hosts a
	/// panel that already dispatches its own intents, and the window merely
	/// places it. It is a `Qt::Window` child of the workbench, so it closes
	/// with its workbench and never outlives the panel it adopted. `Cmd+,`
	/// reaches it through the workbench's preferences action.
	class settings_window final : public QWidget
	{
	public:
		explicit settings_window(QWidget* aParent = nullptr);
		~settings_window() override;
		void set_profile_label(const QString& aLabel);

		settings_window(const settings_window&)                    = delete;
		auto operator=(const settings_window&) -> settings_window& = delete;

		/// \brief Places a live settings surface on a page, replacing its notice.
		void adopt_panel(settings_page aPage, QWidget* aPanel);

		/// \brief [[BPR-1179]]. Removes a page from the sidebar for this window's life.
		///
		/// \details For a page whose capability the composition does not
		/// contain: a page that cannot be reached is not a page that says
		/// nothing. Presenting a withdrawn page presents General instead.
		void withdraw_page(settings_page aPage);

		/// \brief Shows the window, raises it, and selects a page.
		void present(settings_page aPage);
		void set_close_handler(std::function<void()> aHandler);

		[[nodiscard]] auto current_page() const noexcept -> settings_page;
		[[nodiscard]] auto sidebar() const noexcept -> QListWidget*;
		[[nodiscard]] auto pages() const noexcept -> QStackedWidget*;
		[[nodiscard]] auto page_widget(settings_page aPage) const noexcept -> QWidget*;

	protected:
		void keyPressEvent(QKeyEvent* aEvent) override;
		void closeEvent(QCloseEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		void select_page(int aRow);

		std::function<void()> CloseHandler;
		QPointer<QWidget>     ReturnFocus;
		QListWidget*          Sidebar = nullptr;
		QStackedWidget*       Pages   = nullptr;
	};

	[[nodiscard]] auto settings_page_title(settings_page aPage) -> QString;
} // namespace bpr::qt

#endif
