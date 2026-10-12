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
	disabling_and_stale_work();
	malformed_and_limits();
	std::cout << "remembered-connection-policy-tests:pass\n";
}
