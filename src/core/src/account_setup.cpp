#include "bpr/core/account_setup.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto refused(const account_setup_state& aState, const account_setup_refusal aRefusal) -> account_setup_transition
		{
			return account_setup_transition{ .State = aState, .Effects = { }, .Refusal = aRefusal };
		}

		[[nodiscard]] auto browser_running(const account_setup_phase aPhase) noexcept -> bool
		{
			return aPhase == account_setup_phase::requesting_code || aPhase == account_setup_phase::awaiting_browser;
		}

		[[nodiscard]] auto in_browser_flow(const account_setup_state& aState) noexcept -> bool
		{
			return aState.Way == account_setup_way::browser
				&& (browser_running(aState.Phase) || aState.Phase == account_setup_phase::connecting);
		}

		/// The accounts a setup may offer: a real profile and login, once each,
		/// in the host's order, up to the bound.
		[[nodiscard]] auto offered_accounts(const std::vector<account_setup_account>& aAccounts) -> std::vector<account_setup_account>
		{
			auto result = std::vector<account_setup_account>{ };
			for (const auto& account : aAccounts) {
				if (result.size() == maximum_account_setup_accounts) {
					break;
				}
				const auto duplicate = std::ranges::any_of(result, [&account](const account_setup_account& aKept) { return aKept.Profile == account.Profile; });
				if (account.Profile.value() != 0U && !account.Login.empty() && !account.Hostname.empty() && !duplicate) {
					result.push_back(account);
				}
			}
			return result;
		}

		/// The state at the way choice, keeping only the request counter.
		[[nodiscard]] auto at_way(const account_setup_state& aState) -> account_setup_state
		{
			auto next    = account_setup_state{ };
			next.Phase   = account_setup_phase::choosing_way;
			next.Request = aState.Request;
			return next;
		}

		/// Starts a way: the effect it asks for carries a new request.
		[[nodiscard]] auto start_way(const account_setup_state& aState, const account_setup_way aWay) -> account_setup_transition
		{
			if (aState.Request == std::numeric_limits<std::uint64_t>::max()) {
				return refused(aState, account_setup_refusal::exhausted);
			}
			auto next      = at_way(aState);
			next.Way       = aWay;
			next.Request   = aState.Request + 1U;
			auto effect    = account_setup_effect{ };
			effect.Request = next.Request;
			if (aWay == account_setup_way::browser) {
				next.Phase  = account_setup_phase::requesting_code;
				effect.Kind = account_setup_effect_kind::begin_browser_sign_in;
			} else {
				next.Phase  = account_setup_phase::inspecting_cli;
				effect.Kind = account_setup_effect_kind::inspect_github_cli;
			}
			return account_setup_transition{ .State = std::move(next), .Effects = { effect }, .Refusal = account_setup_refusal::none };
		}

		[[nodiscard]] auto problem_of(const github_cli_inspection aOutcome) noexcept -> account_setup_problem
		{
			switch (aOutcome) {
			case github_cli_inspection::read:              return account_setup_problem::none;
			case github_cli_inspection::missing:           return account_setup_problem::gh_missing;
			case github_cli_inspection::outdated:          return account_setup_problem::gh_outdated;
			case github_cli_inspection::failed:            return account_setup_problem::gh_failed;
			case github_cli_inspection::no_usable_account: return account_setup_problem::gh_no_usable_account;
			}
			return account_setup_problem::gh_failed;
		}

		[[nodiscard]] auto problem_of(const device_authorization_stage aStage) noexcept -> std::optional<account_setup_problem>
		{
			switch (aStage) {
			case device_authorization_stage::denied:                 return account_setup_problem::code_denied;
			case device_authorization_stage::expired:                return account_setup_problem::code_expired;
			case device_authorization_stage::disabled:
			case device_authorization_stage::unavailable:            return account_setup_problem::browser_unavailable;
			case device_authorization_stage::failed:                 return account_setup_problem::browser_failed;
			case device_authorization_stage::idle:
			case device_authorization_stage::requested:
			case device_authorization_stage::awaiting_authorization:
			case device_authorization_stage::cancelling:
			case device_authorization_stage::authorized:
			case device_authorization_stage::cancelled:              break;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto on_inspected(const account_setup_state& aState, const github_cli_inspected& aEvent) -> account_setup_transition
		{
			if (aState.Phase != account_setup_phase::inspecting_cli) {
				return refused(aState, account_setup_refusal::not_now);
			}
			if (aEvent.Request != aState.Request) {
				return refused(aState, account_setup_refusal::stale);
			}
			auto next     = aState;
			next.Phase    = account_setup_phase::choosing_account;
			next.Accounts = aEvent.Outcome == github_cli_inspection::read ? offered_accounts(aEvent.Accounts) : std::vector<account_setup_account>{ };
			next.Problem  = problem_of(aEvent.Outcome);
			if (next.Problem == account_setup_problem::none && next.Accounts.empty()) {
				next.Problem = account_setup_problem::gh_signed_out;
			}
			// One account is chosen without asking; Connect is still the
			// reviewer's explicit consent. Several wait for a choice.
			next.Chosen = next.Accounts.size() == 1U ? std::optional{ next.Accounts.front().Profile } : std::nullopt;
			return account_setup_transition{ .State = std::move(next), .Effects = { }, .Refusal = account_setup_refusal::none };
		}

		[[nodiscard]] auto on_device(const account_setup_state& aState, const account_setup_device_observed& aEvent) -> account_setup_transition
		{
			if (!in_browser_flow(aState)) {
				return refused(aState, account_setup_refusal::not_now);
			}
			auto next = aState;
			if (const auto problem = problem_of(aEvent.Stage); problem.has_value()) {
				if (aState.Phase == account_setup_phase::connecting) {
					// The grant already authorized; a later stage is the next flow's.
					return refused(aState, account_setup_refusal::not_now);
				}
				next.Phase   = account_setup_phase::browser_stopped;
				next.Problem = *problem;
				return account_setup_transition{ .State = std::move(next), .Effects = { }, .Refusal = account_setup_refusal::none };
			}
			switch (aEvent.Stage) {
			case device_authorization_stage::requested:
				if (aState.Phase == account_setup_phase::connecting) {
					return refused(aState, account_setup_refusal::not_now);
				}
				next.Phase = account_setup_phase::requesting_code;
				break;
			case device_authorization_stage::awaiting_authorization:
				if (aState.Phase == account_setup_phase::connecting) {
					return refused(aState, account_setup_refusal::not_now);
				}
				next.Phase = account_setup_phase::awaiting_browser;
				break;
			case device_authorization_stage::authorized: next.Phase = account_setup_phase::connecting; break;
			case device_authorization_stage::cancelled:
				// While a new code is asked for, a cancellation is the previous
				// flow's, ended by Back; it does not undo the new request.
				if (aState.Phase != account_setup_phase::awaiting_browser) {
					return refused(aState, account_setup_refusal::not_now);
				}
				return account_setup_transition{ .State = at_way(aState), .Effects = { }, .Refusal = account_setup_refusal::none };
			case device_authorization_stage::idle:
			case device_authorization_stage::cancelling:
			case device_authorization_stage::denied:
			case device_authorization_stage::expired:
			case device_authorization_stage::disabled:
			case device_authorization_stage::unavailable:
			case device_authorization_stage::failed:      return refused(aState, account_setup_refusal::not_now);
			}
			if (next == aState) {
				return refused(aState, account_setup_refusal::not_now);
			}
			return account_setup_transition{ .State = std::move(next), .Effects = { }, .Refusal = account_setup_refusal::none };
		}

		[[nodiscard]] auto on_session(const account_setup_state& aState, const account_setup_session_observed& aEvent) -> account_setup_transition
		{
			if (aState.Phase == account_setup_phase::idle || aState.Phase == account_setup_phase::connected) {
				return refused(aState, account_setup_refusal::not_now);
			}
			if (aEvent.Outcome == account_setup_session::ready) {
				if (aEvent.Login.empty()) {
					return refused(aState, account_setup_refusal::not_now);
				}
				// Before any way was chosen there is nothing to confirm: a
				// remembered connection validated at launch, or one made from
				// Settings, simply ends the setup.
				if (aState.Phase == account_setup_phase::choosing_way) {
					auto next    = account_setup_state{ };
					next.Request = aState.Request;
					return account_setup_transition{ .State = std::move(next), .Effects = { }, .Refusal = account_setup_refusal::none };
				}
				// Accepted by this setup or by another way in while a way runs
				// (an account connected from Settings): the window has an
				// account, and a code still shown is ended.
				auto effects = std::vector<account_setup_effect>{ };
				if (aState.Way == account_setup_way::browser && browser_running(aState.Phase)) {
					effects.push_back({ .Kind = account_setup_effect_kind::cancel_browser_sign_in, .Request = aState.Request, .Profile = std::nullopt });
				}
				auto next     = aState;
				next.Phase    = account_setup_phase::connected;
				next.Problem  = account_setup_problem::none;
				next.Login    = aEvent.Login;
				next.Hostname = aEvent.Hostname;
				return account_setup_transition{ .State = std::move(next), .Effects = std::move(effects), .Refusal = account_setup_refusal::none };
			}
			if (aState.Phase != account_setup_phase::connecting) {
				return refused(aState, account_setup_refusal::not_now);
			}
			auto       next      = aState;
			const auto cancelled = aEvent.Outcome == account_setup_session::cancelled;
			if (aState.Way == account_setup_way::browser) {
				next.Phase   = account_setup_phase::browser_stopped;
				next.Problem = account_setup_problem::connection_failed;
			} else {
				// The account list stays, with its choice, so Connect can be
				// pressed again or another account chosen.
				next.Phase   = account_setup_phase::choosing_account;
				next.Problem = cancelled ? account_setup_problem::none : account_setup_problem::connection_failed;
			}
			return account_setup_transition{ .State = std::move(next), .Effects = { }, .Refusal = account_setup_refusal::none };
		}

		[[nodiscard]] auto on_back(const account_setup_state& aState) -> account_setup_transition
		{
			switch (aState.Phase) {
			case account_setup_phase::requesting_code:
			case account_setup_phase::awaiting_browser:
				return account_setup_transition{ .State   = at_way(aState),
												 .Effects = { { .Kind = account_setup_effect_kind::cancel_browser_sign_in, .Request = aState.Request, .Profile = std::nullopt } },
												 .Refusal = account_setup_refusal::none };
			case account_setup_phase::browser_stopped:
			case account_setup_phase::inspecting_cli:
			case account_setup_phase::choosing_account:
				return account_setup_transition{ .State = at_way(aState), .Effects = { }, .Refusal = account_setup_refusal::none };
			case account_setup_phase::idle:
			case account_setup_phase::choosing_way:
			case account_setup_phase::connecting:
			case account_setup_phase::connected:    break;
			}
			return refused(aState, account_setup_refusal::not_now);
		}

		[[nodiscard]] auto on_connect(const account_setup_state& aState) -> account_setup_transition
		{
			if (aState.Phase != account_setup_phase::choosing_account || !aState.Chosen.has_value()) {
				return refused(aState, account_setup_refusal::not_now);
			}
			if (aState.Request == std::numeric_limits<std::uint64_t>::max()) {
				return refused(aState, account_setup_refusal::exhausted);
			}
			auto next    = aState;
			next.Phase   = account_setup_phase::connecting;
			next.Problem = account_setup_problem::none;
			next.Request = aState.Request + 1U;
			return account_setup_transition{ .State   = std::move(next),
											 .Effects = { { .Kind = account_setup_effect_kind::connect_github_cli, .Request = aState.Request + 1U, .Profile = aState.Chosen } },
											 .Refusal = account_setup_refusal::none };
		}

		[[nodiscard]] auto on_retry(const account_setup_state& aState) -> account_setup_transition
		{
			if (aState.Phase == account_setup_phase::browser_stopped) {
				return start_way(aState, account_setup_way::browser);
			}
			if (aState.Phase == account_setup_phase::choosing_account) {
				return start_way(aState, account_setup_way::github_cli);
			}
			return refused(aState, account_setup_refusal::not_now);
		}
	} // namespace

	auto stable_name(const account_setup_way aWay) noexcept -> std::string_view
	{
		switch (aWay) {
		case account_setup_way::browser:    return "browser";
		case account_setup_way::github_cli: return "github_cli";
		}
		return "unknown";
	}

	auto stable_name(const account_setup_step aStep) noexcept -> std::string_view
	{
		switch (aStep) {
		case account_setup_step::way:     return "way";
		case account_setup_step::sign_in: return "sign_in";
		case account_setup_step::confirm: return "confirm";
		case account_setup_step::ready:   return "ready";
		}
		return "unknown";
	}

	auto stable_name(const account_setup_phase aPhase) noexcept -> std::string_view
	{
		switch (aPhase) {
		case account_setup_phase::idle:             return "idle";
		case account_setup_phase::choosing_way:     return "choosing_way";
		case account_setup_phase::requesting_code:  return "requesting_code";
		case account_setup_phase::awaiting_browser: return "awaiting_browser";
		case account_setup_phase::browser_stopped:  return "browser_stopped";
		case account_setup_phase::inspecting_cli:   return "inspecting_cli";
		case account_setup_phase::choosing_account: return "choosing_account";
		case account_setup_phase::connecting:       return "connecting";
		case account_setup_phase::connected:        return "connected";
		}
		return "unknown";
	}

	auto stable_name(const account_setup_problem aProblem) noexcept -> std::string_view
	{
		switch (aProblem) {
		case account_setup_problem::none:                 return "none";
		case account_setup_problem::gh_missing:           return "gh_missing";
		case account_setup_problem::gh_outdated:          return "gh_outdated";
		case account_setup_problem::gh_failed:            return "gh_failed";
		case account_setup_problem::gh_signed_out:        return "gh_signed_out";
		case account_setup_problem::gh_no_usable_account: return "gh_no_usable_account";
		case account_setup_problem::code_expired:         return "code_expired";
		case account_setup_problem::code_denied:          return "code_denied";
		case account_setup_problem::browser_failed:       return "browser_failed";
		case account_setup_problem::browser_unavailable:  return "browser_unavailable";
		case account_setup_problem::connection_failed:    return "connection_failed";
		}
		return "unknown";
	}

	auto stable_name(const account_setup_effect_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case account_setup_effect_kind::inspect_github_cli:     return "inspect_github_cli";
		case account_setup_effect_kind::begin_browser_sign_in:  return "begin_browser_sign_in";
		case account_setup_effect_kind::cancel_browser_sign_in: return "cancel_browser_sign_in";
		case account_setup_effect_kind::connect_github_cli:     return "connect_github_cli";
		}
		return "unknown";
	}

	auto stable_name(const account_setup_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case account_setup_refusal::none:            return "none";
		case account_setup_refusal::not_now:         return "not_now";
		case account_setup_refusal::stale:           return "stale";
		case account_setup_refusal::unknown_account: return "unknown_account";
		case account_setup_refusal::exhausted:       return "exhausted";
		}
		return "unknown";
	}

	auto account_setup_step_of(const account_setup_state& aState) noexcept -> account_setup_step
	{
		switch (aState.Phase) {
		case account_setup_phase::idle:
		case account_setup_phase::choosing_way:     return account_setup_step::way;
		case account_setup_phase::requesting_code:
		case account_setup_phase::awaiting_browser:
		case account_setup_phase::browser_stopped:
		case account_setup_phase::inspecting_cli:
		case account_setup_phase::choosing_account: return account_setup_step::sign_in;
		case account_setup_phase::connecting:       return account_setup_step::confirm;
		case account_setup_phase::connected:        return account_setup_step::ready;
		}
		return account_setup_step::way;
	}

	auto transition_account_setup(const account_setup_state& aState, const account_setup_event& aEvent) -> account_setup_transition
	{
		if (const auto* const begin = std::get_if<begin_account_setup>(&aEvent)) {
			if (aState.Phase != account_setup_phase::idle && aState.Phase != account_setup_phase::choosing_way) {
				return refused(aState, account_setup_refusal::not_now);
			}
			if (begin->Way.has_value()) {
				return start_way(aState, *begin->Way);
			}
			if (aState.Phase == account_setup_phase::choosing_way) {
				return refused(aState, account_setup_refusal::not_now);
			}
			return account_setup_transition{ .State = at_way(aState), .Effects = { }, .Refusal = account_setup_refusal::none };
		}
		if (const auto* const choose = std::get_if<choose_account_setup_way>(&aEvent)) {
			if (aState.Phase != account_setup_phase::choosing_way) {
				return refused(aState, account_setup_refusal::not_now);
			}
			return start_way(aState, choose->Way);
		}
		if (const auto* const inspected = std::get_if<github_cli_inspected>(&aEvent)) {
			return on_inspected(aState, *inspected);
		}
		if (const auto* const account = std::get_if<choose_account_setup_account>(&aEvent)) {
			if (aState.Phase != account_setup_phase::choosing_account) {
				return refused(aState, account_setup_refusal::not_now);
			}
			if (std::ranges::none_of(aState.Accounts, [account](const account_setup_account& aOffered) { return aOffered.Profile == account->Profile; })) {
				return refused(aState, account_setup_refusal::unknown_account);
			}
			auto next   = aState;
			next.Chosen = account->Profile;
			if (next.Problem == account_setup_problem::connection_failed) {
				next.Problem = account_setup_problem::none;
			}
			return account_setup_transition{ .State = std::move(next), .Effects = { }, .Refusal = account_setup_refusal::none };
		}
		if (std::holds_alternative<connect_account_setup>(aEvent)) {
			return on_connect(aState);
		}
		if (const auto* const device = std::get_if<account_setup_device_observed>(&aEvent)) {
			return on_device(aState, *device);
		}
		if (const auto* const session = std::get_if<account_setup_session_observed>(&aEvent)) {
			return on_session(aState, *session);
		}
		if (std::holds_alternative<retry_account_setup>(aEvent)) {
			return on_retry(aState);
		}
		if (std::holds_alternative<return_to_account_setup_way>(aEvent)) {
			return on_back(aState);
		}
		if (aState.Phase != account_setup_phase::connected) {
			return refused(aState, account_setup_refusal::not_now);
		}
		auto next    = account_setup_state{ };
		next.Request = aState.Request;
		return account_setup_transition{ .State = std::move(next), .Effects = { }, .Refusal = account_setup_refusal::none };
	}
} // namespace bpr::core
