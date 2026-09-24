#ifndef BPR_ADAPTERS_SQLITE_PROFILE_ACTIVATION_HPP
#define BPR_ADAPTERS_SQLITE_PROFILE_ACTIVATION_HPP

#include "bpr/application/profile_activation.hpp"
#include "bpr/application/profile_runtime_lease.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <stop_token>

namespace bpr::adapters::sqlite {
	/// \brief Explains why a profile root has no endpoint an owner could bind.
	enum class profile_activation_endpoint_failure : std::uint8_t {
		platform_unsupported, ///< Retained for a platform with no transport; none of the shipped platforms today.
		root_refused, ///< The root is not an owner-private storage root.
		runtime_directory_absent, ///< POSIX: no owner-private per-user runtime directory was found.
		path_too_long, ///< POSIX: the resolved endpoint does not fit the platform's `sun_path`.
		unavailable ///< The resolution itself failed.
	};
	/// \brief Resolves the rendezvous endpoint both launches of a profile derive from its root.
	/// \details The endpoint is a rendezvous, not durable state. On POSIX it lives in the short
	/// per-user runtime directory (Linux's `$XDG_RUNTIME_DIR` when it names an owner-private
	/// directory short enough for the endpoint ([[BPR-0906]], [[BPR-1113]]), else Linux's
	/// `/run/user/<uid>`, else `$TMPDIR`, which is what macOS gives each user, else an
	/// owner-private child of `/tmp`) under a name digested from the canonical root. Beside the
	/// data root it could not fit: a real macOS profile root spends 108 of Darwin's 104
	/// `sun_path` bytes ([[BPR-0881]]). The name and its separator add 35 bytes, so on Linux an
	/// `$XDG_RUNTIME_DIR` of 73 bytes or more is passed over for the next candidate rather than
	/// refused. The runtime directory is verified owner-private rather than assumed, and a
	/// returned path always fits `sun_path`. `$XDG_RUNTIME_DIR` and the top-level candidates are
	/// verified through a symbolic link, as the directory it leads to; only the owner-private
	/// child is verified without following one. No socket is bound and no root is created; the
	/// one owner-private runtime child may be.
	///
	/// Both launches of a profile must resolve the same runtime directory to meet. On Linux two
	/// launches whose `$XDG_RUNTIME_DIR`s name different private directories resolve different
	/// endpoints, so the second finds no owner to hand off to and reports the lease's
	/// `owner_busy`. That is the symptom [[BPR-0881]] fixed, and here it is intended: exclusive
	/// ownership still holds, and only the handoff is lost. A desktop session normally names the
	/// same `/run/user/<uid>` for every process, so this arises when something sets the variable
	/// per process, as the Linux test launcher does for each test.
	///
	/// On Windows it is the name of an owner-private local named pipe under `\\.\pipe\`,
	/// digested from the canonical root ([[BPR-0979]]); nothing exists until an owner binds it.
	[[nodiscard]] auto profile_activation_endpoint(const std::filesystem::path& aRoot)
		-> std::expected<std::filesystem::path, profile_activation_endpoint_failure>;
	/// \brief Names an endpoint refusal in fixed, non-secret text for a diagnostic.
	[[nodiscard]] auto describe_profile_activation_endpoint_failure(profile_activation_endpoint_failure aFailure) noexcept
		-> const char*;
	/// \brief Binds a private native endpoint under an existing runtime lease.
	/// \details POSIX binds an owner-private `AF_UNIX` listener; Windows creates the first
	/// instance of an owner-private message-mode named pipe whose descriptor names only this
	/// user (and, contained, the package). Both authenticate the peer process's user before
	/// admitting one fixed 16-byte frame, and both answer with a fixed two-byte acknowledgement.
	[[nodiscard]] auto make_profile_activation_server(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, std::shared_ptr<application::profile_runtime_lease> aLease)
		-> std::expected<std::shared_ptr<application::profile_activation_server>, application::profile_activation_outcome>;
	/// \brief Sends once with bounded cancellation and no retry after ambiguous delivery.
	[[nodiscard]] auto send_profile_activation(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, core::profile_launch_intent aIntent, std::stop_token aStop = { })
		-> std::expected<application::profile_activation_outcome, application::profile_activation_outcome>;
}
#endif
