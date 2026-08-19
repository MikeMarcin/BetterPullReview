#ifndef BPR_SRC_APP_SRC_DESIGNATED_LIVE_MACOS_PROVENANCE_HPP
#define BPR_SRC_APP_SRC_DESIGNATED_LIVE_MACOS_PROVENANCE_HPP

/// \file
/// \brief The authenticated launch channel between stage one and the app.
///
/// \details BPR-0262 fact_4 requires the application and its stage-one
/// supervisor to complete a bidirectional fixed-message handshake, validating
/// kernel audit trailers and peer code identity with Security.framework,
/// *before* `QApplication` exists and before any public observation — and to
/// refuse a direct or alternate launch outright.
///
/// fact_3 fixes the mechanism and forbids every cheaper one. Provenance may
/// travel only through an initially empty Mach `SERVICE_SLOT` inherited across
/// a suspended spawn, with the parent's registered-port snapshot restored
/// immediately afterwards. There is no argv, environment, pid-only,
/// nonce-only, generic-inherited-pipe, or persistent-state fallback, and
/// adding one invalidates the recorded approval rather than merely weakening
/// it.
///
/// `SERVICE_SLOT` is an index into the task's registered-port array, not a
/// launchd service name. [[BPR-0357]] established that the whole mechanism
/// works under the capsule's ad-hoc signature, with no hardened runtime and no
/// entitlements; this is that mechanism moved into the product, shared by the
/// two executables that have to agree.
///
/// The fixed messages below are not secrets and carry no authority. What
/// authenticates a peer is the port right it could only have inherited plus
/// the audit trailer the kernel wrote — never the text, and never a pid the
/// other side happened to remember.

#if defined(__APPLE__)

	#include <mach/mach.h>
	#include <sys/types.h>

	#include <filesystem>
	#include <optional>
	#include <string_view>
	#include <vector>

namespace bpr::app::designated_live_provenance {
	/// \brief The application's first word, sent on the inherited right.
	inline constexpr auto application_greeting
		= std::string_view{ "bpr-designated-live-application-hello" };

	/// \brief The supervisor's only acceptable answer.
	inline constexpr auto supervisor_reply
		= std::string_view{ "bpr-designated-live-supervisor-accept" };

	/// \brief How long either side waits for the other, in milliseconds.
	///
	/// \details Bounded so a peer that never answers fails closed instead of
	/// holding a suspended child forever. The handshake is two messages
	/// between a parent and the child it just spawned; it does not need
	/// patience, it needs an end.
	inline constexpr auto handshake_timeout_milliseconds = 20'000;

	/// \brief Stage one's half of the channel.
	///
	/// \details Claiming publishes a send right in an empty `SERVICE_SLOT`.
	/// The slot must be empty first: an occupied one is somebody else's
	/// channel, and overwriting it would be exactly the substitution fact_6
	/// says must fail closed. Destruction restores the snapshot taken at claim
	/// time even when a caller forgets, because a right left published outlives
	/// the launch it was for.
	class supervisor_channel
	{
	public:
		/// \brief Claims `SERVICE_SLOT`, or refuses because it is not free.
		[[nodiscard]] static auto claim() -> std::optional<supervisor_channel>;

		supervisor_channel(const supervisor_channel&)                    = delete;
		auto operator=(const supervisor_channel&) -> supervisor_channel& = delete;
		supervisor_channel(supervisor_channel&& aOther) noexcept;
		auto operator=(supervisor_channel&& aOther) noexcept
			-> supervisor_channel&;
		~supervisor_channel();

		/// \brief Restores the exact registered-port snapshot from claim time.
		///
		/// \details Called immediately after the spawn that inherits the right,
		/// so that no later spawn — of any child, for any reason — inherits a
		/// channel it was not meant to have.
		[[nodiscard]] auto restore() -> bool;

		/// \brief Authenticates the child and answers it.
		///
		/// \details Requires, in order: the child's exact fixed greeting; a
		/// full kernel audit trailer; that the trailer names `aChild` and no
		/// other process; and that the peer's dynamic code identity equals the
		/// on-disk image at `aImage`. Only then is the fixed reply sent, on the
		/// send-once right the child supplied.
		///
		/// The identity question is asked of the audit token, never of the pid.
		/// A pid can be reused between the spawn and the question; an audit
		/// token names the process that actually sent the message.
		[[nodiscard]] auto authenticate(
			pid_t                        aChild,
			const std::filesystem::path& aImage) -> bool;

	private:
		supervisor_channel() = default;

		mach_port_t              Service = MACH_PORT_NULL;
		std::vector<mach_port_t> Snapshot;
		bool                     Restored = false;
	};

	/// \brief The application's half, run before `QApplication` exists.
	///
	/// \details Recovers the right stage one published, greets, and requires
	/// the supervisor's exact reply from a peer whose code identity equals
	/// `aSupervisorImage`. Every failure — an empty slot, a silent peer, a
	/// wrong word, an unverifiable identity — is a refusal. There is nothing
	/// else to fall back to, which is the point.
	[[nodiscard]] auto authenticate_supervisor(
		const std::filesystem::path& aSupervisorImage) -> bool;

	/// \brief The running executable's canonical path, without Qt.
	///
	/// \details The application normally learns this from
	/// `QCoreApplication::applicationFilePath()`, which does not exist yet at
	/// the point this handshake has to happen.
	[[nodiscard]] auto running_executable_path()
		-> std::optional<std::filesystem::path>;

	/// \brief The supervisor image that belongs to the given application image.
	///
	/// \details Both live inside one capsule at fixed relative positions, so
	/// the application can name its expected supervisor without being told
	/// where it is — being told would be an argv-carried claim, which fact_3
	/// forbids.
	[[nodiscard]] auto supervisor_image_for(
		const std::filesystem::path& aApplicationImage)
		-> std::optional<std::filesystem::path>;
}

#endif // defined(__APPLE__)

#endif // BPR_SRC_APP_SRC_DESIGNATED_LIVE_MACOS_PROVENANCE_HPP
