/// \file
/// \brief RFC-BPR-0032 amendment r1. The account setup host against a
/// stubbed gh, device flow and Enterprise server: effects asked for,
/// completions marshalled onto the lane, stale and late answers, reentrant
/// ports and shutdown.

#include "bpr/application/account_setup_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using core::account_setup_phase;
	using core::account_setup_problem;
	using core::account_setup_server;
	using core::account_setup_way;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// A gh, a device flow and a server list that do only what they are told:
	/// each request is recorded, and a test answers it when it chooses.
	struct stub_effects final : application::account_setup_effect_port
	{
		std::vector<std::pair<std::uint64_t, std::string>> Inspections;
		std::vector<std::pair<std::uint64_t, std::string>> Servers;
		std::vector<core::host_profile_id>                 Connections;
		std::vector<core::host_profile_id>                 Tokens;
		int                                                Begins  = 0;
		int                                                Cancels = 0;
		int                                                Opens   = 0;
		/// Called inside begin_browser_sign_in: a device host that publishes
		/// its first stage before returning.
		std::function<void()> OnBegin;

		void prepare_enterprise_server(const std::uint64_t aRequest, const std::string& aHostname) override { Servers.emplace_back(aRequest, aHostname); }
		void inspect_github_cli(const std::uint64_t aRequest, const std::string& aHostname) override { Inspections.emplace_back(aRequest, aHostname); }
		void begin_browser_sign_in() override
		{
			++Begins;
			if (OnBegin) {
				OnBegin();
			}
		}
		void cancel_browser_sign_in() override { ++Cancels; }
		void connect_github_cli(const core::host_profile_id aProfile) override { Connections.push_back(aProfile); }
		void enroll_token(const core::host_profile_id aProfile) override { Tokens.push_back(aProfile); }
		void open_verification_address() override { ++Opens; }
	};

	struct fixture
	{
		stub_effects                                       Effects;
		application::manual_executor                       Lane;
		std::vector<application::account_setup_projection> Published;
		application::account_setup_host                    Host;

		explicit fixture(const bool aCli = true)
			: Host{ Effects, Lane, [this](const application::account_setup_projection& aProjection) { Published.push_back(aProjection); }, aCli }
		{ }
	};

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

	[[nodiscard]] auto account(const std::uint64_t aProfile, std::string aLogin, const bool aActive = false) -> core::account_setup_account
	{
		auto result       = core::account_setup_account{ };
		result.Profile    = core::host_profile_id{ aProfile };
		result.Login      = std::move(aLogin);
		result.Hostname   = "github.com";
		result.ActiveInGh = aActive;
		return result;
	}

	[[nodiscard]] auto answer(const std::uint64_t aRequest, const core::github_cli_inspection aOutcome, std::vector<core::account_setup_account> aAccounts)
		-> core::github_cli_inspected
	{
		auto result     = core::github_cli_inspected{ };
		result.Request  = aRequest;
		result.Outcome  = aOutcome;
		result.Accounts = std::move(aAccounts);
		return result;
	}

	[[nodiscard]] auto session(const core::account_setup_session aOutcome, std::string aLogin = { }) -> core::account_setup_session_observed
	{
		auto result     = core::account_setup_session_observed{ };
		result.Outcome  = aOutcome;
		result.Login    = std::move(aLogin);
		result.Hostname = "github.com";
		return result;
	}

	[[nodiscard]] auto device(const core::device_authorization_stage aStage, std::string aCode = { }, const std::uint32_t aSeconds = 0U)
		-> application::device_authorization_presentation
	{
		auto result                     = application::device_authorization_presentation{ };
		result.Stage                    = aStage;
		result.UserCode                 = std::move(aCode);
		result.VerificationUri          = result.UserCode.empty() ? std::string{ } : std::string{ "github.com/login/device" };
		result.RemainingValiditySeconds = aSeconds;
		return result;
	}

	void test_github_cli_with_stubbed_gh()
	{
		auto setup = fixture{ };
		setup.Host.dispatch(begin());
		expect(setup.Host.active() && setup.Published.back().State.Phase == account_setup_phase::choosing_server, "every way in publishes the server step");
		setup.Host.dispatch(server(account_setup_server::github_com));
		setup.Host.dispatch(core::choose_account_setup_way{ .Way = account_setup_way::github_cli });
		expect(setup.Effects.Inspections.size() == 1U && setup.Effects.Inspections.front().second == "github.com", "gh is inspected for the chosen host");
		const auto request = setup.Effects.Inspections.front().first;

		setup.Host.complete_github_cli(answer(request, core::github_cli_inspection::read, { account(7U, "octo-reviewer"), account(8U, "octo-test", true) }));
		expect(setup.Host.projection().State.Phase == account_setup_phase::inspecting_cli && setup.Lane.pending() == 1U, "the completion waits for the lane");
		setup.Lane.drain();
		expect(setup.Host.projection().State.Chosen == core::host_profile_id{ 8U }, "gh's active account is chosen first");
		setup.Host.dispatch(core::connect_account_setup{ });
		expect(setup.Effects.Connections == std::vector{ core::host_profile_id{ 8U } }, "Connect connects exactly the chosen account");
		setup.Host.observe_session(session(core::account_setup_session::failed));
		expect(setup.Host.projection().State.Problem == account_setup_problem::connection_failed, "a refused account is said in the setup");
		setup.Host.dispatch(core::connect_account_setup{ });
		setup.Host.observe_session(session(core::account_setup_session::ready, "octo-test"));
		expect(setup.Host.projection().Step == core::account_setup_step::ready && setup.Host.projection().State.Login == "octo-test", "Ready names the account");
		setup.Host.dispatch(core::finish_account_setup{ });
		expect(!setup.Host.active(), "Continue leaves the setup");
		for (std::size_t index = 1U; index < setup.Published.size(); ++index) {
			expect(setup.Published[index].Generation > setup.Published[index - 1U].Generation, "every publication raises the generation");
		}
	}

	void test_enterprise_with_stubbed_server()
	{
		auto setup = fixture{ };
		setup.Host.dispatch(begin(account_setup_server::enterprise));
		expect(setup.Host.projection().State.Server == account_setup_server::enterprise && setup.Effects.Servers.empty(), "Add enterprise server opens at the server step");
		setup.Host.dispatch(server(account_setup_server::enterprise, "ghe.example.com"));
		expect(setup.Effects.Servers.size() == 1U && setup.Effects.Servers.front().second == "ghe.example.com", "the host is asked to add the server");
		auto prepared    = core::enterprise_server_prepared{ };
		prepared.Request = setup.Effects.Servers.front().first;
		prepared.Profile = core::host_profile_id{ 12U };
		setup.Host.complete_enterprise_server(prepared);
		expect(setup.Host.projection().State.Phase == account_setup_phase::preparing_server, "the answer waits for the lane");
		setup.Lane.drain();
		expect(setup.Host.projection().State.Phase == account_setup_phase::choosing_way && setup.Host.projection().State.ServerProfile == core::host_profile_id{ 12U },
			   "the added server offers its ways");
		setup.Host.dispatch(core::choose_account_setup_way{ .Way = account_setup_way::browser });
		expect(setup.Effects.Begins == 0, "the browser way is not offered for Enterprise");
		setup.Host.dispatch(core::choose_account_setup_way{ .Way = account_setup_way::token });
		setup.Host.dispatch(core::submit_account_setup_token{ });
		expect(setup.Effects.Tokens == std::vector{ core::host_profile_id{ 12U } }, "Sign in enrolls the held token for the server's profile");
		setup.Host.observe_session(session(core::account_setup_session::ready, "octo-ghe"));
		expect(setup.Host.projection().Step == core::account_setup_step::ready, "the Enterprise account is ready");

		auto stale = fixture{ };
		stale.Host.dispatch(begin(account_setup_server::enterprise, "ghe.example.com"));
		const auto first = stale.Effects.Servers.back().first;
		stale.Host.dispatch(core::return_to_account_setup_server{ });
		auto late    = core::enterprise_server_prepared{ };
		late.Request = first;
		late.Profile = core::host_profile_id{ 12U };
		stale.Host.complete_enterprise_server(late);
		stale.Lane.drain();
		expect(stale.Host.projection().State.Phase == account_setup_phase::choosing_server, "an abandoned server's answer changes nothing");
	}

	void test_browser_with_stubbed_device_flow()
	{
		auto setup = fixture{ };
		// A device host that publishes `requested` from inside begin: the
		// event is queued and applied after the begin, in order.
		setup.Effects.OnBegin = [&setup] { setup.Host.observe_device(device(core::device_authorization_stage::requested)); };
		setup.Host.dispatch(begin(account_setup_server::github_com, "github.com", account_setup_way::browser));
		expect(setup.Effects.Begins == 1 && setup.Host.projection().State.Phase == account_setup_phase::requesting_code, "the browser way asks for a code");
		setup.Host.observe_device(device(core::device_authorization_stage::awaiting_authorization, "WDJB-MJHT", 899U));
		expect(setup.Host.projection().Device.UserCode == "WDJB-MJHT", "the code is in the projection");
		const auto before = setup.Published.size();
		setup.Host.observe_device(device(core::device_authorization_stage::awaiting_authorization, "WDJB-MJHT", 898U));
		expect(setup.Published.size() == before + 1U, "the remaining time is republished");
		setup.Host.open_verification_address();
		expect(setup.Effects.Opens == 1, "Open GitHub goes through the port");
		setup.Host.dispatch(core::return_to_account_setup_way{ });
		expect(setup.Effects.Cancels == 1, "Back cancels the shown code");
		setup.Host.open_verification_address();
		expect(setup.Effects.Opens == 1, "no address is opened once the code is gone");

		setup.Effects.OnBegin = { };
		setup.Host.observe_signed_in(true);
		expect(setup.Host.projection().SignedIn, "the window's account is a projected fact");
		setup.Host.dispatch(core::finish_account_setup{ });
		expect(!setup.Host.active(), "a signed-in window can close the setup without a new account");
	}

	void test_no_cli_and_shutdown()
	{
		auto noCli = fixture{ false };
		noCli.Host.dispatch(begin(account_setup_server::github_com, "github.com", account_setup_way::github_cli));
		expect(!noCli.Host.active() && noCli.Effects.Inspections.empty(), "a build that cannot inspect gh refuses the CLI way");

		auto setup = fixture{ };
		setup.Host.dispatch(begin(account_setup_server::github_com, "github.com", account_setup_way::github_cli));
		setup.Host.complete_github_cli(answer(setup.Effects.Inspections.back().first, core::github_cli_inspection::read, { account(7U, "octo") }));
		const auto published = setup.Published.size();
		setup.Host.shutdown();
		setup.Lane.drain();
		setup.Host.observe_session(session(core::account_setup_session::ready, "octo"));
		expect(setup.Published.size() == published, "after shutdown nothing is published, and a late completion is dropped");

		auto lane = application::manual_executor{ };
		{
			auto effects = stub_effects{ };
			auto host    = application::account_setup_host{ effects, lane, { }, true };
			host.dispatch(begin(account_setup_server::github_com, "github.com", account_setup_way::github_cli));
			host.complete_github_cli(answer(effects.Inspections.back().first, core::github_cli_inspection::read, { }));
		}
		lane.drain();
		expect(lane.pending() == 0U, "a completion that outlives its host runs and does nothing");
	}
} // namespace

auto main() -> int
{
	test_github_cli_with_stubbed_gh();
	test_enterprise_with_stubbed_server();
	test_browser_with_stubbed_device_flow();
	test_no_cli_and_shutdown();
	std::cout << "account-setup-host-tests:pass\n";
	return EXIT_SUCCESS;
}
