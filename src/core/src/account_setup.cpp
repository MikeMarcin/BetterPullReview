#include "bpr/core/account_setup.hpp"

#include "bpr/core/github_hostname.hpp"

#include <algorithm>
#include <limits>
#include <type_traits>
#include <utility>
#include <variant>

namespace bpr::core {
	namespace {
		constexpr std::string_view github_com_hostname = "github.com";

		[[nodiscard]] auto refused(const account_setup_state& aState, const account_setup_refusal aRefusal) -> account_setup_transition
		{
			return account_setup_transition{ .State = aState, .Effects = { }, .Refusal = aRefusal };
		}

		[[nodiscard]] auto moved(account_setup_state aState, std::vector<account_setup_effect> aEffects = { }) -> account_setup_transition
		{
			return account_setup_transition{ .State = std::move(aState), .Effects = std::move(aEffects), .Refusal = account_setup_refusal::none };
		}

		[[nodiscard]] auto browser_running(const account_setup_state& aState) noexcept -> bool
		{
			return aState.Way == account_setup_way::browser
				&& (aState.Phase == account_setup_phase::requesting_code || aState.Phase == account_setup_phase::awaiting_browser);
		}

		[[nodiscard]] auto in_browser_flow(const account_setup_state& aState) noexcept -> bool
		{
			return browser_running(aState) || (aState.Way == account_setup_way::browser && aState.Phase == account_setup_phase::connecting);
		}

		[[nodiscard]] auto cancel_browser(const account_setup_state& aState) -> std::vector<account_setup_effect>
		{
			if (!browser_running(aState)) {
				return { };
			}
			auto effect    = account_setup_effect{ };
			effect.Kind    = account_setup_effect_kind::cancel_browser_sign_in;
			effect.Request = aState.Request;
			return { effect };
		}

