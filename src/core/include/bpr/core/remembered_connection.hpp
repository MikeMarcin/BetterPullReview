#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REMEMBERED_CONNECTION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REMEMBERED_CONNECTION_HPP

#include "bpr/core/github_model.hpp"
#include "bpr/core/profile_window.hpp"

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	inline constexpr std::size_t maximum_remembered_connections = 64U;
	/// \brief [[BPR-1494]]. The most displaced native identities one profile retains.
	inline constexpr std::size_t maximum_retired_connections = 16U;
	enum class remembered_connection_transport : std::uint8_t { native_device,
																github_cli };
	enum class remembered_connection_server : std::uint8_t { github_com,
															 ghes_3_18 };

	/// \brief Token-free validation inputs, never accepted reading or publication authority.
	/// \details A disabled row may contain only Hostname and Server: a sign-out before
	/// any successful selection still suppresses older device-grant restoration.
	struct remembered_connection
	{
		host_profile_id                 Profile;
		std::uint64_t                   ProfileRevision = 0U;
		git_host_id                     Host;
		std::string                     Hostname;
		github_account_id               Account;
		std::string                     Login;
		remembered_connection_server    Server                                                 = remembered_connection_server::github_com;
		remembered_connection_transport Transport                                              = remembered_connection_transport::native_device;
		std::uint64_t                   SelectedOrder                                          = 0U;
		bool                            ReuseEnabled                                           = true;
		auto                            operator==(const remembered_connection&) const -> bool = default;
	};

	/// \brief One profile's complete snapshot, with counters retained after Forget.
	struct remembered_connection_state
	{
		bpr_profile_id                     BprProfile        = default_bpr_profile_id;
		std::uint64_t                      Revision          = 1U;
		std::uint64_t                      LastProfileId     = 0U;
		std::uint64_t                      LastHostId        = 0U;
		std::uint64_t                      LastSelectedOrder = 0U;
		std::vector<remembered_connection> Connections;
		/// \brief [[BPR-1494]]. Native device identities that another account's
		/// native sign-in displaced from their server's selection, oldest first.
		/// \details Never a startup choice: each is a disabled row with its full
		/// identity. They let a returning account's native sign-in re-bind to the
		/// identity its authority-scoped private state was saved under, while the
		/// update check can still prove the identity was that account's own. A CLI
		/// selection on the server, or Forget, drops the server's entries.
		std::vector<remembered_connection> Retired;
		auto                               operator==(const remembered_connection_state&) const -> bool = default;
	};
	enum class remembered_connection_failure : std::uint8_t { invalid,
															  stale,
															  capacity,
															  exhausted };
	using remembered_connection_result = std::expected<remembered_connection_state, remembered_connection_failure>;

	[[nodiscard]] auto valid(const remembered_connection& aRecord) noexcept -> bool;
	[[nodiscard]] auto valid(const remembered_connection_state& aState) noexcept -> bool;
	/// \brief Checks revision, identity continuity and monotonic counters for a complete save.
	[[nodiscard]] auto valid_remembered_connection_update(const remembered_connection_state& aBefore, const remembered_connection_state& aAfter) noexcept -> bool;
	/// \brief Only an admitted successful observation may be supplied by the application host.
	/// \details Selection order and enabled state are assigned here. Different authority
	/// needs a fresh Profile above LastProfileId, or the exact retired identity it held
	/// before; a new hostname needs a fresh Host. A native row displaced by another
	/// account's native selection is retired; a CLI selection drops the server's retired rows.
	[[nodiscard]] auto remember_connection(const remembered_connection_state& aState, std::uint64_t aExpectedRevision, remembered_connection aRecord) -> remembered_connection_result;
	/// \brief Durably suppresses all automatic reuse for this hostname, even without a prior row.
	[[nodiscard]] auto disable_connection_reuse(const remembered_connection_state& aState, std::uint64_t aExpectedRevision, std::string_view aHostname) -> remembered_connection_result;
	/// \brief Removes the selection and retired identities, preserving identity high-water marks and unrelated servers.
	[[nodiscard]] auto forget_connection(const remembered_connection_state& aState, std::uint64_t aExpectedRevision, std::string_view aHostname) -> remembered_connection_result;
	/// \brief Exact-host choice, or most recently selected enabled connection without a host.
	/// \details A disabled exact-host row yields no validation attempt, never another host.
	[[nodiscard]] auto select_remembered_connection(const remembered_connection_state& aState, std::optional<std::string_view> aHostname = std::nullopt) -> std::optional<remembered_connection>;
	/// \brief [[BPR-1494]]. The earlier identity a native sign-in re-binds to once `/user` names its account.
	/// \details aSignIn is the authenticated observation on the profile the sign-in began
	/// with, which composition mints above LastProfileId before the account is known; a
	/// remembered profile never re-binds. The result is this
	/// profile's own native identity, selected or retired, with the same Host, Hostname,
	/// Server, numeric Account and Login under a different Profile; nullopt keeps
	/// aSignIn's identity. A different account never matches, and a native sign-in that
	/// replaces a CLI selection never re-binds, so either receives a fresh identity.
	[[nodiscard]] auto adopt_native_connection(const remembered_connection_state& aState, const remembered_connection& aSignIn) -> std::optional<remembered_connection>;
}
#endif
