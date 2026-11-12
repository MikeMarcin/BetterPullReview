/// \file
/// \brief RFC-BPR-0032 amendment r1. The account setup: server, way, sign in,
/// confirm and ready; GitHub.com and Enterprise; stale answers, failure,
/// cancel, retry, restart and the owner's 2026-11-12 decisions.

#include "bpr/core/account_setup.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using core::account_setup_account;
	using core::account_setup_effect_kind;
	using core::account_setup_phase;
	using core::account_setup_problem;
	using core::account_setup_refusal;
	using core::account_setup_server;
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

	[[nodiscard]] auto account(const std::uint64_t aProfile, std::string aLogin, const bool aActive = false, std::string aHost = "github.com") -> account_setup_account
	{
		auto result       = account_setup_account{ };
		result.Profile    = core::host_profile_id{ aProfile };
		result.Login      = std::move(aLogin);
		result.Hostname   = std::move(aHost);
		result.ActiveInGh = aActive;
		return result;
	}

	[[nodiscard]] auto apply(const account_setup_state& aState, const core::account_setup_event& aEvent) -> core::account_setup_transition
	{
		return core::transition_account_setup(aState, aEvent);
	}

	[[nodiscard]] auto begin(std::optional<account_setup_server> aServer = std::nullopt, std::string aHost = { }, std::optional<account_setup_way> aWay = std::nullopt)
		-> core::begin_account_setup
	{
		auto event     = core::begin_account_setup{ };
		event.Server   = aServer;
		event.Hostname = std::move(aHost);
		event.Way      = aWay;
		return event;
	}

	[[nodiscard]] auto server(const account_setup_server aServer, std::string aHost = { }) -> core::choose_account_setup_server
	{
		auto event     = core::choose_account_setup_server{ };
		event.Server   = aServer;
		event.Hostname = std::move(aHost);
		return event;
	}

	[[nodiscard]] auto ready(std::string aLogin, std::string aHost = "github.com") -> core::account_setup_session_observed
	{
		auto session     = core::account_setup_session_observed{ };
		session.Outcome  = core::account_setup_session::ready;
		session.Login    = std::move(aLogin);
		session.Hostname = std::move(aHost);
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

	[[nodiscard]] auto prepared(const std::uint64_t aRequest, const std::optional<std::uint64_t> aProfile) -> core::enterprise_server_prepared
	{
		auto event    = core::enterprise_server_prepared{ };
		event.Request = aRequest;
		if (aProfile) {
			event.Profile = core::host_profile_id{ *aProfile };
		}
		return event;
	}

	[[nodiscard]] auto at_github_com() -> account_setup_state
	{
		return apply(apply(account_setup_state{ }, begin()).State, server(account_setup_server::github_com)).State;
	}

	void test_server_step_first()
	{
		const auto begun = apply(account_setup_state{ }, begin());
		expect(begun.State.Phase == account_setup_phase::choosing_server && begun.Effects.empty(), "every way in begins at the server, asking nothing");
		expect(core::account_setup_step_of(begun.State) == account_setup_step::server, "the first step is the server (owner, 2026-11-12)");

		const auto github = apply(begun.State, server(account_setup_server::github_com));
		expect(github.State.Phase == account_setup_phase::choosing_way && github.State.Hostname == "github.com" && github.Effects.empty(),
			   "GitHub.com goes straight to its ways");
		expect(core::account_setup_step_of(github.State) == account_setup_step::way, "the second step is the way");

		const auto typedGithub = apply(begun.State, server(account_setup_server::enterprise, "github.com"));
		expect(typedGithub.State.Server == account_setup_server::github_com, "github.com typed as an Enterprise host is GitHub.com");

		const auto invalid = apply(begun.State, server(account_setup_server::enterprise, "https://ghe.example.com/"));
		expect(invalid.State.Phase == account_setup_phase::choosing_server && invalid.State.Problem == account_setup_problem::invalid_hostname
				   && invalid.State.Hostname == "https://ghe.example.com/" && invalid.Effects.empty(),
			   "a hostname with a scheme is refused in place, as typed");

		expect(core::account_setup_way_offered(account_setup_server::github_com, account_setup_way::browser), "browser is offered for GitHub.com");
		expect(!core::account_setup_way_offered(account_setup_server::enterprise, account_setup_way::browser), "browser is not offered for Enterprise");
		expect(core::account_setup_way_offered(account_setup_server::enterprise, account_setup_way::token)
				   && !core::account_setup_way_offered(account_setup_server::github_com, account_setup_way::token),
			   "a token is offered only for Enterprise");
		expect(core::account_setup_way_offered(account_setup_server::enterprise, account_setup_way::github_cli), "gh is offered for both");
		expect(apply(github.State, core::choose_account_setup_way{ .Way = account_setup_way::token }).Refusal == account_setup_refusal::not_offered,
			   "a way not offered for the server is refused");
	}

	void test_enterprise_token()
	{
		auto       state = apply(account_setup_state{ }, begin()).State;
		const auto chose = apply(state, server(account_setup_server::enterprise, "ghe.example.com"));
		expect(chose.State.Phase == account_setup_phase::preparing_server && chose.Effects.size() == 1U
				   && chose.Effects.front().Kind == account_setup_effect_kind::prepare_enterprise_server && chose.Effects.front().Hostname == "ghe.example.com",
			   "an Enterprise host is added before its ways");
		state = chose.State;
		expect(apply(state, prepared(state.Request + 3U, 9U)).Refusal == account_setup_refusal::stale, "another request's answer is stale");

		const auto failed = apply(state, prepared(state.Request, std::nullopt));
		expect(failed.State.Phase == account_setup_phase::choosing_server && failed.State.Problem == account_setup_problem::server_unavailable
				   && failed.State.Hostname == "ghe.example.com",
			   "a server that could not be added is said, with the host kept");
		const auto again = apply(failed.State, core::retry_account_setup{ });
		expect(again.State.Phase == account_setup_phase::preparing_server && again.Effects.size() == 1U, "Try again adds it again");

		state = apply(state, prepared(state.Request, 9U)).State;
		expect(state.Phase == account_setup_phase::choosing_way && state.ServerProfile == core::host_profile_id{ 9U }, "the added server offers its ways");
		const auto token = apply(state, core::choose_account_setup_way{ .Way = account_setup_way::token });
		expect(token.State.Phase == account_setup_phase::entering_token && token.Effects.empty(), "the token way waits for a token");
		const auto submit = apply(token.State, core::submit_account_setup_token{ });
		expect(submit.State.Phase == account_setup_phase::connecting && submit.Effects.size() == 1U && submit.Effects.front().Kind == account_setup_effect_kind::enroll_token
				   && submit.Effects.front().Profile == core::host_profile_id{ 9U },
			   "Sign in enrolls the token for the server's profile");
		const auto refused = apply(submit.State, ended(core::account_setup_session::failed));
		expect(refused.State.Phase == account_setup_phase::entering_token && refused.State.Problem == account_setup_problem::connection_failed,
			   "a refused token returns to the token with the reason");
		const auto done = apply(submit.State, ready("octo-ghe", "ghe.example.com"));
		expect(done.State.Phase == account_setup_phase::connected && done.State.Hostname == "ghe.example.com", "the Enterprise account is ready");

		const auto direct = apply(account_setup_state{ }, begin(account_setup_server::enterprise, "ghe.example.com", account_setup_way::token));
		const auto then   = apply(direct.State, prepared(direct.State.Request, 9U));
		expect(then.State.Phase == account_setup_phase::entering_token, "a link for an Enterprise host goes to the token once the server is added");
		const auto unnamed = apply(account_setup_state{ }, begin(account_setup_server::enterprise));
		expect(unnamed.State.Phase == account_setup_phase::choosing_server && unnamed.State.Server == account_setup_server::enterprise,
			   "Add enterprise server opens the server step with Enterprise picked");
		const auto viaGh = apply(account_setup_state{ }, begin(account_setup_server::enterprise, "ghe.example.com", account_setup_way::github_cli));
		expect(viaGh.State.Phase == account_setup_phase::inspecting_cli && viaGh.Effects.size() == 1U && viaGh.Effects.front().Kind == account_setup_effect_kind::inspect_github_cli
				   && viaGh.Effects.front().Hostname == "ghe.example.com",
			   "gh's way for an Enterprise host asks gh directly; only a token needs the server added");
		const auto named = apply(account_setup_state{ }, begin(account_setup_server::enterprise, "ghe.example.com"));
		expect(named.State.Phase == account_setup_phase::choosing_server && named.State.Hostname == "ghe.example.com" && named.Effects.empty(),
			   "a link or card naming an Enterprise host fills it in and waits: the server is added only on Continue");
	}

	void test_browser_way()
	{
		auto chosen = apply(at_github_com(), core::choose_account_setup_way{ .Way = account_setup_way::browser });
		expect(chosen.State.Phase == account_setup_phase::requesting_code && chosen.Effects.size() == 1U
				   && chosen.Effects.front().Kind == account_setup_effect_kind::begin_browser_sign_in && chosen.Effects.front().Request == chosen.State.Request,
			   "the browser way asks for a code under a new request");
		auto state = apply(chosen.State, core::account_setup_device_observed{ .Stage = device_authorization_stage::awaiting_authorization }).State;
		expect(state.Phase == account_setup_phase::awaiting_browser && core::account_setup_step_of(state) == account_setup_step::sign_in, "a shown code waits");
		const auto expired = apply(state, core::account_setup_device_observed{ .Stage = device_authorization_stage::expired });
		expect(expired.State.Problem == account_setup_problem::code_expired, "an expired code says so");
		const auto retried = apply(expired.State, core::retry_account_setup{ });
		expect(retried.State.Phase == account_setup_phase::requesting_code && retried.Effects.size() == 1U, "Get a new code asks again");
		const auto back = apply(state, core::return_to_account_setup_way{ });
		expect(back.State.Phase == account_setup_phase::choosing_way && back.Effects.size() == 1U
				   && back.Effects.front().Kind == account_setup_effect_kind::cancel_browser_sign_in,
			   "Back ends the shown code");
		const auto toServer = apply(state, core::return_to_account_setup_server{ });
		expect(toServer.State.Phase == account_setup_phase::choosing_server && toServer.Effects.size() == 1U, "Change on the server step ends it too");
		const auto oldCancel = apply(retried.State, core::account_setup_device_observed{ .Stage = device_authorization_stage::cancelled });
		expect(oldCancel.Refusal == account_setup_refusal::not_now, "an earlier flow's cancellation is not the new request's");
		state = apply(state, core::account_setup_device_observed{ .Stage = device_authorization_stage::authorized }).State;
		expect(core::account_setup_step_of(state) == account_setup_step::confirm, "an authorized grant confirms");
		expect(apply(state, core::account_setup_device_observed{ .Stage = device_authorization_stage::failed }).Refusal == account_setup_refusal::not_now,
			   "a terminal after authorization is not this flow's");
		expect(apply(state, core::return_to_account_setup_way{ }).Refusal == account_setup_refusal::not_now, "the account check is not abandoned");
		const auto refusedAccount = apply(state, ended(core::account_setup_session::failed));
		expect(refusedAccount.State.Phase == account_setup_phase::browser_stopped && refusedAccount.State.Problem == account_setup_problem::connection_failed,
			   "a refused account stops the browser way with a retry");
		const auto connected = apply(state, ready("octo-reviewer"));
		expect(connected.State.Phase == account_setup_phase::connected && connected.State.Login == "octo-reviewer"
				   && core::account_setup_step_of(connected.State) == account_setup_step::ready,
			   "the accepted account is ready");
		const auto finished = apply(connected.State, core::finish_account_setup{ });
		expect(finished.State.Phase == account_setup_phase::idle && finished.State.Request == connected.State.Request, "Continue leaves the setup");
	}

	void test_adopts_a_device_flow_begun_elsewhere()
	{
		// A pasted link that needs an account begins the setup at GitHub.com's
		// ways and the device flow itself; the setup follows it.
		const auto begun = apply(account_setup_state{ }, begin(account_setup_server::github_com, "github.com"));
		expect(begun.State.Phase == account_setup_phase::choosing_way, "a link for GitHub.com begins at its ways");
		const auto adopted = apply(begun.State, core::account_setup_device_observed{ .Stage = device_authorization_stage::requested });
		expect(adopted.State.Phase == account_setup_phase::requesting_code && adopted.State.Way == account_setup_way::browser && adopted.Effects.empty(),
			   "a device flow begun for the link is followed as the browser way, without a second begin");
		expect(apply(account_setup_state{ }, core::account_setup_device_observed{ .Stage = device_authorization_stage::requested }).Refusal == account_setup_refusal::not_now,
			   "a device flow outside the setup does not open it");
	}

	void test_github_cli_way()
	{
		const auto started = apply(at_github_com(), core::choose_account_setup_way{ .Way = account_setup_way::github_cli });
		expect(started.Effects.size() == 1U && started.Effects.front().Hostname == "github.com", "gh is inspected for the chosen server's host");
		const auto request = started.Effects.front().Request;
		expect(apply(started.State, inspected(request + 7U, github_cli_inspection::read, { account(4U, "octo") })).Refusal == account_setup_refusal::stale,
			   "an answer to another request is stale");
		const auto several = apply(started.State, inspected(request, github_cli_inspection::read, { account(4U, "octo"), account(5U, "octo-test", true), account(4U, "octo"), account(0U, "x") }));
		expect(several.State.Accounts.size() == 2U, "usable accounts are listed once each");
		expect(several.State.Chosen == core::host_profile_id{ 5U }, "gh's active account is chosen first (owner, 2026-11-12)");
		const auto none = apply(started.State, inspected(request, github_cli_inspection::read, { account(4U, "octo"), account(5U, "test") }));
		expect(!none.State.Chosen.has_value(), "with several and none active, the reviewer chooses");
		const auto single = apply(started.State, inspected(request, github_cli_inspection::read, { account(4U, "octo") }));
		expect(single.State.Chosen == core::host_profile_id{ 4U } && single.Effects.empty(), "one account is chosen, not connected");
		auto state = apply(several.State, core::choose_account_setup_account{ .Profile = core::host_profile_id{ 4U } }).State;
		expect(state.Chosen == core::host_profile_id{ 4U }, "the reviewer may choose another");
		expect(apply(state, core::choose_account_setup_account{ .Profile = core::host_profile_id{ 9U } }).Refusal == account_setup_refusal::unknown_account,
			   "an account not offered cannot be chosen");
		const auto connect = apply(state, core::connect_account_setup{ });
		expect(connect.State.Phase == account_setup_phase::connecting && connect.Effects.front().Kind == account_setup_effect_kind::connect_github_cli
				   && connect.Effects.front().Profile == core::host_profile_id{ 4U },
			   "Connect connects exactly the chosen account");
		const auto refused = apply(connect.State, ended(core::account_setup_session::failed));
		expect(refused.State.Phase == account_setup_phase::choosing_account && refused.State.Chosen == core::host_profile_id{ 4U }
				   && refused.State.Problem == account_setup_problem::connection_failed,
			   "a refused account returns to the list with the choice kept");
		expect(apply(connect.State, ready("octo")).State.Phase == account_setup_phase::connected, "the connected gh account is ready");

		const std::vector<std::pair<github_cli_inspection, account_setup_problem>> cases{
			{ github_cli_inspection::missing, account_setup_problem::gh_missing },
			{ github_cli_inspection::outdated, account_setup_problem::gh_outdated },
			{ github_cli_inspection::failed, account_setup_problem::gh_failed },
			{ github_cli_inspection::no_usable_account, account_setup_problem::gh_no_usable_account },
			{ github_cli_inspection::read, account_setup_problem::gh_signed_out },
		};
		for (const auto& [outcome, problem] : cases) {
			expect(apply(started.State, inspected(request, outcome, { })).State.Problem == problem, "each gh outcome is its own problem");
		}
		const auto missing = apply(started.State, inspected(request, github_cli_inspection::missing, { }));
		const auto again   = apply(missing.State, core::retry_account_setup{ });
		expect(again.State.Phase == account_setup_phase::inspecting_cli && again.State.Request == request + 1U, "Check again is a new request");
		const auto back = apply(again.State, core::return_to_account_setup_way{ });
		expect(apply(back.State, inspected(again.State.Request, github_cli_inspection::read, { account(4U, "octo") })).Refusal == account_setup_refusal::not_now,
			   "an abandoned check's answer changes nothing");
	}

	void test_restart_finish_and_sessions()
	{
		auto       running = apply(at_github_com(), core::choose_account_setup_way{ .Way = account_setup_way::browser }).State;
		const auto restart = apply(running, begin());
		expect(restart.State.Phase == account_setup_phase::choosing_server && restart.Effects.size() == 1U
				   && restart.Effects.front().Kind == account_setup_effect_kind::cancel_browser_sign_in,
			   "Use another account restarts the setup and ends a code still shown");
		const auto closed = apply(running, core::finish_account_setup{ });
		expect(closed.State.Phase == account_setup_phase::idle && closed.Effects.size() == 1U, "Close ends the setup and its code");
		auto connecting = apply(running, core::account_setup_device_observed{ .Stage = device_authorization_stage::authorized }).State;
		expect(apply(connecting, begin()).Refusal == account_setup_refusal::not_now && apply(connecting, core::finish_account_setup{ }).Refusal == account_setup_refusal::not_now,
			   "an account being checked is not abandoned");

		const auto signedIn = apply(running, ready("octo"));
		expect(signedIn.State.Phase == account_setup_phase::connected && signedIn.Effects.size() == 1U, "an account accepted another way ends the code");
		const auto atWay = apply(at_github_com(), ready("octo"));
		expect(atWay.State.Phase == account_setup_phase::idle && atWay.Effects.empty(), "an account accepted before any way started leaves the setup");
		expect(apply(account_setup_state{ }, ready("octo")).Refusal == account_setup_refusal::not_now, "a session outside the setup does not open it");
		expect(apply(running, ready("")).Refusal == account_setup_refusal::not_now, "a session without a login is not an account");

		auto exhausted    = at_github_com();
		exhausted.Request = std::numeric_limits<std::uint64_t>::max();
		const auto none   = apply(exhausted, core::choose_account_setup_way{ .Way = account_setup_way::github_cli });
		expect(none.Refusal == account_setup_refusal::exhausted && none.Effects.empty(), "exhausted identities ask for nothing");
	}

	void test_automatic_setup_is_superseded()
	{
		auto automatic      = begin();
		automatic.Automatic = true;
		const auto home     = apply(account_setup_state{ }, automatic);
		expect(home.State.Automatic && core::account_setup_superseded(home.State), "a setup Home began by itself is superseded by an account");
		const auto way = apply(home.State, server(account_setup_server::github_com));
		expect(core::account_setup_superseded(way.State), "still at the ways, it still is");
		const auto started = apply(way.State, core::choose_account_setup_way{ .Way = account_setup_way::browser });
		expect(!core::account_setup_superseded(started.State), "once a way started it is the reviewer's");
		const auto asked = apply(account_setup_state{ }, begin());
		expect(!core::account_setup_superseded(asked.State), "Add account and Use another account are never superseded");
		const auto closed = apply(home.State, core::finish_account_setup{ });
		expect(closed.State.Phase == account_setup_phase::idle && !closed.State.Automatic, "finishing clears how it began");
	}

	void test_determinism_and_names()
	{
		const auto state = apply(at_github_com(), core::choose_account_setup_way{ .Way = account_setup_way::github_cli }).State;
		const auto event = core::account_setup_event{ inspected(state.Request, github_cli_inspection::read, { account(4U, "octo"), account(5U, "test") }) };
		const auto one   = apply(state, event);
		const auto two   = apply(state, event);
		expect(one.State == two.State && one.Effects == two.Effects && one.Refusal == two.Refusal, "equal inputs give equal results");
		expect(core::stable_name(account_setup_phase::preparing_server) == "preparing_server", "phases have stable names");
		expect(core::stable_name(account_setup_problem::invalid_hostname) == "invalid_hostname", "problems have stable names");
		expect(core::stable_name(account_setup_effect_kind::enroll_token) == "enroll_token", "effects have stable names");
		expect(core::stable_name(account_setup_step::server) == "server", "steps have stable names");
		expect(core::stable_name(account_setup_way::token) == "token", "ways have stable names");
		expect(core::stable_name(account_setup_server::enterprise) == "enterprise", "servers have stable names");
		expect(core::stable_name(account_setup_refusal::not_offered) == "not_offered", "refusals have stable names");
	}
} // namespace

auto main() -> int
{
	test_server_step_first();
	test_enterprise_token();
	test_browser_way();
	test_adopts_a_device_flow_begun_elsewhere();
	test_github_cli_way();
	test_restart_finish_and_sessions();
	test_automatic_setup_is_superseded();
	test_determinism_and_names();
	std::cout << "account-setup-tests:pass\n";
	return EXIT_SUCCESS;
}
