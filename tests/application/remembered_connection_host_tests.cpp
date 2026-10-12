#include "../support/test_expectation.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/remembered_connection_host.hpp"
#include <iostream>
#include <stdexcept>
namespace {
	using namespace bpr;
	using tests::expect;
	struct store final : application::remembered_connection_store_port
	{
		core::remembered_connection_state State;
		bool                              Refuse = false;
		unsigned                          Saves  = 0;
		auto                              load(std::stop_token) -> application::remembered_connection_store_result<core::remembered_connection_state> override { return State; }
		auto                              save(const core::remembered_connection_state& aCandidate, std::uint64_t aExpected, std::stop_token) -> application::remembered_connection_store_result<void> override
		{
			++Saves;
			if (Refuse) {
				return std::unexpected(application::remembered_connection_store_failure::unavailable);
			}
			expect(aExpected == State.Revision && core::valid_remembered_connection_update(State, aCandidate), "every real save rebases on last committed revision and identity counters");
			State = aCandidate;
			return { };
		}
	};
	auto row() -> core::remembered_connection
	{
		core::remembered_connection result;
		result.Profile         = core::host_profile_id{ 3 };
		result.ProfileRevision = 1;
		result.Host            = core::git_host_id{ 1 };
		result.Hostname        = "github.com";
		result.Login           = "octo-reviewer";
		result.Account         = core::github_account_id{ 42 };
		result.Transport       = core::remembered_connection_transport::github_cli;
		return result;
	}
	void run(bool aForget)
	{
		store                                                     storage;
		application::manual_executor                              effects, decisions;
		std::vector<application::connection_metadata_observation> observations;
		application::remembered_connection_host                   host{ storage, storage.load({ }), core::default_bpr_profile_id, effects, decisions, [&](auto aObservation) { observations.push_back(aObservation); } };
		expect(host.remember(row()), "remember admitted after authenticated observation");
		storage.Refuse = true;
		effects.drain();
		decisions.drain();
		expect(storage.State.Revision == 1 && observations.size() == 1 && !observations.back().Saved, "failed remember is explicitly session-only");
		expect(host.state().Revision == 2 && core::select_remembered_connection(host.state()).has_value(), "session state survives refused durable save");
		expect(aForget ? host.forget("github.com") : host.disable("github.com"), "retirement admitted against independent session revision");
		storage.Refuse = false;
		host.shutdown();
		expect(!host.quiescent() && !host.remember(row()), "shutdown closes ingress while draining admitted retirement");
		effects.drain();
		decisions.drain();
		expect(host.quiescent() && storage.State.Revision == 2 && !core::select_remembered_connection(storage.State), "retirement commits one durable revision after failed remember");
		expect(observations.back().Saved, "retirement success reported only after commit");
	}
	void refusal()
	{
		store                                                     storage;
		application::manual_executor                              effects, decisions;
		std::vector<application::connection_metadata_observation> observations;
		application::remembered_connection_host                   host{ storage, storage.load({ }), core::default_bpr_profile_id, effects, decisions, [&](auto aObservation) { observations.push_back(aObservation); } };
		expect(host.remember(row()), "accepted selection admitted");
		effects.drain();
		decisions.drain();
		storage.Refuse = true;
		expect(host.disable("github.com"), "disable admitted before storage refuses");
		effects.drain();
		decisions.drain();
		expect(!observations.back().Saved && observations.back().Action == application::connection_metadata_action::disable, "unsaved sign-out is reported distinctly");
		expect(!core::select_remembered_connection(host.state()) && core::select_remembered_connection(storage.State).has_value(), "failed durable disable cannot reenable this session");
		storage.Refuse = false;
		expect(host.disable("github.com"), "disable retry admitted");
		effects.drain();
		decisions.drain();
		expect(!core::select_remembered_connection(storage.State) && observations.back().Saved, "retry rebases on the still-enabled durable row");
		application::remembered_connection_host refused{ storage, std::unexpected(application::remembered_connection_store_failure::corrupt), core::default_bpr_profile_id, effects, decisions, { } };
		expect(!refused.available() && !refused.remember(row()) && effects.pending() == 0, "refused load grants no restoration or overwriting effect");
	}
	void queued()
	{
		store                                   storage;
		application::manual_executor            effects, decisions;
		application::remembered_connection_host host{ storage, storage.load({ }), core::default_bpr_profile_id, effects, decisions, { } };
		expect(host.remember(row()) && host.disable("github.com") && host.forget("github.com"), "three operations admitted without waiting for disk");
		expect(effects.pending() == 1, "single writer even when executor is reorderable");
		while (!host.quiescent()) {
			effects.drain();
			decisions.drain();
		}
		expect(storage.State.Connections.empty() && storage.State.Revision == 4 && storage.State.LastProfileId == 3, "late save cannot undo later disable/Forget or recycle identities");
	}
}
int main()
try {
	run(false);
	run(true);
	refusal();
	queued();
	std::cout << "remembered-connection-host-tests:pass\n";
} catch (const std::exception& aError) {
	std::cerr << aError.what() << '\n';
	return 1;
}