		[[nodiscard]] auto exhausted(const account_setup_state& aState) noexcept -> bool
		{
			return aState.Request == std::numeric_limits<std::uint64_t>::max();
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

		/// gh's active account first (owner, 2026-11-12); else the only one.
		[[nodiscard]] auto first_choice(const std::vector<account_setup_account>& aAccounts) -> std::optional<host_profile_id>
		{
			const auto active = std::ranges::find_if(aAccounts, &account_setup_account::ActiveInGh);
			if (active != aAccounts.end()) {
				return active->Profile;
			}
			return aAccounts.size() == 1U ? std::optional{ aAccounts.front().Profile } : std::nullopt;
		}

		/// Only the request counter survives a new start.
		[[nodiscard]] auto fresh(const account_setup_state& aState) -> account_setup_state
		{
			auto next    = account_setup_state{ };
			next.Request = aState.Request;
			return next;
		}

		/// The server step, keeping what was typed so it can be changed.
		[[nodiscard]] auto at_server(const account_setup_state& aState) -> account_setup_state
		{
			auto next     = fresh(aState);
			next.Phase    = account_setup_phase::choosing_server;
			next.Server   = aState.Server;
			next.Hostname = aState.Server == account_setup_server::enterprise ? aState.Hostname : std::string{ };
			return next;
		}

		/// The way step for the chosen server.
		[[nodiscard]] auto at_way(const account_setup_state& aState) -> account_setup_state
		{
			auto next          = fresh(aState);
			next.Phase         = account_setup_phase::choosing_way;
			next.Server        = aState.Server;
			next.Hostname      = aState.Hostname;
			next.ServerProfile = aState.ServerProfile;
			return next;
		}

		/// Starts a way: the effect it asks for carries a new request.
		[[nodiscard]] auto start_way(const account_setup_state& aState, const account_setup_way aWay, std::vector<account_setup_effect> aEffects = { })
			-> account_setup_transition
		{
			if (!aState.Server || !account_setup_way_offered(*aState.Server, aWay)) {
				return refused(aState, account_setup_refusal::not_offered);
			}
			if (aWay == account_setup_way::token && !aState.ServerProfile) {
				return refused(aState, account_setup_refusal::not_now);
			}
			if (exhausted(aState)) {
				return refused(aState, account_setup_refusal::exhausted);
			}
			auto next = at_way(aState);
			next.Way  = aWay;
			if (aWay == account_setup_way::token) {
				next.Phase = account_setup_phase::entering_token;
				return moved(std::move(next), std::move(aEffects));
			}
			next.Request    = aState.Request + 1U;
			auto effect     = account_setup_effect{ };
			effect.Request  = next.Request;
			effect.Hostname = next.Hostname;
			if (aWay == account_setup_way::browser) {
				next.Phase  = account_setup_phase::requesting_code;
				effect.Kind = account_setup_effect_kind::begin_browser_sign_in;
			} else {
				next.Phase  = account_setup_phase::inspecting_cli;
				effect.Kind = account_setup_effect_kind::inspect_github_cli;
			}
			aEffects.push_back(std::move(effect));
			return moved(std::move(next), std::move(aEffects));
		}

		/// GitHub.com goes straight to its ways; an Enterprise host is checked,
		/// then added so it has a profile, before its ways are offered.
		[[nodiscard]] auto choose_server(const account_setup_state& aState, const account_setup_server aServer, const std::string& aHostname,
										 const std::optional<account_setup_way> aWay, std::vector<account_setup_effect> aEffects = { }) -> account_setup_transition
		{
			auto next = fresh(aState);
			if (aServer == account_setup_server::github_com || aHostname == github_com_hostname) {
				next.Server   = account_setup_server::github_com;
				next.Hostname = std::string{ github_com_hostname };
				next.Phase    = account_setup_phase::choosing_way;
				if (aWay && account_setup_way_offered(account_setup_server::github_com, *aWay)) {
					return start_way(next, *aWay, std::move(aEffects));
				}
				return moved(std::move(next), std::move(aEffects));
			}
			next.Server   = account_setup_server::enterprise;
			next.Hostname = aHostname;
			if (!valid_github_canonical_hostname(aHostname)) {
				next.Phase   = account_setup_phase::choosing_server;
				next.Problem = aHostname.empty() ? account_setup_problem::none : account_setup_problem::invalid_hostname;
				return moved(std::move(next), std::move(aEffects));
			}
			if (exhausted(aState)) {
				return refused(aState, account_setup_refusal::exhausted);
			}
			next.Phase      = account_setup_phase::preparing_server;
			next.Intended   = aWay;
			next.Request    = aState.Request + 1U;
			auto effect     = account_setup_effect{ };
			effect.Kind     = account_setup_effect_kind::prepare_enterprise_server;
			effect.Request  = next.Request;
			effect.Hostname = aHostname;
			aEffects.push_back(std::move(effect));
			return moved(std::move(next), std::move(aEffects));
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
			case device_authorization_stage::denied:      return account_setup_problem::code_denied;
			case device_authorization_stage::expired:     return account_setup_problem::code_expired;
			case device_authorization_stage::disabled:
			case device_authorization_stage::unavailable: return account_setup_problem::browser_unavailable;
			case device_authorization_stage::failed:      return account_setup_problem::browser_failed;
			case device_authorization_stage::idle:
			case device_authorization_stage::requested:
			case device_authorization_stage::awaiting_authorization:
			case device_authorization_stage::cancelling:
			case device_authorization_stage::authorized:
			case device_authorization_stage::cancelled:   break;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto on_begin(const account_setup_state& aState, const begin_account_setup& aEvent) -> account_setup_transition
		{
			if (aState.Phase == account_setup_phase::connecting) {
				return refused(aState, account_setup_refusal::not_now);
			}
			auto effects = cancel_browser(aState);
			if (!aEvent.Server) {
				auto next  = fresh(aState);
				next.Phase = account_setup_phase::choosing_server;
				return moved(std::move(next), std::move(effects));
			}
			if (*aEvent.Server == account_setup_server::enterprise && aEvent.Hostname.empty()) {
				auto next   = fresh(aState);
				next.Phase  = account_setup_phase::choosing_server;
				next.Server = account_setup_server::enterprise;
				return moved(std::move(next), std::move(effects));
			}
			return choose_server(aState, *aEvent.Server, aEvent.Hostname, aEvent.Way, std::move(effects));
		}

		[[nodiscard]] auto on_prepared(const account_setup_state& aState, const enterprise_server_prepared& aEvent) -> account_setup_transition
		{
			if (aState.Phase != account_setup_phase::preparing_server) {
				return refused(aState, account_setup_refusal::not_now);
			}
			if (aEvent.Request != aState.Request) {
				return refused(aState, account_setup_refusal::stale);
			}
			if (!aEvent.Profile || aEvent.Profile->value() == 0U) {
				auto next    = at_server(aState);
				next.Problem = account_setup_problem::server_unavailable;
				return moved(std::move(next));
			}
			auto next          = at_way(aState);
			next.ServerProfile = aEvent.Profile;
			if (aState.Intended && account_setup_way_offered(account_setup_server::enterprise, *aState.Intended)) {
				return start_way(next, *aState.Intended);
			}
			return moved(std::move(next));
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
			// Connect is still the reviewer's explicit consent to the one chosen.
			next.Chosen = first_choice(next.Accounts);
			return moved(std::move(next));
		}

		[[nodiscard]] auto on_device(const account_setup_state& aState, const account_setup_device_observed& aEvent) -> account_setup_transition
		{
			// A device flow begun elsewhere for GitHub.com (a pasted link that
			// needs an account) is followed as the browser way.
			const auto adopting = aState.Phase == account_setup_phase::choosing_way && aState.Server == account_setup_server::github_com
				&& (aEvent.Stage == device_authorization_stage::requested || aEvent.Stage == device_authorization_stage::awaiting_authorization);
			if (!adopting && !in_browser_flow(aState)) {
				return refused(aState, account_setup_refusal::not_now);
			}
			auto next = aState;
			if (adopting) {
				next     = at_way(aState);
				next.Way = account_setup_way::browser;
			}
			if (const auto problem = problem_of(aEvent.Stage); problem.has_value()) {
				if (aState.Phase == account_setup_phase::connecting) {
					return refused(aState, account_setup_refusal::not_now);
				}
				next.Phase   = account_setup_phase::browser_stopped;
				next.Problem = *problem;
				return moved(std::move(next));
			}
			switch (aEvent.Stage) {
			case device_authorization_stage::requested:
			case device_authorization_stage::awaiting_authorization:
				if (aState.Phase == account_setup_phase::connecting) {
					return refused(aState, account_setup_refusal::not_now);
				}
				next.Phase = aEvent.Stage == device_authorization_stage::requested ? account_setup_phase::requesting_code : account_setup_phase::awaiting_browser;
				break;
			case device_authorization_stage::authorized: next.Phase = account_setup_phase::connecting; break;
			case device_authorization_stage::cancelled:
				// While a new code is asked for, a cancellation is the previous
				// flow's, ended by Back; it does not undo the new request.
				if (aState.Phase != account_setup_phase::awaiting_browser) {
					return refused(aState, account_setup_refusal::not_now);
				}
				return moved(at_way(aState));
			case device_authorization_stage::idle:
			case device_authorization_stage::cancelling:
			case device_authorization_stage::denied:
			case device_authorization_stage::expired:
			case device_authorization_stage::disabled:
			case device_authorization_stage::unavailable:
			case device_authorization_stage::failed:     return refused(aState, account_setup_refusal::not_now);
			}
			if (next == aState) {
				return refused(aState, account_setup_refusal::not_now);
			}
			return moved(std::move(next));
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
				// Before any way started there is nothing to confirm: a remembered
				// connection validated at launch simply ends the setup.
				if (aState.Phase == account_setup_phase::choosing_server || aState.Phase == account_setup_phase::choosing_way) {
					return moved(fresh(aState));
				}
				auto next     = aState;
				next.Phase    = account_setup_phase::connected;
				next.Problem  = account_setup_problem::none;
				next.Login    = aEvent.Login;
				next.Hostname = aEvent.Hostname.empty() ? aState.Hostname : aEvent.Hostname;
				return moved(std::move(next), cancel_browser(aState));
			}
			if (aState.Phase != account_setup_phase::connecting) {
				return refused(aState, account_setup_refusal::not_now);
			}
			auto       next      = aState;
			const auto cancelled = aEvent.Outcome == account_setup_session::cancelled;
			switch (aState.Way.value_or(account_setup_way::browser)) {
			case account_setup_way::browser:
				next.Phase   = account_setup_phase::browser_stopped;
				next.Problem = account_setup_problem::connection_failed;
				break;
			case account_setup_way::github_cli:
				// The list stays, with its choice, so Connect can be pressed again.
				next.Phase   = account_setup_phase::choosing_account;
				next.Problem = cancelled ? account_setup_problem::none : account_setup_problem::connection_failed;
				break;
			case account_setup_way::token:
				next.Phase   = account_setup_phase::entering_token;
				next.Problem = cancelled ? account_setup_problem::none : account_setup_problem::connection_failed;
				break;
			}
			return moved(std::move(next));
		}

		[[nodiscard]] auto on_return_to_way(const account_setup_state& aState) -> account_setup_transition
		{
			switch (aState.Phase) {
			case account_setup_phase::requesting_code:
			case account_setup_phase::awaiting_browser:
			case account_setup_phase::browser_stopped:
			case account_setup_phase::inspecting_cli:
			case account_setup_phase::choosing_account:
			case account_setup_phase::entering_token:   return moved(at_way(aState), cancel_browser(aState));
			case account_setup_phase::idle:
			case account_setup_phase::choosing_server:
			case account_setup_phase::preparing_server:
			case account_setup_phase::choosing_way:
			case account_setup_phase::connecting:
			case account_setup_phase::connected:        break;
			}
			return refused(aState, account_setup_refusal::not_now);
		}

		[[nodiscard]] auto on_return_to_server(const account_setup_state& aState) -> account_setup_transition
		{
			switch (aState.Phase) {
			case account_setup_phase::idle:
			case account_setup_phase::choosing_server:
			case account_setup_phase::connecting:
			case account_setup_phase::connected:       return refused(aState, account_setup_refusal::not_now);
			case account_setup_phase::preparing_server:
			case account_setup_phase::choosing_way:
			case account_setup_phase::requesting_code:
			case account_setup_phase::awaiting_browser:
			case account_setup_phase::browser_stopped:
			case account_setup_phase::inspecting_cli:
			case account_setup_phase::choosing_account:
			case account_setup_phase::entering_token:  break;
			}
			return moved(at_server(aState), cancel_browser(aState));
		}

		[[nodiscard]] auto on_connect(const account_setup_state& aState) -> account_setup_transition
		{
			if (aState.Phase != account_setup_phase::choosing_account || !aState.Chosen.has_value()) {
				return refused(aState, account_setup_refusal::not_now);
			}
			if (exhausted(aState)) {
				return refused(aState, account_setup_refusal::exhausted);
			}
			auto next      = aState;
			next.Phase     = account_setup_phase::connecting;
			next.Problem   = account_setup_problem::none;
			next.Request   = aState.Request + 1U;
			auto effect    = account_setup_effect{ };
			effect.Kind    = account_setup_effect_kind::connect_github_cli;
			effect.Request = next.Request;
			effect.Profile = aState.Chosen;
			return moved(std::move(next), { std::move(effect) });
		}

		[[nodiscard]] auto on_token(const account_setup_state& aState) -> account_setup_transition
		{
			if (aState.Phase != account_setup_phase::entering_token || !aState.ServerProfile) {
				return refused(aState, account_setup_refusal::not_now);
			}
			if (exhausted(aState)) {
				return refused(aState, account_setup_refusal::exhausted);
			}
			auto next       = aState;
			next.Phase      = account_setup_phase::connecting;
			next.Problem    = account_setup_problem::none;
			next.Request    = aState.Request + 1U;
			auto effect     = account_setup_effect{ };
			effect.Kind     = account_setup_effect_kind::enroll_token;
			effect.Request  = next.Request;
			effect.Profile  = aState.ServerProfile;
			effect.Hostname = aState.Hostname;
			return moved(std::move(next), { std::move(effect) });
		}

		[[nodiscard]] auto on_retry(const account_setup_state& aState) -> account_setup_transition
		{
			switch (aState.Phase) {
			case account_setup_phase::browser_stopped:  return start_way(aState, account_setup_way::browser);
			case account_setup_phase::choosing_account: return start_way(aState, account_setup_way::github_cli);
			case account_setup_phase::choosing_server:
				if (aState.Problem == account_setup_problem::server_unavailable && aState.Server == account_setup_server::enterprise) {
					return choose_server(aState, account_setup_server::enterprise, aState.Hostname, std::nullopt);
				}
				break;
			default: break;
			}
			return refused(aState, account_setup_refusal::not_now);
		}

		[[nodiscard]] auto on_finish(const account_setup_state& aState) -> account_setup_transition
		{
			if (aState.Phase == account_setup_phase::idle || aState.Phase == account_setup_phase::connecting) {
				return refused(aState, account_setup_refusal::not_now);
			}
			return moved(fresh(aState), cancel_browser(aState));
		}
	} // namespace

