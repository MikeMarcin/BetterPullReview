#ifndef BPR_TESTS_SUPPORT_RESIZE_CURSOR_HOLD_CHECK_HPP
#define BPR_TESTS_SUPPORT_RESIZE_CURSOR_HOLD_CHECK_HPP

#include "window_system_mouse.hpp"

#include <QApplication>
#include <QCursor>
#include <QHelpEvent>
#include <QSplitterHandle>
#include <QToolTip>
#include <QWindow>

#include <iostream>
#include <string_view>

/// \file
/// \brief [[BPR-1330]]. Whether a resize seam keeps showing its cursor while
/// the pointer stays on it.
/// \details The owner saw the resize cursor appear on arrival and then the
/// arrow while the pointer was still on the seam, first on the drawer seams
/// ([[BPR-0739]]) and then on the Overview summary seam. Qt applies a seam's
/// cursor once, on arrival, and returns early afterwards while it believes the
/// shape is shown, so a cursor the platform replaced is not put back. The
/// offscreen platform has no cursor of its own; the native window's cursor is
/// what it would show, so the check replaces that with the arrow, as the
/// owner's platform did, and asks for the seam's cursor back after each move
/// over the seam and after its tooltip opens with the pointer at rest. Every
/// pointer event goes through the window system, as a hand's does.

namespace bpr::tests {
	namespace resize_cursor_detail {
		inline void settle()
		{
			for (auto pass = 0; pass < 5; ++pass) {
				QApplication::processEvents();
			}
		}
	}

	/// \brief True when `aHandle`'s window shows `aShape` on arrival, again
	/// after each of several moves over the seam and after its tooltip opens
	/// once the arrow has replaced it, and the seam stops asserting it once
	/// the pointer has left. Reports the first moment that fails.
	inline auto resize_cursor_holds(QSplitterHandle& aHandle, const Qt::CursorShape aShape, const std::string_view aSeam) -> bool
	{
		auto* const window   = aHandle.window()->windowHandle();
		const auto  vertical = aHandle.orientation() == Qt::Vertical;
		const auto  centre   = QPointF{ aHandle.rect().center() };
		const auto  across   = [&](const qreal aDistance) { return vertical ? QPointF{ 0.0, aDistance } : QPointF{ aDistance, 0.0 }; };
		const auto  along    = [&](const qreal aDistance) { return vertical ? QPointF{ aDistance, 0.0 } : QPointF{ 0.0, aDistance }; };
		const auto  shows    = [&](const std::string_view aMoment) {
			if (aHandle.underMouse() && window->cursor().shape() == aShape) {
				return true;
			}
			std::cerr << aSeam << ", " << aMoment << ": handle underMouse " << aHandle.underMouse()
					  << ", window cursor " << static_cast<int>(window->cursor().shape())
					  << ", seam cursor " << static_cast<int>(aShape) << '\n';
			return false;
		};

		window_system_mouse(aHandle, centre + across(-24.0), QEvent::MouseMove, Qt::NoButton);
		window_system_mouse(aHandle, centre, QEvent::MouseMove, Qt::NoButton);
		resize_cursor_detail::settle();
		if (!shows("on arrival")) {
			return false;
		}
		for (auto step = 1; step <= 4; ++step) {
			window->setCursor(Qt::ArrowCursor);
			window_system_mouse(aHandle, centre + along(3.0 * step) + across(step % 2 == 0 ? 0.0 : 1.0), QEvent::MouseMove, Qt::NoButton, 16);
			resize_cursor_detail::settle();
			if (!shows("after a move over the seam")) {
				return false;
			}
		}
		window->setCursor(Qt::ArrowCursor);
		const auto at  = (centre + along(12.0)).toPoint();
		auto       tip = QHelpEvent{ QEvent::ToolTip, at, aHandle.mapToGlobal(at) };
		QApplication::sendEvent(&aHandle, &tip);
		resize_cursor_detail::settle();
		QToolTip::hideText();
		if (!shows("after the tooltip opened with the pointer at rest")) {
			return false;
		}
		window_system_mouse(aHandle, centre + across(-24.0), QEvent::MouseMove, Qt::NoButton);
		window->setCursor(Qt::ArrowCursor);
		resize_cursor_detail::settle();
		if (aHandle.underMouse() || window->cursor().shape() == aShape) {
			std::cerr << aSeam << ", after leaving: the seam still asserts its cursor\n";
			return false;
		}
		return true;
	}
}

#endif
