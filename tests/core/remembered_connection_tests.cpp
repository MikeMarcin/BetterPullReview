#include "../support/test_expectation.hpp"
#include "bpr/core/remembered_connection.hpp"

#include <algorithm>
#include <iostream>
#include <limits>

namespace {
	using namespace bpr::core;
	using bpr::tests::expect;
	auto connection(std::uint64_t aProfile, std::uint64_t aHost, std::string aHostname = "github.com") -> remembered_connection
	{
		remembered_connection row;
		row.Profile         = host_profile_id{ aProfile };
		row.ProfileRevision = 1U;
		row.Host            = git_host_id{ aHost };
		row.Hostname        = std::move(aHostname);
		row.Account         = github_account_id{ 17U };
		row.Login           = "fixture_user";
		row.Server          = row.Hostname == "github.com" ? remembered_connection_server::github_com : remembered_connection_server::ghes_3_18;
		row.Transport       = remembered_connection_transport::github_cli;
		return row;
	}
	void selection_and_replacement()
	{
		const auto empty = remembered_connection_state{ };
		expect(valid(empty) && !select_remembered_connection(empty), "fresh state grants no validation attempt");
		const auto first = remember_connection(empty, 1U, connection(2U, 1U));
		expect(first && first->Revision == 2U && select_remembered_connection(*first)->Profile == host_profile_id{ 2U }, "successful observation selects exact connection");
		const auto other = remember_connection(*first, 2U, connection(3U, 2U, "ghe.example"));
		expect(other && select_remembered_connection(*other)->Hostname == "ghe.example", "no destination selects most recent resumable connection");
		expect(select_remembered_connection(*other, "github.com")->Profile == host_profile_id{ 2U }
				   && !select_remembered_connection(*other, "unknown.example"),
			   "exact host never substitutes most recent host");
		const auto reselected = remember_connection(*other, 3U, connection(2U, 1U));
		expect(reselected && select_remembered_connection(*reselected)->Hostname == "github.com", "reselection preserves stable authority and advances order");
		auto mismatch    = connection(2U, 1U);
		mismatch.Account = github_account_id{ 18U };
		expect(!remember_connection(*reselected, 4U, mismatch), "same connection identity cannot alias another numeric account");
		mismatch.Profile    = host_profile_id{ 4U };
		const auto replaced = remember_connection(*reselected, 4U, mismatch);
		expect(replaced && replaced->Connections.size() == 2U && select_remembered_connection(*replaced, "github.com")->Account == github_account_id{ 18U }, "explicit successful replacement uses fresh identity and one row per server");
		auto native       = connection(5U, 1U);
		native.Transport  = remembered_connection_transport::native_device;
		const auto device = remember_connection(*replaced, 5U, native);
		expect(device && select_remembered_connection(*device, "github.com")->Transport == remembered_connection_transport::native_device, "successful device replacement changes only selected server transport");
	}
	auto native_sign_in(std::uint64_t aProfile, std::uint64_t aAccount) -> remembered_connection
	{
		auto row      = connection(aProfile, 1U);
		row.Account   = github_account_id{ aAccount };
		row.Login     = "reviewer-" + std::to_string(aAccount);
		row.Transport = remembered_connection_transport::native_device;
		return row;
	}
	// [[BPR-1494]]. A sign-in mints its profile before `/user` names the account.
	// The account that returns after another account used the server re-binds to
	// its own earlier identity, which the durable update check can prove.
	void returning_native_account_rebinds()
	{
		auto state        = *remember_connection(remembered_connection_state{ }, 1U, native_sign_in(1U, 910001U));
		state             = *disable_connection_reuse(state, state.Revision, "github.com");
		const auto second = remember_connection(state, state.Revision, native_sign_in(2U, 910002U));
		expect(second && second->Retired.size() == 1U && second->Retired.front().Profile == host_profile_id{ 1U } && !second->Retired.front().ReuseEnabled
				   && select_remembered_connection(*second, "github.com")->Profile == host_profile_id{ 2U },
			   "another account's native sign-in retires the first account's identity without selecting it");
		state = *disable_connection_reuse(*second, second->Revision, "github.com");

		const auto adopted = adopt_native_connection(state, native_sign_in(3U, 910001U));
		expect(adopted && adopted->Profile == host_profile_id{ 1U } && adopted->ProfileRevision == 1U && adopted->Host == git_host_id{ 1U },
			   "the returning account re-binds to its own profile, revision and host");
		const auto returned = remember_connection(state, state.Revision, *adopted);
		expect(returned && select_remembered_connection(*returned, "github.com")->Profile == host_profile_id{ 1U } && valid_remembered_connection_update(state, *returned),
			   "the re-bound identity is durably selected again");
		expect(returned->Retired.size() == 1U && returned->Retired.front().Profile == host_profile_id{ 2U } && returned->LastProfileId == 2U,
			   "the displaced account is retired in turn and no identity is minted");

		auto aliased    = native_sign_in(1U, 910003U);
		aliased.Login   = "reviewer-910003";
		const auto fake = remember_connection(state, state.Revision, aliased);
		expect(!fake && fake.error() == remembered_connection_failure::invalid, "a retired identity cannot carry another numeric account");
		auto renamed  = *adopted;
		renamed.Login = "renamed-reviewer";
		expect(!remember_connection(state, state.Revision, renamed), "a retired identity returns only exactly as it was");
		expect(!adopt_native_connection(state, native_sign_in(3U, 910003U)), "a different account keeps its fresh identity");
		auto otherLogin  = native_sign_in(3U, 910001U);
		otherLogin.Login = "renamed-reviewer";
		expect(!adopt_native_connection(state, otherLogin), "a changed login is a different identity");
		auto otherHost = native_sign_in(3U, 910001U);
		otherHost.Host = git_host_id{ 7U };
		expect(!adopt_native_connection(state, otherHost), "another host never shares an identity");
		expect(!adopt_native_connection(state, native_sign_in(2U, 910001U)), "a remembered profile is never re-bound");
		auto cliSignIn      = native_sign_in(3U, 910001U);
		cliSignIn.Transport = remembered_connection_transport::github_cli;
		expect(!adopt_native_connection(state, cliSignIn), "only a native sign-in re-binds");

		auto rewritten                    = *returned;
		rewritten.Retired.front().Account = github_account_id{ 910009U };
		expect(valid(rewritten) && !valid_remembered_connection_update(state, rewritten), "an update cannot rewrite retired history");

		auto cli          = native_sign_in(3U, 910001U);
		cli.Transport     = remembered_connection_transport::github_cli;
		const auto viaCli = remember_connection(state, state.Revision, cli);
		expect(viaCli && viaCli->Retired.empty(), "a CLI selection leaves no native history for its server");
		expect(!adopt_native_connection(*viaCli, native_sign_in(4U, 910001U)), "a native sign-in replacing a CLI selection gets a fresh identity");
		const auto native = remember_connection(*viaCli, viaCli->Revision, native_sign_in(4U, 910001U));
		expect(native && select_remembered_connection(*native, "github.com")->Profile == host_profile_id{ 4U }, "the fresh native identity replaces the CLI selection");

		const auto forgotten = forget_connection(state, state.Revision, "github.com");
		expect(forgotten && forgotten->Retired.empty() && !adopt_native_connection(*forgotten, native_sign_in(3U, 910001U)), "Forget drops the server's retired identities");

		auto bounded = remembered_connection_state{ };
		for (std::uint64_t account = 1U; account <= maximum_retired_connections + 2U; ++account) {
			bounded = *remember_connection(bounded, bounded.Revision, native_sign_in(account, 910000U + account));
		}
		expect(valid(bounded) && bounded.Retired.size() == maximum_retired_connections && bounded.Retired.front().Account == github_account_id{ 910002U },
			   "the oldest retired identity is dropped at the bound");
	}
	void disabling_and_stale_work()
	{
		const auto original = *remember_connection(remembered_connection_state{ }, 1U, connection(2U, 1U));
		const auto disabled = disable_connection_reuse(original, 2U, "github.com");
		expect(disabled && !disabled->Connections.front().ReuseEnabled && !select_remembered_connection(*disabled, "github.com"), "disabled exact host permits no automatic validation");
		const auto late = remember_connection(*disabled, 2U, connection(2U, 1U));
		expect(!late && late.error() == remembered_connection_failure::stale, "late successful connection cannot undo sign out");
		const auto forgotten = forget_connection(*disabled, 3U, "github.com");
		expect(forgotten && forgotten->Connections.empty() && forgotten->LastProfileId == 2U && forgotten->LastHostId == 1U, "Forget removes selection while preserving identity high water");
		expect(!remember_connection(*forgotten, 4U, connection(2U, 1U)), "forgotten identity cannot alias a new authority later");
		const auto fresh = remember_connection(*forgotten, 4U, connection(3U, 2U));
		expect(fresh.has_value(), "a fresh authority can be selected after Forget");
		const auto sentinel = disable_connection_reuse(remembered_connection_state{ }, 1U, "github.com");
		expect(sentinel && valid(*sentinel) && sentinel->Connections.front().Profile.value() == 0U && !select_remembered_connection(*sentinel), "sign out without previous metadata leaves suppression for an older device grant");
		expect(remember_connection(*sentinel, 2U, connection(2U, 1U)).has_value(), "explicit successful selection replaces a suppression-only row");
		auto independent       = remembered_connection_state{ };
		independent.BprProfile = bpr_profile_id{ 9U };
		expect(independent.Connections.empty() && !valid_remembered_connection_update(independent, *disabled), "another BPR profile remains independent");
	}
	void malformed_and_limits()
	{
		const auto first = *remember_connection(remembered_connection_state{ }, 1U, connection(2U, 1U));
		for (int mutation = 0; mutation < 8; ++mutation) {
			auto broken = first;
			switch (mutation) {
			case 0:  broken.Connections.push_back(broken.Connections.front()); break;
			case 1:  broken.Connections.front().Hostname = "GitHub.com"; break;
			case 2:  broken.Connections.front().Login = "fixture\nuser"; break;
			case 3:  broken.Connections.front().Transport = static_cast<remembered_connection_transport>(99); break;
			case 4:  broken.Connections.front().Server = remembered_connection_server::ghes_3_18; break;
			case 5:  broken.LastProfileId = 0U; break;
			case 6:  broken.Connections.front().Account = github_account_id{ }; break;
			case 7:  broken.Revision = 0U; break;
			default: expect(false, "all fixture mutations are named"); break;
			}
			expect(!valid(broken) && !select_remembered_connection(broken), "malformed state never grants validation");
		}
		auto full = remembered_connection_state{ };
		for (std::uint64_t index = 1U; index <= maximum_remembered_connections; ++index) {
			full = *remember_connection(full, full.Revision, connection(index, index, "ghe" + std::to_string(index) + ".example"));
		}
		const auto overflow = remember_connection(full, full.Revision, connection(65U, 65U, "extra.example"));
		expect(!overflow && overflow.error() == remembered_connection_failure::capacity, "bounded server capacity fails without partial state");
		full.Revision = std::numeric_limits<std::uint64_t>::max();
		expect(!forget_connection(full, full.Revision, "ghe1.example"), "revision exhaustion cannot wrap to accept old work");
		full.Revision          = 65U;
		full.LastSelectedOrder = std::numeric_limits<std::uint64_t>::max();
		expect(!remember_connection(full, 65U, connection(66U, 66U, "extra.example")), "selection order exhaustion refuses");
	}
}
int main()
{
	selection_and_replacement();
	returning_native_account_rebinds();
	disabling_and_stale_work();
	malformed_and_limits();
	std::cout << "remembered-connection-policy-tests:pass\n";
}