	auto stable_name(const account_setup_server aServer) noexcept -> std::string_view
	{
		switch (aServer) {
		case account_setup_server::github_com: return "github_com";
		case account_setup_server::enterprise: return "enterprise";
		}
		return "unknown";
	}

	auto stable_name(const account_setup_way aWay) noexcept -> std::string_view
	{
		switch (aWay) {
		case account_setup_way::browser:    return "browser";
		case account_setup_way::github_cli: return "github_cli";
		case account_setup_way::token:      return "token";
		}
		return "unknown";
	}

	auto account_setup_way_offered(const account_setup_server aServer, const account_setup_way aWay) noexcept -> bool
	{
		switch (aWay) {
		case account_setup_way::browser:    return aServer == account_setup_server::github_com;
		case account_setup_way::github_cli: return true;
		case account_setup_way::token:      return aServer == account_setup_server::enterprise;
		}
		return false;
	}

	auto stable_name(const account_setup_step aStep) noexcept -> std::string_view
	{
		switch (aStep) {
		case account_setup_step::server:  return "server";
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
		case account_setup_phase::choosing_server:  return "choosing_server";
		case account_setup_phase::preparing_server: return "preparing_server";
		case account_setup_phase::choosing_way:     return "choosing_way";
		case account_setup_phase::requesting_code:  return "requesting_code";
		case account_setup_phase::awaiting_browser: return "awaiting_browser";
		case account_setup_phase::browser_stopped:  return "browser_stopped";
		case account_setup_phase::inspecting_cli:   return "inspecting_cli";
		case account_setup_phase::choosing_account: return "choosing_account";
		case account_setup_phase::entering_token:   return "entering_token";
		case account_setup_phase::connecting:       return "connecting";
		case account_setup_phase::connected:        return "connected";
		}
		return "unknown";
	}

