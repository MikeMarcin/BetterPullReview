#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <cstring>
#include <unistd.h>

// Toolkit-free native control for the Settings observer (BPR-0975): two plain
// Xlib windows that never request input focus. If the X11 focus revert seen
// with the Qt control also happens here, the toolkit does not cause it.
namespace {
	auto make_window(Display* aDisplay, Atom aDeleteWindow, const char* aTitle, unsigned aWidth, unsigned aHeight) -> Window
	{
		const auto window = XCreateSimpleWindow(aDisplay, DefaultRootWindow(aDisplay), 0, 0, aWidth, aHeight, 0, 0, 0xffffffUL);
		const long pid    = static_cast<long>(::getpid());
		auto       hints  = XWMHints{ };
		hints.flags       = InputHint;
		hints.input       = True;
		XStoreName(aDisplay, window, aTitle);
		XChangeProperty(aDisplay, window, XInternAtom(aDisplay, "_NET_WM_NAME", False), XInternAtom(aDisplay, "UTF8_STRING", False), 8, PropModeReplace,
						reinterpret_cast<const unsigned char*>(aTitle), static_cast<int>(std::strlen(aTitle)));
		XChangeProperty(aDisplay, window, XInternAtom(aDisplay, "_NET_WM_PID", False), XA_CARDINAL, 32, PropModeReplace,
						reinterpret_cast<const unsigned char*>(&pid), 1);
		XSetWMHints(aDisplay, window, &hints);
		XSetWMProtocols(aDisplay, window, &aDeleteWindow, 1);
		XMapWindow(aDisplay, window);
		return window;
	}
}

auto main() -> int
{
	auto* display = XOpenDisplay(nullptr);
	if (display == nullptr) {
		return 2;
	}
	const auto deleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
	const auto first        = make_window(display, deleteWindow, "BPR0975 Xlib focus control A", 900U, 600U);
	const auto second       = make_window(display, deleteWindow, "BPR0975 Xlib focus control B", 760U, 480U);
	auto       open         = 2;
	while (open > 0) {
		auto event = XEvent{ };
		XNextEvent(display, &event);
		if (event.type == ClientMessage && static_cast<Atom>(event.xclient.data.l[0]) == deleteWindow
			&& (event.xclient.window == first || event.xclient.window == second)) {
			XDestroyWindow(display, event.xclient.window);
			--open;
		}
	}
	XCloseDisplay(display);
	return 0;
}
