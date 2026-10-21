#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_OVERVIEW_STATUS_DRAWER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_OVERVIEW_STATUS_DRAWER_HPP

#include "bpr/application/workbench_runtime_ports.hpp"

#include <QSplitter>
#include <QSplitterHandle>
#include <QWidget>

#include <functional>

class QAction;
class QPaintEvent;
class QShowEvent;
class QStackedWidget;
class QTabBar;
class QToolButton;

/// \file
/// \brief [[BPR-1588]]. The Overview's Status drawer (RFC-BPR-0085).
namespace bpr::qt {
	class commit_filter_tab;
	class drawer_header;
	class workbench_drawer_rail;

	/// \brief The Overview's right column as a real drawer.
	///
	/// \details RFC-BPR-0085 principle 7: it resizes by its edge, collapses
	/// from its header's Hide tool, its rail and the window's key, and the
	/// window remembers both. The drawer is presentation only: `adopt` applies
	/// what the host published and changes nothing outward; a gesture (a drag
	/// of the edge, Hide, the rail) reports the new value through the change
	/// dispatch and waits for it to come back. Its header is its one tab,
	/// Status, with the tools at the right; its content is the column the page
	/// already built.
	class overview_status_drawer final : public QWidget
	{
	public:
		using change_dispatch = std::function<void(const application::workbench_status_drawer&)>;

		explicit overview_status_drawer(QWidget* aContent, QWidget* aParent = nullptr);

		/// \brief Puts Prepare review tour and Open on GitHub on the tab row.
		void set_tools(QAction* aPrepareTour, QAction* aOpenOnGithub);
		/// \brief Applies the window's published value without reporting it.
		void adopt(const application::workbench_status_drawer& aDrawer);
		/// \brief The value the drawer presents.
		[[nodiscard]] auto drawer() const noexcept -> const application::workbench_status_drawer&;
		void               set_change_dispatch(change_dispatch aDispatch);
		/// \brief The reviewer's toggle: collapses an expanded drawer or
		/// expands a collapsed one, and reports it.
		void toggle();
		/// \brief A drag of the edge ended at this width; it is clamped and reported.
		void observe_drag(int aWidth);
		/// \brief The width the expanded drawer presents.
		[[nodiscard]] auto expanded_width() const noexcept -> int;

		[[nodiscard]] auto tabs() const noexcept -> QTabBar*;
		[[nodiscard]] auto header() const noexcept -> drawer_header*;
		[[nodiscard]] auto hide_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto rail() const noexcept -> workbench_drawer_rail*;
		[[nodiscard]] auto content() const noexcept -> QWidget*;
		[[nodiscard]] auto tour_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto github_button() const noexcept -> QToolButton*;
		/// \brief RFC-BPR-0089. The Commits tab beside Status.
		[[nodiscard]] auto commits() const noexcept -> commit_filter_tab*;
		/// \brief Shows the Commits tab, expanding a collapsed drawer the way
		/// its rail does, and puts the keyboard on its list.
		void show_commits();

	protected:
		void changeEvent(QEvent* aEvent) override;
		void showEvent(QShowEvent* aEvent) override;
		auto eventFilter(QObject* aObject, QEvent* aEvent) -> bool override;

	private:
		[[nodiscard]] auto floor_width() const -> int;
		void               present();
		void               report(const application::workbench_status_drawer& aDrawer);
		void               refresh_icons();

		application::workbench_status_drawer Drawer;
		change_dispatch                      Dispatch;
		QWidget*                             Expanded = nullptr;
		drawer_header*                       Header   = nullptr;
		QTabBar*                             Tabs     = nullptr;
		QWidget*                             Tools    = nullptr;
		QToolButton*                         Tour     = nullptr;
		QToolButton*                         Github   = nullptr;
		QToolButton*                         Hide     = nullptr;
		workbench_drawer_rail*               Rail     = nullptr;
		QWidget*                             Content  = nullptr;
		QStackedWidget*                      Pages    = nullptr;
		commit_filter_tab*                   Commits  = nullptr;
	};

	/// \brief The splitter between the Overview's main column and Status, with
	/// the workbench seam's look and its held resize cursor ([[BPR-1330]]).
	class overview_status_splitter final : public QSplitter
	{
	public:
		explicit overview_status_splitter(QWidget* aParent = nullptr);

	protected:
		auto createHandle() -> QSplitterHandle* override;
	};

	class overview_status_handle final : public QSplitterHandle
	{
	public:
		overview_status_handle(Qt::Orientation aOrientation, QSplitter* aParent);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
	};
} // namespace bpr::qt

#endif
