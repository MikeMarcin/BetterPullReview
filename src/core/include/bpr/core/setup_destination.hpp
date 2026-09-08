#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SETUP_DESTINATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SETUP_DESTINATION_HPP

#include "bpr/core/github_model.hpp"

#include <span>
#include <string_view>

namespace bpr::core {
	/// \brief Exact bounded navigation input; an empty hostname is local shorthand.
	struct setup_destination
	{
		std::string         Hostname;
		repository_locator  Repository;
		pull_request_number Number;
		std::string         Location;
		auto                operator<=>(const setup_destination&) const = default;
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
	};

	/// \brief Selects only a configured authority with the exact requested hostname.
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
