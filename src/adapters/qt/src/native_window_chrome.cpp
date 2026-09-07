#include "bpr/qt/native_window_chrome.hpp"

#include <QGuiApplication>
#include <QLatin1StringView>
#include <QWidget>

#if defined(__APPLE__)
	#include <objc/message.h>
	#include <objc/runtime.h>
#endif

namespace bpr::qt {
#if defined(__APPLE__)
	void hide_native_title_text(QWidget& aWindow)
	{
		// Only the Cocoa platform hands out an NSView as the window id; the
		// offscreen platform the tests run under hands out something else,
		// and sending it a message is a crash rather than a no-op.
		if (QGuiApplication::platformName() != QLatin1String{ "cocoa" }) {
			return;
		}
		// The window id of a top-level widget on macOS is its NSView; the
		// NSWindow is reached through it. `NSWindowTitleHidden` is 1.
		// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast,
		// performance-no-int-to-ptr)
		auto* const view = reinterpret_cast<id>(aWindow.winId());
		if (view == nullptr) {
			return;
		}
		using object_message = id (*)(id, SEL);
		using long_message   = void (*)(id, SEL, long);
		auto* const window   = reinterpret_cast<object_message>(objc_msgSend)(
			view, sel_registerName("window"));
		if (window == nullptr) {
			return;
		}
		reinterpret_cast<long_message>(objc_msgSend)(
			window, sel_registerName("setTitleVisibility:"), 1L);
		// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast,
		// performance-no-int-to-ptr)
	}
#else
	void hide_native_title_text(QWidget& aWindow)
	{
		static_cast<void>(aWindow);
	}
#endif
} // namespace bpr::qt
