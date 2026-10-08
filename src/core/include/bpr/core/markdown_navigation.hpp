#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_MARKDOWN_NAVIGATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_MARKDOWN_NAVIGATION_HPP

#include "bpr/core/github_model.hpp"
#include "bpr/core/profile_window.hpp"
#include "bpr/core/review_queue.hpp"
#include "bpr/core/window_review_restoration.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

namespace bpr::core {
	/// \brief Opaque accepted body context; cannot be reused across windows or publications.
	struct markdown_context
	{
		bpr_window_context         Window;
		application_session_id     Session;
		github_identity_generation Identity;
		provider_snapshot_id       Snapshot;
		std::uint64_t              Revision                                          = 0;
		auto                       operator==(const markdown_context&) const -> bool = default;
		auto                       operator<=>(const markdown_context& aOther) const
		{
			return std::tie(Window.Profile, Window.Window, Window.Generation, Session, Identity, Snapshot, Revision)
				<=> std::tie(aOther.Window.Profile, aOther.Window.Window, aOther.Window.Generation, aOther.Session, aOther.Identity, aOther.Snapshot, aOther.Revision);
		}
	};

	/// \brief Untrusted destination from one explicit reading gesture. Empty Body identifies the description.
	struct markdown_link_intent
	{
		markdown_context Context;
		std::string      Body;
		std::string      Destination;
	};

	enum class markdown_link_result : std::uint8_t {
		opened,
		stale,
		unavailable,
		refused,
		failed,
	};

	enum class markdown_destination_kind : std::uint8_t {
		absolute_web,
		repository_path,
		pull_request_fragment,
	};

	/// \brief Admitted semantic destination; the host constructs provider-specific addresses.
	struct markdown_destination
	{
		markdown_destination_kind Kind;
		std::string               Target;
	};

	/// \brief Host-observed current body and exact repository revision, with no credential material.
	struct markdown_link_observation
	{
		std::optional<markdown_context> Context;
		bool                            BodyAvailable    = false;
		bool                            BrowserAvailable = false;
		std::string_view                Hostname;
		std::string_view                Owner;
		std::string_view                Repository;
		std::string_view                Revision;
		pull_request_number             Number;
	};

	/// \brief Admits bounded HTTP(S) destinations or repository-root paths bound to the accepted head.
	[[nodiscard]] auto admit_markdown_link(const markdown_link_intent&      aIntent,
										   const markdown_link_observation& aObservation)
		-> std::expected<markdown_destination, markdown_link_result>;

	/// \brief [[BPR-1338]]. One pull request's own page, from parts the host accepted.
	///
	/// \details The hostname is the accepted profile authority's; the owner,
	/// repository and number are the accepted read model's. No part is remote
	/// or typed text taken as an address.
	struct pull_request_page_observation
	{
		bool                BrowserAvailable = false;
		std::string_view    Hostname;
		std::string_view    Owner;
		std::string_view    Repository;
		pull_request_number Number;
	};

	/// \brief [[BPR-1338]]. The admitted parts of one pull request's page.
	///
	/// \details The host turns them into the canonical page address, the one
	/// the provider contract already requires a pull request's `html_url` to
	/// equal: the hostname, the owner, the repository, "pull" and the number.
	struct pull_request_page
	{
		std::string         Hostname;
		std::string         Owner;
		std::string         Repository;
		pull_request_number Number;

		auto operator==(const pull_request_page&) const -> bool = default;
	};

	/// \brief [[BPR-1338]]. Admits one pull request's page from accepted parts.
	///
	/// \details The same safe-authority and safe-name rules as a body link's
	/// repository-relative destination; a part that fails them refuses, and
	/// nothing is repaired or guessed.
	[[nodiscard]] auto admit_pull_request_page(const pull_request_page_observation& aObservation)
		-> std::expected<pull_request_page, markdown_link_result>;

	/// \brief [[BPR-1338]]. A Home queue row's page, on the accepted session's host.
	///
	/// \details The row must be the accepted account's on that host (the
	/// queue names both), and its repository exactly "owner/name". The host
	/// in the address is `aHostname`, the accepted profile's, never the row's.
	[[nodiscard]] auto admit_queue_pull_request_page(const pull_request_ref& aRef, std::string_view aHostname, std::string_view aLogin, bool aBrowserAvailable)
		-> std::expected<pull_request_page, markdown_link_result>;

	/// \brief [[BPR-1338]]. A Recently viewed row's page, on the accepted session's host.
	///
	/// \details A public reading opens on that host; an account's reading
	/// only under that account's exact session authority.
	[[nodiscard]] auto admit_recent_pull_request_page(const window_review_destination& aDestination, const window_review_authority& aAuthority, github_account_id aAccount, bool aBrowserAvailable)
		-> std::expected<pull_request_page, markdown_link_result>;

	/// \brief [[BPR-1342]]. A Home queue row as the pull request a new window
	/// of this profile opens.
	///
	/// \details The row must be the accepted account's on the accepted
	/// session's host, by the same rule as its GitHub page; the destination's
	/// host is `aHostname`, the accepted profile's, and its account the
	/// accepted viewer's.
	[[nodiscard]] auto admit_queue_review_window(const pull_request_ref& aRef, std::string_view aHostname, std::string_view aLogin, github_account_id aAccount)
		-> std::expected<window_review_destination, markdown_link_result>;

	/// \brief [[BPR-1342]]. A Recently viewed entry as the pull request a new
	/// window of this profile opens.
	///
	/// \details The rule its Open already follows in this window: a public
	/// reading opens on its own host, signed in or not; an account's reading
	/// only under that account's exact session authority. Its parts pass the
	/// page's safe-authority and safe-name rules.
	[[nodiscard]] auto admit_recent_review_window(const window_review_destination& aDestination, const std::optional<window_review_authority>& aAuthority, github_account_id aAccount)
		-> std::expected<window_review_destination, markdown_link_result>;
}
#endif
