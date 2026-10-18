#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/github_read_admission.hpp"
#include "bpr/application/github_window_read_host.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/reference_page.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
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
			application::github_pull_request_overview_handler   Overview{ };
		};
		std::vector<call> Calls;
		std::size_t       Cancellations = 0;
		void              fetch_pull_request(application::github_pull_request_provider_request   aRequest,
											 application::github_pull_request_completion_handler aComplete) override { Calls.push_back({ std::move(aRequest), std::move(aComplete) }); }
		void              cancel_pull_request(application::github_pull_request_provider_request) override { ++Cancellations; }
		void              fetch_pull_request_with_observations(application::github_pull_request_provider_request   aRequest,
															   application::github_pull_request_completion_handler aComplete,
															   application::github_pull_request_progress_handler,
															   application::github_pull_request_overview_handler aOverview) override
		{
			Calls.push_back({ std::move(aRequest), std::move(aComplete), std::move(aOverview) });
		}
	};
	struct epoch_handoffs final : application::github_epoch_handoff_port
	{
		std::vector<application::epoch_force_push_observation> ForcePushes;
		application::epoch_timeline_availability               Availability
			= application::epoch_timeline_availability::unavailable;
		std::size_t                                             AcceptCount = 0U;
		std::optional<application::accepted_pull_request_shape> Shape;
		core::git_commit_oid                                    Base;
		core::comparison_base_basis                             Basis = core::comparison_base_basis::merge_base;

		void accept(application::accepted_pull_request_handoff aHandoff) override
		{
			Base         = aHandoff.Base;
			Basis        = aHandoff.BaseBasis;
			Shape        = aHandoff.Shape;
			ForcePushes  = std::move(aHandoff.ForcePushes);
			Availability = aHandoff.TimelineAvailability;
			++AcceptCount;
		}
		std::vector<application::pull_request_authority_invalidation> Invalidations;
		void                                                          invalidate(application::pull_request_authority_invalidation aInvalidation) override { Invalidations.push_back(aInvalidation); }
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

	struct overview_cache final : application::cached_review_overview_port
	{
		std::optional<core::cached_review_overview> Value;
		std::size_t                                 Saves = 0, Loads = 0;
		auto                                        load(const core::cached_review_key&, std::stop_token) -> std::optional<core::cached_review_overview> override
		{
			++Loads;
			return Value;
		}
		auto save(const core::cached_review_overview& aValue, std::stop_token) -> bool override
		{
			++Saves;
			Value = aValue;
			return true;
		}
	};
	struct historical_sources final : application::cached_review_source_port
	{
		std::vector<application::cached_review_source_request>    Requests;
		std::vector<application::cached_review_source_completion> Completions;
		std::vector<std::stop_token>                              Stops;
		auto                                                      read_cached_source(application::cached_review_source_request aRequest, std::stop_token aStop, application::cached_review_source_completion aCompletion) -> bool override
		{
			Requests.push_back(std::move(aRequest));
			Stops.push_back(aStop);
			Completions.push_back(std::move(aCompletion));
			return true;
		}
		void complete(std::size_t aIndex, bool aForeign = false)
		{
			const auto& request = Requests[aIndex];
			Completions[aIndex]({ .Path = request.File.Path, .BaseOid = std::string{ core::canonical_hex(request.Base).view() }, .HeadOid = aForeign ? std::string(40, '9') : std::string{ core::canonical_hex(request.Head).view() }, .Status = application::cached_source_status::available, .BaseText = "old text", .HeadText = "retained text" });
		}
	};
	void test_durable_overview_lifecycle()
	{
		reads                                               provider;
		application::manual_executor                        lane, effects;
		overview_cache                                      cache;
		historical_sources                                  sources;
		epoch_handoffs                                      epochs;
		std::vector<application::github_window_read_update> updates;
		application::github_window_read_host                host{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 1 } },
			core::application_session_id{ 2 },
			provider,
			lane,
			[&](const auto& aUpdate) { updates.push_back(aUpdate); },
			&epochs,
			nullptr,
			nullptr,
			nullptr,
			&cache,
			&effects,
			&sources
		};
		auto identity                                          = session();
		identity.CapabilityEvidence.Capabilities.ReviewThreads = core::github_capability_state::supported;
		identity.CapabilityEvidence.ReadShape.ReviewThreads    = true;
		identity.Session.CapabilityEvidence.Capabilities       = identity.CapabilityEvidence.Capabilities;
		identity.Session.CapabilityEvidence.ReadShape          = identity.CapabilityEvidence.ReadShape;
		expect(host.adopt(identity), "cache course adopts authenticated account");
		const auto                                   open = [&] { host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 1 } }); };
		std::vector<application::github_file_record> serverFiles{
			{ .Path = "src/renamed.cpp", .PreviousPath = "old.cpp", .Change = application::github_file_change::renamed, .Additions = 9, .Deletions = 4 }
		};
		std::vector<application::github_review_thread_record> serverThreads{
			{ .Thread = application::github_thread_id{ "T1" }, .Path = "src/renamed.cpp", .Line = 7, .OriginalLine = 5, .DiffSide = application::github_diff_side::right, .Comments = { { .Comment = application::github_comment_id{ "C1" }, .Body = "Historical root", .Path = "src/renamed.cpp", .Line = 7 }, { .Comment = application::github_comment_id{ "C2" }, .ReplyTo = application::github_comment_id{ "C1" }, .Body = "Historical reply", .Path = "src/renamed.cpp", .Line = 7 } } }
		};
		auto       serverBase = std::string(40, '1'), serverHead = std::string(40, '2');
		const auto succeed = [&] {
			const auto call                = provider.Calls.back();
			auto       data                = snapshot(call.Request);
			data.Files                     = serverFiles;
			data.ReviewThreadsAvailability = call.Request.CapabilityEvidence.ReadShape.ReviewThreads ? application::github_collection_availability::available : application::github_collection_availability::unavailable;
			data.ReviewThreads             = serverThreads;
			data.RateLimits.Graphql        = application::github_rate_limit_record{ application::github_rate_limit_resource::graphql, 5000, 4999, 100 };
			data.PullRequest.BaseOid       = application::commit_oid{ serverBase };
			data.PullRequest.HeadOid       = application::commit_oid{ serverHead };
			// [[BPR-1560]]. GitHub named the merge base, here the recorded base;
			// without one nothing is retained for the offline source reader.
			data.MergeBaseOid          = application::commit_oid{ serverBase };
			data.MergeBaseTipOid       = application::commit_oid{ serverBase };
			data.MergeBaseAvailability = application::github_merge_base_availability::available;
			call.Complete(application::github_pull_request_success{ call.Request, std::move(data) });
			lane.drain();
		};
		const auto fail = [&](application::github_provider_failure aFailure = application::github_provider_failure::network) {
			for (unsigned attempt = 0; attempt < 3; ++attempt) {
				const auto call = provider.Calls.back();
				call.Complete(application::github_pull_request_error{ .Request = call.Request, .Error = { .Failure = aFailure } });
				lane.drain();
				if (!host.state().PendingPullRequest || effects.pending()) {
					break;
				}
			}
		};
		const auto cached = [&] { return !updates.empty() && updates.back().Presentation.CachedOverview.has_value(); };
		open();
		succeed();
		expect(host.accepted_snapshot() && cache.Saves == 0 && effects.pending() == 1, "only accepted snapshot schedules off-lane persistence");
		effects.drain();
		lane.drain();
		expect(cache.Saves == 1 && cache.Value && cache.Value->HeadOid == std::string(40, '2'), "saved overview retains exact accepted revision");
		open();
		fail();
		expect(!host.accepted_snapshot() && effects.pending() == 1 && !cached(), "failed read waits for off-lane cache observation");
		effects.drain();
		lane.drain();
		expect(cached() && !host.state().AcceptedSnapshot && !host.mutation_transport() && epochs.AcceptCount == 1,
			   "cached metadata publishes without current snapshot, mutation or epoch authority");
		expect(cache.Value->FileCatalogue && cache.Value->FileCatalogue->size() == 1
				   && cache.Value->FileCatalogue->front() == core::cached_review_file{ "src/renamed.cpp", "old.cpp", core::cached_file_change::renamed, 9, 4 }
				   && updates.back().Presentation.CachedOverview->FileCatalogue == cache.Value->FileCatalogue,
			   "cached catalogue preserves exact path provenance, operation and counts without live authority");
		expect(cache.Value->ReviewThreads && cache.Value->ReviewThreads->size() == 1
				   && cache.Value->ReviewThreads->front().Comments.back().ReplyTo == "C1"
				   && cache.Value->ReviewThreads->front().Comments.back().Body == "Historical reply"
				   && cache.Value->ReviewThreads->front().Line == 7U
				   && updates.back().Presentation.CachedOverview->ReviewThreads == cache.Value->ReviewThreads,
			   "exact thread roots, replies and anchors persist and project without live authority");
		host.read_cached_source("foreign.cpp");
		expect(sources.Requests.empty(), "unlisted source path is refused before effects");
		host.read_cached_source("src/renamed.cpp");
		expect(sources.Requests.size() == 1 && sources.Requests.back().Catalog.ExistingOnly && sources.Requests.back().File.PreviousPath == "old.cpp"
				   && sources.Requests.back().Catalog.Authority.Account == identity.Session.Viewer.Account,
			   "historical source warrant carries exact current reader, retained pair and rename provenance");
		sources.complete(0);
		expect(updates.back().Presentation.CachedSource && updates.back().Presentation.CachedSource->HeadText == "retained text" && !host.accepted_snapshot() && epochs.AcceptCount == 1 && !host.mutation_transport(), "historical source text publishes without live authority");
		host.read_cached_source("src/renamed.cpp");
		sources.complete(1, true);
		expect(updates.back().Presentation.CachedSource->Status == application::cached_source_status::unavailable && !updates.back().Presentation.CachedSource->HeadText, "foreign head completion cannot publish content");
		host.read_cached_source("src/renamed.cpp");
		host.cancel();
		const auto cancelledCount = updates.size();
		sources.complete(2);
		expect(sources.Stops[2].stop_requested() && updates.size() == cancelledCount && host.is_quiescent(), "cancelled historical source completion drains without publication");
		open();
		fail();
		effects.drain();
		lane.drain();
		host.read_cached_source("src/renamed.cpp");
		expect(host.retire(identity), "retire during historical source read");
		const auto retiredCount = updates.size();
		sources.complete(3);
		expect(sources.Stops[3].stop_requested() && updates.size() == retiredCount && host.is_quiescent(), "old session historical source cannot publish into another account");
		identity.Generation = core::github_identity_generation{ identity.Generation.value() + 1 };
		expect(host.adopt(identity), "readopt for remaining lifecycle controls");
		open();
		fail(application::github_provider_failure::permission);
		expect(!cached() && effects.pending() == 0, "permission failure cannot expose historical metadata");
		open();
		fail();
		host.cancel();
		effects.drain();
		lane.drain();
		expect(!cached() && host.is_quiescent(), "cancelled cache completion drains without publishing");
		open();
		fail();
		expect(host.retire(identity), "identity retirement cancels delayed cached result");
		effects.drain();
		lane.drain();
		expect(!cached() && host.is_quiescent(), "retired cache result cannot reach another session");
		identity.Generation = core::github_identity_generation{ identity.Generation.value() + 1 };
		expect(host.adopt(identity), "replacement generation adopted");
		cache.Value->Key.Account = core::github_account_id{ 99 };
		open();
		fail();
		effects.drain();
		lane.drain();
		expect(!cached(), "core refuses a cache port returning foreign account data");
		serverThreads.front().Comments.front().Body = "Changed server root";
		serverThreads.front().Resolved              = true;
		serverBase                                  = std::string(40, '3');
		serverHead                                  = std::string(40, '4');
		serverFiles                                 = { { .Path = "new-server.cpp", .Change = application::github_file_change::added, .Additions = 27 } };
		open();
		succeed();
		effects.drain();
		lane.drain();
		expect(host.accepted_snapshot() && !cached(), "reconnection adopts new server truth and clears cached presentation");
		expect(cache.Value->BaseOid == serverBase && cache.Value->HeadOid == serverHead
				   && cache.Value->FileCatalogue->front().Path == "new-server.cpp"
				   && cache.Value->FileCatalogue->front().Additions == 27
				   && cache.Value->ReviewThreads->front().Comments.front().Body == "Changed server root"
				   && cache.Value->ReviewThreads->front().Resolved,
			   "changed base/head and catalogue replace one historical observation atomically");
		serverThreads.clear();
		serverFiles.clear();
		open();
		succeed();
		effects.drain();
		lane.drain();
		open();
		fail();
		effects.drain();
		lane.drain();
		expect(cached() && updates.back().Presentation.CachedOverview->FileCatalogue
				   && updates.back().Presentation.CachedOverview->FileCatalogue->empty(),
			   "observed empty file catalogue survives cache admission");
		expect(updates.back().Presentation.CachedOverview->ReviewThreads && updates.back().Presentation.CachedOverview->ReviewThreads->empty(), "observed empty threads survive cache admission");
		serverThreads.push_back({ .Thread = application::github_thread_id{ "large" }, .Path = "file", .DiffSide = application::github_diff_side::right, .Comments = { { .Comment = application::github_comment_id{ "large-comment" }, .Body = std::string(64 * 1024 + 1, 'x'), .Path = "file" } } });
		for (std::size_t index = 0; index <= core::maximum_cached_files; ++index) {
			serverFiles.push_back({ .Path = "file" + std::to_string(index), .Change = application::github_file_change::modified });
		}
		open();
		succeed();
		effects.drain();
		lane.drain();
		expect(cache.Value->Files == serverFiles.size() && !cache.Value->FileCatalogue,
			   "oversized optional catalogue keeps current overview with unavailable files, never a truncated or old catalogue");
		expect(!cache.Value->ReviewThreads, "over-limit thread collection becomes unavailable without keeping old or partial threads");
		serverThreads.clear();
		expect(host.retire(identity), "retire thread-capable session");
		identity.Generation                                    = core::github_identity_generation{ identity.Generation.value() + 1 };
		identity.CapabilityEvidence.Capabilities.ReviewThreads = core::github_capability_state::unsupported;
		identity.CapabilityEvidence.ReadShape.ReviewThreads    = false;
		identity.Session.CapabilityEvidence.Capabilities       = identity.CapabilityEvidence.Capabilities;
		identity.Session.CapabilityEvidence.ReadShape          = identity.CapabilityEvidence.ReadShape;
		expect(host.adopt(identity), "adopt session without thread capability");
		open();
		succeed();
		effects.drain();
		lane.drain();
		expect(!cache.Value->ReviewThreads, "unobserved threads remain unavailable despite empty provider vector");
		open();
		fail();
		host.shutdown();
		effects.drain();
		lane.drain();
		expect(!cached() && host.is_quiescent(), "shutdown drains cached read and prevents presentation");
	}

	void test_provisional_overview_is_bounded_and_never_authority()
	{
		reads                                               provider;
		application::manual_executor                        lane;
		epoch_handoffs                                      epochs;
		std::vector<application::github_window_read_update> updates;
		application::github_window_read_host                host{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 2U }, core::bpr_window_generation{ 1U } },
			core::application_session_id{ 2U },
			provider,
			lane,
			[&](const auto& aUpdate) { updates.push_back(aUpdate); },
			&epochs
		};
		expect(host.adopt(session()), "progressive fixture accepts exact account session");
		const auto observation = [](const reads::call& aCall) {
			auto data                            = snapshot(aCall.Request);
			data.PullRequest.ReportedCommitCount = 5U;
			return application::github_pull_request_overview_observation{ aCall.Request.Context, data.Repository, data.PullRequest, 1523U };
		};
		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 1U } });
		const auto first           = provider.Calls.back();
		const auto before          = updates.size();
		auto       foreign         = observation(first);
		foreign.Context.Generation = core::request_generation{ 999U };
		first.Overview(foreign);
		expect(lane.pending() == 0U, "foreign observations do not consume the current request's single publication");
		for (auto n = 0; n < 1000; ++n) {
			first.Overview(observation(first));
		}
		expect(lane.pending() == 1U && updates.size() == before, "repeated provider observations retain one bounded decision-lane task");
		lane.drain();
		expect(updates.size() == before + 1U && updates.back().Presentation.LoadingOverview
				   && updates.back().Presentation.LoadingOverview->Title == "Host orchestration"
				   && updates.back().Presentation.LoadingOverview->ReportedFiles == 1523U
				   && !updates.back().Presentation.PullRequest && !host.accepted_snapshot() && epochs.AcceptCount == 0U,
			   "validated useful metadata is visible without snapshot, epoch or file authority");
		first.Complete(application::github_pull_request_error{ first.Request, { .Failure = application::github_provider_failure::unavailable } });
		lane.drain();
		first.Overview(observation(first));
		expect(lane.pending() == 0U && !host.accepted_snapshot(), "terminal failure rejects late metadata and never accepts partial content");
		host.dispatch(core::open_pull_request_intent{ { "owner", "other" }, core::pull_request_number{ 2U } });
		const auto second = provider.Calls.back();
		second.Overview(observation(second));
		host.cancel();
		const auto afterCancel = updates.size();
		lane.drain();
		expect(updates.size() == afterCancel, "queued observations cannot revive a cancelled target");
		second.Complete(application::github_pull_request_cancelled{ second.Request });
		lane.drain();
		host.dispatch(core::open_pull_request_intent{ { "owner", "fresh" }, core::pull_request_number{ 3U } });
		const auto third   = provider.Calls.back();
		auto       invalid = observation(third);
		invalid.PullRequest.Title.assign(1025U, 'x');
		third.Overview(invalid);
		const auto beforeInvalid = updates.size();
		lane.drain();
		expect(updates.size() == beforeInvalid, "oversized metadata cannot reach the projection");
		third.Complete(application::github_pull_request_success{ third.Request, snapshot(third.Request) });
		lane.drain();
		expect(host.accepted_snapshot() && updates.back().Presentation.PullRequest && !updates.back().Presentation.LoadingOverview,
			   "only the complete validated terminal promotes authoritative content");
		host.dispatch(core::open_pull_request_intent{ { "owner", "last" }, core::pull_request_number{ 4U } });
		const auto fourth                      = provider.Calls.back();
		auto       wrongRepository             = observation(fourth);
		wrongRepository.PullRequest.Repository = core::repository_id{ 999U };
		fourth.Overview(wrongRepository);
		const auto beforeMismatch = updates.size();
		lane.drain();
		expect(updates.size() == beforeMismatch, "metadata cannot cross its exact repository relationship");
		host.shutdown();
		fourth.Complete(application::github_pull_request_cancelled{ fourth.Request });
		lane.drain();
		expect(host.is_quiescent(), "progressive publication accounts for all queued work");
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
			expect(host.state().ActiveProfile == first.Request.Context.Profile
					   && host.state().Host == first.Request.Context.Host && host.state().Account == first.Request.Context.Account
					   && host.state().IdentityGeneration == first.Request.Context.IdentityGeneration
					   && host.state().AccessMode == core::github_access_mode::authenticated
					   && call.Request.AccountTransport == first.Request.AccountTransport
					   && call.Request.AuthenticationSession == first.Request.AuthenticationSession,
				   "403 and 404 refuse only the read, preserving the exact authenticated session and transport");
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
		const auto identity = session();
		expect(host.adopt(identity), "timeline fixture adopts session");
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
		expect(epochs.Shape && epochs.Shape->FileCount == accepted.Files.size()
				   && epochs.Shape->ReturnedCommitCount == accepted.Commits.size()
				   && epochs.Shape->ReportedCommitCount == accepted.PullRequest.ReportedCommitCount,
			   "handoff count facts come from the accepted snapshot");

		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 2 } });
		const auto second             = provider.Calls.back();
		auto       degraded           = snapshot(second.Request);
		degraded.TimelineAvailability = application::github_timeline_availability::refused;
		second.Complete(application::github_pull_request_success{ second.Request, degraded });
		lane.drain();
		expect(epochs.AcceptCount == 2U && epochs.ForcePushes.empty()
				   && epochs.Availability == application::epoch_timeline_availability::refused,
			   "timeline refusal retains the live handoff without fabricated updates");
		const auto currentShape = epochs.Shape;
		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 3 } });
		const auto stale = provider.Calls.back();
		expect(host.retire(identity), "identity switch retires pending shape observation");
		auto replacement       = session();
		replacement.Generation = core::github_identity_generation{ 2 };
		expect(host.adopt(replacement), "replacement identity becomes active");
		stale.Complete(application::github_pull_request_success{ stale.Request, snapshot(stale.Request) });
		lane.drain();
		expect(epochs.AcceptCount == 2U && epochs.Shape == currentShape && !host.accepted_snapshot(),
			   "superseded snapshot cannot replace accepted shape");
	}

	/// [[BPR-1573]]. Close review: the host lets go of the accepted pull request
	/// as opening another one does (its epoch authority is invalidated once and
	/// its mutation transport revoked), stops a read in flight without
	/// publishing its late answer, and changes nothing when nothing is open.
	void test_close_review_releases_the_read()
	{
		reads                                               provider;
		application::manual_executor                        lane;
		epoch_handoffs                                      epochs;
		std::vector<application::github_window_read_update> updates;
		application::github_window_read_host                host{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 9 }, core::bpr_window_generation{ 1 } },
			core::application_session_id{ 2 },
			provider,
			lane,
			[&updates](const auto& aUpdate) { updates.push_back(aUpdate); },
			&epochs
		};
		expect(host.adopt(session()), "close fixture adopts session");
		updates.clear();
		host.dispatch(core::close_pull_request_intent{ });
		expect(updates.size() == 1U && !updates.back().Transition.PullRequest && updates.back().Transition.Diagnostics.size() == 1U
				   && updates.back().Transition.Diagnostics.front().Code == core::github_reason_code::no_active_pull_request
				   && epochs.Invalidations.empty() && provider.Cancellations == 0U,
			   "a close with nothing open is refused and releases nothing");
		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 1 } });
		const auto first = provider.Calls.back();
		first.Complete(application::github_pull_request_success{ first.Request, snapshot(first.Request) });
		lane.drain();
		expect(epochs.AcceptCount == 1U && host.accepted_snapshot(), "the pull request is accepted with an epoch authority");
		updates.clear();
		host.dispatch(core::close_pull_request_intent{ });
		expect(epochs.Invalidations.size() == 1U && epochs.Invalidations.front().PullRequest == core::pull_request_id{ 81U }
				   && !host.accepted_snapshot() && !host.state().ActiveLocator && !host.state().PullRequest,
			   "close review invalidates the accepted epoch authority");
		expect(updates.size() == 1U && updates.back().Transition.PullRequest && !updates.back().Transition.PullRequest->Summary
				   && updates.back().Transition.Status && updates.back().Transition.Status->Status == core::github_status::session_ready,
			   "close review publishes a window with no pull request and its session kept");
		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 2 } });
		const auto pending = provider.Calls.back();
		const auto cancels = provider.Cancellations;
		host.dispatch(core::close_pull_request_intent{ });
		expect(provider.Cancellations == cancels + 1U && !host.state().PendingPullRequest, "close review stops the read in flight");
		updates.clear();
		pending.Complete(application::github_pull_request_success{ pending.Request, snapshot(pending.Request) });
		lane.drain();
		expect(updates.empty() && !host.accepted_snapshot() && epochs.AcceptCount == 1U && host.is_quiescent(),
			   "the closed read's late answer is neither published nor accepted");
		host.shutdown();
	}

	/// [[BPR-1560]]. A merge base GitHub named outlives a later failed read of
	/// it for the same head, recorded base and base branch, and never an
	/// answer that proves it stale: the base branch naming another merge base,
	/// or the pull request moving under the read. After such a refusal a
	/// failed read brings nothing back.
	void test_merge_base_carries_forward_only_across_failures()
	{
		reads                                provider;
		application::manual_executor         lane;
		epoch_handoffs                       epochs;
		application::github_window_read_host host{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 8 }, core::bpr_window_generation{ 1 } },
			core::application_session_id{ 2 },
			provider,
			lane,
			[](const auto&) { },
			&epochs
		};
		expect(host.adopt(session()), "merge-base fixture adopts session");
		const auto mergeBase = std::string(40, 'a');
		const auto recorded  = std::string(40, '1');
		const auto answer    = [&](const application::github_merge_base_availability aAvailability) {
			const auto call = provider.Calls.back();
			auto       data = snapshot(call.Request);
			if (aAvailability == application::github_merge_base_availability::available) {
				data.MergeBaseOid    = application::commit_oid{ mergeBase };
				data.MergeBaseTipOid = application::commit_oid{ std::string(40, 'b') };
			}
			data.MergeBaseAvailability = aAvailability;
			call.Complete(application::github_pull_request_success{ call.Request, std::move(data) });
			lane.drain();
		};
		const auto from = [&](const std::string& aBase, const core::comparison_base_basis aBasis) {
			return epochs.Base == core::parse_git_commit_oid(aBase) && epochs.Basis == aBasis;
		};
		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 1 } });
		answer(application::github_merge_base_availability::available);
		expect(epochs.AcceptCount == 1U && from(mergeBase, core::comparison_base_basis::merge_base), "the named merge base is the comparison base");
		host.dispatch(core::refresh_pull_request_intent{ });
		answer(application::github_merge_base_availability::failed);
		expect(epochs.AcceptCount == 2U && from(mergeBase, core::comparison_base_basis::merge_base)
				   && host.accepted_snapshot()->MergeBaseAvailability == application::github_merge_base_availability::available,
			   "a failed read keeps the merge base named for the same head and recorded base");
		host.dispatch(core::refresh_pull_request_intent{ });
		answer(application::github_merge_base_availability::refused);
		expect(epochs.AcceptCount == 3U && from(recorded, core::comparison_base_basis::disputed_base)
				   && host.accepted_snapshot()->MergeBaseAvailability == application::github_merge_base_availability::refused
				   && !host.accepted_snapshot()->MergeBaseOid,
			   "an answer proving the merge base stale is never overridden by the one before it");
		host.dispatch(core::refresh_pull_request_intent{ });
		answer(application::github_merge_base_availability::failed);
		expect(epochs.AcceptCount == 4U && from(recorded, core::comparison_base_basis::recorded_base),
			   "a failed read after a refusal brings no earlier merge base back");
		host.dispatch(core::refresh_pull_request_intent{ });
		answer(application::github_merge_base_availability::available);
		expect(epochs.AcceptCount == 5U && from(mergeBase, core::comparison_base_basis::merge_base), "a merge base named again is the comparison base again");
	}

	void test_loading_budget_shape()
	{
		using shape         = application::accepted_pull_request_shape;
		using route         = application::pull_request_object_loading;
		const auto classify = [](std::optional<shape> aShape) {
			return application::object_loading_for(aShape);
		};
		expect(classify(std::nullopt) == route::complete, "missing shape grants no lazy acquisition");
		expect(classify(shape{ 100U, 100U, 100U }) == route::selected_objects, "PR counts cannot bound the repository closure");
		expect(classify(shape{ 101U, 1U, 1U }) == route::selected_objects, "a second file page permits bounded selected objects");
		expect(classify(shape{ 1U, 101U, 101U }) == route::selected_objects, "a second commit page permits bounded selected objects");
		expect(classify(shape{ 3000U, 250U, 1000U }) == route::selected_objects, "reported total remains distinct from capped returned commits");
		expect(classify(shape{ 101U, 1U, std::nullopt }) == route::complete, "missing reported count grants no lazy acquisition");
		expect(classify(shape{ 101U, 1U, 2U }) == route::complete, "inconsistent count grants no lazy acquisition");
		expect(classify(shape{ 3001U, 1U, 1U }) == route::complete, "file limit violation grants no lazy acquisition");
		expect(classify(shape{ 101U, 251U, 251U }) == route::complete, "commit limit violation grants no lazy acquisition");
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

	/// [[BPR-1338]]. Open on GitHub names the window's current publication; the
	/// host builds the page from the accepted authority and snapshot.
	void test_pull_request_page_admission()
	{
		struct browser final : application::device_authorization_browser_port
		{
			std::vector<std::string> Opened;
			bool                     Succeeds = true;
			auto                     open(const std::string_view aAddress) -> bool override
			{
				Opened.emplace_back(aAddress);
				return Succeeds;
			}
		} opener;
		reads                                                      provider;
		application::manual_executor                               lane;
		std::optional<application::github_pull_request_projection> projected;
		application::github_window_read_host                       host{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 1 } },
			core::application_session_id{ 2 },
			provider,
			lane,
			[&](const auto& aUpdate) { if (aUpdate.Presentation.PullRequest) { projected = aUpdate.Presentation.PullRequest; } }
		};
		const auto unpublished = core::markdown_context{ { core::default_bpr_profile_id, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 1 } }, core::application_session_id{ 2 }, core::github_identity_generation{ 1 }, core::provider_snapshot_id{ 1 }, 1 };
		expect(host.open_pull_request_page(unpublished, &opener) == core::markdown_link_result::stale && opener.Opened.empty(), "no accepted pull request, no page");
		expect(host.adopt(session()), "page fixture adopts accepted session");
		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 42 } });
		auto call = provider.Calls.back();
		auto data = snapshot(call.Request);
		// Remote text that looks like an address is never where the page comes from.
		data.PullRequest.Title = "https://evil.invalid/owner/repo/pull/42";
		data.PullRequest.Body  = "[x](https://evil.invalid/)";
		call.Complete(application::github_pull_request_success{ call.Request, data });
		lane.drain();
		expect(projected && projected->MarkdownContext && opener.Opened.empty(), "acceptance opens nothing");
		const auto current = *projected->MarkdownContext;
		expect(host.open_pull_request_page(current, &opener) == core::markdown_link_result::opened && opener.Opened.size() == 1
				   && opener.Opened.back() == "https://github.com/owner/repo/pull/42",
			   "the page is the canonical address from the accepted hostname, repository and number");
		expect(host.open_pull_request_page(current, nullptr) == core::markdown_link_result::unavailable && opener.Opened.size() == 1, "no browser capability, no page");
		auto foreign          = current;
		foreign.Window.Window = core::bpr_window_id{ 99 };
		expect(host.open_pull_request_page(foreign, &opener) == core::markdown_link_result::stale && opener.Opened.size() == 1, "another window cannot borrow this page");
		opener.Succeeds = false;
		expect(host.open_pull_request_page(current, &opener) == core::markdown_link_result::failed, "a browser failure is explicit");
		opener.Succeeds = true;
		host.dispatch(core::refresh_pull_request_intent{ });
		call = provider.Calls.back();
		call.Complete(application::github_pull_request_success{ call.Request, snapshot(call.Request) });
		lane.drain();
		expect(host.open_pull_request_page(current, &opener) == core::markdown_link_result::stale && opener.Opened.size() == 2, "a new publication retires the old gesture");
		expect(host.open_pull_request_page(*projected->MarkdownContext, &opener) == core::markdown_link_result::opened && opener.Opened.back() == "https://github.com/owner/repo/pull/42", "the new publication opens the same page");
		host.dispatch(core::open_pull_request_intent{ { "owner", "other" }, core::pull_request_number{ 43 } });
		expect(host.open_pull_request_page(*projected->MarkdownContext, &opener) == core::markdown_link_result::stale && opener.Opened.size() == 3, "replacing the pull request revokes its page before the replacement arrives");
		host.shutdown();
		provider.Calls.back().Complete(application::github_pull_request_cancelled{ provider.Calls.back().Request });
		lane.drain();
	}

	/// [[BPR-1566]]. A named reference page opens at exactly its HTTPS address
	/// through the window's browser, and only there.
	void test_reference_page_opening()
	{
		struct browser final : application::device_authorization_browser_port
		{
			std::vector<std::string> Opened;
			bool                     Succeeds = true;
			bool                     Throws   = false;
			auto                     open(const std::string_view aAddress) -> bool override
			{
				if (Throws) {
					throw std::runtime_error{ "launch" };
				}
				Opened.emplace_back(aAddress);
				return Succeeds;
			}
		} opener;
		const auto page = core::reference_page::conventional_comments;
		expect(application::reference_page_address({ .Hostname = "conventionalcomments.org", .Path = "/" }) == "https://conventionalcomments.org/", "the address is HTTPS, the host and the path");
		expect(application::open_reference_page(page, &opener) == core::markdown_link_result::opened && opener.Opened == std::vector<std::string>{ "https://conventionalcomments.org/" },
			   "Conventional Comments opens at exactly its site's root, once");
		expect(application::open_reference_page(page, nullptr) == core::markdown_link_result::unavailable && opener.Opened.size() == 1U, "no browser capability, no page");
		expect(application::open_reference_page(static_cast<core::reference_page>(9), &opener) == core::markdown_link_result::refused && opener.Opened.size() == 1U,
			   "a value that names no page reaches no browser");
		opener.Succeeds = false;
		expect(application::open_reference_page(page, &opener) == core::markdown_link_result::failed, "a browser that declines is a failure");
		opener.Throws = true;
		expect(application::open_reference_page(page, &opener) == core::markdown_link_result::failed && opener.Opened.size() == 2U, "and so is one that throws");
	}

	void test_markdown_link_admission()
	{
		struct browser final : application::device_authorization_browser_port
		{
			std::vector<std::string> Opened;
			bool                     Succeeds = true;
			auto                     open(const std::string_view aAddress) -> bool override
			{
				Opened.emplace_back(aAddress);
				return Succeeds;
			}
		} opener;
		reads                                                      provider;
		application::manual_executor                               lane;
		std::optional<application::github_pull_request_projection> projected;
		application::github_window_read_host                       host{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 1 } },
			core::application_session_id{ 2 },
			provider,
			lane,
			[&](const auto& aUpdate) { if (aUpdate.Presentation.PullRequest) { projected = aUpdate.Presentation.PullRequest; } }
		};
		expect(host.adopt(session()), "Markdown fixture adopts accepted session");
		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 42 } });
		auto call             = provider.Calls.back();
		auto data             = snapshot(call.Request);
		data.PullRequest.Body = "# Original\r\n\r\n[link](./guide.md)\r\n";
		auto check            = application::github_check_record{ };
		check.Check           = application::github_check_id{ "details" };
		check.Name            = "Branch Freshness";
		check.Status          = application::github_check_status::completed;
		check.Conclusion      = application::github_check_conclusion::failure;
		check.DetailsUrl      = "https://ci.example.invalid/result";
		data.Checks.push_back(std::move(check));
		call.Complete(application::github_pull_request_success{ call.Request, data });
		lane.drain();
		expect(projected && projected->MarkdownContext && projected->Body == data.PullRequest.Body
				   && host.accepted_snapshot()->PullRequest.Body == data.PullRequest.Body && opener.Opened.empty(),
			   "acceptance preserves exact source and performs no browser effect");
		const auto old     = *projected->MarkdownContext;
		const auto gesture = core::markdown_link_intent{ old, { }, "./guide.md" };
		expect(host.open_markdown_link(gesture, &opener) == core::markdown_link_result::opened
				   && opener.Opened.back() == "https://github.com/owner/repo/blob/2222222222222222222222222222222222222222/guide.md",
			   "current explicit gesture reaches browser with exact accepted revision");
		expect(host.open_markdown_link({ old, "absent", "https://example.invalid" }, &opener) == core::markdown_link_result::stale, "absent body cannot lend navigation identity");
		expect(host.open_markdown_link(gesture, nullptr) == core::markdown_link_result::unavailable, "construction cut disables browser capability");
		expect(host.open_markdown_link({ old, { }, "file:///tmp/secret" }, &opener) == core::markdown_link_result::refused && opener.Opened.size() == 1, "hostile scheme never reaches browser");
		for (const auto* address : { "https://user:secret@example.invalid", "https://example.invalid/?access_token=secret", "javascript:alert(1)", "//ambiguous.invalid/a", "../outside.md" }) {
			expect(host.open_markdown_link({ old, { }, address }, &opener) == core::markdown_link_result::refused && opener.Opened.size() == 1, "credential-bearing and ambiguous destinations never reach the public host port");
		}
		auto foreign          = old;
		foreign.Window.Window = core::bpr_window_id{ 99 };
		expect(host.open_markdown_link({ foreign, { }, "https://example.invalid" }, &opener) == core::markdown_link_result::stale && opener.Opened.size() == 1, "another window cannot borrow the current body's navigation authority");
		foreign                   = old;
		foreign.Window.Generation = core::bpr_window_generation{ 99 };
		expect(host.open_markdown_link({ foreign, { }, "https://example.invalid" }, &opener) == core::markdown_link_result::stale && opener.Opened.size() == 1, "a replaced window generation cannot borrow body authority");
		browser    checkOpener;
		const auto checkGesture = core::markdown_link_intent{ old, "check:details", "https://ci.example.invalid/result" };
		expect(host.open_markdown_link(checkGesture, &checkOpener) == core::markdown_link_result::opened && checkOpener.Opened.size() == 1, "accepted exact check destination uses the existing host browser admission");
		expect(host.open_markdown_link({ old, "check:details", "https://different.invalid" }, &checkOpener) == core::markdown_link_result::stale && checkOpener.Opened.size() == 1, "check identity cannot lend authority to a substituted destination");
		host.dispatch(core::refresh_pull_request_intent{ });
		call = provider.Calls.back();
		data = snapshot(call.Request); // deliberately reuses provider snapshot id
		call.Complete(application::github_pull_request_success{ call.Request, data });
		lane.drain();
		expect(projected->MarkdownContext != old && host.open_markdown_link(gesture, &opener) == core::markdown_link_result::stale, "fresh body publication invalidates gestures even if provider reuses snapshot id");
		expect(host.open_markdown_link(checkGesture, &checkOpener) == core::markdown_link_result::stale && checkOpener.Opened.size() == 1, "old check result loses navigation authority on accepted refresh");
		opener.Succeeds = false;
		expect(host.open_markdown_link({ *projected->MarkdownContext, { }, "http://example.invalid/doc" }, &opener) == core::markdown_link_result::failed, "browser failure remains explicit");
		host.dispatch(core::open_pull_request_intent{ { "owner", "other" }, core::pull_request_number{ 43 } });
		expect(host.open_markdown_link({ *projected->MarkdownContext, { }, "https://example.invalid" }, &opener) == core::markdown_link_result::stale, "authority replacement revokes current links before replacement completion");
		const auto pending = provider.Calls.back();
		call.Complete(application::github_pull_request_success{ call.Request, data });
		lane.drain();
		expect(!host.accepted_snapshot(), "late prior completion cannot restore body authority");
		host.shutdown();
		pending.Complete(application::github_pull_request_cancelled{ pending.Request });
		lane.drain();
		expect(host.is_quiescent() && host.open_markdown_link(gesture, &opener) == core::markdown_link_result::stale, "shutdown refuses gestures and drains terminals");
	}

}
namespace {
	auto limited_error(bool aSecondary, application::github_rate_limit_resource aResource = application::github_rate_limit_resource::core) -> application::github_provider_error
	{
		auto rate  = application::github_rate_limit_record{ aResource, 5000, aSecondary ? 4999U : 0U, 105U, aSecondary };
		auto error = application::github_provider_error{ .Failure = application::github_provider_failure::rate_limit, .RetryAfterSeconds = 2, .RateLimitCause = rate };
		expect(application::observe(error.RateLimits, rate), "valid typed rate cause");
		return error;
	}

