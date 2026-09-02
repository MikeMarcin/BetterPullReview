#ifndef BPR_SRC_APP_SRC_SYSTEM_BROWSER_OPENER_HPP
#define BPR_SRC_APP_SRC_SYSTEM_BROWSER_OPENER_HPP

#include "bpr/application/device_authorization_application_host.hpp"

#include <string_view>

namespace bpr::app {
	/// \brief Opens one validated address in the operating system's browser.
	///
	/// \details The production implementation of
	/// `device_authorization_browser_port`, and the reason it lives in
	/// composition rather than in the Qt adapter: [[BPR-0280]] declared the
	/// port because `VerifyArchitectureBoundaries.cmake` refuses `QUrl`,
	/// `QDesktopServices`, and a literal scheme anywhere under
	/// `src/adapters/qt/`, so the presentation adapter cannot hold an address
	/// and cannot acquire an opener. Composition can, and this is the one
	/// place in the shipped graph that hands an address to the platform.
	///
	/// It defers entirely to the platform's own launch service —
	/// `LSOpenCFURLRef` on macOS, `xdg-open` on Linux, `ShellExecute` on
	/// Windows, all of which `QDesktopServices::openUrl` reaches. That is the
	/// whole implementation: no embedded web view, no browser engine, no
	/// network of its own, no loopback listener, and no process this
	/// application supervises afterwards. Whatever the reviewer already
	/// trusts to open links is what opens, with the reviewer's own cookies in
	/// the reviewer's own browser, which is the point of delegating
	/// authentication at all.
	///
	/// The address arrives already validated twice: once by the wire parser
	/// against the server authority, and once by the device-authorization
	/// host against the compiled profile authority immediately before this
	/// call. This refuses anything that is not an absolute `https` address
	/// with no userinfo anyway, because an opener that trusts its caller
	/// completely is one bad call away from launching a `file:` or a
	/// `javascript:` address, and refusing costs one parse.
	///
	/// `false` is an ordinary answer. The host reports it as `failed`, the
	/// user code and the address stay on screen, and the reviewer finishes on
	/// whatever device they like.
	class system_browser_opener final
		: public application::device_authorization_browser_port
	{
	public:
		system_browser_opener() noexcept;

		[[nodiscard]] auto open(std::string_view aAddress) -> bool override;
	};
} // namespace bpr::app

#endif // BPR_SRC_APP_SRC_SYSTEM_BROWSER_OPENER_HPP
