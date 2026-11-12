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
	/// \brief RFC-BPR-0032 amendment r1 (account setup in the window). The
	/// two ways a first-run reviewer connects a GitHub.com account.
	///
	/// \details The browser way is the native device grant (RFC-BPR-0016):
	/// BetterPullReview shows a code and the reviewer enters it on GitHub. The
	/// GitHub CLI way uses an account gh already holds (RFC-BPR-0015,
	/// RFC-BPR-0078); BetterPullReview never runs `gh auth login`, so a gh
	/// with no account is told how to add one and checked again.
	enum class account_setup_way : std::uint8_t {
		browser,
		github_cli,
	};

	[[nodiscard]] auto stable_name(account_setup_way aWay) noexcept -> std::string_view;

	/// \brief The setup's four steps, in order. They are a real sequence, so
	/// the presentation numbers them.
	enum class account_setup_step : std::uint8_t {
		/// Choose how to connect.
		way,
		/// Browser: get a code and enter it. GitHub CLI: check gh and choose
		/// an account.
		sign_in,
		/// GitHub accepted the sign-in and BetterPullReview is checking which
		/// account it is.
		confirm,
		/// Connected; the reviewer continues to Home.
		ready,
	};

	[[nodiscard]] auto stable_name(account_setup_step aStep) noexcept -> std::string_view;

	/// \brief Where the setup stands, finer than its step.
	enum class account_setup_phase : std::uint8_t {
		/// Not shown.
		idle,
		choosing_way,
		/// Browser: a code has been asked for.
		requesting_code,
		/// Browser: the code is shown and GitHub is being asked whether it
		/// was entered.
		awaiting_browser,
		/// Browser: the flow ended without a grant; `Problem` says why and
		/// Try again asks for a new code.
		browser_stopped,
		/// GitHub CLI: gh's version and accounts are being read.
		inspecting_cli,
		/// GitHub CLI: gh answered. Its usable accounts are listed, or
		/// `Problem` says why there are none.
		choosing_account,
		/// The chosen account is being checked with GitHub.
		connecting,
		connected,
	};

	[[nodiscard]] auto stable_name(account_setup_phase aPhase) noexcept -> std::string_view;

	/// \brief Why the setup cannot go on without the reviewer, a closed set
	/// so presentation says what happened and what to do.
	enum class account_setup_problem : std::uint8_t {
		none,
		/// gh is not installed where BetterPullReview looks.
		gh_missing,
		/// gh is older than the version whose account inventory is read.
		gh_outdated,
		/// gh is installed but could not be run or read.
		gh_failed,
		/// gh holds no signed-in account.
		gh_signed_out,
		/// gh holds accounts, but none is signed in with secure storage.
		gh_no_usable_account,
		/// The browser code expired before it was entered.
		code_expired,
		/// The code was declined on GitHub.
		code_denied,
		/// The device sign-in could not run (network, server, storage).
		browser_failed,
		/// This build or host offers no device sign-in.
		browser_unavailable,
		/// GitHub refused the account, or it could not be checked.
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
		/// The inventory was read and lists accounts, none usable.
		no_usable_account,
	};

	/// \brief One gh account the setup offers: signed in with secure
	/// storage, on the host asked about, and pinned to its own profile.
	struct account_setup_account
	{
		host_profile_id Profile;
		std::string     Login;
		std::string     Hostname;
		/// gh's own active account on that host. Said, never preferred:
		/// RFC-BPR-0078 requires an explicit choice.
		bool ActiveInGh = false;

		auto operator<=>(const account_setup_account&) const = default;
	};

	/// \brief The longest list of accounts the setup keeps.
	inline constexpr std::size_t maximum_account_setup_accounts = 32U;

	/// \brief The setup as values.
	struct account_setup_state
	{
		account_setup_phase                Phase = account_setup_phase::idle;
		std::optional<account_setup_way>   Way;
		std::vector<account_setup_account> Accounts;
		std::optional<host_profile_id>     Chosen;
		account_setup_problem              Problem = account_setup_problem::none;
		/// The connected account, once connected.
		std::string Login;
		std::string Hostname;
		/// The identity of the newest effect the setup asked for. A completion
		/// that names another is stale and changes nothing.
		std::uint64_t Request = 0U;

		auto operator<=>(const account_setup_state&) const = default;
	};

	/// \brief The step a state shows.
	[[nodiscard]] auto account_setup_step_of(const account_setup_state& aState) noexcept -> account_setup_step;

	/// \brief Home's first-launch surface begins the setup, optionally with a
	/// way already chosen.
	struct begin_account_setup
	{
		std::optional<account_setup_way> Way;
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

	/// \brief A device-grant stage, as the device host published it.
	struct account_setup_device_observed
	{
		device_authorization_stage Stage = device_authorization_stage::idle;
	};

	/// \brief What became of the window's GitHub session.
	enum class account_setup_session : std::uint8_t {
		/// An authenticated account was accepted.
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

	/// \brief Try again: a new code, or gh checked again.
	struct retry_account_setup
	{ };

	/// \brief Back to choosing a way; a running flow is cancelled.
	struct return_to_account_setup_way
	{ };

	/// \brief Leave the finished setup for Home.
	struct finish_account_setup
	{ };

	using account_setup_event = std::variant<begin_account_setup, choose_account_setup_way, github_cli_inspected, choose_account_setup_account, connect_account_setup,
											 account_setup_device_observed, account_setup_session_observed, retry_account_setup, return_to_account_setup_way,
											 finish_account_setup>;

	/// \brief What the host is asked to do.
	enum class account_setup_effect_kind : std::uint8_t {
		/// Read gh's version and account inventory; answer with
		/// `github_cli_inspected` naming the request.
		inspect_github_cli,
		/// Begin the native device grant for GitHub.com.
		begin_browser_sign_in,
		/// End a device grant this setup began.
		cancel_browser_sign_in,
		/// Connect the chosen gh account; consent is given.
		connect_github_cli,
	};

	[[nodiscard]] auto stable_name(account_setup_effect_kind aKind) noexcept -> std::string_view;

	struct account_setup_effect
	{
		account_setup_effect_kind      Kind    = account_setup_effect_kind::inspect_github_cli;
		std::uint64_t                  Request = 0U;
		std::optional<host_profile_id> Profile;

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
	/// an answer naming an older one is refused as stale, so a gh check or a
	/// device flow abandoned by Back can never move a later choice.
	[[nodiscard]] auto transition_account_setup(const account_setup_state& aState, const account_setup_event& aEvent) -> account_setup_transition;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_ACCOUNT_SETUP_HPP