	void test_shared_backoff_lifecycle()
	{
		auto                                                now       = core::github_read_time{ 1000, 100 };
		auto                                                admission = std::make_shared<application::github_read_admission>([&] { return now; });
		reads                                               provider;
		application::manual_executor                        lane;
		std::vector<application::github_window_read_update> updates;
		application::github_window_read_host                first{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 1 } }, core::application_session_id{ 2 }, provider, lane, [&](const auto& aUpdate) { updates.push_back(aUpdate); }, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, admission
		};
		application::github_window_read_host second{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 3 }, core::bpr_window_generation{ 1 } }, core::application_session_id{ 3 }, provider, lane, [](const auto&) { }, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, admission
		};
		auto identity = session();
		expect(first.adopt(identity) && second.adopt(identity), "two windows share accepted session and admission");
		const auto open = core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 1 } };
		first.dispatch(open);
		auto call = provider.Calls.back();
		call.Complete(application::github_pull_request_success{ call.Request, snapshot(call.Request) });
		lane.drain();
		first.dispatch(core::refresh_pull_request_intent{ });
		call = provider.Calls.back();
		call.Complete(application::github_pull_request_error{ call.Request, limited_error(false) });
		lane.drain();
		expect(provider.Calls.size() == 2 && first.is_quiescent() && first.accepted_snapshot(), "primary refusal schedules zero automatic attempts and retains snapshot");
		for (int i = 0; i < 10; ++i) {
			first.dispatch(core::refresh_pull_request_intent{ });
			second.dispatch(open);
		}
		expect(provider.Calls.size() == 2 && second.is_quiescent(), "Refresh and sibling Open cannot bypass primary reset");
		expect(updates.back().Presentation.HostDiagnostics.back().RetryAfterSeconds == 5U, "blocked explicit retry projects actual remaining primary delay");
		first.cancel();
		now.MonotonicMilliseconds = 5999;
		second.dispatch(open);
		expect(provider.Calls.size() == 2, "one millisecond before deadline remains refused");
		now.MonotonicMilliseconds = 6000;
		lane.drain();
		expect(provider.Calls.size() == 2, "deadline expiration alone starts no work");
		second.dispatch(open);
		expect(provider.Calls.size() == 3, "explicit retry at deadline gets one request");
		call = provider.Calls.back();
		second.cancel();
		call.Complete(application::github_pull_request_error{ call.Request, limited_error(true) });
		lane.drain();
		expect(second.is_quiescent() && !second.accepted_snapshot(), "cancelled request's late rate failure cannot publish");
		second.dispatch(open);
		expect(provider.Calls.size() == 4, "late cancelled refusal does not change current admission");
		call = provider.Calls.back();
		expect(second.retire(identity), "retire exact request authority");
		auto replacement                                 = identity;
		replacement.Generation                           = core::github_identity_generation{ 2 };
		replacement.Session.Viewer.Account               = core::github_account_id{ 2 };
		replacement.Session.Viewer.Node                  = application::github_actor_id{ "other-actor" };
		replacement.CapabilityEvidence.Authority.Account = replacement.Session.Viewer.Account;
		expect(second.adopt(replacement), "replacement account adopts independently");
		call.Complete(application::github_pull_request_success{ call.Request, snapshot(call.Request) });
		lane.drain();
		expect(!second.accepted_snapshot(), "old account success cannot publish after retirement");
		admission->observe(identity.ProfileAuthority, identity.Session.Viewer, limited_error(true));
		second.dispatch(open);
		expect(provider.Calls.size() == 5, "other account does not inherit cooldown");
		call = provider.Calls.back();
		second.cancel();
		call.Complete(application::github_pull_request_cancelled{ call.Request });
		lane.drain();
		expect(first.retire(identity), "retire rate-limited session");
		identity.Generation = core::github_identity_generation{ 2 };
		expect(first.adopt(identity), "reauthenticate same account with new authority");
		first.dispatch(open);
		expect(provider.Calls.size() == 5, "same account reauthentication cannot erase cooldown");
		now.MonotonicMilliseconds += 2000;
		first.dispatch(open);
		call = provider.Calls.back();
		// Existing transient retry policy is also bounded: exactly three attempts.
		for (unsigned attempt = 1; attempt <= application::maximum_github_provider_attempts; ++attempt) {
			call = provider.Calls.back();
			expect(call.Request.Attempt.value() == attempt, "transient retry carries bounded attempt identity");
			call.Complete(application::github_pull_request_error{ call.Request, { .Failure = application::github_provider_failure::timeout } });
			lane.drain();
		}
		expect(provider.Calls.size() == 8 && first.is_quiescent(), "transient failure stops at three attempts");
		first.dispatch(open);
		call = provider.Calls.back();
		admission->observe(identity.ProfileAuthority, identity.Session.Viewer, limited_error(true));
		call.Complete(application::github_pull_request_error{ call.Request, { .Failure = application::github_provider_failure::timeout } });
		lane.drain();
		expect(provider.Calls.size() == 9 && first.is_quiescent(), "transient automatic retry rechecks a sibling's newly observed rate minimum");

		first.shutdown();
		second.shutdown();
		expect(first.is_quiescent() && second.is_quiescent(), "all cancelled/retired terminals drain");
	}

	void test_resource_and_account_admission()
	{
		auto                               now = core::github_read_time{ 0, 100 };
		application::github_read_admission admission{ [&] { return now; } };
		const auto                         identity = session();
		const auto&                        profile  = identity.ProfileAuthority;
		const auto&                        viewer   = identity.Session.Viewer;
		admission.observe(profile, viewer, limited_error(true, application::github_rate_limit_resource::graphql));
		expect(!admission.refusal(profile, viewer, false) && admission.refusal(profile, viewer, true), "GraphQL scope gates only PR shapes that use GraphQL");
		auto otherProfile     = profile;
		otherProfile.Hostname = application::github_canonical_hostname{ "ghe.example.invalid" };
		expect(!admission.refusal(otherProfile, viewer, true), "other host is independent");
		otherProfile         = profile;
		otherProfile.Profile = core::host_profile_id{ 99 };
		expect(admission.refusal(otherProfile, viewer, true).has_value(), "another connection to same host/account shares cooldown");
		admission.observe(profile, viewer, limited_error(true, application::github_rate_limit_resource::search));
		expect(admission.refusal(profile, viewer, false).has_value(), "search delay covers PR's ancillary search resource");
		now.MonotonicMilliseconds = 1999;
		admission.succeeded(profile, viewer, true);
		expect(admission.refusal(profile, viewer, true).has_value(), "older concurrent success cannot clear active cooldown");
		now.MonotonicMilliseconds = 2000;
		expect(!admission.refusal(profile, viewer, true), "all resource minimums expire exactly");
		application::github_read_admission bounded{ [&] { return now; } };
		for (unsigned index = 0; index < 256; ++index) {
			auto account = viewer;
			account.Node = application::github_actor_id{ std::to_string(index) };
			bounded.observe(profile, account, limited_error(true));
		}
		expect(bounded.refusal(profile, viewer, true)->Failure == application::github_provider_failure::response_limit, "full live registry fails closed for an unretained account");
		auto late              = limited_error(true);
		late.RetryAfterSeconds = 10;
		bounded.observe(profile, viewer, late);
		now.MonotonicMilliseconds += 2000;
		expect(bounded.refusal(profile, viewer, true)->Failure == application::github_provider_failure::response_limit, "late in-flight overflow preserves its longer minimum");
		now.MonotonicMilliseconds += 8000;
		expect(!bounded.refusal(profile, viewer, true), "bounded registry recovers after overflow minimum");
	}
	void test_merge_observation_freshness()
	{
		reads                                provider;
		application::manual_executor         lane;
		application::github_window_read_host host{ { core::default_bpr_profile_id, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 1 } }, core::application_session_id{ 2 }, provider, lane, [](const auto&) { } };
		expect(host.adopt(session()), "optional merge course adopts exact account");
		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 1 } });
		auto call                           = provider.Calls.back();
		auto accepted                       = snapshot(call.Request);
		accepted.Merge.Availability         = application::github_observation_availability::available;
		accepted.Merge.Assessment           = application::github_merge_assessment::blocked;
		accepted.Merge.Review               = application::github_review_decision::required;
		accepted.ReviewRequestsAvailability = application::github_observation_availability::available;
		auto request                        = application::github_review_request_record{ };
		request.Request                     = "request";
		request.Actor                       = "team";
		request.Login                       = "owners";
		request.Organization                = "owner";
		request.AsCodeOwner                 = true;
		request.Kind                        = application::github_requested_actor_kind::team;
		accepted.ReviewRequests.push_back(request);
		call.Complete(application::github_pull_request_success{ call.Request, accepted });
		lane.drain();
		for (int repetition = 0; repetition < 2; ++repetition) {
			host.dispatch(core::refresh_pull_request_intent{ });
			call                                   = provider.Calls.back();
			auto unavailable                       = snapshot(call.Request);
			unavailable.Merge.Availability         = application::github_observation_availability::permission;
			unavailable.ReviewRequestsAvailability = application::github_observation_availability::incomplete;
			call.Complete(application::github_pull_request_success{ call.Request, unavailable });
			lane.drain();
			expect(host.accepted_snapshot()->Merge.Stale && host.accepted_snapshot()->Merge.Assessment == application::github_merge_assessment::blocked && host.accepted_snapshot()->ReviewRequestsStale && host.accepted_snapshot()->ReviewRequests.size() == 1, "repeated optional failure retains exact same-head block and roster as explicitly stale");
		}
		for (const auto empty : { false, true }) {
			host.dispatch(core::refresh_pull_request_intent{ });
			call                                 = provider.Calls.back();
			auto fallback                        = snapshot(call.Request);
			fallback.ReviewRequestsAvailability  = application::github_observation_availability::permission;
			fallback.ReviewRequestActorsObserved = true;
			if (!empty) {
				auto actor    = application::github_review_request_record{ };
				actor.Request = "rest:fresh-user";
				actor.Actor   = "fresh-user";
				actor.Login   = "new-reviewer";
				actor.Kind    = application::github_requested_actor_kind::user;
				fallback.ReviewRequests.push_back(actor);
			}
			call.Complete(application::github_pull_request_success{ call.Request, fallback });
			lane.drain();
			const auto& current = *host.accepted_snapshot();
			expect(!current.ReviewRequestsStale && current.ReviewRequestsAvailability == application::github_observation_availability::permission && current.ReviewRequests.size() == (empty ? 0U : 1U), "fresh complete REST fallback, including empty, replaces old code-owner requests while richer availability stays qualified");
			if (!empty) {
				expect(current.ReviewRequests.front().Login == "new-reviewer" && !current.ReviewRequests.front().AsCodeOwner, "current fallback actor never inherits a stale actor or code-owner origin");
			}
		}
		host.dispatch(core::refresh_pull_request_intent{ });
		call                               = provider.Calls.back();
		auto removed                       = snapshot(call.Request);
		removed.ReviewRequestsAvailability = application::github_observation_availability::available;
		removed.Merge.Availability         = application::github_observation_availability::available;
		removed.Merge.Review               = application::github_review_decision::approved;
		call.Complete(application::github_pull_request_success{ call.Request, removed });
		lane.drain();
		expect(host.accepted_snapshot()->ReviewRequests.empty() && !host.accepted_snapshot()->ReviewRequestsStale && !host.accepted_snapshot()->Merge.Stale, "accepted removal and approval replace retained stale requests");
		host.dispatch(core::refresh_pull_request_intent{ });
		const auto late = provider.Calls.back();
		host.cancel();
		lane.drain();
		host.dispatch(core::open_pull_request_intent{ { "owner", "repo" }, core::pull_request_number{ 2 } });
		late.Complete(application::github_pull_request_success{ late.Request, accepted });
		lane.drain();
		expect(!host.accepted_snapshot() || host.accepted_snapshot()->PullRequest.Number.value() != 2, "late merge facts cannot cross replacement identity");
		call                          = provider.Calls.back();
		auto different                = snapshot(call.Request);
		different.PullRequest.HeadOid = application::commit_oid{ std::string(40, '3') };
		call.Complete(application::github_pull_request_success{ call.Request, different });
		lane.drain();
		expect(host.accepted_snapshot()->PullRequest.Number.value() == 2 && host.accepted_snapshot()->ReviewRequests.empty() && !host.accepted_snapshot()->Merge.Stale, "replacement head never borrows stale requirements");
		host.shutdown();
		lane.drain();
		expect(host.is_quiescent(), "optional merge lifecycle quiesces");
	}

}

int main()
{
	test_merge_observation_freshness();
	test_durable_overview_lifecycle();
	test_markdown_link_admission();
	test_pull_request_page_admission();
	test_reference_page_opening();
	test_provisional_overview_is_bounded_and_never_authority();
	test_loading_budget_shape();
	test_shared_backoff_lifecycle();
	test_resource_and_account_admission();
	test_reentrant_read_publication();
	test_safe_read_presentation_matches_accepted_state();
	test_two_windows_have_distinct_diagnostic_courses();
	test_timeline_evidence_reaches_epoch_handoff_without_blocking_live_read();
	test_merge_base_carries_forward_only_across_failures();
	test_close_review_releases_the_read();
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
