#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_NATIVE_WINDOW_CHROME_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_NATIVE_WINDOW_CHROME_HPP

class QWidget;

namespace bpr::qt {
	/// \brief Hides the platform title text of a shown top-level window.
	///
	/// \details [[BPR-0646]]. RFC-BPR-0028 makes the strip the title row, and
	/// the identity the strip shows is the title. Qt's expanded client area
	/// puts the content under the title bar and drops its background, but
	/// still draws the title text over it; on macOS this asks the window to
	/// stop, through the Objective-C runtime and without an Objective-C++
	/// translation unit. Elsewhere it does nothing. The window keeps its
	/// title for the Dock, Mission Control and the Window menu.
	///
	/// \pre The window has been shown, so its native window exists.
	void hide_native_title_text(QWidget& aWindow);
} // namespace bpr::qt

#endif
