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
	struct diagnostics final : application::diagnostic_log_port
	{
		std::vector<application::diagnostic_event> Events;
		auto                                       record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
		{
			Events.push_back(aEvent);
			return application::diagnostic_log_status::available;
		}
		auto status() const noexcept -> application::diagnostic_log_status override { return application::diagnostic_log_status::available; }
	};
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
	struct epoch_handoffs final : application::github_epoch_handoff_port
	{
		std::vector<application::epoch_force_push_observation> ForcePushes;
		application::epoch_timeline_availability               Availability
			= application::epoch_timeline_availability::unavailable;
		std::size_t AcceptCount = 0U;

		void accept(application::accepted_pull_request_handoff aHandoff) override
		{
			ForcePushes  = std::move(aHandoff.ForcePushes);
			Availability = aHandoff.TimelineAvailability;
			++AcceptCount;
		}
		void invalidate(application::pull_request_authority_invalidation) override { }
	};
	auto session() -> application::github_profile_session
	{
		application::github_profile_session result{
			.BprProfile = core::default_bpr_profile_id, .ApplicationSession = core::application_session_id{ 1 }, .ProfileAuthority = { .Profile = core::host_profile_id{ 1 }, .ProfileRevision = application::github_profile_revision{ 1 }, .Host = core::git_host_id{ 1 }, .Hostname = application::github_canonical_hostname{ "github.com" }, .ProbeContractRevision = application::github_probe_contract_revision{ 1 } }, .Session = { .Profile = core::host_profile_id{ 1 }, .Host = core::git_host_id{ 1 }, .Viewer = { .Account = core::github_account_id{ 1 }, .Node = application::github_actor_id{ "actor" }, .Login = "reader" }, .CapabilityEvidence = { .Revision = core::capability_revision{ 1 }, .ContractVersion = 1 } }, .Generation = core::github_identity_generation{ 1 }, .CapabilityEvidence = { .Authority = { .Profile = core::host_profile_id{ 1 }, .ProfileRevision = application::github_profile_revision{ 1 }, .Host = core::git_host_id{ 1 }, .Account = core::github_account_id{ 1 }, .ProbeContractRevision = application::github_probe_contract_revision{ 1 } }, .Revision = core::capability_revision{ 1 } }, .AccountTransport = std::make_shared<transport>()
		};
		return result;
	}
	[[nodiscard]] auto
	snapshot(const application::github_pull_request_provider_request& aRequest)
		-> application::github_pull_request_snapshot
	{
		return {
			.Profile = aRequest.Context.Profile,
			.Host    = aRequest.Context.Host,
			.Account = aRequest.Context.Account,
			.Viewer  = {
				.Actor = aRequest.Viewer.Node,
				.Login = aRequest.Viewer.Login,
			},
			.Snapshot   = core::provider_snapshot_id{ 91U },
			.Repository = {
				.Repository    = core::repository_id{ 71U },
				.Node          = application::github_node_id{ "repository-node" },
				.Owner         = aRequest.Context.Repository.Owner,
				.Name          = aRequest.Context.Repository.Name,
				.DefaultBranch = "main",
			},
			.PullRequest = {
				.PullRequest = core::pull_request_id{ 81U },
				.Repository  = core::repository_id{ 71U },
				.Number      = aRequest.Context.Number,
				.Node        = application::github_node_id{ "pull-request-node" },
				.Title       = "Host orchestration",
				.Body        = "Bounded.",
				.State       = application::github_pull_request_state::open,
				.Draft       = false,
				.Author      = std::nullopt,
				.BaseOid     = application::commit_oid{ "1111111111111111111111111111111111111111" },
				.HeadOid     = application::commit_oid{ "2222222222222222222222222222222222222222" },
				.BaseRef     = "main",
				.HeadRef     = "host",
			},
			.Files                     = { },
			.Commits                   = { },
			.Reviews                   = { },
			.ReviewThreadsAvailability = application::github_collection_availability::unavailable,
			.ReviewThreads             = { },
			.IssueComments             = { },
			.Checks                    = { },
			.SearchResults             = { },
			.CapabilityEvidence        = {
				.Capabilities    = aRequest.CapabilityEvidence.Capabilities,
				.Revision        = aRequest.CapabilityEvidence.Revision,
				.ReadShape       = aRequest.CapabilityEvidence.ReadShape,
				.ContractVersion = 1U,
			},
			.RateLimits = {
				.Core   = application::github_rate_limit_record{ application::github_rate_limit_resource::core, 5000U, 4999U, 100U },
				.Search = application::github_rate_limit_record{ application::github_rate_limit_resource::search, 30U, 29U, 100U },
			},
		};
	}

	void test_safe_read_presentation_matches_accepted_state()
	{
		reads                                               provider;
		application::manual_executor                        lane;
		diagnostics                                         log;
		std::vector<application::github_window_read_update> updates;
		application::github_window_read_host                host{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 1 } },
			core::application_session_id{ 2 },
			provider,
			lane,
			[&updates](const auto& aUpdate) { updates.push_back(aUpdate); },
			nullptr,
			nullptr,
			nullptr,
			&log
		};
		expect(host.adopt(session()), "presentation fixture adopts authenticated session");
		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 1 } });
		const auto first = provider.Calls.back();
		expect(first.Request.Context.ApplicationSession == core::application_session_id{ 2U }
				   && first.Request.AuthenticationSession == session().ApplicationSession,
			   "window causal session and trusted authentication provenance remain distinct");
		auto accepted               = snapshot(first.Request);
		accepted.SearchAvailability = application::github_search_availability::result_limit;
		first.Complete(application::github_pull_request_success{ first.Request, accepted, application::diagnostic_paged_operation::list_reviews });
		lane.drain();
		expect(log.Events.size() == 2U
				   && log.Events[0].Outcome == application::diagnostic_outcome::started
				   && log.Events[1].Outcome == application::diagnostic_outcome::succeeded
				   && log.Events[1].PagedOperation == application::diagnostic_paged_operation::list_reviews
				   && log.Events[1].Request == first.Request.Context.Request.value(),
			   "accepted load has one typed succeeded diagnostic");
		expect(host.accepted_snapshot().has_value() && updates.back().Presentation.PullRequest.has_value(), "a refused ancillary search still permits the accepted exact PR projection");
		const auto& shown = *updates.back().Presentation.PullRequest;
		expect(shown.Snapshot == host.accepted_snapshot()->Snapshot && shown.Repository == accepted.Repository.Repository
				   && shown.PullRequest == accepted.PullRequest.PullRequest && shown.Title == accepted.PullRequest.Title
				   && shown.Body == accepted.PullRequest.Body && shown.Number == accepted.PullRequest.Number
				   && shown.RateLimits.Core->Remaining == accepted.RateLimits.Core->Remaining,
			   "safe accepted fields match the exact authoritative snapshot");
		host.dispatch(core::open_pull_request_intent{ { "owner", "other" }, core::pull_request_number{ 2 } });
		expect(!host.accepted_snapshot() && !updates.back().Presentation.PullRequest,
			   "a replacement read clears raw snapshot and publishes no stale PR payload");
		const auto second = provider.Calls.back();
		const auto error  = application::github_provider_error{
			.Failure           = application::github_provider_failure::unavailable,
			.RetryAfterSeconds = 9U,
		};
		second.Complete(application::github_pull_request_error{ second.Request, error, application::diagnostic_paged_operation::list_pull_request_files });
		lane.drain();
		expect(log.Events.size() == 4U
				   && log.Events[3].Outcome == application::diagnostic_outcome::failed
				   && log.Events[3].PagedOperation == application::diagnostic_paged_operation::list_pull_request_files
				   && std::get<core::github_failure>(log.Events[3].GithubFailure) == core::github_failure::network
				   && log.Events[3].Summary.text().empty(),
			   "failed load retains mapped failure and last typed page without text");
		expect(updates.back().ProviderError == error && updates.back().Presentation.HostDiagnostics.size() == 1U
				   && updates.back().Presentation.HostDiagnostics.front().Reason == core::reason_for(core::github_failure::network)
				   && updates.back().Presentation.HostDiagnostics.front().RetryAfterSeconds == error.RetryAfterSeconds,
			   "accepted provider advisory maps typed failure and retry facts in the same update");
		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 3 } });
		const auto third   = provider.Calls.back();
		auto       invalid = snapshot(third.Request);
		invalid.PullRequest.Title.clear();
		third.Complete(application::github_pull_request_success{ third.Request, invalid, application::diagnostic_paged_operation::get_pull_request });
		lane.drain();
		expect(log.Events.size() == 6U
				   && log.Events[5].Outcome == application::diagnostic_outcome::failed
				   && std::holds_alternative<application::github_read_validation_error>(log.Events[5].GithubFailure)
				   && std::get<application::github_read_validation_error>(log.Events[5].GithubFailure) == application::github_read_validation_error::missing_required_text,
			   "snapshot refusal records validation error name in typed failure field");
		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 4 } });
		const auto fourth = provider.Calls.back();
		fourth.Complete(application::github_pull_request_error{
			fourth.Request,
			{ .Failure = application::github_provider_failure::malformed_response },
			application::diagnostic_paged_operation::list_reviews,
			application::github_read_validation_error::invalid_relationship,
			application::github_read_validation_error::relationship_actor_login,
		});
		lane.drain();
		expect(log.Events.size() == 8U
				   && log.Events[7].PagedOperation == application::diagnostic_paged_operation::list_reviews
				   && std::get<application::github_read_validation_error>(log.Events[7].GithubFailure) == application::github_read_validation_error::invalid_relationship
				   && log.Events[7].RelationshipDetail == application::github_read_validation_error::relationship_actor_login,
			   "provider-side validation refusal retains its exact typed name");
		for (const auto status : { 403U, 404U }) {
			host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ status } });
			const auto call = provider.Calls.back();
			call.Complete(application::github_pull_request_error{
				call.Request,
				{ .Failure                = application::github_provider_failure::permission,
				  .HttpStatus             = static_cast<std::uint16_t>(status),
				  .TransportFailureSource = application::diagnostic_transport_failure_source::http_response },
				application::diagnostic_paged_operation::get_repository,
			});
			lane.drain();
			const auto& event = log.Events.back();
			expect(event.Outcome == application::diagnostic_outcome::failed
					   && event.HttpStatus == status
					   && event.TransportFailureSource == application::diagnostic_transport_failure_source::http_response
					   && event.PagedOperation == application::diagnostic_paged_operation::get_repository
					   && std::get<core::github_failure>(event.GithubFailure) == core::github_failure::permission
					   && event.Summary.text().empty() && !host.accepted_snapshot(),
				   "GetRepository denial retains only status/source with unchanged permission refusal");
		}
		for (const auto& update : updates) {
			expect(update.Presentation.Transition && update.Presentation.Transition->Status == update.Transition.Status
					   && update.Presentation.Transition->PullRequest == update.Transition.PullRequest
					   && update.Presentation.Transition->Identity == update.Transition.Identity
					   && update.Presentation.Transition->Diagnostics == update.Transition.Diagnostics,
				   "every safe transition preserves exact status, clear, identity and diagnostics deltas");
			expect(!update.Presentation.Session, "read updates do not overwrite profile-owned session and grant presentation");
		}
		expect(!host.projection_delivery_failed(), "mapping and delivery stay inside the successful host boundary");
	}

	void test_timeline_evidence_reaches_epoch_handoff_without_blocking_live_read()
	{
		reads                                provider;
		application::manual_executor         lane;
		epoch_handoffs                       epochs;
		application::github_window_read_host host{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 7 }, core::bpr_window_generation{ 1 } },
			core::application_session_id{ 2 },
			provider,
			lane,
			[](const auto&) { },
			&epochs
		};
		expect(host.adopt(session()), "timeline fixture adopts session");
		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 1 } });
		const auto first              = provider.Calls.back();
		auto       accepted           = snapshot(first.Request);
		accepted.TimelineAvailability = application::github_timeline_availability::available;
		accepted.ForcePushEvents.push_back({
			.EventId                   = 917U,
			.AfterHeadOid              = "3333333333333333333333333333333333333333",
			.CreatedAtUnixMilliseconds = 1'789'000'000'000LL,
		});
		first.Complete(application::github_pull_request_success{ first.Request, accepted });
		lane.drain();
		expect(epochs.AcceptCount == 1U && epochs.ForcePushes.size() == 1U
				   && epochs.ForcePushes.front().EventId == 917U
				   && epochs.Availability == application::epoch_timeline_availability::available,
			   "accepted timeline crosses the exact live epoch handoff");

		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 2 } });
		const auto second             = provider.Calls.back();
		auto       degraded           = snapshot(second.Request);
		degraded.TimelineAvailability = application::github_timeline_availability::refused;
		second.Complete(application::github_pull_request_success{ second.Request, degraded });
		lane.drain();
		expect(epochs.AcceptCount == 2U && epochs.ForcePushes.empty()
				   && epochs.Availability == application::epoch_timeline_availability::refused,
			   "timeline refusal retains the live handoff without fabricated updates");
	}

	void test_two_windows_have_distinct_diagnostic_courses()
	{
		reads                                firstReads, secondReads;
		application::manual_executor         lane;
		diagnostics                          log;
		application::github_window_read_host first{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 11 }, core::bpr_window_generation{ 1 } },
			core::application_session_id{ 12 },
			firstReads,
			lane,
			[](const auto&) { },
			nullptr,
			nullptr,
			nullptr,
			&log
		};
		application::github_window_read_host second{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 12 }, core::bpr_window_generation{ 1 } },
			core::application_session_id{ 13 },
			secondReads,
			lane,
			[](const auto&) { },
			nullptr,
			nullptr,
			nullptr,
			&log
		};
		expect(first.adopt(session()) && second.adopt(session()), "both windows adopt recorded identity");
		first.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 1 } });
		second.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 1 } });
		expect(log.Events.size() == 2U && log.Events[0].Request == log.Events[1].Request
				   && log.Events[0].WindowSession == 12U && log.Events[1].WindowSession == 13U,
			   "window-local request ids remain distinct in the shared diagnostic stream");
		first.cancel();
		second.cancel();
		firstReads.Calls.front().Complete(application::github_pull_request_cancelled{ firstReads.Calls.front().Request });
		secondReads.Calls.front().Complete(application::github_pull_request_cancelled{ secondReads.Calls.front().Request });
		lane.drain();
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
	test_safe_read_presentation_matches_accepted_state();
	test_two_windows_have_distinct_diagnostic_courses();
	test_timeline_evidence_reaches_epoch_handoff_without_blocking_live_read();
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
