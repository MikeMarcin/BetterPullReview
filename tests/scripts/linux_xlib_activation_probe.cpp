#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <poll.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

// Toolkit-free probe of every activation request an X11 client can make
// (BPR-0975). Two plain Xlib windows, A and B, mapped in that order. Standard
// input carries one command per line; `wslg_activation_probe_course.py` sends
// them and reads the replies:
//
//   net-active W SOURCE TIME  EWMH _NET_ACTIVE_WINDOW for W, sent to the root
//                             with SOURCE 0, 1 (application) or 2 (pager)
//   set-focus W TIME          XSetInputFocus(W, RevertToParent, TIME)
//   raise W                   XRaiseWindow(W)
//   remap W                   XUnmapWindow(W), then XMapWindow(W) once unmapped
//   quit                      destroy both windows and exit 0
//
// TIME is `0` (CurrentTime), `user` (the time of the last key either window
// received, which also becomes W's _NET_WM_USER_TIME first, as toolkits do) or
// `server` (a fresh X server time from a PropertyNotify round trip). Every
// FocusIn, FocusOut, key, map and unmap on A or B is printed as it arrives.
// Closing both windows natively (WM_DELETE_WINDOW) also exits 0.
namespace {
	struct probe
	{
		Display*              Connection = nullptr;
		std::array<Window, 2> Windows{ };
		std::array<bool, 2>   Open{ true, true };
		std::array<int, 2>    Unmaps{ };
		std::array<int, 2>    Maps{ };
		Atom                  DeleteWindow   = None;
		Atom                  ActiveWindow   = None;
		Atom                  UserTime       = None;
		Atom                  TimeProbe      = None;
		Time                  LastUserTime   = CurrentTime;
		Window                LastFocusIn    = None;
		Time                  LastServerTime = CurrentTime;
	};

	void say(const std::string& aLine)
	{
		std::fputs((aLine + "\n").c_str(), stdout);
		std::fflush(stdout);
	}

	auto name_of(const probe& aProbe, Window aWindow) -> std::string
	{
		if (aWindow == aProbe.Windows[0]) {
			return "A";
		}
		if (aWindow == aProbe.Windows[1]) {
			return "B";
		}
		return std::to_string(aWindow);
	}