	auto stable_name(const account_setup_problem aProblem) noexcept -> std::string_view
	{
		switch (aProblem) {
		case account_setup_problem::none:                 return "none";
		case account_setup_problem::invalid_hostname:     return "invalid_hostname";
		case account_setup_problem::server_unavailable:   return "server_unavailable";
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
		case account_setup_effect_kind::prepare_enterprise_server: return "prepare_enterprise_server";
		case account_setup_effect_kind::inspect_github_cli:        return "inspect_github_cli";
		case account_setup_effect_kind::begin_browser_sign_in:     return "begin_browser_sign_in";
		case account_setup_effect_kind::cancel_browser_sign_in:    return "cancel_browser_sign_in";
		case account_setup_effect_kind::connect_github_cli:        return "connect_github_cli";
		case account_setup_effect_kind::enroll_token:              return "enroll_token";
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
		case account_setup_refusal::not_offered:     return "not_offered";
		case account_setup_refusal::exhausted:       return "exhausted";
		}
		return "unknown";
	}

	auto account_setup_step_of(const account_setup_state& aState) noexcept -> account_setup_step
	{
		switch (aState.Phase) {
		case account_setup_phase::idle:
		case account_setup_phase::choosing_server:
		case account_setup_phase::preparing_server: return account_setup_step::server;
		case account_setup_phase::choosing_way:     return account_setup_step::way;
		case account_setup_phase::requesting_code:
		case account_setup_phase::awaiting_browser:
		case account_setup_phase::browser_stopped:
		case account_setup_phase::inspecting_cli:
		case account_setup_phase::choosing_account:
		case account_setup_phase::entering_token:   return account_setup_step::sign_in;
		case account_setup_phase::connecting:       return account_setup_step::confirm;
		case account_setup_phase::connected:        return account_setup_step::ready;
		}
		return account_setup_step::server;
	}

