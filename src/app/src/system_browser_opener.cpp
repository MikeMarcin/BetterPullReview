#include "system_browser_opener.hpp"

#include <QDesktopServices>
#include <QString>
#include <QUrl>

#include <string>
#include <string_view>

namespace bpr::app {
	namespace {
		// The only scheme this opener will hand to the platform. The compiled
		// profile authority the host checks against names no other one, and a
		// launch service will happily open `file:` or a registered custom
		// scheme for whoever asks.
		constexpr auto browser_scheme = "https";
	} // namespace

	// Defined out of line so the shipped image carries a definition the
	// composed-host audit can find by name in both graphs. An opener that is
	// linked but never constructed, or constructed but never bound, is the
	// state this whole slice existed to leave behind.
	system_browser_opener::system_browser_opener() noexcept = default;

	auto system_browser_opener::open(const std::string_view aAddress) -> bool
	{
		// Strict parsing, and no repair. `QUrl::fromUserInput` guesses at
		// what a human meant and would turn a rejected address into an
		// accepted one; there is no human here, only an address the host has
		// already validated, so anything that does not parse exactly is a
		// refusal rather than a hint.
		const auto address = QUrl{
			QString::fromUtf8(aAddress.data(), static_cast<qsizetype>(aAddress.size())),
			QUrl::StrictMode
		};
		if (!address.isValid() || address.isRelative()
			|| address.scheme() != QLatin1String{ browser_scheme }
			|| address.host().isEmpty() || !address.userInfo().isEmpty()) {
			return false;
		}

		// The whole effect. `openUrl` asks the desktop which program handles
		// this scheme and starts it detached; it opens nothing in process and
		// this returns without waiting for the browser to render anything.
		return QDesktopServices::openUrl(address);
	}
} // namespace bpr::app
