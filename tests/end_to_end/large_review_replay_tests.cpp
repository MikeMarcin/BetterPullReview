#include "../../src/app/src/large_review_fixture.hpp"

#include "github_paged_read.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stop_token>
#include <string_view>
#include <utility>

#if !defined(_WIN32)
	#include <csignal>
	#include <string>
	#include <sys/resource.h>
	#include <unistd.h>
#endif

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;
	namespace fixture = bpr::app::detail::large_review_fixture;

	void require(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	class account_lease final : public application::github_account_transport_lease
	{ };

	[[nodiscard]] auto profile() -> validated_github_host_profile
	{
		const auto result = validate_github_host_profile(github_host_profile_candidate{
			.Profile        = core::host_profile_id{ 11U },
			.Host           = core::git_host_id{ 22U },
			.Revision       = application::github_profile_revision{ 3U },
			.Server         = application::github_server_contract::github_com_2022_11_28,
			.Authentication = application::github_authentication_transport::native_https,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::explicit_user,
			.Hostname       = "github.com",
			.RestRoot       = "https://api.github.com",
			.GraphqlRoot    = "https://api.github.com/graphql",
		});
		require(result.has_value(), "local fixture profile validates");
		return *result;
	}

	[[nodiscard]] auto provider_request() -> application::github_pull_request_provider_request
	{
		auto result             = application::github_pull_request_provider_request{ };
		result.Context          = { .Request = core::request_id{ 41U }, .ApplicationSession = core::application_session_id{ 7U }, .Profile = core::host_profile_id{ 11U }, .Host = core::git_host_id{ 22U }, .Account = core::github_account_id{ 800001U }, .Repository = { .Owner = "bpr-fixtures", .Name = "large-review" }, .Number = core::pull_request_number{ 17U }, .Capabilities = core::capability_revision{ 44U }, .IdentityGeneration = core::github_identity_generation{ 3U }, .Generation = core::request_generation{ 9U } };
		result.ProfileAuthority = { .Profile = result.Context.Profile, .ProfileRevision = application::github_profile_revision{ 3U }, .Host = result.Context.Host, .Hostname = application::github_canonical_hostname{ "github.com" }, .ServerContract = application::github_server_contract::github_com_2022_11_28, .Authentication = application::github_authentication_transport::native_https, .ProbeContractRevision = application::github_probe_contract_revision{ 1U } };
		result.Viewer           = { .Account = result.Context.Account, .Node = application::github_actor_id{ "SYNTHETIC_USER_800001" }, .Login = "fixture-reviewer", .DisplayName = "Fixture Reviewer" };
		const auto& authority   = result.ProfileAuthority;
		auto        observation = application::github_capability_probe_observation{
			.Authority = { .Profile = authority.Profile, .ProfileRevision = authority.ProfileRevision, .Host = authority.Host, .Account = result.Context.Account, .ServerContract = authority.ServerContract, .ProbeContractRevision = authority.ProbeContractRevision }, .ObservedAt = application::github_freshness_epoch{ 100U }
		};
		for (std::size_t index = 0U; index < observation.Facts.size(); ++index) {
			observation.Facts[index] = { .Capability = application::canonical_github_capabilities[index],
										 .State      = core::github_capability_state::supported,
										 .Source     = application::github_capability_evidence_source::observed_contract };
		}
		for (std::size_t index = 0U; index < observation.SchemaFacts.size(); ++index) {
			observation.SchemaFacts[index] = { .Capability = application::canonical_github_schema_capabilities[index],
											   .State      = core::github_capability_state::supported,
											   .Source     = application::github_capability_evidence_source::observed_contract };
		}
		result.CapabilityEvidence = { .Authority = observation.Authority, .Facts = observation.Facts, .SchemaFacts = observation.SchemaFacts, .Capabilities = application::projected_capabilities(observation), .ReadShape = application::projected_read_shape(observation), .Revision = result.Context.Capabilities, .ObservedAt = observation.ObservedAt, .ExpiresBefore = application::github_freshness_epoch{ 400U } };
		result.AccountTransport   = std::make_shared<account_lease>();
		result.Attempt            = application::github_provider_attempt{ 1U };
		return result;
	}

	class paged_transport final : public github_wire_transport_port
	{
	public:
		explicit paged_transport(std::vector<recorded_exchange> aExchanges)
			: Replay{ std::move(aExchanges) }
		{ }
		auto perform(const github_wire_request& aRequest, std::stop_token aStop) -> github_wire_completion override
		{
			require(!aStop.stop_requested(), "fixture unexpectedly cancelled");
			const auto operation = aRequest.Wire.Operation;
			if (std::ranges::find(requirement_operations, operation) != requirement_operations.end()) {
				require(Calls == 29U && RequirementsRefused < requirement_operations.size()
							&& operation == requirement_operations[RequirementsRefused],
						"optional requirement reads follow the complete snapshot in exact order");
				auto semantic = fixture::request(operation);
				if (operation == read_operation::list_branch_rules || operation == read_operation::get_branch_protection) {
					semantic.PullRequestNumber = 0U;
					semantic.BranchRef         = "main";
				}
				const auto expected = compile_read_request(semantic);
				require(expected && *expected == aRequest.Wire, "optional refusal validates the exact canonical request");
				++RequirementsRefused;
				return github_wire_error{ .Correlation = aRequest.Correlation,
										  .Error       = { .Failure = operation == read_operation::list_requested_reviewers
															   ? application::github_provider_failure::unavailable
															   : application::github_provider_failure::unsupported_capability } };
			}
			++Calls;
			LastOperation = aRequest.Wire.Operation;
			auto response = Replay.perform(aRequest.Wire);
			require(response.has_value(), "production pager requested a different head, page or cursor");
			// The app fixture's transport adds the same resource-scoped headers.
			const auto search  = aRequest.Wire.Operation == read_operation::search_pull_requests;
			const auto graphql = aRequest.Wire.Operation == read_operation::list_review_threads;
			response->Headers.push_back({ "X-RateLimit-Limit", search ? "30" : "5000" });
			response->Headers.push_back({ "X-RateLimit-Remaining", search ? "29" : "4999" });
			response->Headers.push_back({ "X-RateLimit-Reset", "2000000000" });
			response->Headers.push_back({ "X-RateLimit-Resource", search ? "search" : graphql ? "graphql"
																							  : "core" });
			return github_wire_success{ aRequest.Correlation, std::move(*response) };
		}
		fixture::sequential_replay Replay;
		std::size_t                Calls               = 0U;
		std::size_t                RequirementsRefused = 0U;
		read_operation             LastOperation       = read_operation::get_viewer_identity;

	private:
		static constexpr auto requirement_operations = std::array{
			read_operation::get_merge_assessment,
			read_operation::list_review_requests,
			read_operation::list_requested_reviewers,
			read_operation::list_branch_rules,
			read_operation::get_branch_protection,
		};
	};

	void test_all_epoch_snapshots()
	{
		const auto  manifest        = fixture::read_json(std::filesystem::path{ BPR_LARGE_REVIEW_FIXTURE_ROOT } / "manifest.json");
		const auto  epochs          = fixture::read_json(std::filesystem::path{ BPR_LARGE_REVIEW_FIXTURE_ROOT } / "epoch-deltas.json");
		const auto  selectedProfile = profile();
		const auto  request         = provider_request();
		std::size_t headIndex       = 0U;
		for (const auto& headValue : manifest["heads"].get_array()) {
			const auto& head      = headValue.get_string();
			auto        exchanges = fixture::load(BPR_LARGE_REVIEW_FIXTURE_ROOT, head);

			std::size_t fileIndex = 0U;
			for (const auto& exchange : exchanges) {
				require(validate_outbound_read_request(exchange.Request).has_value(), "epoch wire request is canonical");
				if (exchange.Request.Operation != read_operation::list_pull_request_files) {
					continue;
				}
				glz::generic body;
				require(!glz::read_json(body, exchange.Response.Body), "epoch files decode");
				for (const auto& file : body.get_array()) {
					const auto& entry        = manifest["files"].get_array().at(fileIndex++);
					const auto& path         = entry["path"].get_string();
					const auto* expectedBlob = &entry["head"];
					// The recipe changes source N once, in head N+2. This direct
					// ordinal rule is independent of the compiler's reverse replay.
					if (path.starts_with("src/file_")) {
						const auto source = std::stoul(path.substr(9U, 4U));
						if (source < 31U) {
							expectedBlob = &epochs["deltas"].get_array().at(source)["files"].get_array().front()[headIndex > source ? "after" : "before"];
						}
					}
					require(file["filename"].get_string() == entry["path"].get_string()
								&& file["sha"].get_string() == (*expectedBlob)["oid"].get_string(),
							"REST file identities match the stored epoch delta");
				}
			}
			require(fileIndex == 515U, "epoch REST file inventory is complete");
			// Session establishment consumes /user before the PR pager.
			exchanges.erase(exchanges.begin());
			paged_transport transport{ std::move(exchanges) };
			const auto      normalized = perform_paged_github_read(transport, selectedProfile, request, core::provider_snapshot_id{ 1U }, { });
			if (!normalized) {
				std::cerr << "epoch=" << headIndex + 1U << " calls=" << transport.Calls << " operation=" << static_cast<int>(transport.LastOperation);
				if (const auto* error = std::get_if<application::github_provider_error>(&normalized.error())) {
					std::cerr << " failure=" << static_cast<int>(error->Failure);
				}
				std::cerr << '\n';
			}
			require(normalized.has_value(), "every stored head normalizes through the production pager");
			using observation = application::github_observation_availability;
			require(transport.RequirementsRefused == 5U
						&& normalized->Merge.Availability == observation::unsupported
						&& normalized->ReviewRequestsAvailability == observation::unsupported
						&& !normalized->ReviewRequestActorsObserved
						&& normalized->BranchPolicy.RulesAvailability == observation::unsupported
						&& normalized->BranchPolicy.ProtectionAvailability == observation::unsupported,
					"refused optional surfaces remain explicitly unsupported without invented actors or policy");
			require(!transport.Replay.consumed_all() && transport.Calls == 29U,
					"ordinary PR read leaves exactly the independent timeline request");
			const auto timeline = perform_paged_github_timeline_read(transport, selectedProfile, request, core::repository_id{ 800002U }, { });
			require(timeline && timeline->ForcePushEvents.empty() && transport.Calls == 30U
						&& transport.LastOperation == read_operation::list_pull_request_timeline,
					"all stored heads serve the exact complete empty synthetic timeline through its production pager");
			require(transport.Replay.consumed_all() && normalized->PullRequest.HeadOid.value() == head
						&& normalized->PullRequest.BaseOid.value() == fixture::base_oid
						&& normalized->Files.size() == 515U && normalized->ReviewThreads.size() == 1000U
						&& normalized->Commits.size() == 1U && normalized->Commits.front().Oid.value() == head,
					"production pager exhausts the exact epoch and preserves complete typed counts and commit identity");
			++headIndex;
		}
		for (const auto& invalid : { std::string{ }, std::string{ fixture::base_oid }, std::string{ fixture::head_oid.substr(0U, 12U) }, std::string(40U, '0') }) {
			auto refused = false;
			try {
				(void)fixture::load(BPR_LARGE_REVIEW_FIXTURE_ROOT, invalid);
			} catch (const std::exception&) {
				refused = true;
			}
			require(refused, "unknown, empty, base and abbreviated head identities refuse");
		}
		for (const auto mutation : { 0, 1, 2 }) {
			auto changed = epochs;
			if (mutation == 0) {
				changed["deltas"].get_array().front()["head"] = fixture::head_oid;
			}
			if (mutation == 1) {
				changed["deltas"].get_array().front()["files"].get_array().front()["after"]["oid"] = std::string(40U, '0');
			}
			if (mutation == 2) {
				changed["deltas"].get_array().pop_back();
			}
			auto refused = false;
			try {
				(void)fixture::manifest_for_head(manifest, changed, manifest["heads"].get_array().front().get_string());
			} catch (const std::exception&) {
				refused = true;
			}
			require(refused, "substituted chain, delta source and missing epoch refuse");
		}
	}
}

