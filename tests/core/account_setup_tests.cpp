/// \file
/// \brief RFC-BPR-0032 amendment r1. The in-window account setup: its step
/// order, both ways in, stale answers, failure, cancel and retry.

#include "bpr/core/account_setup.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using core::account_setup_account;
	using core::account_setup_effect_kind;
	using core::account_setup_phase;
	using core::account_setup_problem;
	using core::account_setup_refusal;
	using core::account_setup_state;
	using core::account_setup_step;
	using core::account_setup_way;
	using core::device_authorization_stage;
	using core::github_cli_inspection;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto account(const std::uint64_t aProfile, std::string aLogin, const bool aActive = false) -> account_setup_account
	{
		auto result       = account_setup_account{ };
		result.Profile    = core::host_profile_id{ aProfile };
		result.Login      = std::move(aLogin);
		result.Hostname   = "github.com";
		result.ActiveInGh = aActive;
		return result;
	}

	[[nodiscard]] auto apply(const account_setup_state& aState, const core::account_setup_event& aEvent) -> core::account_setup_transition
	{
		return core::transition_account_setup(aState, aEvent);
	}

	[[nodiscard]] auto ready(std::string aLogin) -> core::account_setup_session_observed
	{
		auto session     = core::account_setup_session_observed{ };
		session.Outcome  = core::account_setup_session::ready;
		session.Login    = std::move(aLogin);
		session.Hostname = "github.com";
		return session;
	}

	[[nodiscard]] auto ended(const core::account_setup_session aOutcome) -> core::account_setup_session_observed
	{
		auto session    = core::account_setup_session_observed{ };
		session.Outcome = aOutcome;
		return session;
	}

	[[nodiscard]] auto inspected(const std::uint64_t aRequest, const github_cli_inspection aOutcome, std::vector<account_setup_account> aAccounts) -> core::github_cli_inspected
	{
		auto event     = core::github_cli_inspected{ };
		event.Request  = aRequest;
		event.Outcome  = aOutcome;
		event.Accounts = std::move(aAccounts);
		return event;
	}

	void test_begin_and_steps()
	{
		const auto begun = apply(account_setup_state{ }, core::begin_account_setup{ });
		expect(begun.Refusal == account_setup_refusal::none && begun.State.Phase == account_setup_phase::choosing_way, "Home begins at the way choice");
		expect(begun.Effects.empty(), "showing the way choice runs nothing");
		expect(core::account_setup_step_of(begun.State) == account_setup_step::way, "the first step is the way");

		const auto again = apply(begun.State, core::begin_account_setup{ });
		expect(again.Refusal == account_setup_refusal::not_now && again.State == begun.State, "beginning twice changes nothing");

		const auto direct = apply(account_setup_state{ }, core::begin_account_setup{ .Way = account_setup_way::github_cli });
		expect(direct.State.Phase == account_setup_phase::inspecting_cli && direct.Effects.size() == 1U
				   && direct.Effects.front().Kind == account_setup_effect_kind::inspect_github_cli,
			   "Use GitHub CLI on Home starts at gh's check");
		expect(core::account_setup_step_of(direct.State) == account_setup_step::sign_in, "checking gh is the second step");
		expect(apply(account_setup_state{ }, core::finish_account_setup{ }).Refusal == account_setup_refusal::not_now, "only a connected setup finishes");
	}

	void test_browser_way()
	{
		auto state  = apply(account_setup_state{ }, core::begin_account_setup{ }).State;
		auto chosen = apply(state, core::choose_account_setup_way{ .Way = account_setup_way::browser });
		expect(chosen.State.Phase == account_setup_phase::requesting_code, "the browser way asks for a code");
		expect(chosen.Effects.size() == 1U && chosen.Effects.front().Kind == account_setup_effect_kind::begin_browser_sign_in
				   && chosen.Effects.front().Request == chosen.State.Request && chosen.State.Request == 1U,
			   "the code request carries a new identity");
		state = chosen.State;

		state = apply(state, core::account_setup_device_observed{ .Stage = device_authorization_stage::awaiting_authorization }).State;
		expect(state.Phase == account_setup_phase::awaiting_browser && core::account_setup_step_of(state) == account_setup_step::sign_in, "a shown code waits for the browser");

		state = apply(state, core::account_setup_device_observed{ .Stage = device_authorization_stage::authorized }).State;
		expect(state.Phase == account_setup_phase::connecting && core::account_setup_step_of(state) == account_setup_step::confirm, "an authorized grant confirms the account");

		const auto late = apply(state, core::account_setup_device_observed{ .Stage = device_authorization_stage::failed });
		expect(late.Refusal == account_setup_refusal::not_now && late.State == state, "a terminal after authorization is not this flow's failure");

		const auto connected = apply(state, ready("octo-reviewer"));
		expect(connected.State.Phase == account_setup_phase::connected && connected.State.Login == "octo-reviewer"
				   && core::account_setup_step_of(connected.State) == account_setup_step::ready,
			   "the accepted account is the ready step");
		expect(connected.Effects.empty(), "a finished grant needs no cancel");

		const auto finished = apply(connected.State, core::finish_account_setup{ });
		expect(finished.State.Phase == account_setup_phase::idle && finished.State.Login.empty(), "Continue leaves the setup");
		expect(finished.State.Request == connected.State.Request, "the request counter survives the finish");
	}

	void test_browser_failure_retry_and_back()
	{
		auto state = apply(account_setup_state{ }, core::begin_account_setup{ .Way = account_setup_way::browser }).State;
		state      = apply(state, core::account_setup_device_observed{ .Stage = device_authorization_stage::awaiting_authorization }).State;

		const auto expired = apply(state, core::account_setup_device_observed{ .Stage = device_authorization_stage::expired });
		expect(expired.State.Phase == account_setup_phase::browser_stopped && expired.State.Problem == account_setup_problem::code_expired, "an expired code says so");
		const auto denied = apply(state, core::account_setup_device_observed{ .Stage = device_authorization_stage::denied });
		expect(denied.State.Problem == account_setup_problem::code_denied, "a declined code says so");
		const auto unavailable = apply(state, core::account_setup_device_observed{ .Stage = device_authorization_stage::unavailable });
		expect(unavailable.State.Problem == account_setup_problem::browser_unavailable, "no device sign-in says so");
		const auto failed = apply(state, core::account_setup_device_observed{ .Stage = device_authorization_stage::failed });
		expect(failed.State.Problem == account_setup_problem::browser_failed, "a failed flow says so");

		const auto retried = apply(expired.State, core::retry_account_setup{ });
		expect(retried.State.Phase == account_setup_phase::requesting_code && retried.State.Problem == account_setup_problem::none, "Try again asks for a new code");
		expect(retried.Effects.size() == 1U && retried.Effects.front().Kind == account_setup_effect_kind::begin_browser_sign_in
				   && retried.State.Request == expired.State.Request + 1U,
			   "the new code is a new request");

		const auto back = apply(state, core::return_to_account_setup_way{ });
		expect(back.State.Phase == account_setup_phase::choosing_way && !back.State.Way.has_value(), "Back returns to the way choice");
		expect(back.Effects.size() == 1U && back.Effects.front().Kind == account_setup_effect_kind::cancel_browser_sign_in, "Back ends the shown code");
		const auto stoppedBack = apply(expired.State, core::return_to_account_setup_way{ });
		expect(stoppedBack.State.Phase == account_setup_phase::choosing_way && stoppedBack.Effects.empty(), "Back from a stopped flow cancels nothing");

		// Back, then the browser again: the old flow's cancellation arrives
		// while the new code is asked for, and does not undo it.
		auto       renewed   = apply(back.State, core::choose_account_setup_way{ .Way = account_setup_way::browser }).State;
		const auto oldCancel = apply(renewed, core::account_setup_device_observed{ .Stage = device_authorization_stage::cancelled });
		expect(oldCancel.Refusal == account_setup_refusal::not_now && oldCancel.State == renewed, "an earlier flow's cancellation is not the new request's");
		renewed              = apply(renewed, core::account_setup_device_observed{ .Stage = device_authorization_stage::awaiting_authorization }).State;
		const auto cancelled = apply(renewed, core::account_setup_device_observed{ .Stage = device_authorization_stage::cancelled });
		expect(cancelled.State.Phase == account_setup_phase::choosing_way, "a code cancelled elsewhere returns to the way choice");

		const auto outside = apply(account_setup_state{ }, core::account_setup_device_observed{ .Stage = device_authorization_stage::awaiting_authorization });
		expect(outside.Refusal == account_setup_refusal::not_now, "a device flow the setup did not begin is not followed");
		auto       connecting     = apply(state, core::account_setup_device_observed{ .Stage = device_authorization_stage::authorized }).State;
		const auto refusedAccount = apply(connecting, ended(core::account_setup_session::failed));
		expect(refusedAccount.State.Phase == account_setup_phase::browser_stopped && refusedAccount.State.Problem == account_setup_problem::connection_failed,
			   "an account GitHub refused stops the browser way with a retry");
		expect(apply(connecting, core::return_to_account_setup_way{ }).Refusal == account_setup_refusal::not_now, "the account check is not abandoned midway");
	}

	void test_github_cli_way()
	{
		auto       state   = apply(account_setup_state{ }, core::begin_account_setup{ }).State;
		const auto started = apply(state, core::choose_account_setup_way{ .Way = account_setup_way::github_cli });
		state              = started.State;
		const auto request = started.Effects.front().Request;

		const auto stale = apply(state, inspected(request + 7U, github_cli_inspection::read, { account(4U, "octo") }));
		expect(stale.Refusal == account_setup_refusal::stale && stale.State == state, "an answer to another request is stale");

		const auto several = apply(state, inspected(request, github_cli_inspection::read, { account(4U, "octo", true), account(5U, "octo-test"), account(4U, "octo"), account(0U, "nobody"), account(6U, "") }));
		expect(several.State.Phase == account_setup_phase::choosing_account && several.State.Accounts.size() == 2U, "usable accounts are listed once each");
		expect(!several.State.Chosen.has_value(), "several accounts wait for a choice, even with one active in gh");
		expect(apply(several.State, core::connect_account_setup{ }).Refusal == account_setup_refusal::not_now, "Connect needs a chosen account");
		expect(apply(several.State, core::choose_account_setup_account{ .Profile = core::host_profile_id{ 9U } }).Refusal == account_setup_refusal::unknown_account,
			   "an account not offered cannot be chosen");

		state = apply(several.State, core::choose_account_setup_account{ .Profile = core::host_profile_id{ 5U } }).State;
		expect(state.Chosen == core::host_profile_id{ 5U }, "the reviewer's choice is kept");
		const auto connect = apply(state, core::connect_account_setup{ });
		expect(connect.State.Phase == account_setup_phase::connecting && connect.Effects.size() == 1U
				   && connect.Effects.front().Kind == account_setup_effect_kind::connect_github_cli && connect.Effects.front().Profile == core::host_profile_id{ 5U },
			   "Connect asks the host to connect exactly the chosen account");
		expect(core::account_setup_step_of(connect.State) == account_setup_step::confirm, "connecting is the confirm step");

		const auto refused = apply(connect.State, ended(core::account_setup_session::failed));
		expect(refused.State.Phase == account_setup_phase::choosing_account && refused.State.Problem == account_setup_problem::connection_failed
				   && refused.State.Chosen == core::host_profile_id{ 5U } && refused.State.Accounts.size() == 2U,
			   "a refused account returns to the list with the choice kept");
		const auto rechosen = apply(refused.State, core::choose_account_setup_account{ .Profile = core::host_profile_id{ 4U } });
		expect(rechosen.State.Problem == account_setup_problem::none, "choosing again clears the refusal");

		const auto done = apply(connect.State, ready("octo-test"));
		expect(done.State.Phase == account_setup_phase::connected && done.State.Login == "octo-test", "the connected gh account is ready");

		const auto single = apply(state = started.State, inspected(request, github_cli_inspection::read, { account(4U, "octo") }));
		expect(single.State.Chosen == core::host_profile_id{ 4U }, "one account is chosen without asking");
		expect(single.Effects.empty(), "and is not connected without Connect");
	}

	void test_github_cli_problems()
	{
		const auto started = apply(account_setup_state{ }, core::begin_account_setup{ .Way = account_setup_way::github_cli });
		const auto request = started.State.Request;
		const auto cases   = std::vector<std::pair<github_cli_inspection, account_setup_problem>>{
			{ github_cli_inspection::missing, account_setup_problem::gh_missing },
			{ github_cli_inspection::outdated, account_setup_problem::gh_outdated },
			{ github_cli_inspection::failed, account_setup_problem::gh_failed },
			{ github_cli_inspection::no_usable_account, account_setup_problem::gh_no_usable_account },
			{ github_cli_inspection::read, account_setup_problem::gh_signed_out },
		};
		for (const auto& [outcome, problem] : cases) {
			const auto answered = apply(started.State, inspected(request, outcome, { }));
			expect(answered.State.Phase == account_setup_phase::choosing_account && answered.State.Problem == problem && answered.State.Accounts.empty(),
				   "each gh outcome is its own problem");
		}
		const auto missing = apply(started.State, inspected(request, github_cli_inspection::missing, { account(4U, "octo") }));
		expect(missing.State.Accounts.empty(), "accounts beside a failed inspection are not offered");

		const auto again = apply(missing.State, core::retry_account_setup{ });
		expect(again.State.Phase == account_setup_phase::inspecting_cli && again.State.Problem == account_setup_problem::none && again.Effects.size() == 1U
				   && again.Effects.front().Kind == account_setup_effect_kind::inspect_github_cli && again.State.Request == request + 1U,
			   "Check again inspects gh under a new request");
		const auto late = apply(again.State, inspected(request, github_cli_inspection::read, { account(4U, "octo") }));
		expect(late.Refusal == account_setup_refusal::stale, "the first check's late answer is stale");

		const auto back = apply(again.State, core::return_to_account_setup_way{ });
		expect(back.State.Phase == account_setup_phase::choosing_way && back.Effects.empty(), "Back abandons a running gh check");
		const auto afterBack = apply(back.State, inspected(again.State.Request, github_cli_inspection::read, { account(4U, "octo") }));
		expect(afterBack.Refusal == account_setup_refusal::not_now && afterBack.State == back.State, "the abandoned check's answer changes nothing");
	}

	void test_already_signed_in_and_exhaustion()
	{
		auto       state    = apply(account_setup_state{ }, core::begin_account_setup{ .Way = account_setup_way::browser }).State;
		const auto signedIn = apply(state, ready("octo"));
		expect(signedIn.State.Phase == account_setup_phase::connected, "an account signed in another way finishes the setup");
		expect(signedIn.Effects.size() == 1U && signedIn.Effects.front().Kind == account_setup_effect_kind::cancel_browser_sign_in, "and ends the code still shown");
		const auto atWay = apply(apply(account_setup_state{ }, core::begin_account_setup{ }).State, ready("octo"));
		expect(atWay.State.Phase == account_setup_phase::idle && atWay.Effects.empty() && atWay.Refusal == account_setup_refusal::none,
			   "an account accepted before a way was chosen (a remembered connection) leaves the setup without a Ready step");
		expect(apply(account_setup_state{ }, ready("octo")).Refusal == account_setup_refusal::not_now, "a session outside the setup does not open it");
		expect(apply(state, ready("")).Refusal == account_setup_refusal::not_now, "a session without a login is not an account");

		auto exhausted    = apply(account_setup_state{ }, core::begin_account_setup{ }).State;
		exhausted.Request = std::numeric_limits<std::uint64_t>::max();
		const auto none   = apply(exhausted, core::choose_account_setup_way{ .Way = account_setup_way::github_cli });
		expect(none.Refusal == account_setup_refusal::exhausted && none.Effects.empty() && none.State == exhausted, "exhausted identities ask for nothing");
	}

	void test_determinism_and_names()
	{
		const auto state = apply(account_setup_state{ }, core::begin_account_setup{ .Way = account_setup_way::github_cli }).State;
		const auto event = core::account_setup_event{ inspected(state.Request, github_cli_inspection::read, { account(4U, "octo"), account(5U, "test") }) };
		const auto one   = apply(state, event);
		const auto two   = apply(state, event);
		expect(one.State == two.State && one.Effects == two.Effects && one.Refusal == two.Refusal, "equal inputs give equal results");
		expect(core::stable_name(account_setup_phase::awaiting_browser) == "awaiting_browser", "phases have stable names");
		expect(core::stable_name(account_setup_problem::gh_signed_out) == "gh_signed_out", "problems have stable names");
		expect(core::stable_name(account_setup_effect_kind::connect_github_cli) == "connect_github_cli", "effects have stable names");
		expect(core::stable_name(account_setup_step::confirm) == "confirm", "steps have stable names");
		expect(core::stable_name(account_setup_way::browser) == "browser", "ways have stable names");
		expect(core::stable_name(account_setup_refusal::stale) == "stale", "refusals have stable names");
	}
} // namespace

auto main() -> int
{
	test_begin_and_steps();
	test_browser_way();
	test_browser_failure_retry_and_back();
	test_github_cli_way();
	test_github_cli_problems();
	test_already_signed_in_and_exhaustion();
	test_determinism_and_names();
	std::cout << "account-setup-tests:pass\n";
	return EXIT_SUCCESS;
}