	auto transition_account_setup(const account_setup_state& aState, const account_setup_event& aEvent) -> account_setup_transition
	{
		return std::visit(
			[&aState](const auto& aValue) -> account_setup_transition {
				using event = std::decay_t<decltype(aValue)>;
				if constexpr (std::is_same_v<event, begin_account_setup>) {
					return on_begin(aState, aValue);
				} else if constexpr (std::is_same_v<event, choose_account_setup_server>) {
					if (aState.Phase != account_setup_phase::choosing_server) {
						return refused(aState, account_setup_refusal::not_now);
					}
					return choose_server(aState, aValue.Server, aValue.Hostname, std::nullopt);
				} else if constexpr (std::is_same_v<event, enterprise_server_prepared>) {
					return on_prepared(aState, aValue);
				} else if constexpr (std::is_same_v<event, choose_account_setup_way>) {
					if (aState.Phase != account_setup_phase::choosing_way) {
						return refused(aState, account_setup_refusal::not_now);
					}
					return start_way(aState, aValue.Way);
				} else if constexpr (std::is_same_v<event, github_cli_inspected>) {
					return on_inspected(aState, aValue);
				} else if constexpr (std::is_same_v<event, choose_account_setup_account>) {
					if (aState.Phase != account_setup_phase::choosing_account) {
						return refused(aState, account_setup_refusal::not_now);
					}
					if (std::ranges::none_of(aState.Accounts, [&aValue](const account_setup_account& aOffered) { return aOffered.Profile == aValue.Profile; })) {
						return refused(aState, account_setup_refusal::unknown_account);
					}
					auto next   = aState;
					next.Chosen = aValue.Profile;
					if (next.Problem == account_setup_problem::connection_failed) {
						next.Problem = account_setup_problem::none;
					}
					return moved(std::move(next));
				} else if constexpr (std::is_same_v<event, connect_account_setup>) {
					return on_connect(aState);
				} else if constexpr (std::is_same_v<event, submit_account_setup_token>) {
					return on_token(aState);
				} else if constexpr (std::is_same_v<event, account_setup_device_observed>) {
					return on_device(aState, aValue);
				} else if constexpr (std::is_same_v<event, account_setup_session_observed>) {
					return on_session(aState, aValue);
				} else if constexpr (std::is_same_v<event, retry_account_setup>) {
					return on_retry(aState);
				} else if constexpr (std::is_same_v<event, return_to_account_setup_way>) {
					return on_return_to_way(aState);
				} else if constexpr (std::is_same_v<event, return_to_account_setup_server>) {
					return on_return_to_server(aState);
				} else {
					return on_finish(aState);
				}
			},
			aEvent);
	}
} // namespace bpr::core