#if !defined(_WIN32)
extern "C" void large_review_replay_processor_limit_reached(int /*aSignal*/)
{
	static const char message[] = "LARGE_REVIEW_REPLAY_CPU_LIMIT: the contract used its processor-time budget\n";
	// GCC keeps glibc's warn_unused_result through a void cast, so the result
	// is tested; either way the process ends here.
	if (write(STDERR_FILENO, message, sizeof message - 1U) < 0) {
		_exit(EXIT_FAILURE);
	}
	_exit(EXIT_FAILURE);
}

namespace {
	/// [[BPR-1598]]. The row's run budget, in processor time: on a loaded
	/// machine this one thread of work takes several times as long in wall
	/// time, while a change that makes it do more work still uses more
	/// processor time. Past the limit the kernel raises SIGXCPU, and the
	/// handler names the budget before the process ends.
	void limit_processor_time(std::string_view aArgument)
	{
		constexpr auto prefix = std::string_view{ "--processor-seconds=" };
		require(aArgument.starts_with(prefix), "usage: --processor-seconds=N");
		const auto seconds = static_cast<rlim_t>(std::stoul(std::string{ aArgument.substr(prefix.size()) }));
		require(seconds > 0U, "the processor-time budget is positive");
		require(std::signal(SIGXCPU, large_review_replay_processor_limit_reached) != SIG_ERR, "the processor-time handler is installed");
		auto limit = rlimit{ };
		require(::getrlimit(RLIMIT_CPU, &limit) == 0, "the processor-time limit is readable");
		limit.rlim_cur = limit.rlim_max == RLIM_INFINITY ? seconds : std::min(seconds, limit.rlim_max);
		require(::setrlimit(RLIMIT_CPU, &limit) == 0, "the processor-time budget is set");
	}
}
#endif

