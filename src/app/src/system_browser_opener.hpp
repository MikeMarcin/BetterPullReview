#ifndef BPR_SRC_APP_SRC_SYSTEM_BROWSER_OPENER_HPP
#define BPR_SRC_APP_SRC_SYSTEM_BROWSER_OPENER_HPP

#include "bpr/application/device_authorization_application_host.hpp"

#include <QUrl>

#include <optional>
#include <string_view>

namespace bpr::app {
	/// \brief The parsed address `open` would hand to the platform, or nothing.
	///
	/// \details Split out of `open` by [[BPR-0537]] so the decision is
	/// observable without the effect. The whole claim this opener makes is
	/// about what it refuses, and until this existed the only way to ask was
	/// to call `open` and watch a browser not appear, which is not an
	/// observation a test can make. Answering with the parsed address rather
	/// than with a `bool` keeps the parse single: what is validated here is
	/// the object that is forwarded, so no second parse can disagree with the
	/// first.
	///
	/// A value means an absolute `https` address that has a host and carries
	/// no userinfo. Everything else — a relative or unparsable string, any
	/// other scheme, a missing host, embedded credentials — is `nullopt`.
	[[nodiscard]] auto validated_browser_address(std::string_view aAddress)
		-> std::optional<QUrl>;

#if defined(BPR_BUILD_TESTING)
	namespace detail {
		/// \brief The signature of the platform launch `open` finishes with.
		using browser_launch_function = bool (*)(const QUrl&);

		/// \brief Substitutes that launch, answering the one it replaced.
		///
		/// \details The test seam, and tests-enabled only: a shipped image
		/// carries neither this declaration nor the indirection behind it,
		/// so nothing in a release can redirect where a validated address
		/// goes. It exists for the half of the claim
		/// `validated_browser_address` cannot carry — that a refused address
		/// reaches no launch at all — which is a statement about a call that
		/// does not happen, and so needs a stand-in that can count.
		///
		/// `nullptr` restores the platform launch. The caller restores it
		/// either way, and nothing calls this while another thread is inside
		/// `open`.
		auto substitute_browser_launch(browser_launch_function aLaunch)
			-> browser_launch_function;
	} // namespace detail
#endif

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
	/// `LSOpenCFURLRef` on macOS, `xdg-open` on Linux, and the Windows shell's
	/// default-handler launch, all of which `QDesktopServices::openUrl`
	/// reaches. Naming that last one exactly is not worth it: the
	/// architecture gate forbids the alternate process and shell launch APIs
	/// by name and cannot tell a call from a sentence, so spelling it here
	/// fails the gate for a comment. That is the whole implementation: no
	/// embedded web view, no browser engine, no
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
	/// `javascript:` address, and refusing costs one parse. The refusal
	/// itself is `validated_browser_address` above, which is where to read
	/// what it accepts.
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
