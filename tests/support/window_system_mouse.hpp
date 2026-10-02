#ifndef BPR_TESTS_SUPPORT_WINDOW_SYSTEM_MOUSE_HPP
#define BPR_TESTS_SUPPORT_WINDOW_SYSTEM_MOUSE_HPP

#include <QApplication>
#include <QEvent>
#include <QPointF>
#include <QWidget>
#include <QWindow>

QT_BEGIN_NAMESPACE
// QtGui's window-system entry for a mouse event, the one QTest drives. The
// event goes through QGuiApplication, which decides what a double click is,
// and through the widget window, which decides which events a widget gets.
// The name is QtGui's own.
// NOLINTNEXTLINE(readability-identifier-naming)
Q_GUI_EXPORT void qt_handleMouseEvent(QWindow* aWindow, const QPointF& aLocal, const QPointF& aGlobal, Qt::MouseButtons aState, Qt::MouseButton aButton, QEvent::Type aType,
									  Qt::KeyboardModifiers aModifiers, int aTimestamp);
QT_END_NAMESPACE

/// \file
/// \brief [[BPR-1156]]. Left-button mouse events as the window system reports
/// them, rather than events sent straight to a widget, so a test sees the
/// press, release, double-click and move sequence a hand produces.

namespace bpr::tests {
	/// \brief One left-button event at a point of `aWidget`, `aAfter`
	/// milliseconds after the previous one on the event clock.
	inline void window_system_mouse(QWidget& aWidget, const QPointF& aAt, const QEvent::Type aType, const Qt::MouseButtons aHeld, const int aAfter = 30)
	{
		static auto clock  = 100000;
		auto* const window = aWidget.window()->windowHandle();
		const auto  local  = QPointF{ aWidget.mapTo(aWidget.window(), aAt.toPoint()) };
		clock += aAfter;
		qt_handleMouseEvent(window, local, window->mapToGlobal(local), aHeld, aType == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, aType, Qt::NoModifier, clock);
		QApplication::processEvents();
	}

	/// \brief `aCount` clicks in quick succession at one point, each
	/// released, with the pointer drifting `aDrift` pixels while the last
	/// press is held, as a hand's does.
	inline void window_system_clicks(QWidget& aWidget, const QPointF& aAt, const int aCount, const qreal aDrift = 0.0)
	{
		for (int count = 1; count <= aCount; ++count) {
			const auto last = count == aCount;
			window_system_mouse(aWidget, aAt, QEvent::MouseButtonPress, Qt::LeftButton, count == 1 ? 2000 : 60);
			if (last && aDrift != 0.0) {
				window_system_mouse(aWidget, aAt + QPointF{ aDrift, 0.0 }, QEvent::MouseMove, Qt::LeftButton, 10);
				window_system_mouse(aWidget, aAt + QPointF{ aDrift, 1.0 }, QEvent::MouseMove, Qt::LeftButton, 10);
			}
			window_system_mouse(aWidget, aAt + QPointF{ last ? aDrift : 0.0, 0.0 }, QEvent::MouseButtonRelease, Qt::NoButton, 40);
		}
	}
}

#endif
