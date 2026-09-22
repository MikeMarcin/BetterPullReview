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
	/// per-user runtime directory (Linux's `/run/user/<uid>`, else `$TMPDIR`, which is what
	/// macOS gives each user, else an owner-private child of `/tmp`) under a name digested from
	/// the canonical root. Beside the data root it could not fit: a real macOS profile root
	/// spends 108 of Darwin's 104 `sun_path` bytes ([[BPR-0881]]). The runtime directory is
	/// verified owner-private rather than assumed, and a returned path always fits `sun_path`.
	/// No socket is bound and no root is created; the one owner-private runtime child may be.
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