auto main(int argc, char** argv) -> int
{
	if (argc == 2) {
#if defined(_WIN32)
		static_cast<void>(argv);
		require(false, "Windows has no processor-time budget; the row's timeout is its budget");
#else
		limit_processor_time(argv[1]);
#endif
	} else {
		require(argc == 1, "usage: [--processor-seconds=N]");
	}
	const auto exchanges = fixture::load(BPR_LARGE_REVIEW_FIXTURE_ROOT);
	require(exchanges.size() == 31U, "exact recorded course size");
	const auto& timeline = exchanges.back();
	require(timeline.Request.Operation == read_operation::list_pull_request_timeline
				&& timeline.Request.Path == "/repos/bpr-fixtures/large-review/issues/17/timeline"
				&& timeline.Request.Query == "page=1&per_page=100"
				&& timeline.Response.Body == "[]" && timeline.Response.Completeness == page_completeness::complete,
			"independent exact timeline path, pagination and empty event body");
	fixture::sequential_replay replay{ exchanges };
	require(!replay.perform(timeline.Request), "timeline cannot bypass unconsumed PR pages");
	for (std::size_t index = 0; index < exchanges.size(); ++index) {
		const auto& exchange = exchanges[index];
		require(validate_outbound_read_request(exchange.Request).has_value(), "recorded request must be canonical");
		if (index == 3U) {
			require(exchange.Response.Completeness == page_completeness::has_next_page, "first files page must require continuation");
			strict_recorded_transport legacy{ { exchange } };
			require(!legacy.perform(exchange.Request), "legacy validator must still reject a partial page");
			require(!replay.perform(exchanges[index + 1U].Request), "skipping REST page must refuse without advancing");
		}
		if (index == 12U) {
			auto semantic = exchange.Request.Semantic;
			require(semantic.GraphqlPage.has_value(), "continuation is GraphQL");
			semantic.GraphqlPage->AfterCursor = "files-wrong";
			const auto other                  = compile_read_request(semantic);
			require(other.has_value() && !replay.perform(*other), "another canonical cursor must refuse without advancing");
			auto damaged = exchange.Request;
			damaged.Body += " ";
			require(!replay.perform(damaged), "noncanonical body must refuse without advancing");
		}
		const auto response = replay.perform(exchange.Request);
		require(response.has_value() && *response == exchange.Response, "exact request returns its exact recorded response");
	}
	require(replay.consumed_all() && !replay.perform(exchanges.front().Request), "exhaustion cannot serve another course");
	const auto headWant           = "0032want " + std::string{ fixture::head_oid } + "\n";
	const auto baseWant           = "0032want " + std::string{ fixture::base_oid } + "\n";
	const auto unknownWant        = "0032want " + std::string(40U, 'f') + "\n";
	const auto disconnectedWant   = std::string{ "0032want 67c9afbd86e28b19e6bd224f966b4a48e1294e6a\n" };
	auto       duplicateBaseWants = baseWant;
	duplicateBaseWants.append(baseWant).append(headWant);
	require(fixture::exact_history_fetch_wants(headWant, fixture::base_oid, fixture::head_oid)
				&& fixture::exact_history_fetch_wants(baseWant + headWant, fixture::base_oid, fixture::head_oid),
			"historical fetch admits exact head and optional exact base");
	for (const auto& body : { std::string{ }, baseWant, unknownWant, headWant + unknownWant, disconnectedWant, headWant + disconnectedWant, headWant + headWant,
							  duplicateBaseWants, headWant.substr(0U, headWant.size() - 1U) }) {
		require(!fixture::exact_history_fetch_wants(body, fixture::base_oid, fixture::head_oid),
				"historical fetch refuses missing, substituted, additional unknown, duplicate and incomplete wants");
	}
	test_all_epoch_snapshots();
	std::cout << "LARGE_REVIEW_REPLAY_CONTRACT_OK\n";
	return EXIT_SUCCESS;
}
