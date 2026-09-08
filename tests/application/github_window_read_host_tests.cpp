#include "bpr/application/github_window_read_host.hpp"
#include "bpr/application/manual_executor.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
	using namespace bpr;
	void expect(bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	struct transport final : application::github_account_transport_lease
	{ };
	struct reads final : application::github_pull_request_port
	{
		struct call
		{
			application::github_pull_request_provider_request   Request;
			application::github_pull_request_completion_handler Complete;
		};
		std::vector<call> Calls;
		std::size_t       Cancellations = 0;
		void              fetch_pull_request(application::github_pull_request_provider_request   aRequest,
											 application::github_pull_request_completion_handler aComplete) override { Calls.push_back({ std::move(aRequest), std::move(aComplete) }); }
		void              cancel_pull_request(application::github_pull_request_provider_request) override { ++Cancellations; }
	};
	auto session() -> application::github_profile_session
	{
		application::github_profile_session result{
			.BprProfile = core::default_bpr_profile_id, .ApplicationSession = core::application_session_id{ 1 }, .ProfileAuthority = { .Profile = core::host_profile_id{ 1 }, .ProfileRevision = application::github_profile_revision{ 1 }, .Host = core::git_host_id{ 1 }, .Hostname = application::github_canonical_hostname{ "github.com" }, .ProbeContractRevision = application::github_probe_contract_revision{ 1 } }, .Session = { .Profile = core::host_profile_id{ 1 }, .Host = core::git_host_id{ 1 }, .Viewer = { .Account = core::github_account_id{ 1 }, .Node = application::github_actor_id{ "actor" }, .Login = "reader" }, .CapabilityEvidence = { .Revision = core::capability_revision{ 1 }, .ContractVersion = 1 } }, .Generation = core::github_identity_generation{ 1 }, .CapabilityEvidence = { .Authority = { .Profile = core::host_profile_id{ 1 }, .ProfileRevision = application::github_profile_revision{ 1 }, .Host = core::git_host_id{ 1 }, .Account = core::github_account_id{ 1 }, .ProbeContractRevision = application::github_probe_contract_revision{ 1 } }, .Revision = core::capability_revision{ 1 } }, .AccountTransport = std::make_shared<transport>()
		};
		return result;
	}
	void test_reentrant_read_publication()
	{
		reads                        provider;
		application::manual_executor lane;
		auto                         identity          = session();
		auto                         replacement       = identity;
		replacement.Generation                         = core::github_identity_generation{ 2 };
		application::github_window_read_host* target   = nullptr;
		bool                                  replaced = false;
		application::github_window_read_host  host{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 1 } },
			core::application_session_id{ 2 },
			provider,
			lane,
			[&](const application::github_window_read_update& aUpdate) {
				if (replaced || aUpdate.Transition.Requests.empty()) {
					return;
				}
				replaced = true;
				expect(target->retire(identity) && target->adopt(replacement),
					   "publication may synchronously replace exact profile generation");
				target->dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 2 } });
			}
		};
		target = &host;
		expect(host.adopt(identity), "reentrant fixture adopts initial identity");
		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 1 } });
		expect(provider.Calls.size() == 1 && provider.Calls.front().Request.Context.Number == core::pull_request_number{ 2 }
				   && provider.Calls.front().Request.Context.IdentityGeneration == replacement.Generation,
			   "unwinding retired open A cannot overwrite replacement open B provider work");
		const auto pending = provider.Calls.front();
		pending.Complete(application::github_pull_request_cancelled{ pending.Request });
		lane.drain();
		expect(!host.state().PendingPullRequest && host.is_quiescent(), "replacement B terminal drains the actual pending request");

		reads                                 secondProvider;
		application::github_window_read_host* secondTarget = nullptr;
		bool                                  opened       = false;
		application::github_window_read_host  second{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 3 }, core::bpr_window_generation{ 1 } },
			core::application_session_id{ 3 },
			secondProvider,
			lane,
			[&](const application::github_window_read_update& aUpdate) {
				if (opened || aUpdate.Transition.Requests.empty()) {
					return;
				}
				opened = true;
				secondTarget->dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 2 } });
			}
		};
		secondTarget = &second;
		expect(second.adopt(identity), "second reentrant fixture adopts identity");
		second.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 1 } });
		expect(secondProvider.Calls.size() == 1 && secondProvider.Calls.front().Request.Context.Number == core::pull_request_number{ 1 },
			   "reentrant open B respects the current in-progress A decision without duplicating effects");
		const auto first = secondProvider.Calls.front();
		first.Complete(application::github_pull_request_cancelled{ first.Request });
		lane.drain();
	}

}
int main()
{
	test_reentrant_read_publication();
	reads                                provider;
	application::manual_executor         lane;
	std::size_t                          publications = 0;
	application::github_window_read_host host{
		{ core::default_bpr_profile_id, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 1 } },
		core::application_session_id{ 2 },
		provider,
		lane,
		[&publications](const auto&) { ++publications; }
	};
	auto identity      = session();
	auto foreign       = identity;
	foreign.BprProfile = core::bpr_profile_id{ 2 };
	expect(!host.adopt(foreign), "foreign BPR profile cannot lend its account authority");
	expect(host.adopt(identity), "exact authenticated profile observation is accepted");
	host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 1 } });
	expect(provider.Calls.size() == 1 && !host.is_quiescent(), "read owns one pending terminal");
	host.dispatch(core::pull_request_loaded_event{ provider.Calls.front().Request.Context,
												   core::repository_id{ 1 }, core::pull_request_id{ 1 }, core::provider_snapshot_id{ 1 } });
	expect(!host.state().AcceptedSnapshot, "public dispatch cannot manufacture provider completion acceptance");
	auto retired       = identity;
	retired.Generation = core::github_identity_generation{ 2 };
	expect(!host.retire(retired) && provider.Cancellations == 0, "wrong generation cannot cancel current read");
	expect(host.retire(identity) && provider.Cancellations == 1, "exact retirement cancels current read");
	expect(!host.adopt(identity), "retired identity cannot be replayed");
	const auto before = publications;
	provider.Calls.front().Complete(application::github_pull_request_cancelled{ provider.Calls.front().Request });
	lane.drain();
	expect(host.is_quiescent() && publications == before, "cancel terminal drains without publishing retired result");
	expect(host.adopt(retired), "new profile generation can reauthenticate existing window");
	host.shutdown();
	expect(!host.adopt(retired), "closed window cannot resume authentication");
	return EXIT_SUCCESS;
}
