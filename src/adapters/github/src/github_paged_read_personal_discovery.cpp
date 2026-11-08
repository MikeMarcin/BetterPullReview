#include "bpr/application/review_queue_facts.hpp"
#include "bpr/core/home_discovery_plan.hpp"
#include "github_http_helpers.hpp"
#include "github_paged_read.hpp"
#include "github_paged_read_page_wire.hpp"
#include "github_paged_read_record_parsing.hpp"
#include "github_review_comment_grouping.hpp"

#include <QString>

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail {
	namespace {
		/// [[BPR-0988]]. The authenticated half of `provider_request_valid`,
		/// for a read scoped to a repository rather than a pull request.
		[[nodiscard]] auto discovery_request_valid(
			const validated_github_host_profile&           aProfile,
			const application::personal_discovery_request& aRequest,
			const github_paged_read_limits&                aLimits) noexcept -> bool
		{
			const auto& context     = aRequest.Context;
			const auto& authority   = aRequest.ProfileAuthority;
			const auto& evidence    = aRequest.CapabilityEvidence;
			const auto  observation = application::github_capability_probe_observation{
				.Authority   = evidence.Authority,
				.ObservedAt  = evidence.ObservedAt,
				.Facts       = evidence.Facts,
				.SchemaFacts = evidence.SchemaFacts,
			};
			const auto reason = aRequest.Reason == application::personal_discovery_reason::retained
				? aRequest.Number && aRequest.Number->value() != 0U && aRequest.Number->value() <= core::maximum_pull_request_number
				: (aRequest.Reason == application::personal_discovery_reason::authored
				   || aRequest.Reason == application::personal_discovery_reason::requested)
					&& !aRequest.Number;
			// [[BPR-1680]]. A search names its context's repository first and
			// any further ones in order, once each, inside the core's bound; a
			// retained read names none.
			const auto& repositories      = aRequest.Repositories;
			const auto  repositoriesValid = repositories.empty()
				|| (aRequest.Reason != application::personal_discovery_reason::retained && repositories.size() <= core::maximum_discovery_search_repositories
					&& repositories.front() == context.Repository && std::ranges::all_of(repositories, [](const core::repository_locator& aRepository) { return core::valid(aRepository); })
					&& std::ranges::adjacent_find(repositories, std::greater_equal<>{ }) == repositories.end());
			// [[BPR-1680]]. Its known pull requests are of its repositories,
			// valid, in order and once each.
			const auto& known      = aRequest.Known;
			const auto  knownValid = known.empty()
				|| (aRequest.Reason != application::personal_discovery_reason::retained && known.size() <= application::maximum_personal_discovery_retained
					&& std::ranges::all_of(known, [&context, &repositories](const application::personal_discovery_known& aKnown) {
						   const auto named = repositories.empty() ? aKnown.Repository == context.Repository : std::ranges::binary_search(repositories, aKnown.Repository);
						   return named && aKnown.Number.value() != 0U && aKnown.Number.value() <= core::maximum_pull_request_number;
					   })
					&& std::ranges::adjacent_find(known, std::greater_equal<>{ }) == known.end());
			// [[BPR-1680]]. A batched retained read names two or more pull
			// requests, ascending, the first its `Number`, inside the core's bound.
			const auto& numbers      = aRequest.Numbers;
			const auto  numbersValid = numbers.empty()
				|| (aRequest.Reason == application::personal_discovery_reason::retained && numbers.size() >= 2U
					&& numbers.size() <= core::maximum_discovery_batch_pull_requests && aRequest.Number == numbers.front()
					&& std::ranges::all_of(numbers, [](const core::pull_request_number aNumber) { return aNumber.value() != 0U && aNumber.value() <= core::maximum_pull_request_number; })
					&& std::ranges::adjacent_find(numbers, std::greater_equal<>{ }) == numbers.end());
			return reason && repositoriesValid && knownValid && numbersValid && limits_valid(aLimits) && application::detail::valid_context(context)
				&& (!aRequest.AuthenticationSession || aRequest.AuthenticationSession->value() != 0U)
				&& application::valid(authority) && aRequest.AccountTransport != nullptr
				&& aRequest.Viewer.Account.value() != 0U && aRequest.Viewer.Account == context.Account
				&& bounded_text(aRequest.Viewer.Login, aLimits.Records.MaximumLoginBytes, true)
				&& authority.Profile == context.Profile && authority.Profile == aProfile.profile()
				&& authority.ProfileRevision == aProfile.revision()
				&& authority.Host == context.Host && authority.Host == aProfile.host()
				&& authority.Hostname.value() == aProfile.hostname()
				&& authority.ServerContract == aProfile.server_contract()
				&& authority.Authentication == aProfile.authentication_transport()
				&& application::valid(observation)
				&& evidence.Authority.Profile == authority.Profile
				&& evidence.Authority.ProfileRevision == authority.ProfileRevision
				&& evidence.Authority.Host == authority.Host
				&& evidence.Authority.Account == context.Account
				&& evidence.Authority.ServerContract == authority.ServerContract
				&& evidence.Authority.ProbeContractRevision == authority.ProbeContractRevision
				&& evidence.Revision == context.Capabilities
				&& evidence.ExpiresBefore > evidence.ObservedAt
				&& evidence.ExpiresBefore.value() - evidence.ObservedAt.value()
				<= application::maximum_github_capability_freshness_window
				&& evidence.Capabilities == application::projected_capabilities(observation)
				&& evidence.ReadShape == application::projected_read_shape(observation);
		}

		/// [[BPR-0988]]. One search item, checked against the exact repository
		/// the read is scoped to. The name compares without case: a saved
		/// repository may be written in another case than GitHub's. A search
		/// of several repositories ([[BPR-1680]]) attributes the row to the one
		/// it names, as the request names it.
		[[nodiscard]] auto parse_discovery_item(const page_json_value&                    aValue,
												std::span<const core::repository_locator> aRepositories,
												const validated_github_host_profile&      aProfile,
												const github_paged_read_limits&           aLimits)
			-> std::expected<application::personal_discovery_row, github_paged_read_failure>
		{
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue), { "number", "pull_request", "repository_url", "state", "title", "user" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object      = json_object(aValue);
			const auto number      = uint32(json_member(object, "number"));
			const auto pullRequest = json_member(object, "pull_request");
			const auto user        = json_member(object, "user");
			const auto title       = text(json_member(object, "title"));
			const auto draft       = json_member(object, "draft");
			if (!number.has_value() || *number == 0U || *number > core::maximum_pull_request_number
				|| !json_is_string(json_member(object, "title")) || !bounded_text(title, aLimits.Records.MaximumTitleBytes)
				|| !json_is_object(pullRequest) || !one_of(json_member(object, "state"), { "open" })
				|| (has_required_keys(object, { "draft" }) && !json_is_bool(draft))
				|| !nonempty_string(json_member(object, "repository_url"))
				|| (!json_is_null(user) && (!json_is_object(user) || !nonempty_string(json_member(json_object(user), "login"))))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto url        = text(json_member(object, "repository_url"));
			const auto repository = std::ranges::find_if(aRepositories, [&aProfile, &url](const core::repository_locator& aRepository) {
				return application::detail::ascii_equal_fold(url, std::string{ aProfile.rest_root() } + "/repos/" + aRepository.Owner + "/" + aRepository.Name);
			});
			if (repository == aRepositories.end()) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto author = json_is_null(user) ? std::string{ } : text(json_member(json_object(user), "login"));
			if (!bounded_text(author, aLimits.Records.MaximumLoginBytes)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto row = application::personal_discovery_row{
				.Number = core::pull_request_number{ *number },
				.Title  = title,
				.Author = author,
				.Draft  = json_is_bool(draft) ? std::optional<bool>{ json_bool(draft) } : std::nullopt,
			};
			row.Repository = *repository;
			return row;
		}

		struct requested_reviewers
		{
			bool          NamesViewer = false;
			std::uint32_t Awaited     = 0U;
		};

		/// [[BPR-0988]]. The users and teams a pull request still awaits, and
		/// whether the viewer is one of the users.
		[[nodiscard]] auto parse_requested_reviewers(const parsed_page& aPage, const std::string_view aLogin, const github_paged_read_limits& aLimits)
			-> std::expected<requested_reviewers, github_paged_read_failure>
		{
			if (!singleton_complete(aPage) || !json_is_object(aPage.Document)
				|| !has_required_keys(json_object(aPage.Document), { "teams", "users" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aPage.Document);
			if (!json_is_array(json_member(object, "users")) || !json_is_array(json_member(object, "teams"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto users = json_array(json_member(object, "users"));
			const auto teams = json_array(json_member(object, "teams"));
			if (users.size() + teams.size() > aLimits.MaximumPageRecords) {
				return failed(application::github_provider_failure::response_limit);
			}
			auto result = requested_reviewers{ .Awaited = static_cast<std::uint32_t>(users.size() + teams.size()) };
			for (const auto& value : users) {
				if (!json_is_object(value) || !nonempty_string(json_member(json_object(value), "login"))) {
					return failed(application::github_provider_failure::malformed_response);
				}
				result.NamesViewer = result.NamesViewer || application::detail::ascii_equal_fold(text(json_member(json_object(value), "login")), aLogin);
			}
			if (!std::ranges::all_of(teams, [](const page_json_value& aValue) { return json_is_object(aValue); })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			return result;
		}

		[[nodiscard]] auto requested_reviewers_parent(const core::repository_request_context& aContext, const core::pull_request_number aNumber)
			-> core::pull_request_request_context
		{
			return {
				.Request             = aContext.Request,
				.ApplicationSession  = aContext.ApplicationSession,
				.Profile             = aContext.Profile,
				.Host                = aContext.Host,
				.Account             = aContext.Account,
				.Repository          = aContext.Repository,
				.Number              = aNumber,
				.ExpectedRepository  = std::nullopt,
				.ExpectedPullRequest = std::nullopt,
				.Capabilities        = aContext.Capabilities,
				.IdentityGeneration  = aContext.IdentityGeneration,
				.Generation          = core::request_generation{ 1U },
				.Mode                = core::github_access_mode::authenticated,
			};
		}

		/// [[BPR-1770]]. A bounded membership page carries its identity anchor
		/// alongside the first roster page. Later pages must repeat that exact
		/// repository/PR/revision, total, and state. No reviews or checks are read.
		[[nodiscard]] auto read_discovery_membership(
			github_wire_transport_port& aTransport, const validated_github_host_profile& aProfile,
			const application::personal_discovery_request& aRequest, const core::repository_request_context& aContext,
			const std::vector<std::uint32_t>& aNumbers, const server_contract aServer, const std::stop_token aStop,
			const github_paged_read_limits& aLimits, application::github_rate_limit_state& aRates, shared_home_budget& aBudget, std::uint32_t* aRecoveries = nullptr)
			-> std::expected<std::vector<requested_reviewers>, github_paged_read_failure>
		{
			auto limits                          = aLimits;
			limits.MaximumTotalPages             = std::min(limits.MaximumTotalPages, std::size_t{ 32 });
			limits.MaximumAggregateResponseBytes = std::min(limits.MaximumAggregateResponseBytes, std::size_t{ 8U * 1024U * 1024U });
			production_read_execution execution{ aTransport, aProfile, aContext, limits, aStop, aRates, aRequest.AuthenticationSession, nullptr, &aBudget };
			auto                      semantic = semantic_read_request{ .Server = aServer, .Operation = read_operation::list_home_review_membership, .Owner = aContext.Repository.Owner, .Repository = aContext.Repository.Name };
			semantic.PullRequestNumbers        = aNumbers;
			const auto first                   = execution.fetch(semantic);
			if (!first) {
				return std::unexpected{ first.error() };
			}
			const auto malformed  = [&execution] { return execution.terminal_failure(application::github_provider_failure::malformed_response); };
			const auto repository = [&](const parsed_page& aPage, const std::vector<std::uint32_t>& aAsked) -> std::optional<page_json_object> {
				if (!singleton_complete(aPage) || !json_is_object(aPage.Document) || !graphql_success_envelope(json_object(aPage.Document))) {
					return std::nullopt;
				}
				const auto data = json_member(json_object(aPage.Document), "data");
				if (!json_is_object(data) || !has_exact_keys(json_object(data), { "repository" })) {
					return std::nullopt;
				}
				const auto value = json_member(json_object(data), "repository");
				if (!json_is_object(value)) {
					return std::nullopt;
				}
				auto fields = json_object(value);
				if (fields.size() != aAsked.size() + 2U || !nonempty_string(json_member(fields, "id"))
					|| !bounded_text(text(json_member(fields, "id")), limits.Records.MaximumIdentityBytes)
					|| !application::detail::ascii_equal_fold(text(json_member(fields, "nameWithOwner")), aContext.Repository.Owner + "/" + aContext.Repository.Name)) {
					return std::nullopt;
				}
				for (std::size_t index = 0; index < aAsked.size(); ++index) {
					if (!json_is_object(json_member(fields, "p" + std::to_string(index)))) {
						return std::nullopt;
					}
				}
				return fields;
			};
			const auto initial = repository(*first, aNumbers);
			if (!initial) {
				return malformed();
			}
			const auto repositoryId = text(json_member(*initial, "id"));
			auto       results      = std::vector<requested_reviewers>{ };
			auto       pullIds      = std::set<std::string>{ };
			for (std::size_t index = 0; index < aNumbers.size(); ++index) {
				auto pull = json_object(json_member(*initial, "p" + std::to_string(index)));
				if (!has_exact_keys(pull, { "id", "number", "state", "headRefOid", "baseRefOid", "baseRefName", "reviewRequests" })
					|| uint32(json_member(pull, "number")) != aNumbers[index]
					|| !nonempty_string(json_member(pull, "id")) || !bounded_text(text(json_member(pull, "id")), limits.Records.MaximumIdentityBytes)
					|| !pullIds.insert(text(json_member(pull, "id"))).second
					|| !one_of(json_member(pull, "state"), { "OPEN", "CLOSED", "MERGED" })
					|| !hexadecimal_oid(json_member(pull, "headRefOid")) || !hexadecimal_oid(json_member(pull, "baseRefOid"))
					|| !nonempty_string(json_member(pull, "baseRefName")) || !bounded_text(text(json_member(pull, "baseRefName")), limits.Records.MaximumIdentityBytes)) {
					return malformed();
				}
				const auto anchor   = pull;
				auto       count    = std::size_t{ 0 };
				auto       expected = std::optional<std::uint64_t>{ };
				auto       requests = std::set<std::string>{ }, actors = std::set<std::string>{ }, cursors = std::set<std::string>{ };
				auto       result       = requested_reviewers{ };
				auto       next         = semantic;
				next.PullRequestNumbers = { aNumbers[index] };
				while (true) {
					for (const auto* key : { "id", "number", "state", "headRefOid", "baseRefOid", "baseRefName" }) {
						if (json_member(pull, key).Json != json_member(anchor, key).Json) {
							return malformed();
						}
					}
					const auto connection = parse_graphql_connection(json_member(pull, "reviewRequests"), limits);
					if (!connection) {
						return std::unexpected{ execution.enrich(connection.error()) };
					}
					if (connection->TotalCount > 1000U) {
						return execution.terminal_failure(application::github_provider_failure::response_limit);
					}
					if (expected && *expected != connection->TotalCount) {
						return malformed();
					}
					expected = connection->TotalCount;
					for (const auto& node : connection->Nodes) {
						if (!json_is_object(node)) {
							return malformed();
						}
						const auto fields = json_object(node);
						if (!has_exact_keys(fields, { "id", "requestedReviewer" }) || !nonempty_string(json_member(fields, "id"))
							|| !bounded_text(text(json_member(fields, "id")), limits.Records.MaximumIdentityBytes)
							|| !requests.insert(text(json_member(fields, "id"))).second) {
							return malformed();
						}
						const auto actor = json_member(fields, "requestedReviewer");
						if (!json_is_null(actor)) {
							if (!json_is_object(actor)) {
								return malformed();
							}
							const auto identity = json_object(actor);
							const auto user     = text(json_member(identity, "__typename")) == "User";
							if (!(user ? has_exact_keys(identity, { "__typename", "id", "login" }) : has_exact_keys(identity, { "__typename", "id" }))
								|| !one_of(json_member(identity, "__typename"), { "User", "Team" }) || !nonempty_string(json_member(identity, "id"))
								|| !bounded_text(text(json_member(identity, "id")), limits.Records.MaximumIdentityBytes)
								|| !actors.insert(text(json_member(identity, "id"))).second
								|| (user && (!nonempty_string(json_member(identity, "login")) || !bounded_text(text(json_member(identity, "login")), limits.Records.MaximumLoginBytes)))) {
								return malformed();
							}
							if (user) {
								const auto sameId    = text(json_member(identity, "id")) == aRequest.Viewer.Node.value();
								const auto sameLogin = application::detail::ascii_equal_fold(text(json_member(identity, "login")), aRequest.Viewer.Login);
								if (sameId != sameLogin) {
									return malformed();
								}
								result.NamesViewer = result.NamesViewer || (sameId && sameLogin);
							}
						}
						++count;
					}
					if (count > *expected) {
						return malformed();
					}
					if (!connection->NextCursor) {
						if (count != *expected) {
							return malformed();
						}
						result.Awaited     = static_cast<std::uint32_t>(count);
						result.NamesViewer = result.NamesViewer && text(json_member(anchor, "state")) == "OPEN";
						results.push_back(result);
						break;
					}
					if (next.Page == 10U || count >= *expected || !cursors.insert(*connection->NextCursor).second) {
						return execution.terminal_failure(application::github_provider_failure::response_limit);
					}
					if (next.Page == 1U && aRecoveries) {
						++*aRecoveries;
					}
					++next.Page;
					next.ReviewCursor = *connection->NextCursor;
					const auto page   = execution.fetch(next);
					if (!page) {
						return std::unexpected{ page.error() };
					}
					const auto continued = repository(*page, next.PullRequestNumbers);
					if (!continued || text(json_member(*continued, "id")) != repositoryId) {
						return malformed();
					}
					pull = json_object(json_member(*continued, "p0"));
				}
			}
			return results;
		}

		[[nodiscard]] auto recover_discovery_reviewers(
			github_wire_transport_port& aTransport, const validated_github_host_profile& aProfile,
			const application::personal_discovery_request& aRequest, const core::pull_request_request_context& aContext,
			const server_contract aServer, const std::stop_token aStop, const github_paged_read_limits& aLimits,
			application::github_rate_limit_state& aRates, shared_home_budget& aBudget)
			-> std::expected<requested_reviewers, github_paged_read_failure>
		{
			auto context       = aRequest.Context;
			context.Repository = aContext.Repository;
			const auto read    = read_discovery_membership(aTransport, aProfile, aRequest, context, { aContext.Number.value() }, aServer, aStop, aLimits, aRates, aBudget);
			if (!read) {
				return std::unexpected{ read.error() };
			}
			return read->front();
		}

		/// [[BPR-1407]]. A pull request's age, size and comment count. A field
		/// GitHub leaves out is unknown; one it sends in another shape is refused.
		[[nodiscard]] auto parse_change_facts(const page_json_object& aObject) -> std::optional<core::queue_change_facts>
		{
			auto result = core::queue_change_facts{ };
			if (aObject.contains("created_at")) {
				const auto created = json_member(aObject, "created_at");
				const auto opened  = json_is_string(created) ? parse_timeline_timestamp(text(created)) : std::nullopt;
				if (!opened) {
					return std::nullopt;
				}
				result.OpenedAt = *opened / 1000;
			}
			const auto count = [&aObject](const std::string_view aKey, std::optional<std::uint64_t>& aValue) {
				if (!aObject.contains(aKey)) {
					return true;
				}
				const auto value = uint32(json_member(aObject, aKey));
				aValue           = value;
				return value.has_value();
			};
			auto conversation = std::optional<std::uint64_t>{ };
			auto review       = std::optional<std::uint64_t>{ };
			if (!count("additions", result.Additions) || !count("deletions", result.Deletions)
				|| !count("comments", conversation) || !count("review_comments", review)) {
				return std::nullopt;
			}
			// A total needs both; either alone would undercount.
			if (conversation && review) {
				result.Comments = *conversation + *review;
			}
			return result;
		}

		[[nodiscard]] auto read_home_summary(production_read_execution& aExecution, const validated_github_host_profile& aProfile, const application::personal_discovery_request& aRequest, server_contract aServer, const core::pull_request_request_context& aContext, const parsed_page& aPage, const github_paged_read_limits& aLimits, bool* aLimited = nullptr) -> std::expected<core::queue_waiting_facts, github_paged_read_failure>
		{
			if (!json_is_object(aPage.Document)) {
				return core::queue_waiting_facts{ };
			}
			const auto object = json_object(aPage.Document);
			if (!json_is_object(json_member(object, "base")) || !json_is_object(json_member(object, "head")) || !json_is_object(json_member(json_object(json_member(object, "base")), "repo"))) {
				return core::queue_waiting_facts{ };
			}
			const auto base = json_object(json_member(object, "base"));
			const auto head = json_object(json_member(object, "head"));
			const auto repo = json_object(json_member(base, "repo"));
			if (integer(json_member(object, "number")) != aContext.Number.value() || !application::detail::ascii_equal_fold(text(json_member(repo, "full_name")), aContext.Repository.Owner + "/" + aContext.Repository.Name)) {
				return aExecution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			auto snapshot                  = application::github_pull_request_snapshot{ };
			snapshot.Viewer.Actor          = aRequest.Viewer.Node;
			snapshot.Viewer.Login          = aRequest.Viewer.Login;
			snapshot.Repository.Repository = core::repository_id{ integer(json_member(repo, "id")) };
			snapshot.Repository.Node       = application::github_node_id{ text(json_member(repo, "node_id")) };
			snapshot.Repository.Owner      = aContext.Repository.Owner;
			snapshot.Repository.Name       = aContext.Repository.Name;
			snapshot.PullRequest.Node      = application::github_node_id{ text(json_member(object, "node_id")) };
			snapshot.PullRequest.Number    = aContext.Number;
			snapshot.PullRequest.HeadOid   = application::commit_oid{ text(json_member(head, "sha")) };
			snapshot.PullRequest.BaseOid   = application::commit_oid{ text(json_member(base, "sha")) };
			snapshot.PullRequest.BaseRef   = text(json_member(base, "ref"));
			if (snapshot.Repository.Node.value().empty() || snapshot.PullRequest.Node.value().empty() || snapshot.PullRequest.BaseRef.empty() || !hexadecimal_oid(json_member(base, "sha")) || !hexadecimal_oid(json_member(head, "sha"))) {
				return core::queue_waiting_facts{ };
			}
			if (!aExecution.bind_repository(snapshot.Repository.Repository)) {
				return aExecution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto noteLimit = [aLimited](const github_paged_read_failure& aFailure) {
				const auto* error = std::get_if<application::github_provider_error>(&aFailure);
				if (aLimited && error && error->Failure == application::github_provider_failure::response_limit) {
					*aLimited = true;
				}
			};
			const auto requests = read_pending_requests(aExecution, aServer, aContext, snapshot, aLimits);
			if (!requests) {
				noteLimit(requests.error());
				if (fatal_optional_read_failure(requests.error())) {
					return std::unexpected{ requests.error() };
				}
				snapshot.ReviewRequestsAvailability = observation_failure(requests.error());
			}
			const auto assessment = read_merge_assessment(aExecution, aServer, aContext, snapshot);
			if (!assessment) {
				noteLimit(assessment.error());
			}
			if (!assessment && fatal_optional_read_failure(assessment.error())) {
				return std::unexpected{ assessment.error() };
			}
			auto semantic               = semantic_request_for(aServer, aContext, read_operation::list_checks);
			semantic.HeadOid            = snapshot.PullRequest.HeadOid.value();
			auto checks                 = read_checks(aExecution, aProfile, semantic, snapshot.PullRequest.HeadOid.value(), snapshot.Checks, aLimits);
			snapshot.ChecksAvailability = checks ? application::github_collection_availability::available : application::github_collection_availability::unavailable;
			if (!checks) {
				noteLimit(checks.error());
			}
			if (!checks && fatal_optional_read_failure(checks.error())) {
				return std::unexpected{ checks.error() };
			}
			semantic                      = semantic_request_for(aServer, aContext, read_operation::list_commit_statuses);
			semantic.HeadOid              = snapshot.PullRequest.HeadOid.value();
			auto statuses                 = std::vector<application::github_check_record>{ };
			auto statusRead               = read_rest_array_collection(aExecution, aProfile, semantic, statuses, aLimits.Records.MaximumChecks, aLimits.MaximumPageRecords, [&snapshot](const page_json_value& aValue) { return parse_commit_status(aValue, snapshot.PullRequest.HeadOid.value()); });
			snapshot.StatusesAvailability = statusRead ? application::github_collection_availability::available : application::github_collection_availability::unavailable;
			if (!statusRead) {
				noteLimit(statusRead.error());
			}
			if (!statusRead && fatal_optional_read_failure(statusRead.error())) {
				return std::unexpected{ statusRead.error() };
			}
			auto contexts = std::unordered_set<std::string>{ };
			for (auto& status : statuses) {
				if (contexts.insert(status.Name).second) {
					snapshot.Checks.push_back(std::move(status));
				}
			}
			auto budget = application::detail::github_text_budget{ aLimits.Records };
			if (application::detail::validate_merge_observations(snapshot, budget, aLimits.Records) != application::github_read_validation_error::none) {
				snapshot.ReviewRequests.clear();
				snapshot.ReviewRequestsAvailability = observation::incomplete;
			}
			return application::queue_waiting_for(snapshot);
		}

		/// [[BPR-1379]]. Retained rows need the current title, head and complete
		/// submitted-review evidence, without opening the snapshot read path.
		[[nodiscard]] auto read_retained_metadata(github_wire_transport_port&          aTransport,
												  const validated_github_host_profile& aProfile, const application::personal_discovery_request& aRequest,
												  const server_contract aServer, const std::stop_token aStop, const github_paged_read_limits& aLimits,
												  application::github_rate_limit_state& aRates) -> github_personal_discovery_read_result
		{
			auto limits                          = aLimits;
			limits.MaximumTotalPages             = std::min<std::size_t>(limits.MaximumTotalPages, 32U);
			limits.MaximumAggregateResponseBytes = std::min(limits.MaximumAggregateResponseBytes, std::size_t{ 8U * 1024U * 1024U });
			limits.Records.MaximumReviews        = std::min(limits.Records.MaximumReviews, application::maximum_personal_discovery_reviews);
			const auto                context    = requested_reviewers_parent(aRequest.Context, *aRequest.Number);
			auto                      homeBudget = shared_home_budget{ };
			production_read_execution execution{ aTransport, aProfile, context, limits, aStop, aRates, aRequest.AuthenticationSession, nullptr, &homeBudget };
			const auto                page = execution.fetch(semantic_request_for(aServer, context, read_operation::get_pull_request));
			if (!page) {
				return std::unexpected{ page.error() };
			}
			if (!singleton_complete(*page) || !json_is_object(page->Document)
				|| !has_required_keys(json_object(page->Document), { "base", "head", "number", "requested_reviewers", "requested_teams", "state", "title", "url", "user" })) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto object   = json_object(page->Document);
			const auto number   = uint32(json_member(object, "number"));
			const auto title    = json_member(object, "title");
			const auto draft    = json_member(object, "draft");
			const auto head     = json_member(object, "head");
			const auto users    = json_member(object, "requested_reviewers");
			const auto teams    = json_member(object, "requested_teams");
			const auto author   = optional_rest_actor(json_member(object, "user"));
			const auto expected = std::string{ aProfile.rest_root() } + "/repos/" + context.Repository.Owner + "/"
				+ context.Repository.Name + "/pulls/" + std::to_string(context.Number.value());
			if (!number || *number != context.Number.value() || !json_is_string(title)
				|| !bounded_text(text(title), limits.Records.MaximumTitleBytes, true)
				|| (has_required_keys(object, { "draft" }) && !json_is_bool(draft))
				|| !one_of(json_member(object, "state"), { "open", "closed" })
				|| !json_is_string(json_member(object, "url"))
				|| !application::detail::ascii_equal_fold(text(json_member(object, "url")), expected)
				|| !json_is_object(head) || !hexadecimal_oid(json_member(json_object(head), "sha"))
				|| !author.Valid || !json_is_array(users) || !json_is_array(teams)) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			if (json_array(users).size() + json_array(teams).size() > limits.MaximumPageRecords) {
				return execution.terminal_failure(application::github_provider_failure::response_limit);
			}
			const auto base = json_member(object, "base");
			if (!json_is_object(base)) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto repository = json_member(json_object(base), "repo");
			if (!json_is_object(repository) || !exact_integer(json_member(json_object(repository), "id"), true)
				|| !nonempty_string(json_member(json_object(repository), "full_name"))
				|| !application::detail::ascii_equal_fold(text(json_member(json_object(repository), "full_name")), context.Repository.Owner + "/" + context.Repository.Name)
				|| !execution.bind_repository(core::repository_id{ integer(json_member(json_object(repository), "id")) })) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			auto budget = application::detail::github_text_budget{ limits.Records };
			if (application::detail::validate_author(author.Actor, budget, limits.Records) != application::github_read_validation_error::none) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			for (const auto& user : json_array(users)) {
				const auto reviewer = optional_rest_actor(user);
				if (!reviewer.Valid || !reviewer.Actor
					|| application::detail::validate_author(reviewer.Actor, budget, limits.Records) != application::github_read_validation_error::none) {
					return execution.terminal_failure(application::github_provider_failure::malformed_response);
				}
			}
			if (!std::ranges::all_of(json_array(teams), [](const page_json_value& aValue) { return json_is_object(aValue); })) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			auto row = application::personal_discovery_row{
				.Number           = context.Number,
				.Title            = text(title),
				.Author           = author.Actor ? author.Actor->Login : std::string{ },
				.Detailed         = true,
				.ReviewersAwaited = static_cast<std::uint32_t>(json_array(users).size() + json_array(teams).size()),
				.HeadOid          = application::commit_oid{ text(json_member(json_object(head), "sha")) },
				.Closed           = text(json_member(object, "state")) == "closed",
				.Draft            = json_is_bool(draft) ? std::optional<bool>{ json_bool(draft) } : std::nullopt,
			};
			row.AuthorIdentity = author.Actor ? std::optional{ author.Actor->Actor } : std::nullopt;
			row.Repository     = context.Repository;
			// [[BPR-1680]]. What the requested-reviewers read would say of the
			// viewer, so a search row in the same refresh need not ask it.
			row.RequestsViewer = std::ranges::any_of(json_array(users), [&aRequest](const page_json_value& aUser) {
				const auto reviewer = optional_rest_actor(aUser);
				return reviewer.Actor && application::detail::ascii_equal_fold(reviewer.Actor->Login, aRequest.Viewer.Login);
			});
			const auto change  = parse_change_facts(object);
			if (!change) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			row.Change = *change;
			if (row.Closed) {
				return github_personal_discovery_read{ .Rows = { std::move(row) }, .RateLimits = aRates };
			}
			auto       seen    = std::unordered_set<std::uint64_t>{ };
			const auto reviews = read_rest_array_collection(execution, aProfile,
															semantic_request_for(aServer, context, read_operation::list_reviews), row.Reviews,
															limits.Records.MaximumReviews, limits.MaximumPageRecords,
															[&budget, &seen, &limits](const page_json_value& aValue)
																-> std::expected<application::personal_discovery_review, github_paged_read_failure> {
																const auto review = parse_review(aValue);
																if (!review) {
																	return std::unexpected{ review.error() };
																}
																if (!seen.insert(review->DatabaseId).second
																	|| application::detail::validate_review(*review, budget, limits.Records) != application::github_read_validation_error::none) {
																	return failed(application::github_provider_failure::malformed_response);
																}
																// [[BPR-1718]]. `submitted_at`, when it was read and parses.
																const auto& submitted = review->Observation.CreatedAt;
																const auto  at        = submitted.empty() ? std::nullopt : parse_timeline_timestamp(submitted);
																return application::personal_discovery_review{
																	.DatabaseId  = review->DatabaseId,
																	.Author      = review->Author ? std::optional{ review->Author->Actor } : std::nullopt,
																	.State       = review->State,
																	.Commit      = review->Commit,
																	.SubmittedAt = at.transform([](const std::int64_t aMilliseconds) { return aMilliseconds / 1000; }),
																};
															});
			if (!reviews) {
				return std::unexpected{ execution.enrich(reviews.error()) };
			}
			const auto summary = read_home_summary(execution, aProfile, aRequest, aServer, context, *page, limits, &row.WaitingLimited);
			if (!summary) {
				return std::unexpected{ summary.error() };
			}
			row.Waiting = *summary;
			// The validated singleton carries the complete requested arrays. It
			// can settle the count when the richer code-owner query is unavailable;
			// a nonempty count preserves old actor names as last known.
			if (!row.Waiting.Requests) {
				row.Waiting.Requests = row.ReviewersAwaited;
				row.Waiting.People   = static_cast<std::uint32_t>(json_array(users).size());
				row.Waiting.Teams    = static_cast<std::uint32_t>(json_array(teams).size());
			}
			row.Waiting.Head = row.HeadOid->value();
			return github_personal_discovery_read{ .Rows = { std::move(row) }, .RateLimits = aRates };
		}

		/// [[BPR-1680]]. One connection of Home's batched query: its first page,
		/// its total, and whether more follow than the page holds.
		struct home_connection
		{
			std::uint64_t   Total = 0U;
			page_json_array Nodes;
			bool            More = false;
		};

		[[nodiscard]] auto parse_home_connection(const page_json_value& aValue) -> std::optional<home_connection>
		{
			if (!json_is_object(aValue) || !has_exact_keys(json_object(aValue), { "nodes", "pageInfo", "totalCount" })) {
				return std::nullopt;
			}
			const auto connection = json_object(aValue);
			const auto pageInfo   = json_member(connection, "pageInfo");
			if (!exact_integer(json_member(connection, "totalCount")) || !json_is_array(json_member(connection, "nodes")) || !json_is_object(pageInfo)
				|| !has_exact_keys(json_object(pageInfo), { "hasNextPage" }) || !json_is_bool(json_member(json_object(pageInfo), "hasNextPage"))) {
				return std::nullopt;
			}
			auto result = home_connection{
				.Total = integer(json_member(connection, "totalCount")),
				.Nodes = json_array(json_member(connection, "nodes")),
				.More  = json_bool(json_member(json_object(pageInfo), "hasNextPage")),
			};
			if (result.Nodes.size() > core::discovery_batch_page || result.Total < result.Nodes.size()
				|| (result.More && result.Nodes.size() != core::discovery_batch_page)) {
				return std::nullopt;
			}
			result.More = result.More || result.Total > result.Nodes.size();
			return result;
		}

		/// [[BPR-1680]]. One check context of a commit's status rollup, as the
		/// REST check runs and commit statuses would have named it.
		[[nodiscard]] auto parse_home_check(const page_json_value& aValue, const std::string_view aHead, const std::size_t aIndex)
			-> std::optional<application::github_check_record>
		{
			if (!json_is_object(aValue)) {
				return std::nullopt;
			}
			const auto object   = json_object(aValue);
			auto       result   = application::github_check_record{ };
			result.Check        = application::github_check_id{ "home:" + std::to_string(aIndex) };
			result.EvaluatedOid = application::commit_oid{ std::string{ aHead } };
			using status        = application::github_check_status;
			using conclusion    = application::github_check_conclusion;
			if (text(json_member(object, "__typename")) == "CheckRun") {
				const auto state = text(json_member(object, "status"));
				const auto ended = json_member(object, "conclusion");
				if (!has_exact_keys(object, { "__typename", "conclusion", "name", "status" }) || !nonempty_string(json_member(object, "name"))
					|| !one_of(json_member(object, "status"), { "QUEUED", "IN_PROGRESS", "COMPLETED", "WAITING", "PENDING", "REQUESTED" })
					|| (!json_is_null(ended)
						&& !one_of(ended, { "SUCCESS", "FAILURE", "NEUTRAL", "CANCELLED", "SKIPPED", "TIMED_OUT", "ACTION_REQUIRED", "STALE", "STARTUP_FAILURE" }))
					|| ((state == "COMPLETED") == json_is_null(ended))) {
					return std::nullopt;
				}
				result.Name       = text(json_member(object, "name"));
				result.Status     = state == "QUEUED" ? status::queued : state == "IN_PROGRESS" ? status::in_progress
					: state == "COMPLETED"                                                      ? status::completed
					: state == "WAITING"                                                        ? status::waiting
					: state == "REQUESTED"                                                      ? status::requested
																								: status::pending;
				const auto value  = json_is_null(ended) ? std::string{ } : text(ended);
				result.Conclusion = value.empty() ? conclusion::none : value == "SUCCESS" ? conclusion::success
					: value == "NEUTRAL"                                                  ? conclusion::neutral
					: value == "CANCELLED"                                                ? conclusion::cancelled
					: value == "SKIPPED"                                                  ? conclusion::skipped
					: value == "TIMED_OUT"                                                ? conclusion::timed_out
					: value == "ACTION_REQUIRED"                                          ? conclusion::action_required
					: value == "STALE"                                                    ? conclusion::stale
																						  : conclusion::failure;
				return result;
			}
			if (text(json_member(object, "__typename")) == "StatusContext") {
				if (!has_exact_keys(object, { "__typename", "context", "state" }) || !nonempty_string(json_member(object, "context"))
					|| !one_of(json_member(object, "state"), { "ERROR", "EXPECTED", "FAILURE", "PENDING", "SUCCESS" })) {
					return std::nullopt;
				}
				const auto state   = text(json_member(object, "state"));
				result.Name        = text(json_member(object, "context"));
				result.Source      = application::github_check_source::commit_status;
				result.Status      = state == "PENDING" || state == "EXPECTED" ? status::pending : status::completed;
				result.Conclusion  = result.Status == status::pending ? conclusion::none : state == "SUCCESS" ? conclusion::success
																											  : conclusion::failure;
				result.StatusError = state == "ERROR";
				return result;
			}
			return std::nullopt;
		}

		/// [[BPR-1680]]. What one alias of Home's batched query establishes, or
		/// none when it is not exactly the pull request asked for. A pull
		/// request with more reviews, review requests or check contexts than
		/// one page is `Paged`: it is read the REST way, which pages.
		struct home_batch_row
		{
			application::personal_discovery_row Row;
			bool                                Paged = false;
		};

		[[nodiscard]] auto parse_home_pull_request(const page_json_object& aPull, const application::personal_discovery_request& aRequest,
												   const core::pull_request_number aNumber, const github_paged_read_limits& aLimits)
			-> std::optional<home_batch_row>
		{
			if (!has_exact_keys(aPull, { "additions", "author", "autoMergeRequest", "baseRefName", "baseRefOid", "comments", "commits", "createdAt", "deletions", "headRefOid", "id", "isDraft", "mergeStateStatus", "number", "reviewDecision", "reviewRequests", "reviews", "state", "title" })
				|| uint32(json_member(aPull, "number")) != aNumber.value() || !json_is_string(json_member(aPull, "title"))
				|| !bounded_text(text(json_member(aPull, "title")), aLimits.Records.MaximumTitleBytes, true) || !json_is_bool(json_member(aPull, "isDraft"))
				|| !one_of(json_member(aPull, "state"), { "OPEN", "CLOSED", "MERGED" }) || !hexadecimal_oid(json_member(aPull, "headRefOid"))
				|| !hexadecimal_oid(json_member(aPull, "baseRefOid")) || !nonempty_string(json_member(aPull, "baseRefName"))
				|| !nonempty_string(json_member(aPull, "id")) || !json_is_object(json_member(aPull, "comments"))
				|| !exact_integer(json_member(json_object(json_member(aPull, "comments")), "totalCount"))) {
				return std::nullopt;
			}
			const auto author = optional_graphql_actor(json_member(aPull, "author"));
			if (!author.Valid) {
				return std::nullopt;
			}
			auto row = application::personal_discovery_row{
				.Number   = aNumber,
				.Title    = text(json_member(aPull, "title")),
				.Author   = author.Actor ? author.Actor->Login : std::string{ },
				.Detailed = true,
				.HeadOid  = application::commit_oid{ text(json_member(aPull, "headRefOid")) },
				.Closed   = text(json_member(aPull, "state")) != "OPEN",
				.Draft    = json_bool(json_member(aPull, "isDraft")),
			};
			row.AuthorIdentity = author.Actor ? std::optional{ author.Actor->Actor } : std::nullopt;
			row.Repository     = aRequest.Context.Repository;
			auto result        = home_batch_row{ };

			const auto reviews  = parse_home_connection(json_member(aPull, "reviews"));
			const auto requests = parse_home_connection(json_member(aPull, "reviewRequests"));
			const auto commits  = json_member(aPull, "commits");
			if (!reviews || !requests || !json_is_object(commits) || !has_exact_keys(json_object(commits), { "nodes" })
				|| !json_is_array(json_member(json_object(commits), "nodes")) || json_array(json_member(json_object(commits), "nodes")).size() > 1U) {
				return std::nullopt;
			}
			// The last commit's status rollup: none at all is no checks.
			auto contexts = std::optional<home_connection>{ home_connection{ } };
			for (const auto& node : json_array(json_member(json_object(commits), "nodes"))) {
				const auto commit = json_is_object(node) ? json_member(json_object(node), "commit") : page_json_value{ };
				if (!json_is_object(commit) || !has_exact_keys(json_object(commit), { "oid", "statusCheckRollup" })
					|| text(json_member(json_object(commit), "oid")) != row.HeadOid->value()) {
					return std::nullopt;
				}
				const auto rollup = json_member(json_object(commit), "statusCheckRollup");
				if (!json_is_null(rollup)) {
					if (!json_is_object(rollup) || !has_exact_keys(json_object(rollup), { "contexts" })) {
						return std::nullopt;
					}
					contexts = parse_home_connection(json_member(json_object(rollup), "contexts"));
				}
			}
			if (!contexts) {
				return std::nullopt;
			}
			if (reviews->More || requests->More || contexts->More) {
				result.Row   = std::move(row);
				result.Paged = true;
				return result;
			}

			// Submitted-review evidence, and the review comments it holds.
			auto seen           = std::unordered_set<std::uint64_t>{ };
			auto reviewComments = std::uint64_t{ 0U };
			for (const auto& node : reviews->Nodes) {
				const auto object    = json_is_object(node) ? json_object(node) : page_json_object{ };
				const auto commit    = json_member(object, "commit");
				const auto actor     = optional_graphql_actor(json_member(object, "author"));
				const auto submitted = json_member(object, "submittedAt");
				// [[BPR-1718]]. When it was submitted: null only while pending.
				const auto at = json_is_string(submitted) ? parse_timeline_timestamp(text(submitted)) : std::nullopt;
				if (!has_exact_keys(object, { "author", "comments", "commit", "databaseId", "state", "submittedAt" }) || (!json_is_null(submitted) && !at)
					|| !exact_integer(json_member(object, "databaseId"), true)
					|| !one_of(json_member(object, "state"), { "PENDING", "COMMENTED", "APPROVED", "CHANGES_REQUESTED", "DISMISSED" }) || !actor.Valid
					|| (!json_is_null(commit) && (!json_is_object(commit) || !hexadecimal_oid(json_member(json_object(commit), "oid"))))
					|| !json_is_object(json_member(object, "comments")) || !exact_integer(json_member(json_object(json_member(object, "comments")), "totalCount"))
					|| !seen.insert(integer(json_member(object, "databaseId"))).second) {
					return std::nullopt;
				}
				const auto state = review_state(json_text(json_member(object, "state")));
				row.Reviews.push_back({
					.DatabaseId  = integer(json_member(object, "databaseId")),
					.Author      = actor.Actor ? std::optional{ actor.Actor->Actor } : std::nullopt,
					.State       = state,
					.Commit      = json_is_null(commit) ? std::nullopt : std::optional{ application::commit_oid{ text(json_member(json_object(commit), "oid")) } },
					.SubmittedAt = at.transform([](const std::int64_t aMilliseconds) { return aMilliseconds / 1000; }),
				});
				// A pending review's comments are not yet the pull request's.
				if (state != application::github_review_state::pending) {
					reviewComments += integer(json_member(json_object(json_member(object, "comments")), "totalCount"));
				}
			}

			// The waiting summary, from the same snapshot fields the REST
			// summary reads fill.
			auto snapshot                = application::github_pull_request_snapshot{ };
			snapshot.Viewer.Actor        = aRequest.Viewer.Node;
			snapshot.Viewer.Login        = aRequest.Viewer.Login;
			snapshot.Repository.Owner    = aRequest.Context.Repository.Owner;
			snapshot.Repository.Name     = aRequest.Context.Repository.Name;
			snapshot.PullRequest.Node    = application::github_node_id{ text(json_member(aPull, "id")) };
			snapshot.PullRequest.Number  = aNumber;
			snapshot.PullRequest.HeadOid = *row.HeadOid;
			snapshot.PullRequest.BaseOid = application::commit_oid{ text(json_member(aPull, "baseRefOid")) };
			snapshot.PullRequest.BaseRef = text(json_member(aPull, "baseRefName"));
			auto actors                  = std::unordered_set<std::string>{ };
			auto identities              = std::unordered_set<std::string>{ };
			for (const auto& node : requests->Nodes) {
				auto record = parse_pending_request(node);
				if (!record || !identities.insert(record->Request).second || (!record->Actor.empty() && !actors.insert(record->Actor).second)) {
					return std::nullopt;
				}
				row.RequestsViewer = row.RequestsViewer
					|| (record->Kind == application::github_requested_actor_kind::user && application::detail::ascii_equal_fold(record->Login, aRequest.Viewer.Login));
				snapshot.ReviewRequests.push_back(std::move(*record));
			}
			snapshot.ReviewRequestsAvailability = observation::available;
			row.ReviewersAwaited                = static_cast<std::uint32_t>(requests->Total);
			if (!apply_merge_fields(aPull, snapshot)) {
				return std::nullopt;
			}
			auto names = std::unordered_set<std::string>{ };
			for (std::size_t index = 0U; index < contexts->Nodes.size(); ++index) {
				auto check = parse_home_check(contexts->Nodes[index], row.HeadOid->value(), index);
				if (!check) {
					return std::nullopt;
				}
				// One result per context name, as the REST statuses keep the latest.
				if (names.insert(check->Name).second) {
					snapshot.Checks.push_back(std::move(*check));
				}
			}
			snapshot.ChecksAvailability   = application::github_collection_availability::available;
			snapshot.StatusesAvailability = application::github_collection_availability::available;
			auto budget                   = application::detail::github_text_budget{ aLimits.Records };
			if (application::detail::validate_merge_observations(snapshot, budget, aLimits.Records) != application::github_read_validation_error::none) {
				return std::nullopt;
			}
			row.Waiting      = application::queue_waiting_for(snapshot);
			row.Waiting.Head = row.HeadOid->value();

			// Age, size and conversation.
			const auto opened = json_is_string(json_member(aPull, "createdAt")) ? parse_timeline_timestamp(text(json_member(aPull, "createdAt"))) : std::nullopt;
			const auto added  = uint32(json_member(aPull, "additions"));
			const auto taken  = uint32(json_member(aPull, "deletions"));
			if (!opened || !added || !taken) {
				return std::nullopt;
			}
			row.Change.OpenedAt  = *opened / 1000;
			row.Change.Additions = *added;
			row.Change.Deletions = *taken;
			row.Change.Comments  = integer(json_member(json_object(json_member(aPull, "comments")), "totalCount")) + reviewComments;
			result.Row           = std::move(row);
			return result;
		}

		/// [[BPR-1680]]. Whether a failure is about the whole session (or the
		/// read was cancelled) rather than one pull request or the batched
		/// query itself, so falling back to the REST reads would fail the same.
		[[nodiscard]] auto session_failure(const github_paged_read_failure& aFailure) noexcept -> bool
		{
			const auto* error = std::get_if<application::github_provider_error>(&aFailure);
			if (error == nullptr) {
				return true;
			}
			switch (error->Failure) {
			case application::github_provider_failure::authentication:
			case application::github_provider_failure::rate_limit:
			case application::github_provider_failure::network:
			case application::github_provider_failure::timeout:
			case application::github_provider_failure::certificate:
			case application::github_provider_failure::redirect:
			case application::github_provider_failure::process_startup:
			case application::github_provider_failure::cli_unavailable:
			case application::github_provider_failure::vault:                  return true;
			case application::github_provider_failure::permission:
			case application::github_provider_failure::api_version:
			case application::github_provider_failure::unsupported_capability:
			case application::github_provider_failure::malformed_response:
			case application::github_provider_failure::response_limit:
			case application::github_provider_failure::unavailable:            return false;
			}
			return true;
		}

		/// [[BPR-1680]]. Several retained pull requests of one repository in one
		/// GraphQL query. One with more than a page of anything, and every one
		/// when the query fails other than for the whole session (an older
		/// server without one of its fields, a pull request GitHub no longer
		/// has, a malformed answer), is read the REST way, one at a time, as
		/// before. `Unsupported` says the server lacked a field.
		struct home_batch_result
		{
			github_personal_discovery_read_result Read;
			bool                                  Unsupported = false;
		};

		[[nodiscard]] auto read_retained_batch(github_wire_transport_port& aTransport, const validated_github_host_profile& aProfile,
											   const application::personal_discovery_request& aRequest, const server_contract aServer, const std::stop_token aStop,
											   const github_paged_read_limits& aLimits, application::github_rate_limit_state& aRates, const bool aBatched)
			-> home_batch_result
		{
			auto rows        = std::vector<application::personal_discovery_row>{ };
			auto restNumbers = std::vector<core::pull_request_number>{ };
			auto unsupported = false;
			if (aBatched) {
				production_read_execution batch{ aTransport, aProfile, aRequest.Context, aLimits, aStop, aRates, aRequest.AuthenticationSession, nullptr };
				auto                      semantic = semantic_read_request{
					.Server     = aServer,
					.Operation  = read_operation::list_home_pull_requests,
					.Owner      = aRequest.Context.Repository.Owner,
					.Repository = aRequest.Context.Repository.Name,
					.Page       = 1U,
				};
				for (const auto number : aRequest.Numbers) {
					semantic.PullRequestNumbers.push_back(number.value());
				}
				const auto page   = batch.fetch(semantic);
				const auto answer = [&]() -> std::optional<std::vector<home_batch_row>> {
					if (!page || !json_is_object(page->Document) || !has_exact_keys(json_object(page->Document), { "data" })) {
						return std::nullopt;
					}
					const auto data = json_member(json_object(page->Document), "data");
					if (!json_is_object(data) || !has_exact_keys(json_object(data), { "repository" })) {
						return std::nullopt;
					}
					const auto repository = json_member(json_object(data), "repository");
					if (!json_is_object(repository) || json_object(repository).size() != aRequest.Numbers.size() + 2U
						|| !nonempty_string(json_member(json_object(repository), "id"))
						|| !application::detail::ascii_equal_fold(text(json_member(json_object(repository), "nameWithOwner")),
																  aRequest.Context.Repository.Owner + "/" + aRequest.Context.Repository.Name)) {
						return std::nullopt;
					}
					auto parsed = std::vector<home_batch_row>{ };
					for (std::size_t index = 0U; index < aRequest.Numbers.size(); ++index) {
						const auto pull = json_member(json_object(repository), "p" + std::to_string(index));
						auto       row  = json_is_object(pull) ? parse_home_pull_request(json_object(pull), aRequest, aRequest.Numbers[index], aLimits) : std::nullopt;
						if (!row) {
							return std::nullopt;
						}
						parsed.push_back(std::move(*row));
					}
					return parsed;
				}();
				// A failure of the whole session fails the read as it would
				// have failed its first REST request.
				if (!page && session_failure(page.error())) {
					return { .Read = std::unexpected{ page.error() } };
				}
				if (!page) {
					const auto* error = std::get_if<application::github_provider_error>(&page.error());
					unsupported       = error != nullptr && error->Failure == application::github_provider_failure::unsupported_capability;
				}
				if (answer) {
					for (auto& row : *answer) {
						if (row.Paged) {
							restNumbers.push_back(row.Row.Number);
						} else {
							rows.push_back(std::move(row.Row));
						}
					}
				} else {
					restNumbers = aRequest.Numbers;
				}
			} else {
				restNumbers = aRequest.Numbers;
			}
			auto firstFailure = std::optional<github_paged_read_failure>{ };
			auto unread       = std::vector<application::personal_discovery_unread>{ };
			for (const auto number : restNumbers) {
				auto single    = aRequest;
				single.Number  = number;
				single.Numbers = { };
				auto read      = read_retained_metadata(aTransport, aProfile, single, aServer, aStop, aLimits, aRates);
				if (!read) {
					if (session_failure(read.error())) {
						return { .Read = std::unexpected{ read.error() }, .Unsupported = unsupported };
					}
					// This pull request alone could not be read: the host
					// fails it and applies the rest. [[BPR-1874]]. It says
					// why, for the diagnostics log.
					firstFailure = firstFailure.value_or(read.error());
					if (const auto* error = std::get_if<application::github_provider_error>(&read.error()); error != nullptr) {
						unread.push_back({ .Number = number, .Error = *error });
					}
					continue;
				}
				for (auto& row : read->Rows) {
					rows.push_back(std::move(row));
				}
			}
			// Nothing read at all fails as the one read would have.
			if (rows.empty() && firstFailure) {
				return { .Read = std::unexpected{ *firstFailure }, .Unsupported = unsupported };
			}
			std::ranges::sort(rows, { }, &application::personal_discovery_row::Number);
			return { .Read = github_personal_discovery_read{ .Rows = std::move(rows), .RateLimits = aRates, .Unread = std::move(unread) }, .Unsupported = unsupported };
		}
	}

	auto perform_personal_discovery_read(
		github_wire_transport_port&                    aTransport,
		const validated_github_host_profile&           aProfile,
		const application::personal_discovery_request& aRequest,
		const std::stop_token                          aStop,
		const github_paged_read_limits&                aLimits,
		const bool aBatched, core::discovery_work* aWork) noexcept -> github_personal_discovery_read_result
	{
		auto  localWork = core::discovery_work{ };
		auto& work      = aWork ? *aWork : localWork;
		work            = { };
		const auto note = [&work](const core::discovery_completeness_reason aReason) {
			using reason = core::discovery_completeness_reason;
			if (work.Completeness == reason::none || work.Completeness == reason::complete) {
				work.Completeness = aReason;
			} else if (work.Completeness != aReason) {
				work.Completeness = reason::multiple_limits;
			}
		};

		const auto noteRetained = [&note](const github_personal_discovery_read_result& aRead) {
			if (aRead) {
				note(std::ranges::any_of(aRead->Rows, &application::personal_discovery_row::WaitingLimited)
						 ? core::discovery_completeness_reason::summary_limit
						 : core::discovery_completeness_reason::complete);
			}
		};

		auto rates = application::github_rate_limit_state{ };
		try {
			const auto server = server_contract_for(aProfile);
			if (!server.has_value() || !discovery_request_valid(aProfile, aRequest, aLimits)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			if (aStop.stop_requested()) {
				return cancelled();
			}
			if (aRequest.Reason == application::personal_discovery_reason::retained && !aRequest.Numbers.empty()) {
				auto batch = read_retained_batch(aTransport, aProfile, aRequest, *server, aStop, aLimits, rates, aBatched);
				if (batch.Read) {
					batch.Read->BatchUnsupported = batch.Unsupported;
				}
				noteRetained(batch.Read);
				return std::move(batch.Read);
			}
			if (aRequest.Reason == application::personal_discovery_reason::retained) {
				auto read = read_retained_metadata(aTransport, aProfile, aRequest, *server, aStop, aLimits, rates);
				noteRetained(read);
				return read;
			}
			const auto requested     = aRequest.Reason == application::personal_discovery_reason::requested;
			auto       limits        = aLimits;
			limits.MaximumTotalPages = 1U;
			// [[BPR-1680]]. The saved repositories this one search names.
			const auto                repositories = aRequest.Repositories.empty() ? std::vector<core::repository_locator>{ aRequest.Context.Repository } : aRequest.Repositories;
			production_read_execution search{ aTransport, aProfile, aRequest.Context, limits, aStop, rates, aRequest.AuthenticationSession, nullptr };
			auto                      semantic = semantic_read_request{
				.Server            = *server,
				.Operation         = read_operation::search_personal_pull_requests,
				.Owner             = aRequest.Context.Repository.Owner,
				.Repository        = aRequest.Context.Repository.Name,
				.PullRequestNumber = 0U,
				.HeadOid           = { },
				.SearchTerms       = requested ? std::string{ "user-review-requested:@me" } : "author:" + aRequest.Viewer.Login,
				.Page              = 1U,
				.GraphqlPage       = std::nullopt,
			};
			for (std::size_t index = 1U; index < repositories.size(); ++index) {
				semantic.SearchRepositories.push_back(repositories[index].Owner + "/" + repositories[index].Name);
			}
			auto page = search.fetch(semantic);
			if (!page.has_value()) {
				auto failure = page.error();
				// GitHub refuses a search whose repository qualifier names
				// nothing this account can read (422): removed, renamed or no
				// longer visible. That is lost access, not an API mismatch.
				if (auto* error = std::get_if<application::github_provider_error>(&failure);
					error != nullptr && error->Failure == application::github_provider_failure::api_version) {
					error->Failure = application::github_provider_failure::permission;
				}
				return std::unexpected{ std::move(failure) };
			}
			if (!json_is_object(page->Document) || !has_required_keys(json_object(page->Document), { "incomplete_results", "items", "total_count" })) {
				return search.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(page->Document);
			if (!exact_integer(json_member(object, "total_count")) || !json_is_array(json_member(object, "items"))
				|| !json_is_bool(json_member(object, "incomplete_results"))) {
				return search.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto items      = json_array(json_member(object, "items"));
			work.SearchCandidates = static_cast<std::uint32_t>(items.size());
			if (items.size() > application::maximum_personal_discovery_rows || items.size() > limits.MaximumPageRecords) {
				return search.terminal_failure(application::github_provider_failure::response_limit);
			}
			// One page is all a discovery read takes, so a continuation is never
			// followed: it only makes the answer partial, as a total larger
			// than the page does.
			if (page->Completeness == page_completeness::unknown
				|| (page->Completeness == page_completeness::has_next_page && items.size() != limits.MaximumPageRecords)) {
				return search.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto more   = page->Completeness == page_completeness::has_next_page || integer(json_member(object, "total_count")) > items.size();
			auto       result = github_personal_discovery_read{ };
			if (json_bool(json_member(object, "incomplete_results"))) {
				result.Completeness = application::personal_discovery_completeness::incomplete_results;
				note(core::discovery_completeness_reason::search_incomplete);
			} else if (more) {
				result.Completeness = application::personal_discovery_completeness::result_limit;
				note(core::discovery_completeness_reason::search_limit);
			}
			auto seen = std::set<std::pair<core::repository_locator, std::uint32_t>>{ };
			for (const auto& value : items) {
				auto row = parse_discovery_item(value, repositories, aProfile, aLimits);
				if (!row.has_value()) {
					return std::unexpected{ search.enrich(row.error()) };
				}
				if (!seen.emplace(row->Repository, row->Number.value()).second) {
					return search.terminal_failure(application::github_provider_failure::malformed_response);
				}
				// A search expansion may name another actor; only the viewer's
				// own pull requests are authored ones.
				if (!requested && !application::detail::ascii_equal_fold(row->Author, aRequest.Viewer.Login)) {
					continue;
				}
				result.Rows.push_back(std::move(*row));
			}
			auto       homeBudget     = shared_home_budget{ };
			auto       recoveryBudget = shared_home_budget{ };
			auto       details        = std::size_t{ 0U };
			auto       kept           = std::vector<application::personal_discovery_row>{ };
			auto       overflow       = false;
			const auto known          = [&aRequest](const application::personal_discovery_row& aRow) {
				return std::ranges::binary_search(aRequest.Known, application::personal_discovery_known{ .Repository = aRow.Repository, .Number = aRow.Number });
			};
			auto membership = std::map<std::pair<core::repository_locator, core::pull_request_number>, requested_reviewers>{ };
			if (requested) {
				auto targets = std::vector<core::discovery_retained_target>{ };
				for (const auto& row : result.Rows) {
					if (!known(row) && targets.size() < application::maximum_personal_discovery_details) {
						targets.push_back({ row.Repository, row.Number });
					}
				}
				for (const auto& batch : core::plan_retained_batches(targets)) {
					auto context       = aRequest.Context;
					context.Repository = batch.Repository;
					auto numbers       = std::vector<std::uint32_t>{ };
					for (const auto number : batch.Numbers) {
						numbers.push_back(number.value());
					}
					const auto read = read_discovery_membership(aTransport, aProfile, aRequest, context, numbers, *server, aStop, aLimits, rates, recoveryBudget, &work.RosterRecoveries);
					if (!read) {
						const auto* error = std::get_if<application::github_provider_error>(&read.error());
						if (!error || error->Failure != application::github_provider_failure::unsupported_capability) {
							if (error && error->Failure == application::github_provider_failure::response_limit) {
								note(core::discovery_completeness_reason::roster_limit);
							}
							return std::unexpected{ read.error() };
						}
						// A server without the narrow schema uses the existing
						// complete REST roster. Never fall back on auth/identity errors.
						break;
					}
					for (std::size_t index = 0; index < batch.Numbers.size(); ++index) {
						membership.emplace(std::make_pair(batch.Repository, batch.Numbers[index]), (*read)[index]);
					}
				}
			}

			for (auto& row : result.Rows) {
				// [[BPR-1680]]. Read in full by this refresh's retained read:
				// no requested-reviewers, pull request or summary read again,
				// and no detail read spent.
				if (known(row)) {
					row.Reused = true;
					++work.RetainedReuse;
					kept.push_back(std::move(row));
					continue;
				}
				if (details == application::maximum_personal_discovery_details) {
					overflow = true;
					note(core::discovery_completeness_reason::candidate_limit);
					if (!requested) {
						kept.push_back(std::move(row));
					}
					continue;
				}
				++details;
				// Each row's detail is read under its own repository.
				auto rowContext       = aRequest.Context;
				rowContext.Repository = row.Repository;
				production_read_execution detail{ aTransport, aProfile, requested_reviewers_parent(rowContext, row.Number), limits, aStop, rates, aRequest.AuthenticationSession, nullptr };
				auto                      parsed = std::expected<requested_reviewers, github_paged_read_failure>{ };
				const auto                found  = membership.find(std::make_pair(row.Repository, row.Number));
				if (found != membership.end()) {
					parsed = found->second;
				} else {
					const auto reviewers = detail.fetch(semantic_read_request{
						.Server            = *server,
						.Operation         = read_operation::list_requested_reviewers,
						.Owner             = row.Repository.Owner,
						.Repository        = row.Repository.Name,
						.PullRequestNumber = row.Number.value(),
					});
					parsed               = reviewers ? parse_requested_reviewers(*reviewers, aRequest.Viewer.Login, aLimits)
													 : std::expected<requested_reviewers, github_paged_read_failure>{ std::unexpected{ reviewers.error() } };
				}

				if (!parsed) {
					const auto* error = std::get_if<application::github_provider_error>(&parsed.error());
					if (error == nullptr || error->Failure != application::github_provider_failure::response_limit) {
						return std::unexpected{ detail.enrich(parsed.error()) };
					}
					++work.RosterRecoveries;
					parsed = recover_discovery_reviewers(aTransport, aProfile, aRequest, requested_reviewers_parent(rowContext, row.Number), *server, aStop, aLimits, rates, recoveryBudget);
					if (!parsed) {
						const auto* recoveryError = std::get_if<application::github_provider_error>(&parsed.error());
						if (recoveryError && recoveryError->Failure == application::github_provider_failure::response_limit) {
							note(core::discovery_completeness_reason::roster_limit);
						}
						return std::unexpected{ parsed.error() };
					}
				}
				// Asked of a team the viewer belongs to is not asked of the
				// viewer: automatic team-request discovery is a non-goal.
				if (requested && !parsed->NamesViewer) {
					++work.Rejected;
					continue;
				}
				if (requested) {
					++work.DirectMatches;
				}
				row.Detailed         = true;
				row.ReviewersAwaited = parsed->Awaited;
				row.Waiting.Requests = parsed->Awaited;
				if (homeBudget.Pages < 32) {
					auto summaryLimits                          = aLimits;
					summaryLimits.MaximumTotalPages             = std::min(summaryLimits.MaximumTotalPages, std::size_t{ 32 });
					summaryLimits.MaximumAggregateResponseBytes = std::min(summaryLimits.MaximumAggregateResponseBytes, std::size_t{ 8U * 1024U * 1024U });
					const auto                context           = requested_reviewers_parent(rowContext, row.Number);
					production_read_execution summaryExecution{ aTransport, aProfile, context, summaryLimits, aStop, rates, aRequest.AuthenticationSession, nullptr, &homeBudget };
					const auto                metadata = summaryExecution.fetch(semantic_request_for(*server, context, read_operation::get_pull_request));
					if (metadata && json_is_object(metadata->Document)) {
						// The summary is optional, so a malformed change fact
						// leaves the row's change unknown rather than failing it.
						row.Change         = parse_change_facts(json_object(metadata->Document)).value_or(core::queue_change_facts{ });
						auto       limited = false;
						const auto summary = read_home_summary(summaryExecution, aProfile, aRequest, *server, context, *metadata, summaryLimits, &limited);
						if (limited) {
							if (result.Completeness == application::personal_discovery_completeness::complete) {
								result.Completeness = application::personal_discovery_completeness::detail_limit;
							}
							note(core::discovery_completeness_reason::summary_limit);
						}
						if (summary) {
							application::merge_queue_waiting(row.Waiting, *summary);
							if (!summary->Requests) {
								// Optional names do not revoke the complete REST count.
								row.Waiting.RequestsStale = false;
							}
						} else if (fatal_optional_read_failure(summary.error())) {
							return std::unexpected{ summary.error() };
						}
					} else if (!metadata) {
						if (fatal_optional_read_failure(metadata.error())) {
							return std::unexpected{ metadata.error() };
						}
						const auto* error = std::get_if<application::github_provider_error>(&metadata.error());
						if (error && error->Failure == application::github_provider_failure::response_limit) {
							if (result.Completeness == application::personal_discovery_completeness::complete) {
								result.Completeness = application::personal_discovery_completeness::detail_limit;
							}
							note(core::discovery_completeness_reason::summary_limit);
						}
					}
				} else {
					if (result.Completeness == application::personal_discovery_completeness::complete) {
						result.Completeness = application::personal_discovery_completeness::detail_limit;
					}
					note(core::discovery_completeness_reason::summary_limit);
				}
				kept.push_back(std::move(row));
			}
			if (requested && overflow && result.Completeness == application::personal_discovery_completeness::complete) {
				result.Completeness = application::personal_discovery_completeness::detail_limit;
				note(core::discovery_completeness_reason::candidate_limit);
			}
			if (homeBudget.Pages >= 32 && result.Completeness == application::personal_discovery_completeness::complete) {
				if (result.Completeness == application::personal_discovery_completeness::complete) {
					result.Completeness = application::personal_discovery_completeness::detail_limit;
				}
				note(core::discovery_completeness_reason::summary_limit);
			}
			if (work.Completeness == core::discovery_completeness_reason::none) {
				work.Completeness = core::discovery_completeness_reason::complete;
			}
			result.Rows       = std::move(kept);
			result.RateLimits = rates;
			return result;
		} catch (...) {
			return failed(application::github_provider_failure::response_limit, std::nullopt, rates);
		}
	}
}
