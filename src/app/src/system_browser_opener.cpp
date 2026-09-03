#include "system_browser_opener.hpp"

#include <QDesktopServices>
#include <QString>
#include <QUrl>

#include <optional>
#include <string>
#include <string_view>

namespace bpr::app {
	namespace {
		// The only scheme this opener will hand to the platform. The compiled
		// profile authority the host checks against names no other one, and a
		// launch service will happily open `file:` or a registered custom
		// scheme for whoever asks.
		constexpr auto browser_scheme = "https";

		// The whole effect, in one function so the rest of this file can be
		// tested without it. `openUrl` asks the desktop which program handles
		// this scheme and starts it detached; it opens nothing in process and
		// returns without waiting for the browser to render anything.
		auto launch_in_platform_browser(const QUrl& aAddress) -> bool
		{
			return QDesktopServices::openUrl(aAddress);
		}

#if defined(BPR_BUILD_TESTING)
		// The launch the next `open` will finish with. A function pointer
		// rather than a virtual, so the opener stays `final` and a shipped
		// image carries neither the indirection nor a way to redirect it.
		auto installed_launch() -> detail::browser_launch_function&
		{
			static auto launch = detail::browser_launch_function{
				&launch_in_platform_browser
			};
			return launch;
		}
#endif
	} // namespace

	auto validated_browser_address(const std::string_view aAddress)
		-> std::optional<QUrl>
	{
		// Strict parsing, and no repair. `QUrl::fromUserInput` guesses at
		// what a human meant and would turn a rejected address into an
		// accepted one; there is no human here, only an address the host has
		// already validated, so anything that does not parse exactly is a
		// refusal rather than a hint.
		auto address = QUrl{
			QString::fromUtf8(aAddress.data(), static_cast<qsizetype>(aAddress.size())),
			QUrl::StrictMode
		};
		if (!address.isValid() || address.isRelative()
			|| address.scheme() != QLatin1String{ browser_scheme }
			|| address.host().isEmpty() || !address.userInfo().isEmpty()) {
			return std::nullopt;
		}
		return address;
	}

#if defined(BPR_BUILD_TESTING)
	namespace detail {
		auto substitute_browser_launch(const browser_launch_function aLaunch)
			-> browser_launch_function
		{
			auto&      slot     = installed_launch();
			const auto replaced = slot;
			slot                = aLaunch != nullptr ? aLaunch : &launch_in_platform_browser;
			return replaced;
		}
	} // namespace detail
#endif

	// Defined out of line so the shipped image carries a definition the
	// composed-host audit can find by name in both graphs. An opener that is
	// linked but never constructed, or constructed but never bound, is the
	// state this whole slice existed to leave behind.
	system_browser_opener::system_browser_opener() noexcept = default;

	auto system_browser_opener::open(const std::string_view aAddress) -> bool
	{
		auto address = validated_browser_address(aAddress);
		if (!address.has_value()) {
			return false;
		}

		// Reached only by an address that passed, which is the property the
		// tests-enabled build substitutes a counting stand-in to observe.
#if defined(BPR_BUILD_TESTING)
		return installed_launch()(*address);
#else
		return launch_in_platform_browser(*address);
#endif
	}
} // namespace bpr::app
