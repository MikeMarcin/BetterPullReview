/// \file
/// \brief RFC-BPR-0032 amendment r1. The account setup host against a
/// stubbed gh and device flow: effects asked for, completions marshalled
/// onto the lane, stale and late answers, reentrant ports and shutdown.

#include "bpr/application/account_setup_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using core::account_setup_phase;
	using core::account_setup_problem;
	using core::account_setup_way;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// A gh and a device flow that do only what they are told: each request
	/// is recorded, and a test answers it when it chooses.
	struct stub_effects final : application::account_setup_effect_port
	{
		std::vector<std::uint64_t>         Inspections;
		std::vector<core::host_profile_id> Connections;
		int                                Begins  = 0;
		int                                Cancels = 0;
		int                                Opens   = 0;
		/// Called inside begin_browser_sign_in: a device host that publishes
		/// its first stage before returning.
		std::function<void()> OnBegin;

		void inspect_github_cli(const std::uint64_t aRequest) override { Inspections.push_back(aRequest); }
		void begin_browser_sign_in() override
		{
			++Begins;
			if (OnBegin) {
				OnBegin();
			}
		}
		void cancel_browser_sign_in() override { ++Cancels; }
		void connect_github_cli(const core::host_profile_id aProfile) override { Connections.push_back(aProfile); }
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

	[[nodiscard]] auto account(const std::uint64_t aProfile, std::string aLogin) -> core::account_setup_account
	{
		auto result     = core::account_setup_account{ };
		result.Profile  = core::host_profile_id{ aProfile };
		result.Login    = std::move(aLogin);
		result.Hostname = "github.com";
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
		result.VerificationUri          = result.UserCode.empty() ? std::string{ } : std::string{ "https://github.com/login/device" };
		result.RemainingValiditySeconds = aSeconds;
		return result;
	}

	void test_github_cli_with_stubbed_gh()
	{
		auto setup = fixture{ };
		setup.Host.dispatch(core::begin_account_setup{ });
		expect(setup.Host.active() && setup.Published.size() == 1U && setup.Published.back().State.Phase == account_setup_phase::choosing_way,
			   "Home's begin publishes the way choice");
		setup.Host.dispatch(core::choose_account_setup_way{ .Way = account_setup_way::github_cli });
		expect(setup.Effects.Inspections.size() == 1U && setup.Host.projection().State.Phase == account_setup_phase::inspecting_cli,
			   "choosing gh asks the port to inspect it");
		const auto request = setup.Effects.Inspections.front();

		// The stubbed gh answers from a worker; nothing moves until the lane runs.
		setup.Host.complete_github_cli(answer(request, core::github_cli_inspection::read, { account(7U, "octo-reviewer"), account(8U, "octo-test") }));
		expect(setup.Host.projection().State.Phase == account_setup_phase::inspecting_cli, "a completion is not applied off the lane");
		expect(setup.Lane.pending() == 1U, "the completion is posted to the decision lane");
		setup.Lane.drain();
		expect(setup.Host.projection().State.Phase == account_setup_phase::choosing_account && setup.Host.projection().State.Accounts.size() == 2U,
			   "gh's two accounts are listed");

		setup.Host.dispatch(core::choose_account_setup_account{ .Profile = core::host_profile_id{ 8U } });
		setup.Host.dispatch(core::connect_account_setup{ });
		expect(setup.Effects.Connections == std::vector{ core::host_profile_id{ 8U } }, "Connect connects exactly the chosen account");
		expect(setup.Host.projection().Step == core::account_setup_step::confirm, "the confirm step shows while GitHub checks it");

		setup.Host.observe_session(session(core::account_setup_session::failed));
		expect(setup.Host.projection().State.Problem == account_setup_problem::connection_failed, "a refused account is said in the setup");
		setup.Host.dispatch(core::connect_account_setup{ });
		setup.Host.observe_session(session(core::account_setup_session::ready, "octo-test"));
		expect(setup.Host.projection().Step == core::account_setup_step::ready && setup.Host.projection().State.Login == "octo-test", "the ready step names the account");
		setup.Host.dispatch(core::finish_account_setup{ });
		expect(!setup.Host.active(), "Continue leaves the setup");
		for (std::size_t index = 1U; index < setup.Published.size(); ++index) {
			expect(setup.Published[index].Generation > setup.Published[index - 1U].Generation, "every publication raises the generation");
		}
	}

	void test_stale_and_failed_gh()
	{
		auto setup = fixture{ };
		setup.Host.dispatch(core::begin_account_setup{ .Way = account_setup_way::github_cli });
		const auto first = setup.Effects.Inspections.back();
		setup.Host.complete_github_cli(answer(first, core::github_cli_inspection::missing, { }));
		setup.Lane.drain();
		expect(setup.Host.projection().State.Problem == account_setup_problem::gh_missing, "a missing gh is said");

		setup.Host.dispatch(core::retry_account_setup{ });
		expect(setup.Effects.Inspections.size() == 2U && setup.Effects.Inspections.back() != first, "Check again is a new request");
		const auto published = setup.Published.size();
		setup.Host.complete_github_cli(answer(first, core::github_cli_inspection::read, { account(7U, "octo") }));
		setup.Lane.drain();
		expect(setup.Published.size() == published && setup.Host.projection().State.Phase == account_setup_phase::inspecting_cli, "the first check's late answer is dropped");

		setup.Host.dispatch(core::return_to_account_setup_way{ });
		setup.Host.complete_github_cli(answer(setup.Effects.Inspections.back(), core::github_cli_inspection::read, { account(7U, "octo") }));
		setup.Lane.drain();
		expect(setup.Host.projection().State.Phase == account_setup_phase::choosing_way, "an abandoned check's answer changes nothing");
	}

	void test_browser_with_stubbed_device_flow()
	{
		auto setup = fixture{ };
		// A device host that publishes `requested` from inside begin: the
		// event is queued and applied after the begin, in order.
		setup.Effects.OnBegin = [&setup] { setup.Host.observe_device(device(core::device_authorization_stage::requested)); };
		setup.Host.dispatch(core::begin_account_setup{ .Way = account_setup_way::browser });
		expect(setup.Effects.Begins == 1 && setup.Host.projection().State.Phase == account_setup_phase::requesting_code, "the browser way asks for a code");

		setup.Host.observe_device(device(core::device_authorization_stage::awaiting_authorization, "WDJB-MJHT", 899U));
		expect(setup.Host.projection().State.Phase == account_setup_phase::awaiting_browser && setup.Host.projection().Device.UserCode == "WDJB-MJHT",
			   "the code is shown in the setup");
		const auto before = setup.Published.size();
		setup.Host.observe_device(device(core::device_authorization_stage::awaiting_authorization, "WDJB-MJHT", 898U));
		expect(setup.Published.size() == before + 1U && setup.Host.projection().Device.RemainingValiditySeconds == 898U, "the remaining time is republished");

		setup.Host.open_verification_address();
		expect(setup.Effects.Opens == 1, "Open GitHub goes through the port");

		setup.Host.dispatch(core::return_to_account_setup_way{ });
		expect(setup.Effects.Cancels == 1 && setup.Host.projection().State.Phase == account_setup_phase::choosing_way, "Back cancels the shown code");
		setup.Host.open_verification_address();
		expect(setup.Effects.Opens == 1, "no address is opened once the code is gone");

		setup.Effects.OnBegin = { };
		setup.Host.dispatch(core::choose_account_setup_way{ .Way = account_setup_way::browser });
		setup.Host.observe_device(device(core::device_authorization_stage::awaiting_authorization, "ABCD-EFGH", 899U));
		setup.Host.observe_device(device(core::device_authorization_stage::expired));
		expect(setup.Host.projection().State.Problem == account_setup_problem::code_expired && setup.Host.projection().Device.UserCode.empty(),
			   "an expired code is said, and no code remains");
		setup.Host.dispatch(core::retry_account_setup{ });
		expect(setup.Effects.Begins == 3 && setup.Host.projection().State.Phase == account_setup_phase::requesting_code, "Try again asks for a new code");
		setup.Host.observe_device(device(core::device_authorization_stage::awaiting_authorization, "IJKL-MNOP", 899U));
		setup.Host.observe_device(device(core::device_authorization_stage::authorized));
		expect(setup.Host.projection().Step == core::account_setup_step::confirm, "an authorized grant confirms");
		setup.Host.observe_session(session(core::account_setup_session::ready, "octo-reviewer"));
		expect(setup.Host.projection().Step == core::account_setup_step::ready, "the account is ready");
	}

	void test_no_cli_and_shutdown()
	{
		auto noCli = fixture{ false };
		noCli.Host.dispatch(core::begin_account_setup{ .Way = account_setup_way::github_cli });
		expect(!noCli.Host.active() && noCli.Effects.Inspections.empty(), "a build that cannot inspect gh refuses the CLI way");
		noCli.Host.dispatch(core::begin_account_setup{ });
		noCli.Host.dispatch(core::choose_account_setup_way{ .Way = account_setup_way::github_cli });
		expect(noCli.Host.projection().State.Phase == account_setup_phase::choosing_way && !noCli.Host.projection().GithubCliOffered, "and says it is not offered");

		auto setup = fixture{ };
		setup.Host.dispatch(core::begin_account_setup{ .Way = account_setup_way::github_cli });
		setup.Host.complete_github_cli(answer(setup.Effects.Inspections.back(), core::github_cli_inspection::read, { account(7U, "octo") }));
		const auto published = setup.Published.size();
		setup.Host.shutdown();
		setup.Lane.drain();
		setup.Host.dispatch(core::return_to_account_setup_way{ });
		setup.Host.observe_session(session(core::account_setup_session::ready, "octo"));
		expect(setup.Published.size() == published, "after shutdown nothing is published, and a late completion is dropped");

		auto destroyed = std::make_unique<fixture>();
		destroyed->Host.dispatch(core::begin_account_setup{ .Way = account_setup_way::github_cli });
		auto lane = application::manual_executor{ };
		{
			auto effects = stub_effects{ };
			auto host    = application::account_setup_host{ effects, lane, { }, true };
			host.dispatch(core::begin_account_setup{ .Way = account_setup_way::github_cli });
			host.complete_github_cli(answer(effects.Inspections.back(), core::github_cli_inspection::read, { }));
		}
		lane.drain();
		expect(lane.pending() == 0U, "a completion that outlives its host runs and does nothing");
	}
} // namespace

auto main() -> int
{
	test_github_cli_with_stubbed_gh();
	test_stale_and_failed_gh();
	test_browser_with_stubbed_device_flow();
	test_no_cli_and_shutdown();
	std::cout << "account-setup-host-tests:pass\n";
	return EXIT_SUCCESS;
}
