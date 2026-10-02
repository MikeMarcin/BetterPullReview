#ifndef BPR_TESTS_SUPPORT_WINDOW_SYSTEM_KEYS_HPP
#define BPR_TESTS_SUPPORT_WINDOW_SYSTEM_KEYS_HPP

#include <QApplication>
#include <QEvent>
#include <QString>
#include <QWidget>
#include <QWindow>

QT_BEGIN_NAMESPACE
// QtGui's window-system entry for a key event, the one QTest drives. The event
// goes through QGuiApplication, the window's shortcut map and focus routing,
// as a key a hand presses does. The name is QtGui's own.
// NOLINTNEXTLINE(readability-identifier-naming)
Q_GUI_EXPORT void qt_handleKeyEvent(QWindow* aWindow, QEvent::Type aType, int aKey, Qt::KeyboardModifiers aModifiers, const QString& aText, bool aAutorepeat, ushort aCount);
QT_END_NAMESPACE

/// \file
/// \brief [[BPR-1157]]. Keys as the window system reports them to the window
/// that holds `aWidget`, rather than events sent straight to a widget, so the
/// window's shortcuts and focus decide where a key goes.

namespace bpr::tests {
	/// \brief One press and release of `aKey` with `aModifiers`.
	inline void window_system_key(QWidget& aWidget, const int aKey, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier, const QString& aText = { })
	{
		auto* const window = aWidget.window()->windowHandle();
		qt_handleKeyEvent(window, QEvent::KeyPress, aKey, aModifiers, aText, false, 1);
		qt_handleKeyEvent(window, QEvent::KeyRelease, aKey, aModifiers, aText, false, 1);
		QApplication::processEvents();
	}
}

#endif
