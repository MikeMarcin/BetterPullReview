#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SETUP_DESTINATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SETUP_DESTINATION_HPP

#include "bpr/core/github_model.hpp"
#include "bpr/core/pull_request_location.hpp"

#include <span>
#include <string_view>

namespace bpr::core {
	/// \brief Preparation can establish repository authority without opening a review.
	enum class setup_destination_kind : std::uint8_t {
		pull_request,
		repository,
	};
	/// \brief Exact bounded navigation input; an empty hostname is local shorthand.
	///
	/// \details `Location` keeps the requested path and fragment as written, so
	/// an explanation can say what was asked for; `Target` is the typed
	/// location it was read as ([[BPR-0987]]).
	struct setup_destination
	{
		std::string            Hostname;
		repository_locator     Repository;
		pull_request_number    Number;
		std::string            Location;
		pull_request_location  Target;
		setup_destination_kind Kind                                        = setup_destination_kind::pull_request;
		auto                   operator<=>(const setup_destination&) const = default;
	};

	struct setup_request_id_tag;
	struct setup_owner_id_tag;
	using setup_request_id = strong_id<setup_request_id_tag>;
	using setup_owner_id   = strong_id<setup_owner_id_tag>;

	enum class setup_destination_stage : std::uint8_t {
		add_server,
		sign_in,
		authenticating,
		opening,
		ready,
		/// [[BPR-0732]]. A known public host with no account: opened signed
		/// out, in one gesture, with no sign-in stop.
		public_preview,
	};

	/// \brief A continuation belongs to one request and receiving environment revision.
	struct pending_setup_destination
	{
		setup_request_id               Request;
		setup_owner_id                 Owner;
		setup_destination              Destination;
		std::optional<host_profile_id> Profile;
		setup_destination_stage        Stage                                               = setup_destination_stage::add_server;
		auto                           operator<=>(const pending_setup_destination&) const = default;
	};

	struct setup_profile_observation
	{
		host_profile_id Profile;
		std::string     Hostname;
		bool            Authenticated = false;
		/// [[BPR-0732]]. The host can read this profile's public pull requests
		/// with no account (GitHub.com with an anonymous authority composed).
		bool PreviewAvailable = false;
		/// A successful remembered CLI selection permits targeted validation;
		/// it does not establish an authenticated session or reading authority.
		bool RevalidationAllowed = false;
	};

	/// \brief Selects only a configured authority with the exact requested hostname.
	///
	/// \details An authenticated profile is `ready`; an exact remembered
	/// choice is `authenticating` until its current account is validated.
	/// [[BPR-0732]]. A profile
	/// with no account that can read public pull requests is
	/// `public_preview`: the core decides that a pasted public link opens
	/// signed out rather than stopping at sign-in, because the owner's
	/// direction is one paste field that opens without an account, and the
	/// only reason to stop is a host that refuses preview, which stays at
	/// `sign_in`. An unknown hostname is `add_server`.
	[[nodiscard]] auto resolve_setup_destination(pending_setup_destination aPending,
												 setup_owner_id aOwner, std::span<const setup_profile_observation> aProfiles)
		-> std::optional<pending_setup_destination>;

	/// \brief Accepts only the exact current continuation under the same owner.
	[[nodiscard]] auto accept_setup_destination(
		const std::optional<pending_setup_destination>& aPending,
		setup_request_id aRequest, setup_owner_id aOwner,
		host_profile_id aProfile, std::string_view aHostname)
		-> std::optional<setup_destination>;
}
#endif
