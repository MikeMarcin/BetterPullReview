#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_ACCOUNT_SETUP_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_ACCOUNT_SETUP_HPP

#include "bpr/core/device_authorization_model.hpp"
#include "bpr/core/github_model.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace bpr::core {
	/// \brief RFC-BPR-0032 amendment r1. Which server an account is for: the
	/// setup's first step (owner, 2026-11-12: "server is the first step,
	/// github.com or enterprise").
	enum class account_setup_server : std::uint8_t {
		github_com,
		enterprise,
	};

	[[nodiscard]] auto stable_name(account_setup_server aServer) noexcept -> std::string_view;

	/// \brief The ways an account is connected.
	///
	/// \details The browser way is the native device grant (RFC-BPR-0016),
	/// offered for GitHub.com. The GitHub CLI way uses an account gh already
	/// holds on the server (RFC-BPR-0015, RFC-BPR-0078); BetterPullReview never
	/// runs `gh auth login`. The token way takes a personal access token for a
	/// GitHub Enterprise server, typed into the setup.
	enum class account_setup_way : std::uint8_t {
		browser,
		github_cli,
		token,
	};

	[[nodiscard]] auto stable_name(account_setup_way aWay) noexcept -> std::string_view;

	/// \brief Whether a way is offered for a server.
	[[nodiscard]] auto account_setup_way_offered(account_setup_server aServer, account_setup_way aWay) noexcept -> bool;

	/// \brief The setup's five steps, in order. They are a real sequence, so
	/// the presentation numbers them.
	enum class account_setup_step : std::uint8_t {
		/// GitHub.com or a GitHub Enterprise server.
		server,
		/// Browser, GitHub CLI or token.
		way,
		/// Browser: enter the code. GitHub CLI: choose an account. Token: enter it.
		sign_in,
		/// GitHub accepted the sign-in and the account is being checked.
		confirm,
		/// Connected; the reviewer continues.
		ready,
	};

	[[nodiscard]] auto stable_name(account_setup_step aStep) noexcept -> std::string_view;

	/// \brief Where the setup stands, finer than its step.
	enum class account_setup_phase : std::uint8_t {
		/// Not shown.
		idle,
		choosing_server,
		/// An Enterprise server is being added so it has a profile to sign in to.
		preparing_server,
		choosing_way,
		/// Browser: a code has been asked for.
		requesting_code,
		/// Browser: the code is shown and GitHub is asked whether it was entered.
		awaiting_browser,
		/// Browser: the flow ended without a grant; `Problem` says why.
		browser_stopped,
		/// GitHub CLI: gh's version and accounts are being read.
		inspecting_cli,
		/// GitHub CLI: gh answered. Its usable accounts are listed, or
		/// `Problem` says why there are none.
		choosing_account,
		/// Token: waiting for the reviewer to enter one.
		entering_token,
		/// The account is being checked with GitHub.
		connecting,
		connected,
	};

	[[nodiscard]] auto stable_name(account_setup_phase aPhase) noexcept -> std::string_view;

	/// \brief Why the setup cannot go on without the reviewer, a closed set
	/// so presentation says what happened and what to do.
	enum class account_setup_problem : std::uint8_t {
		none,
		/// The Enterprise hostname is not a hostname.
		invalid_hostname,
		/// The Enterprise server could not be added.
		server_unavailable,
		/// gh is not installed where BetterPullReview looks.
		gh_missing,
		/// gh is older than the version whose account inventory is read.
		gh_outdated,
		/// gh is installed but could not be run or read.
		gh_failed,
		/// gh holds no signed-in account on the server.
		gh_signed_out,
		/// gh holds accounts there, but none is signed in with secure storage.
		gh_no_usable_account,
		/// The browser code expired before it was entered.
		code_expired,
		/// The code was declined on GitHub.
		code_denied,
		/// The device sign-in could not run (network, server, storage).
		browser_failed,
		/// This build or host offers no device sign-in.
		browser_unavailable,
		/// GitHub refused the account or token, or it could not be checked.
		connection_failed,
	};

	[[nodiscard]] auto stable_name(account_setup_problem aProblem) noexcept -> std::string_view;

	/// \brief What gh's inspection found, as decoded by the host.
	enum class github_cli_inspection : std::uint8_t {
		/// gh ran and its inventory was read; the accounts say what it holds.
		read,
		missing,
		outdated,
		failed,
		/// The inventory lists accounts on the server, none usable.
		no_usable_account,
	};

	/// \brief One gh account the setup offers: signed in with secure
	/// storage, on the server asked about, and pinned to its own profile.
	struct account_setup_account
	{
		host_profile_id Profile;
		std::string     Login;
		std::string     Hostname;
		/// gh's own active account on that host. Chosen first (owner,
		/// 2026-11-12) and said; the reviewer may choose another.
		bool ActiveInGh = false;

		auto operator<=>(const account_setup_account&) const = default;
	};

	/// \brief The longest list of accounts the setup keeps.
	inline constexpr std::size_t maximum_account_setup_accounts = 32U;

	/// \brief The setup as values.
	struct account_setup_state
	{
		account_setup_phase                Phase = account_setup_phase::idle;
		std::optional<account_setup_server> Server;
		/// The server's hostname: `github.com`, or the Enterprise host; once
		/// connected, the accepted session's host.
		std::string                        Hostname;
		/// The Enterprise server's native profile, once added.
		std::optional<host_profile_id>     ServerProfile;
		std::optional<account_setup_way>   Way;
		/// A way asked for before the Enterprise server was added.
		std::optional<account_setup_way>   Intended;
		std::vector<account_setup_account> Accounts;
		std::optional<host_profile_id>     Chosen;
		account_setup_problem              Problem = account_setup_problem::none;
		/// The connected account, once connected.
		std::string Login;
		/// The identity of the newest effect the setup asked for. A completion
		/// that names another is stale and changes nothing.
		std::uint64_t Request = 0U;

		auto operator<=>(const account_setup_state&) const = default;
	};

	/// \brief The step a state shows.
	[[nodiscard]] auto account_setup_step_of(const account_setup_state& aState) noexcept -> account_setup_step;

	/// \brief Every way in begins (or begins again) the one setup: Home's
	/// first run, Settings' Add account, Use another account, a server card's
	/// Sign in, and a pasted link that needs an account. A server, its host
	/// and a way may already be known; what is not is asked.
	struct begin_account_setup
	{
		std::optional<account_setup_server> Server;
		std::string                         Hostname;
		std::optional<account_setup_way>    Way;
	};

	/// \brief The server step's choice. `Hostname` is the Enterprise host as
	/// typed; it is ignored for GitHub.com.
	struct choose_account_setup_server
	{
		account_setup_server Server = account_setup_server::github_com;
		std::string          Hostname;
	};

	/// \brief The host's answer to `prepare_enterprise_server`.
	struct enterprise_server_prepared
	{
		std::uint64_t                  Request = 0U;
		std::optional<host_profile_id> Profile;
	};

	struct choose_account_setup_way
	{
		account_setup_way Way = account_setup_way::browser;
	};

	/// \brief The host's decoded answer to an `inspect_github_cli` request.
	struct github_cli_inspected
	{
		std::uint64_t                      Request = 0U;
		github_cli_inspection              Outcome = github_cli_inspection::failed;
		std::vector<account_setup_account> Accounts;
	};

	struct choose_account_setup_account
	{
		host_profile_id Profile;
	};

	/// \brief Connect, pressed under the account's consent text. The press
	/// is the reviewer's consent to that one account's reads.
	struct connect_account_setup
	{ };

	/// \brief Sign in, pressed with a token entered. The token itself never
	/// enters the core: the host holds it for the `enroll_token` it asks for.
	struct submit_account_setup_token
	{ };

	/// \brief A device-grant stage, as the device host published it.
	struct account_setup_device_observed
	{
		device_authorization_stage Stage = device_authorization_stage::idle;
	};

	/// \brief What became of the window's GitHub session.
	enum class account_setup_session : std::uint8_t {
		/// An authenticated account was newly accepted.
		ready,
		failed,
		cancelled,
	};

	struct account_setup_session_observed
	{
		account_setup_session Outcome = account_setup_session::failed;
		std::string           Login;
		std::string           Hostname;
	};

	/// \brief Try again: a new code, gh checked again, or the server added again.
	struct retry_account_setup
	{ };

	/// \brief Back to choosing a way; a running flow is cancelled.
	struct return_to_account_setup_way
	{ };

	/// \brief Back to choosing the server; a running flow is cancelled.
	struct return_to_account_setup_server
	{ };

	/// \brief Leave the setup: finished, or closed before an account.
	struct finish_account_setup
	{ };

	using account_setup_event = std::variant<begin_account_setup, choose_account_setup_server, enterprise_server_prepared, choose_account_setup_way,
											 github_cli_inspected, choose_account_setup_account, connect_account_setup, submit_account_setup_token,
											 account_setup_device_observed, account_setup_session_observed, retry_account_setup, return_to_account_setup_way,
											 return_to_account_setup_server, finish_account_setup>;

	/// \brief What the host is asked to do.
	enum class account_setup_effect_kind : std::uint8_t {
		/// Add the Enterprise server `Hostname`; answer `enterprise_server_prepared`.
		prepare_enterprise_server,
		/// Read gh's version and its accounts on `Hostname`; answer
		/// `github_cli_inspected` naming the request.
		inspect_github_cli,
		/// Begin the native device grant for GitHub.com.
		begin_browser_sign_in,
		/// End a device grant this setup began.
		cancel_browser_sign_in,
		/// Connect the chosen gh account `Profile`; consent is given.
		connect_github_cli,
		/// Sign in to `Profile` with the token the host holds.
		enroll_token,
	};

	[[nodiscard]] auto stable_name(account_setup_effect_kind aKind) noexcept -> std::string_view;

	struct account_setup_effect
	{
		account_setup_effect_kind      Kind    = account_setup_effect_kind::inspect_github_cli;
		std::uint64_t                  Request = 0U;
		std::optional<host_profile_id> Profile;
		std::string                    Hostname;

		auto operator<=>(const account_setup_effect&) const = default;
	};

	/// \brief Why an event changed nothing.
	enum class account_setup_refusal : std::uint8_t {
		none,
		/// The event does not apply in this phase.
		not_now,
		/// A completion for a request that is no longer the newest.
		stale,
		/// The account is not one the setup offered.
		unknown_account,
		/// The way is not offered for the chosen server.
		not_offered,
		/// Request identities are exhausted.
		exhausted,
	};

	[[nodiscard]] auto stable_name(account_setup_refusal aRefusal) noexcept -> std::string_view;

	struct account_setup_transition
	{
		account_setup_state               State;
		std::vector<account_setup_effect> Effects;
		account_setup_refusal             Refusal = account_setup_refusal::none;
	};

	/// \brief The setup's one transition function.
	///
	/// \details Deterministic: equal state and event give equal results. A
	/// refused event returns the state unchanged, no effects and the reason.
	/// Every effect that expects an answer carries a new request identity, and
	/// an answer naming an older one is refused as stale, so a server, gh check
	/// or device flow abandoned by Back can never move a later choice.
	[[nodiscard]] auto transition_account_setup(const account_setup_state& aState, const account_setup_event& aEvent) -> account_setup_transition;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_ACCOUNT_SETUP_HPP
