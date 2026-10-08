#include "bpr/qt/resize_handle_cursor.hpp"

#import <AppKit/AppKit.h>

#include <QGuiApplication>
#include <QLatin1StringView>
#include <QWindow>

namespace bpr::qt {
	void refresh_platform_cursor(QWindow& aWindow)
	{
		// Only the Cocoa platform hands out an NSView as the window id; the
		// offscreen platform the tests run under hands out something else.
		if (QGuiApplication::platformName() != QLatin1String{ "cocoa" } || aWindow.handle() == nullptr) {
			return;
		}
		auto* const view = (__bridge NSView*)reinterpret_cast<void*>(aWindow.winId()); // NOLINT(performance-no-int-to-ptr)
		if (view == nil || view.window == nil || !NSApp.active) {
			return;
		}
		// Qt's view answers a cursor update by setting the cursor it holds for
		// the window -- the one Qt last applied, here the seam's. AppKit
		// otherwise sends it only when the pointer crosses into the view, and
		// Qt's own setter returns early for an unchanged shape, so neither
		// would put back a cursor the platform replaced while the pointer
		// stayed on the seam.
		const auto location = view.window.mouseLocationOutsideOfEventStream;
		if ([view hitTest:[view.superview convertPoint:location fromView:nil]] != view) {
			return;
		}
		[view cursorUpdate:[NSEvent enterExitEventWithType:NSEventTypeCursorUpdate
												  location:location
											 modifierFlags:0
												 timestamp:0
											  windowNumber:view.window.windowNumber
												   context:nil
											   eventNumber:0
											trackingNumber:0
												  userData:nullptr]];
	}
} // namespace bpr::qt
