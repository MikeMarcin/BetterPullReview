#ifndef BPR_SRC_APP_SRC_WINDOWS_TITLE_ROW_HOST_HPP
#define BPR_SRC_APP_SRC_WINDOWS_TITLE_ROW_HOST_HPP

#include "bpr/qt/workbench_window.hpp"

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPointer>

#include <cstdint>

namespace bpr::app {
	/// \brief Makes the workbench strip the Windows title row.
	///
	/// \details [[BPR-0653]]. The host performs. It removes the system title bar
	/// from the frame Qt computes, keeping the resize borders, shadow and rounded
	/// corners, and answers `WM_NCHITTEST` for the strip: bare surface is the
	/// caption, so dragging, double-click, Aero shake and edge snapping are the
	/// platform's own; the three caption buttons are the platform's button
	/// codes, so hovering maximise offers the snap layouts. Which part a point is
	/// on is the window's answer (`title_row_part_at`); the host only turns it
	/// into a code, reports non-client hover and press back for the buttons to
	/// draw, and performs minimise, maximise-or-restore and close as the system
	/// menu would.
	///
	/// The title bar is removed with Qt's Windows native interface
	/// (`QNativeInterface::Private::QWindowsWindow::setCustomMargins`), a private
	/// API: it is pinned to the Qt the build provides (6.10.3) and is rechecked
	/// on every Qt upgrade.
	///
	/// Only the `windows` platform has a title bar to replace. On any other
	/// platform plugin -- the offscreen tests -- the host does nothing and the
	/// buttons stay inert.
	class windows_title_row_host final : public QObject,
										 public QAbstractNativeEventFilter
	{
	public:
		explicit windows_title_row_host(qt::workbench_window& aWindow);
		~windows_title_row_host() override;
		windows_title_row_host(const windows_title_row_host&)                    = delete;
		auto operator=(const windows_title_row_host&) -> windows_title_row_host& = delete;

		/// \brief Whether the strip replaces the system title bar here.
		[[nodiscard]] auto active() const noexcept -> bool;

		auto nativeEventFilter(const QByteArray& aEventType, void* aMessage, qintptr* aResult)
			-> bool override;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		void attach();
		void apply_frame();
		void present();
		void perform(qt::title_row_part aPart);

		QPointer<qt::workbench_window> Window;
		bool                           Active     = false;
		quintptr                       Handle     = 0U;
		int                            AppliedTop = 0;
		qt::title_row_part             Hot        = qt::title_row_part::client;
		qt::title_row_part             Pressed    = qt::title_row_part::client;
	};
} // namespace bpr::app

#endif
