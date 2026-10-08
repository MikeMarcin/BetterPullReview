#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_RESIZE_HANDLE_CURSOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_RESIZE_HANDLE_CURSOR_HPP

class QWidget;
class QWindow;

/// \file
/// \brief [[BPR-1330]]. One cursor hold shared by every resize seam.
/// \details The owner, 2026-10-08, on the Overview summary seam: "The resize
/// handle mouse cursor on the overview page below summary seems to flicker to
/// the correct cursor and then return to the normal pointer like the drawer
/// resize handle used to". The drawer's report ([[BPR-0739]], [[BPR-0746]])
/// was never closed by a change: the owner later found it working on the same
/// program, and BPR-0746's first hypothesis stayed open -- Qt applies a
/// handle's cursor once, on the pointer's arrival, and both its window layer
/// and the Cocoa plugin return early while they believe that shape is still
/// shown, so a cursor the platform replaced behind Qt's back is never put
/// back while the pointer stays on the seam.
///
/// The hold closes that gap for any seam it is installed on. On the pointer's
/// arrival, on every move over the seam and after the seam's tooltip opens it
/// re-applies the seam's own cursor, once the event that caused it has been
/// handled: to the native window when Qt's record has drifted from the seam's
/// shape, and on macOS to AppKit through the window's own view, which sets the
/// cursor that view already holds. It sets nothing while the pointer is not on
/// the seam or the seam is disabled, and it never shows another shape in
/// between, so it cannot itself flicker.
namespace bpr::qt {
	/// \brief Installs the cursor hold on `aHandle`; the hold is its child.
	void hold_resize_cursor(QWidget& aHandle);

	/// \brief Has the platform show `aWindow`'s cursor again, on macOS
	/// through AppKit; elsewhere the platform re-applies a window's cursor on
	/// every pointer move by itself, so this does nothing.
	void refresh_platform_cursor(QWindow& aWindow);
} // namespace bpr::qt
#endif
