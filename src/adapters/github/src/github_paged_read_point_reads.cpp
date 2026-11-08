#include "bpr/core/home_discovery_plan.hpp"
#include "github_http_helpers.hpp"
#include "github_paged_read.hpp"
#include "github_paged_read_page_wire.hpp"
#include "github_paged_read_record_parsing.hpp"
#include "github_review_comment_grouping.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail {
	namespace {
		/// [[BPR-1741]]. One subject's `viewerCanReact` and
		/// `reactionGroups`: at most the eight known contents, each once; a
		/// group's `reactors` names at most five, each a non-empty login of a
		/// user, bot, organization or mannequin (a reactor GitHub no longer
		/// names is counted and not named).
		[[nodiscard]] auto reaction_summary_of(const page_json_object& aSubject) -> std::optional<core::reaction_summary>
		{
			const auto permitted = json_member(aSubject, "viewerCanReact");
			const auto groups    = json_member(aSubject, "reactionGroups");
			if (!json_is_bool(permitted) || !(json_is_array(groups) || json_is_null(groups))) {
				return std::nullopt;
			}
			auto summary = core::reaction_summary{ .ViewerCanReact = json_bool(permitted) };
			if (json_is_null(groups)) {
				return summary;
			}
			const auto answered = json_array(groups);
			if (answered.size() > core::reaction_contents.size()) {
				return std::nullopt;
			}
			for (const auto& value : answered) {
				if (!json_is_object(value)) {
					return std::nullopt;
				}
				const auto group    = json_object(value);
				const auto content  = core::reaction_from_graphql_name(text(json_member(group, "content")));
				const auto viewer   = json_member(group, "viewerHasReacted");
				const auto reactors = json_member(group, "reactors");
				if (!content || !json_is_bool(viewer) || !json_is_object(reactors)) {
					return std::nullopt;
				}
				const auto connection = json_object(reactors);
				const auto total      = uint32(json_member(connection, "totalCount"));
				const auto nodes      = json_member(connection, "nodes");
				if (!total || !json_is_array(nodes)) {
					return std::nullopt;
				}
				auto record = core::reaction_group{ .Content = *content, .Count = *total, .ViewerReacted = json_bool(viewer) };
				for (const auto& node : json_array(nodes)) {
					if (json_is_null(node)) {
						continue;
					}
					if (!json_is_object(node)) {
						return std::nullopt;
					}
					const auto actor = json_object(node);
					if (!one_of(json_member(actor, "__typename"), { "User", "Bot", "Organization", "Mannequin" })) {
						return std::nullopt;
					}
					const auto login = json_member(actor, "login");
					if (json_is_undefined(login)) {
						continue;
					}
					if (!nonempty_string(login) || text(login).size() > 256U) {
						return std::nullopt;
					}
					record.Reactors.push_back(text(login));
				}
				summary.Groups.push_back(std::move(record));
			}
			return core::canonical_reaction_summary(std::move(summary));
		}
	}

	auto perform_github_merge_base_read(
		github_wire_transport_port&                              aTransport,
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		const core::repository_id                                aRepository,
		const github_merge_base_basis&                           aBasis,
		const std::string_view                                   aHead,
		const std::stop_token                                    aStop,
		const github_paged_read_limits&                          aLimits,
		const github_provider_read_authority                     aReadAuthority,
		github_paged_response_observer*                          aObserver) noexcept -> github_merge_base_read_result
	{
		auto rates = application::github_rate_limit_state{ };
		try {
			if (aReadAuthority == github_provider_read_authority::anonymous_public) {
				const auto admission = admit_anonymous_read(read_operation::get_merge_base, aProfile.server_contract());
				if (!admission) {
					return failed(provider_failure_for(admission.error()));
				}
			}
			if (!provider_request_valid(aProfile, aRequest, core::provider_snapshot_id{ 1U }, aLimits, aReadAuthority)
				|| aRepository.value() == 0U
				|| (aRequest.Context.ExpectedRepository && *aRequest.Context.ExpectedRepository != aRepository)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto limits              = aLimits;
			limits.MaximumTotalPages = 1U;
			production_read_execution execution{ aTransport, aProfile, aRequest.Context, limits, aStop, rates, aRequest.AuthenticationSession, aObserver };
			if (!execution.bind_repository(aRepository)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto semantic      = semantic_request_for(*server_contract_for(aProfile), aRequest.Context, read_operation::get_merge_base);
			semantic.BaseOid   = aBasis.BaseRef.empty() ? aBasis.BaseOid : std::string{ };
			semantic.BranchRef = aBasis.BaseRef;
			semantic.HeadOid   = std::string{ aHead };
			auto page          = execution.fetch(semantic);
			if (!page) {
				return std::unexpected{ page.error() };
			}
			if (!json_is_object(page->Document)) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto document  = json_object(page->Document);
			const auto baseValue = json_member(document, "base_commit");
			const auto mergeBase = json_member(document, "merge_base_commit");
			if (!json_is_object(baseValue) || !json_is_object(mergeBase)) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto baseSha  = json_member(json_object(baseValue), "sha");
			const auto mergeSha = json_member(json_object(mergeBase), "sha");
			const auto url      = json_member(document, "url");
			// The answer is bound to what was asked: GitHub echoes the compared
			// pair in `url`, whose head must be exactly the head asked about; a
			// reply about another base commit, or in another object format, is
			// not this pull request's comparison base.
			const auto echoedHead = json_is_string(url) && text(url).ends_with("..." + std::string{ aHead });
			const auto aheadBy    = json_member(document, "ahead_by");
			if (!echoedHead || !hexadecimal_oid(baseSha) || !hexadecimal_oid(mergeSha)
				|| (aBasis.BaseRef.empty() && text(baseSha) != aBasis.BaseOid)
				|| text(mergeSha).size() != aHead.size() || text(baseSha).size() != aHead.size()
				|| (!json_is_undefined(aheadBy) && !exact_integer(aheadBy))) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			return github_merge_base_read{
				.MergeBaseOid  = text(mergeSha),
				.BaseCommitOid = text(baseSha),
				.AheadBy       = json_is_undefined(aheadBy) ? std::nullopt : std::optional<std::uint64_t>{ integer(aheadBy) },
				.RateLimits    = rates,
			};
		} catch (...) {
			return failed(application::github_provider_failure::response_limit, std::nullopt, rates);
		}
	}

	auto parse_reaction_summary(const std::string_view aObjectJson) -> std::optional<core::reaction_summary>
	{
		try {
			const auto document = parse_json_document(aObjectJson);
			if (!document || !json_is_object(*document)) {
				return std::nullopt;
			}
			return reaction_summary_of(json_object(*document));
		} catch (...) {
			return std::nullopt;
		}
	}

	auto perform_github_reaction_read(
		github_wire_transport_port&                              aTransport,
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		const core::repository_id                                aRepository,
		const std::string_view                                   aRepositoryNode,
		const std::string_view                                   aPullRequestNode,
		const std::vector<std::string>&                          aSubjects,
		const std::stop_token                                    aStop,
		const github_paged_read_limits&                          aLimits,
		const github_provider_read_authority                     aReadAuthority,
		github_paged_response_observer*                          aObserver) noexcept -> github_reaction_read_result
	{
		auto rates = application::github_rate_limit_state{ };
		try {
			// Reactions carry the viewer's own state; a signed-out read has no viewer.
			if (aReadAuthority == github_provider_read_authority::anonymous_public) {
				return failed(application::github_provider_failure::unsupported_capability);
			}
			if (!provider_request_valid(aProfile, aRequest, core::provider_snapshot_id{ 1U }, aLimits, aReadAuthority)
				|| aRepository.value() == 0U || aRepositoryNode.empty() || aPullRequestNode.empty()
				|| aSubjects.empty() || aSubjects.size() > maximum_reaction_subjects_per_read
				|| (aRequest.Context.ExpectedRepository && *aRequest.Context.ExpectedRepository != aRepository)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto limits              = aLimits;
			limits.MaximumTotalPages = 1U;
			production_read_execution execution{ aTransport, aProfile, aRequest.Context, limits, aStop, rates, aRequest.AuthenticationSession, aObserver };
			if (!execution.bind_repository(aRepository)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto semantic   = semantic_request_for(*server_contract_for(aProfile), aRequest.Context, read_operation::list_reactions);
			semantic.Nodes  = aSubjects;
			const auto page = execution.fetch(semantic);
			if (!page) {
				return std::unexpected{ page.error() };
			}
			const auto malformed = [&execution] { return execution.terminal_failure(application::github_provider_failure::malformed_response); };
			if (!json_is_object(page->Document)) {
				return malformed();
			}
			const auto data = json_member(json_object(page->Document), "data");
			if (!json_is_object(data)) {
				return malformed();
			}
			const auto fields     = json_object(data);
			const auto repository = json_member(fields, "repository");
			const auto nodes      = json_member(fields, "nodes");
			if (!json_is_object(repository) || !json_is_array(nodes) || text(json_member(json_object(repository), "id")) != aRepositoryNode) {
				return malformed();
			}
			const auto pull = json_member(json_object(repository), "pullRequest");
			if (!json_is_object(pull) || text(json_member(json_object(pull), "id")) != aPullRequestNode) {
				return malformed();
			}
			const auto answered = json_array(nodes);
			if (answered.size() != aSubjects.size()) {
				return malformed();
			}
			auto result = github_reaction_read{ };
			result.Records.reserve(answered.size());
			for (std::size_t index = 0; index < answered.size(); ++index) {
				if (json_is_null(answered[index])) {
					continue;
				}
				if (!json_is_object(answered[index])) {
					return malformed();
				}
				const auto node = json_object(answered[index]);
				if (!one_of(json_member(node, "__typename"), { "PullRequestReviewComment", "IssueComment", "PullRequestReview" })
					|| text(json_member(node, "id")) != aSubjects[index]) {
					return malformed();
				}
				auto summary = reaction_summary_of(node);
				if (!summary) {
					return malformed();
				}
				result.Records.push_back(application::github_reaction_record{ .Subject = aSubjects[index], .Reactions = std::move(*summary) });
			}
			result.RateLimits = std::move(rates);
			return result;
		} catch (...) {
			return failed(application::github_provider_failure::response_limit, std::nullopt, rates);
		}
	}

	auto perform_github_team_membership_read(
		github_wire_transport_port&                              aTransport,
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		const core::repository_id                                aRepository,
		const std::string_view                                   aOrganization,
		const std::string_view                                   aSlug,
		const std::string_view                                   aLogin,
		const std::stop_token                                    aStop,
		const github_paged_read_limits&                          aLimits,
		const github_provider_read_authority                     aReadAuthority,
		github_paged_response_observer*                          aObserver) noexcept -> github_team_membership_read_result
	{
		auto rates = application::github_rate_limit_state{ };
		try {
			// Membership is the viewer's own question; a signed-out read has no viewer.
			if (aReadAuthority == github_provider_read_authority::anonymous_public) {
				return failed(application::github_provider_failure::unsupported_capability);
			}
			if (!provider_request_valid(aProfile, aRequest, core::provider_snapshot_id{ 1U }, aLimits, aReadAuthority)
				|| aRepository.value() == 0U
				|| (aRequest.Context.ExpectedRepository && *aRequest.Context.ExpectedRepository != aRepository)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto limits              = aLimits;
			limits.MaximumTotalPages = 1U;
			production_read_execution execution{ aTransport, aProfile, aRequest.Context, limits, aStop, rates, aRequest.AuthenticationSession, aObserver };
			if (!execution.bind_repository(aRepository)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto semantic         = semantic_request_for(*server_contract_for(aProfile), aRequest.Context, read_operation::get_team_membership);
			semantic.Organization = std::string{ aOrganization };
			semantic.Team         = std::string{ aSlug };
			semantic.Login        = std::string{ aLogin };
			auto page             = execution.fetch(semantic);
			if (!page) {
				return std::unexpected{ page.error() };
			}
			if (!json_is_object(page->Document)) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto data = json_member(json_object(page->Document), "data");
			if (!json_is_object(data)) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto organization = json_member(json_object(data), "organization");
			if (json_is_null(organization)) {
				return github_team_membership_read{ .Member = std::nullopt, .RateLimits = rates };
			}
			if (!json_is_object(organization)) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			const auto teams = json_member(json_object(organization), "teams");
			const auto nodes = json_is_object(teams) ? json_member(json_object(teams), "nodes") : page_json_value{ };
			if (!json_is_array(nodes)) {
				return execution.terminal_failure(application::github_provider_failure::malformed_response);
			}
			// The search matches names and slugs, so only the exact slug, in
			// GitHub's case-insensitive sense, is the requested team.
			const auto same = [](const std::string_view aLeft, const std::string_view aRight) {
				return std::ranges::equal(aLeft, aRight, [](const char aOne, const char aOther) {
					const auto lower = [](const char aChar) { return aChar >= 'A' && aChar <= 'Z' ? static_cast<char>(aChar - 'A' + 'a') : aChar; };
					return lower(aOne) == lower(aOther);
				});
			};
			auto member = false;
			for (const auto& node : json_array(nodes)) {
				if (!json_is_object(node) || !json_is_string(json_member(json_object(node), "slug"))) {
					return execution.terminal_failure(application::github_provider_failure::malformed_response);
				}
				member = member || same(text(json_member(json_object(node), "slug")), aSlug);
			}
			return github_team_membership_read{ .Member = member, .RateLimits = rates };
		} catch (...) {
			return failed(application::github_provider_failure::response_limit, std::nullopt, rates);
		}
	}

	auto perform_github_pull_request_confirmation(
		github_wire_transport_port&                              aTransport,
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		const application::github_pull_request_snapshot&         aSnapshot,
		const std::stop_token                                    aStop,
		const github_paged_read_limits&                          aLimits,
		const github_provider_read_authority                     aReadAuthority,
		github_paged_response_observer*                          aObserver) noexcept -> github_pull_request_confirmation_result
	{
		auto rates = application::github_rate_limit_state{ };
		try {
			if (!provider_request_valid(aProfile, aRequest, core::provider_snapshot_id{ 1U }, aLimits, aReadAuthority)
				|| aSnapshot.Repository.Repository.value() == 0U
				|| (aRequest.Context.ExpectedRepository && *aRequest.Context.ExpectedRepository != aSnapshot.Repository.Repository)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto limits              = aLimits;
			limits.MaximumTotalPages = 1U;
			production_read_execution execution{ aTransport, aProfile, aRequest.Context, limits, aStop, rates, aRequest.AuthenticationSession, aObserver };
			if (!execution.bind_repository(aSnapshot.Repository.Repository)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto page = execution.fetch(semantic_request_for(*server_contract_for(aProfile), aRequest.Context, read_operation::get_pull_request));
			if (!page) {
				return std::unexpected{ page.error() };
			}
			auto pullRequest = parse_pull_request(*page, aRequest.Context, aSnapshot.Repository, aLimits.Records);
			if (!pullRequest) {
				return std::unexpected{ execution.enrich(pullRequest.error()) };
			}
			const auto& now = pullRequest->Record;
			const auto& was = aSnapshot.PullRequest;
			return github_pull_request_confirmation{
				.Unchanged = now.PullRequest == was.PullRequest && now.HeadOid == was.HeadOid && now.BaseOid == was.BaseOid
					&& now.BaseRef == was.BaseRef && now.State == was.State && pullRequest->ChangedFiles == aSnapshot.Files.size()
					// [[BPR-1572]]. The commit count the stacked-base rule
					// compares with `ahead_by` must be the snapshot's too.
					&& now.ReportedCommitCount == was.ReportedCommitCount,
				.RateLimits = rates,
			};
		} catch (...) {
			return failed(application::github_provider_failure::response_limit, std::nullopt, rates);
		}
	}

	auto perform_markdown_image_resolution(github_wire_transport_port& aTransport, const validated_github_host_profile& aProfile,
										   const application::markdown_image_resolution_request& aRequest, const std::stop_token aStop,
										   const github_paged_read_limits& aLimits) noexcept -> github_markdown_image_resolution_result
	{
		try {
			const auto server        = aProfile.server_contract() == application::github_server_contract::github_com_2022_11_28 ? server_contract::github_com : server_contract::ghes_3_18;
			auto       rates         = application::github_rate_limit_state{ };
			auto       limits        = aLimits;
			limits.MaximumTotalPages = 1U;
			production_read_execution execution{ aTransport, aProfile, aRequest.Context, limits, aStop, rates, aRequest.AuthenticationSession, nullptr };
			auto                      semantic = semantic_request_for(server, aRequest.Context, read_operation::resolve_body_images);
			semantic.Nodes                     = aRequest.Nodes;
			const auto page                    = execution.fetch(semantic);
			if (!page) {
				return std::unexpected{ page.error() };
			}
			const auto malformed = [&execution] { return execution.terminal_failure(application::github_provider_failure::malformed_response); };
			if (!json_is_object(page->Document)) {
				return malformed();
			}
			const auto data = json_member(json_object(page->Document), "data");
			if (!json_is_object(data)) {
				return malformed();
			}
			const auto fields     = json_object(data);
			const auto repository = json_member(fields, "repository");
			const auto nodes      = json_member(fields, "nodes");
			if (!json_is_object(repository) || !json_is_array(nodes) || text(json_member(json_object(repository), "id")) != aRequest.RepositoryNode) {
				return malformed();
			}
			const auto pull = json_member(json_object(repository), "pullRequest");
			if (!json_is_object(pull) || text(json_member(json_object(pull), "id")) != aRequest.PullRequestNode) {
				return malformed();
			}
			const auto answered = json_array(nodes);
			if (answered.size() != aRequest.Nodes.size()) {
				return malformed();
			}
			auto result = github_markdown_image_resolution{ };
			for (std::size_t index = 0; index < answered.size(); ++index) {
				auto body = application::markdown_image_resolved_body{ .Node = aRequest.Nodes[index], .Served = { } };
				if (!json_is_null(answered[index])) {
					if (!json_is_object(answered[index])) {
						return malformed();
					}
					const auto node = json_object(answered[index]);
					const auto html = json_member(node, "bodyHTML");
					if (!one_of(json_member(node, "__typename"), { "PullRequest", "IssueComment", "PullRequestReview", "PullRequestReviewComment" })
						|| text(json_member(node, "id")) != aRequest.Nodes[index] || !json_is_string(html)) {
						return malformed();
					}
					if (auto served = core::served_markdown_images(text(html))) {
						body.Served = std::move(*served);
					}
				}
				result.Bodies.push_back(std::move(body));
			}
			result.RateLimits = std::move(rates);
			return result;
		} catch (...) {
			return std::unexpected{ github_paged_read_failure{ provider_error(application::github_provider_failure::response_limit) } };
		}
	}
}
