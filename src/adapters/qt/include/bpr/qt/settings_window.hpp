#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SETTINGS_WINDOW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SETTINGS_WINDOW_HPP

#include <QPointer>
#include <QString>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

class QAction;
class QLabel;
class QLineEdit;
class QListWidgetItem;
class QKeyEvent;
class QCloseEvent;
class QListWidget;
class QStackedWidget;

namespace bpr::qt {
	class filter_empty_notice;

	/// \brief Names one page of the Settings window, in sidebar order.
	///
	/// \details The order is RFC-BPR-0018's: the pages a reviewer reaches first
	/// come first. Accounts, repositories, reviews and appearance host live
	/// surfaces; other pages name where a setting will live so a reviewer
	/// looking for it is told, rather than shown an empty page. Repositories
	/// follows Accounts & hosts because a saved repository belongs to one of
	/// those accounts (RFC-BPR-0062, owner direction 2026-10-07).
	enum class settings_page : std::uint8_t {
		general,
		accounts_and_hosts,
		repositories,
		reviews_and_queue,
		appearance,
		notifications,
		agents_and_context,
		storage_and_privacy,
	};

	inline constexpr std::size_t settings_page_count = 8U;

	/// \brief [[BPR-1289]]. What a Settings search result holds besides the
	/// name it shows (`Qt::DisplayRole`).
	enum settings_search_role : int {
		/// Where the setting lives, "Page · Section"; empty for a page.
		settings_search_location_role = Qt::UserRole + 1,
		/// The name's matched runs, as `QPoint{ start, length }` in UTF-16.
		settings_search_name_spans_role,
		/// The location's matched runs, likewise.
		settings_search_location_spans_role,
	};

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

		/// \brief [[BPR-1289]]. The search field at the top of the sidebar.
		[[nodiscard]] auto search_field() const noexcept -> QLineEdit*;
		/// \brief [[BPR-1289]]. The results that replace the page list while
		/// the search field holds text.
		[[nodiscard]] auto search_results() const noexcept -> QListWidget*;
		/// \brief [[BPR-1289]]. The notice shown when nothing matches, with Clear.
		[[nodiscard]] auto search_notice() const noexcept -> QWidget*;
		/// \brief [[BPR-1289]]. Opens a result's page and focuses its setting.
		void go_to_search_result(int aRow);

	protected:
		void changeEvent(QEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;
		void closeEvent(QCloseEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		/// \brief One search result: what it names, where it lives and what
		/// choosing it focuses.
		struct search_hit
		{
			settings_page     Page = settings_page::general;
			QString           Name;
			QString           Location;
			QPointer<QWidget> Target;
		};

		void select_page(int aRow);
		/// \brief Lists what matches the search field, or the page list when empty.
		void refresh_search();
		/// \brief Moves the chosen result while the caret stays in the field.
		void step_search_result(int aDelta);
		/// \brief Draws the field's magnifier in the palette's quiet ink.
		void               refresh_search_icon();
		[[nodiscard]] auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

		std::function<void()>   CloseHandler;
		QPointer<QWidget>       ReturnFocus;
		QWidget*                SidebarColumn = nullptr;
		QLineEdit*              Search        = nullptr;
		QAction*                SearchIcon    = nullptr;
		QListWidget*            Sidebar       = nullptr;
		QListWidget*            Results       = nullptr;
		filter_empty_notice*    SearchNotice  = nullptr;
		QStackedWidget*         Pages         = nullptr;
		std::vector<search_hit> Hits;
	};

	[[nodiscard]] auto settings_page_title(settings_page aPage) -> QString;
} // namespace bpr::qt

#endif
