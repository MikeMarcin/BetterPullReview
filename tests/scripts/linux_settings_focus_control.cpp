#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QByteArray>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QSocketNotifier>
#include <QWidget>
#include <QWindow>
#include <QtGui/qguiapplication_platform.h>

#include <xcb/xcb.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unistd.h>
#include <utility>

// Native control for the Settings observer: two ordinary Qt windows, with no
// BetterPullReview composition, profile, setting, provider or storage code.
//
// BPR-1024. With `--commands`, standard input asks, one line at a time, for
// what the product asks for: `activate A` or `activate B` calls that window's
// activateWindow(), `focus` prints the window Qt treats as focused, and `quit`
// quits. Each window prints the keys Qt delivers to it and Qt prints every
// change of its focus window, so a course can compare the X11 window a key was
// sent to with the window Qt delivers it to. BPR-0975 adds `remap A` or
// `remap B`: hide() and show() that window, then raise() and activateWindow()
// it, so a compositor that activates only newly mapped windows can be told
// apart from one that refuses activation outright. The control runs on any Qt
// platform plugin, including `wayland`, and prints its plugin's name first.
//
// `--consume-activation-echo` also drops every `_NET_ACTIVE_WINDOW` client
// message before Qt handles it. Qt receives its own activation request, because
// it selects StructureNotify on the root, and treats it as focus-in on the
// requested window (qxcbwindow.cpp 1783-1784 in 6.11.2). Without that message
// Qt's focus window changes only on the X server's FocusIn.
namespace {
	void say(const std::string& aLine)
	{
		std::fputs((aLine + "\n").c_str(), stdout);
		std::fflush(stdout);
	}

	class control_window final : public QWidget
	{
	public:
		explicit control_window(std::string aName)
			: Name{ std::move(aName) }
		{
			setFocusPolicy(Qt::StrongFocus);
		}

		[[nodiscard]] auto name() const -> const std::string& { return Name; }

	protected:
		void keyPressEvent(QKeyEvent* aEvent) override
		{
			say("KEY:" + Name + ":" + aEvent->text().toStdString());
			aEvent->accept();
		}

	private:
		std::string Name;
	};

	class activation_echo_filter final : public QAbstractNativeEventFilter
	{
	public:
		explicit activation_echo_filter(xcb_atom_t aActiveWindow)
			: ActiveWindow{ aActiveWindow }
		{
		}

		auto nativeEventFilter(const QByteArray& aEventType, void* aMessage, qintptr* /*aResult*/) -> bool override
		{
			if (aEventType != "xcb_generic_event_t") {
				return false;
			}
			const auto* event = static_cast<const xcb_generic_event_t*>(aMessage);
			if ((event->response_type & 0x7fU) != XCB_CLIENT_MESSAGE) {
				return false;
			}
			const auto* message = static_cast<const xcb_client_message_event_t*>(aMessage);
			if (message->type != ActiveWindow) {
				return false;
			}
			say("ACTIVATION_ECHO_CONSUMED:" + std::to_string(message->window));
			return true;
		}

	private:
		xcb_atom_t ActiveWindow;
	};

	auto active_window_atom() -> xcb_atom_t
	{
		// The offscreen plugin also answers the X11 interface, without a connection.
		auto* x11        = qApp->nativeInterface<QNativeInterface::QX11Application>();
		auto* connection = x11 != nullptr ? x11->connection() : nullptr;
		if (connection == nullptr) {
			return XCB_ATOM_NONE;
		}
		constexpr auto name  = std::string_view{ "_NET_ACTIVE_WINDOW" };
		auto*          reply = xcb_intern_atom_reply(connection, xcb_intern_atom(connection, 0, static_cast<std::uint16_t>(name.size()), name.data()), nullptr);
		if (reply == nullptr) {
			return XCB_ATOM_NONE;
		}
		const auto atom = reply->atom;
		std::free(reply);
		return atom;
	}

	auto has_argument(int aArgc, char** aArgv, std::string_view aName) -> bool
	{
		const auto arguments = std::span{ aArgv, static_cast<std::size_t>(aArgc) }.subspan(1);
		return std::ranges::any_of(arguments, [&](const char* aArgument) { return aName == aArgument; });
	}

	auto window_name(const QWindow* aWindow, const control_window& aFirst, const control_window& aSecond) -> std::string
	{
		if (aWindow == nullptr) {
			return "none";
		}
		if (aWindow == aFirst.windowHandle()) {
			return aFirst.name();
		}
		return aWindow == aSecond.windowHandle() ? aSecond.name() : "other";
	}

	void run_command(const std::string& aCommand, control_window& aFirst, control_window& aSecond)
	{
		if (aCommand == "activate A" || aCommand == "activate B") {
			auto& target = aCommand.back() == 'A' ? aFirst : aSecond;
			target.activateWindow();
			say("ACTIVATE_REQUESTED:" + target.name());
		} else if (aCommand == "remap A" || aCommand == "remap B") {
			auto& target = aCommand.back() == 'A' ? aFirst : aSecond;
			target.hide();
			target.show();
			target.raise();
			target.activateWindow();
			say("REMAP_REQUESTED:" + target.name());
		} else if (aCommand == "focus") {
			say("QT_FOCUS_WINDOW:" + window_name(QGuiApplication::focusWindow(), aFirst, aSecond));
		} else if (aCommand == "quit") {
			QCoreApplication::quit();
		} else {
			say("COMMAND_UNKNOWN:" + aCommand);
		}
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	QApplication application{ aArgc, aArgv };
	auto         echo = std::unique_ptr<activation_echo_filter>{ };
	if (has_argument(aArgc, aArgv, "--consume-activation-echo")) {
		const auto atom = active_window_atom();
		if (atom == XCB_ATOM_NONE) {
			say("ACTIVATION_ECHO_FILTER:UNAVAILABLE " + QGuiApplication::platformName().toStdString());
			return 2;
		}
		echo = std::make_unique<activation_echo_filter>(atom);
		application.installNativeEventFilter(echo.get());
		say("ACTIVATION_ECHO_FILTER:INSTALLED " + std::to_string(atom));
	}
	control_window first{ "A" };
	first.setWindowTitle(QStringLiteral("BPR0018 native focus control A"));
	first.resize(900, 600);
	first.show();
	control_window second{ "B" };
	second.setWindowTitle(QStringLiteral("BPR0018 native focus control B"));
	second.resize(760, 480);
	second.show();
	second.raise();
	second.activateWindow();

	auto commands = std::unique_ptr<QSocketNotifier>{ };
	auto pending  = std::string{ };
	if (has_argument(aArgc, aArgv, "--commands")) {
		say("PLATFORM:" + QGuiApplication::platformName().toStdString());
		QObject::connect(&application, &QGuiApplication::focusWindowChanged, [&](QWindow* aWindow) {
			say("QT_FOCUS_CHANGED:" + window_name(aWindow, first, second));
		});
		commands = std::make_unique<QSocketNotifier>(STDIN_FILENO, QSocketNotifier::Read);
		QObject::connect(commands.get(), &QSocketNotifier::activated, [&] {
			char       buffer[256];
			const auto count = ::read(STDIN_FILENO, buffer, sizeof buffer);
			if (count <= 0) {
				commands->setEnabled(false);
				return;
			}
			pending.append(buffer, static_cast<std::size_t>(count));
			for (auto end = pending.find('\n'); end != std::string::npos; end = pending.find('\n')) {
				const auto command = pending.substr(0, end);
				pending.erase(0, end + 1);
				run_command(command, first, second);
			}
		});
	}
	return application.exec();
}