	auto window_named(const probe& aProbe, std::string_view aName) -> Window
	{
		if (aName == "A") {
			return aProbe.Windows[0];
		}
		return aName == "B" ? aProbe.Windows[1] : Window{ None };
	}

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
		XSelectInput(aDisplay, window, FocusChangeMask | KeyPressMask | StructureNotifyMask | PropertyChangeMask);
		XMapWindow(aDisplay, window);
		return window;
	}

	/// \brief Prints one event on A or B and keeps the probe's state; false once both are closed.
	auto handle(probe& aProbe, const XEvent& aEvent) -> bool
	{
		const auto which = name_of(aProbe, aEvent.xany.window);
		switch (aEvent.type) {
		case FocusIn:
		case FocusOut:
			if (aEvent.type == FocusIn && aEvent.xfocus.detail != NotifyPointer) {
				aProbe.LastFocusIn = aEvent.xfocus.window;
			}
			say(std::string{ aEvent.type == FocusIn ? "FOCUS_IN:" : "FOCUS_OUT:" } + which + ":mode=" + std::to_string(aEvent.xfocus.mode)
				+ ":detail=" + std::to_string(aEvent.xfocus.detail));
			break;
		case KeyPress:
			aProbe.LastUserTime = aEvent.xkey.time;
			XChangeProperty(aProbe.Connection, aEvent.xkey.window, aProbe.UserTime, XA_CARDINAL, 32, PropModeReplace,
							reinterpret_cast<const unsigned char*>(&aProbe.LastUserTime), 1);
			say("KEY:" + which + ":" + std::to_string(aEvent.xkey.time));
			break;
		case MapNotify:
		case UnmapNotify:
			for (auto index = 0U; index < aProbe.Windows.size(); ++index) {
				if (aEvent.xany.window == aProbe.Windows[index]) {
					++(aEvent.type == MapNotify ? aProbe.Maps : aProbe.Unmaps)[index];
				}
			}
			say(std::string{ aEvent.type == MapNotify ? "MAPPED:" : "UNMAPPED:" } + which);
			break;
		case PropertyNotify:
			if (aEvent.xproperty.atom == aProbe.TimeProbe) {
				aProbe.LastServerTime = aEvent.xproperty.time;
			}
			break;
		case ClientMessage:
			if (static_cast<Atom>(aEvent.xclient.data.l[0]) == aProbe.DeleteWindow) {
				for (auto index = 0U; index < aProbe.Windows.size(); ++index) {
					if (aProbe.Open[index] && aEvent.xclient.window == aProbe.Windows[index]) {
						XDestroyWindow(aProbe.Connection, aProbe.Windows[index]);
						aProbe.Open[index] = false;
						say("CLOSED:" + which);
					}
				}
			}
			break;
		default:
			break;
		}
		return aProbe.Open[0] || aProbe.Open[1];
	}

	/// \brief Handles queued events until `aDone` holds or two seconds pass.
	template <typename TDone>
	auto pump_until(probe& aProbe, TDone aDone) -> bool
	{
		for (auto round = 0; round < 200 && !aDone(); ++round) {
			while (XPending(aProbe.Connection) > 0) {
				auto event = XEvent{ };
				XNextEvent(aProbe.Connection, &event);
				handle(aProbe, event);
			}
			if (!aDone()) {
				auto descriptor = pollfd{ ConnectionNumber(aProbe.Connection), POLLIN, 0 };
				::poll(&descriptor, 1, 10);
			}
		}
		return aDone();
	}

	auto server_time(probe& aProbe) -> Time
	{
		aProbe.LastServerTime = CurrentTime;
		XChangeProperty(aProbe.Connection, aProbe.Windows[0], aProbe.TimeProbe, XA_CARDINAL, 32, PropModeAppend, nullptr, 0);
		XFlush(aProbe.Connection);
		pump_until(aProbe, [&] { return aProbe.LastServerTime != CurrentTime; });
		return aProbe.LastServerTime;
	}

	auto time_of(probe& aProbe, std::string_view aMode, Window aTarget) -> Time
	{
		if (aMode == "user") {
			XChangeProperty(aProbe.Connection, aTarget, aProbe.UserTime, XA_CARDINAL, 32, PropModeReplace,
							reinterpret_cast<const unsigned char*>(&aProbe.LastUserTime), 1);
			return aProbe.LastUserTime;
		}
		return aMode == "server" ? server_time(aProbe) : CurrentTime;
	}

	void net_active(probe& aProbe, Window aTarget, long aSource, Time aTime)
	{
		auto event                 = XEvent{ };
		event.xclient.type         = ClientMessage;
		event.xclient.send_event   = True;
		event.xclient.display      = aProbe.Connection;
		event.xclient.window       = aTarget;
		event.xclient.message_type = aProbe.ActiveWindow;
		event.xclient.format       = 32;
		event.xclient.data.l[0]    = aSource;
		event.xclient.data.l[1]    = static_cast<long>(aTime);
		event.xclient.data.l[2]    = static_cast<long>(aProbe.LastFocusIn);
		XSendEvent(aProbe.Connection, DefaultRootWindow(aProbe.Connection), False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
		XFlush(aProbe.Connection);
		say("SENT:net-active:" + name_of(aProbe, aTarget) + ":source=" + std::to_string(aSource) + ":time=" + std::to_string(aTime)
			+ ":current=" + name_of(aProbe, aProbe.LastFocusIn));
	}

	void remap(probe& aProbe, Window aTarget)
	{
		const auto index  = aTarget == aProbe.Windows[0] ? 0U : 1U;
		const auto unmaps = aProbe.Unmaps[index];
		const auto maps   = aProbe.Maps[index];
		XUnmapWindow(aProbe.Connection, aTarget);
		XFlush(aProbe.Connection);
		// Qt's hide() and show() make the same two requests.
		const auto unmapped = pump_until(aProbe, [&] { return aProbe.Unmaps[index] > unmaps; });
		XMapWindow(aProbe.Connection, aTarget);
		XFlush(aProbe.Connection);
		const auto mapped = pump_until(aProbe, [&] { return aProbe.Maps[index] > maps; });
		say("SENT:remap:" + name_of(aProbe, aTarget) + ":unmapped=" + std::to_string(unmapped) + ":mapped=" + std::to_string(mapped));
	}

	/// \brief Runs one command line; false for `quit`.
	auto run_command(probe& aProbe, const std::string& aLine) -> bool
	{
		auto       words = std::array<std::string, 4>{ };
		auto       count = 0U;
		const auto text  = std::string_view{ aLine };
		for (auto begin = std::size_t{ 0 }; begin < text.size() && count < words.size();) {
			const auto end = text.find(' ', begin);
			words[count++] = std::string{ text.substr(begin, end == std::string_view::npos ? std::string_view::npos : end - begin) };
			begin          = end == std::string_view::npos ? text.size() : end + 1;
		}
		if (words[0] == "quit") {
			return false;
		}
		const auto target = window_named(aProbe, words[1]);
		if (target == None) {
			say("COMMAND_UNKNOWN:" + aLine);
		} else if (words[0] == "net-active" && count == 4) {
			const auto source = std::stol(words[2]);
			net_active(aProbe, target, source, time_of(aProbe, words[3], target));
		} else if (words[0] == "set-focus" && count == 3) {
			const auto time = time_of(aProbe, words[2], target);
			XSetInputFocus(aProbe.Connection, target, RevertToParent, time);
			XFlush(aProbe.Connection);
			say("SENT:set-focus:" + words[1] + ":time=" + std::to_string(time));
		} else if (words[0] == "raise") {
			XRaiseWindow(aProbe.Connection, target);
			XFlush(aProbe.Connection);
			say("SENT:raise:" + words[1]);
		} else if (words[0] == "remap") {
			remap(aProbe, target);
		} else {
			say("COMMAND_UNKNOWN:" + aLine);
		}
		return true;
	}
}

auto main() -> int
{
	auto probeState       = probe{ };
	probeState.Connection = XOpenDisplay(nullptr);
	if (probeState.Connection == nullptr) {
		return 2;
	}
	auto* const display     = probeState.Connection;
	probeState.DeleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
	probeState.ActiveWindow = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
	probeState.UserTime     = XInternAtom(display, "_NET_WM_USER_TIME", False);
	probeState.TimeProbe    = XInternAtom(display, "_BPR_ACTIVATION_PROBE_TIME", False);
	probeState.Windows[0]   = make_window(display, probeState.DeleteWindow, "BPR0975 activation probe A", 900U, 600U);
	probeState.Windows[1]   = make_window(display, probeState.DeleteWindow, "BPR0975 activation probe B", 760U, 480U);
	XFlush(display);
	say("READY:A=" + std::to_string(probeState.Windows[0]) + ":B=" + std::to_string(probeState.Windows[1]));

	auto pending = std::string{ };
	auto running = true;
	while (running) {
		while (running && XPending(display) > 0) {
			auto event = XEvent{ };
			XNextEvent(display, &event);
			running = handle(probeState, event);
		}
		if (!running) {
			break;
		}
		auto descriptors = std::array<pollfd, 2>{ pollfd{ ConnectionNumber(display), POLLIN, 0 }, pollfd{ STDIN_FILENO, POLLIN, 0 } };
		if (::poll(descriptors.data(), descriptors.size(), 100) <= 0 || (descriptors[1].revents & (POLLIN | POLLHUP)) == 0) {
			continue;
		}
		auto       buffer = std::array<char, 256>{ };
		const auto read   = ::read(STDIN_FILENO, buffer.data(), buffer.size());
		if (read <= 0) {
			running = false;
			break;
		}
		pending.append(buffer.data(), static_cast<std::size_t>(read));
		for (auto end = pending.find('\n'); running && end != std::string::npos; end = pending.find('\n')) {
			const auto line = pending.substr(0, end);
			pending.erase(0, end + 1);
			running = run_command(probeState, line);
		}
	}
	for (auto index = 0U; index < probeState.Windows.size(); ++index) {
		if (probeState.Open[index]) {
			XDestroyWindow(display, probeState.Windows[index]);
		}
	}
	XCloseDisplay(display);
	say("EXIT:0");
	return 0;
}
