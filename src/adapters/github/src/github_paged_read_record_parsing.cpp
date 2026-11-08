#include "github_paged_read_record_parsing.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <initializer_list>
#include <map>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <variant>

namespace bpr::adapters::github::detail {
	auto actor_kind(const std::string_view aType) noexcept
		-> application::github_actor_kind
	{
		if (aType == "Bot") {
			return application::github_actor_kind::bot;
		}
		return aType == "User" ? application::github_actor_kind::user
							   : application::github_actor_kind::other;
	}

	auto parse_rest_actor(const page_json_value& aValue)
		-> std::optional<application::github_actor_record>
	{
		if (json_is_null(aValue)) {
			return std::optional<application::github_actor_record>{ };
		}
		if (!json_is_object(aValue)
			|| !has_required_keys(json_object(aValue), { "login", "node_id" })
			|| !nonempty_string(json_member(json_object(aValue), "login"))
			|| !nonempty_string(json_member(json_object(aValue), "node_id"))) {
			return std::nullopt;
		}
		const auto object = json_object(aValue);
		// REST already holds a bot in the canonical `<name>[bot]` form, so
		// only the stated type is recorded. An absent or non-string `type`
		// is no type evidence.
		const auto type = json_member(object, "type");
		return application::github_actor_record{
			.Actor = application::github_actor_id{ text(json_member(object, "node_id")) },
			.Login = text(json_member(object, "login")),
			.Kind  = json_is_string(type) ? actor_kind(text(type)) : application::github_actor_kind::unobserved,
		};
	}

	auto optional_rest_actor(const page_json_value& aValue)
		-> optional_actor_parse
	{
		if (json_is_null(aValue)) {
			return { .Valid = true, .Actor = std::nullopt };
		}
		const auto actor = parse_rest_actor(aValue);
		return actor.has_value()
			? optional_actor_parse{ .Valid = true, .Actor = actor }
			: optional_actor_parse{ };
	}

	auto optional_graphql_actor(const page_json_value& aValue)
		-> optional_actor_parse
	{
		if (json_is_null(aValue)) {
			return { .Valid = true, .Actor = std::nullopt };
		}
		if (!json_is_object(aValue)
			|| (!has_exact_keys(json_object(aValue), { "id", "login" })
				&& !(has_exact_keys(json_object(aValue), { "__typename", "id", "login" })
					 && nonempty_string(json_member(json_object(aValue), "__typename"))))
			|| !nonempty_string(json_member(json_object(aValue), "id"))
			|| !nonempty_string(json_member(json_object(aValue), "login"))) {
			return { };
		}
		const auto object   = json_object(aValue);
		const auto typeName = json_member(object, "__typename");
		const auto kind     = json_is_string(typeName) ? actor_kind(text(typeName)) : application::github_actor_kind::unobserved;
		auto       login    = text(json_member(object, "login"));
		// [[BPR-1321]] GitHub's GraphQL `Bot.login` omits the `[bot]` suffix
		// that REST carries for the same node. Only a typed `Bot` takes the
		// REST form; snapshot validation still compares the result exactly
		// and refuses it against any observation not typed as a bot.
		constexpr std::string_view botSuffix = "[bot]";
		if (kind == application::github_actor_kind::bot && !login.ends_with(botSuffix)) {
			login += botSuffix;
		}
		return {
			.Valid = true,
			.Actor = application::github_actor_record{
				.Actor = application::github_actor_id{ text(json_member(object, "id")) },
				.Login = std::move(login),
				.Kind  = kind,
			},
		};
	}

	auto pull_request_state(const QString& aState,
							const bool     aMerged) noexcept
		-> application::github_pull_request_state
	{
		if (aMerged) {
			return application::github_pull_request_state::merged;
		}
		return aState == QStringLiteral("open")
			? application::github_pull_request_state::open
			: application::github_pull_request_state::closed;
	}

	auto review_state(const QString& aValue) noexcept
		-> application::github_review_state
	{
		if (aValue == QStringLiteral("PENDING")) {
			return application::github_review_state::pending;
		}
		if (aValue == QStringLiteral("COMMENTED")) {
			return application::github_review_state::commented;
		}
		if (aValue == QStringLiteral("APPROVED")) {
			return application::github_review_state::approved;
		}
		if (aValue == QStringLiteral("CHANGES_REQUESTED")) {
			return application::github_review_state::changes_requested;
		}
		return application::github_review_state::dismissed;
	}

	auto check_status(const QString& aValue) noexcept
		-> application::github_check_status
	{
		if (aValue == QStringLiteral("queued")) {
			return application::github_check_status::queued;
		}
		if (aValue == QStringLiteral("in_progress")) {
			return application::github_check_status::in_progress;
		}
		if (aValue == QStringLiteral("completed")) {
			return application::github_check_status::completed;
		}
		if (aValue == QStringLiteral("waiting")) {
			return application::github_check_status::waiting;
		}
		if (aValue == QStringLiteral("requested")) {
			return application::github_check_status::requested;
		}
		return application::github_check_status::pending;
	}

	auto check_conclusion(const page_json_value& aValue) noexcept
		-> application::github_check_conclusion
	{
		if (json_is_null(aValue)) {
			return application::github_check_conclusion::none;
		}
		const auto conclusion = json_text(aValue);
		if (conclusion == QStringLiteral("success")) {
			return application::github_check_conclusion::success;
		}
		if (conclusion == QStringLiteral("failure")) {
			return application::github_check_conclusion::failure;
		}
		if (conclusion == QStringLiteral("neutral")) {
			return application::github_check_conclusion::neutral;
		}
		if (conclusion == QStringLiteral("cancelled")) {
			return application::github_check_conclusion::cancelled;
		}
		if (conclusion == QStringLiteral("skipped")) {
			return application::github_check_conclusion::skipped;
		}
		if (conclusion == QStringLiteral("timed_out")) {
			return application::github_check_conclusion::timed_out;
		}
		if (conclusion == QStringLiteral("action_required")) {
			return application::github_check_conclusion::action_required;
		}
		return application::github_check_conclusion::stale;
	}

	auto bounded_graphql_token(const std::string_view aValue,
							   const std::size_t      aMaximum) noexcept
		-> bool
	{
		return !aValue.empty() && aValue.size() <= aMaximum
			&& std::ranges::all_of(aValue,
								   [](const char aCharacter) {
									   const auto byte = static_cast<unsigned char>(aCharacter);
									   return byte >= 0x21U && byte <= 0x7eU;
								   });
	}

	auto
	parse_graphql_connection(const page_json_value&          aValue,
							 const github_paged_read_limits& aLimits)
		-> std::expected<graphql_connection_page, github_paged_read_failure>
	{
		if (!json_is_object(aValue)
			|| !has_exact_keys(json_object(aValue),
							   { "nodes", "pageInfo", "totalCount" })) {
			return failed(application::github_provider_failure::malformed_response);
		}
		const auto connection = json_object(aValue);
		if (!exact_integer(json_member(connection, "totalCount"))
			|| !json_is_array(json_member(connection, "nodes"))
			|| !json_is_object(json_member(connection, "pageInfo"))
			|| !has_exact_keys(json_object(json_member(connection, "pageInfo")),
							   { "endCursor", "hasNextPage" })) {
			return failed(application::github_provider_failure::malformed_response);
		}
		const auto nodes    = json_array(json_member(connection, "nodes"));
		const auto pageInfo = json_object(json_member(connection, "pageInfo"));
		if (static_cast<std::size_t>(nodes.size()) > aLimits.MaximumPageRecords
			|| !json_is_bool(json_member(pageInfo, "hasNextPage"))) {
			return failed(application::github_provider_failure::response_limit);
		}
		const auto                 total       = integer(json_member(connection, "totalCount"));
		const auto                 hasNext     = json_bool(json_member(pageInfo, "hasNextPage"));
		const auto                 cursorValue = json_member(pageInfo, "endCursor");
		std::optional<std::string> nextCursor;
		if (hasNext) {
			if (static_cast<std::size_t>(nodes.size()) != 100U
				|| !json_is_string(cursorValue)) {
				return failed(
					application::github_provider_failure::malformed_response);
			}
			auto cursor = text(cursorValue);
			if (!bounded_graphql_token(cursor, aLimits.MaximumCursorBytes)
				|| total <= static_cast<std::uint64_t>(nodes.size())) {
				return failed(
					application::github_provider_failure::malformed_response);
			}
			nextCursor = std::move(cursor);
		} else if ((nodes.empty() && !json_is_null(cursorValue))
				   || (!nodes.empty()
					   && (!json_is_string(cursorValue)
						   || !bounded_graphql_token(text(cursorValue),
													 aLimits.MaximumCursorBytes)))) {
			return failed(application::github_provider_failure::malformed_response);
		}
		if (total < static_cast<std::uint64_t>(nodes.size())) {
			return failed(application::github_provider_failure::malformed_response);
		}
		return graphql_connection_page{
			.TotalCount = total,
			.Nodes      = nodes,
			.NextCursor = std::move(nextCursor),
		};
	}

	auto parse_comment_observation(const page_json_value& aValue, bool aGraphql)
		-> std::expected<application::github_comment_observation, github_paged_read_failure>
	{
		const auto                              object = json_object(aValue);
		application::github_comment_observation result;
		const auto                              scalar = [&](std::string_view aKey, std::string& aOut) {
			const auto value = json_member(object, aKey);
			if (json_is_undefined(value) || json_is_null(value)) {
				return true;
			}
			if (!json_is_string(value)) {
				return false;
			}
			aOut = text(value);
			return true;
		};
		if (!scalar(aGraphql ? "url" : "html_url", result.WebUrl) || !scalar(aGraphql ? "updatedAt" : "updated_at", result.UpdatedAt)
			|| !scalar(aGraphql ? "createdAt" : "created_at", result.CreatedAt)) {
			return failed(application::github_provider_failure::malformed_response);
		}
		if (result.CreatedAt.empty() && !aGraphql && !scalar("submitted_at", result.CreatedAt)) {
			return failed(application::github_provider_failure::malformed_response);
		}
		const auto id = json_member(object, aGraphql ? "fullDatabaseId" : "id");
		if (!json_is_undefined(id) && !json_is_null(id)) {
			if (exact_integer(id, true)) {
				result.DatabaseId = integer(id);
			} else if (json_is_string(id)) {
				const auto digits = text(id);
				const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), result.DatabaseId);
				if (digits.empty() || digits.front() == '0' || parsed.ec != std::errc{ } || parsed.ptr != digits.data() + digits.size() || !result.DatabaseId) {
					return failed(application::github_provider_failure::malformed_response);
				}
			} else {
				return failed(application::github_provider_failure::malformed_response);
			}
		}
		const auto body = json_member(object, "body");
		if (!json_is_undefined(body)) {
			if (!json_is_string(body)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			result.RawBody = text(body);
		}
		if (aGraphql) {
			for (const auto& entry : { std::pair{ "commit", &result.Commit }, std::pair{ "originalCommit", &result.OriginalCommit }, std::pair{ "pullRequestReview", &result.ReviewNode } }) {
				const auto value = json_member(object, entry.first);
				if (json_is_undefined(value) || json_is_null(value)) {
					continue;
				}
				const auto key = std::string_view{ entry.first } == "pullRequestReview" ? "id" : "oid";
				if (!json_is_object(value) || !has_exact_keys(json_object(value), { key }) || !nonempty_string(json_member(json_object(value), key))) {
					return failed(application::github_provider_failure::malformed_response);
				}
				*entry.second = text(json_member(json_object(value), key));
			}
		} else if (!scalar("commit_id", result.Commit) || !scalar("original_commit_id", result.OriginalCommit)) {
			return failed(application::github_provider_failure::malformed_response);
		}
		for (const auto& entry : { std::pair{ aGraphql ? "startLine" : "start_line", &result.StartLine }, std::pair{ aGraphql ? "originalStartLine" : "original_start_line", &result.OriginalStartLine } }) {
			const auto value = json_member(object, entry.first);
			if (json_is_undefined(value) || json_is_null(value)) {
				continue;
			}
			if (!valid_line(value)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			*entry.second = optional_line(value);
		}
		return result;
	}

	auto graphql_repository_and_pull_request(
		const parsed_page&                             aPage,
		const application::github_repository_record&   aRepository,
		const application::github_pull_request_record& aPullRequest,
		page_json_object*                              aDataOutput,
		page_json_object*                              aRepositoryOutput,
		page_json_object*                              aPullRequestOutput)
		-> std::expected<void, github_paged_read_failure>
	{
		if (!singleton_complete(aPage) || !json_is_object(aPage.Document)
			|| !graphql_success_envelope(json_object(aPage.Document))) {
			return failed(application::github_provider_failure::malformed_response);
		}
		const auto dataValue = json_member(json_object(aPage.Document), "data");
		if (!json_is_object(dataValue)
			|| !has_exact_keys(json_object(dataValue), { "repository" })) {
			return failed(application::github_provider_failure::malformed_response);
		}
		const auto repositoryValue = json_member(json_object(dataValue), "repository");
		if (!json_is_object(repositoryValue)
			|| !has_exact_keys(json_object(repositoryValue), { "id", "pullRequest" })
			|| text(json_member(json_object(repositoryValue), "id"))
				!= aRepository.Node.value()) {
			return failed(application::github_provider_failure::malformed_response);
		}
		const auto pullRequestValue = json_member(json_object(repositoryValue), "pullRequest");
		if (!json_is_object(pullRequestValue)
			|| !has_required_keys(
				json_object(pullRequestValue), { "headRefOid", "id" })
			|| text(json_member(json_object(pullRequestValue), "id"))
				!= aPullRequest.Node.value()
			|| text(json_member(json_object(pullRequestValue), "headRefOid"))
				!= aPullRequest.HeadOid.value()) {
			return failed(application::github_provider_failure::malformed_response);
		}
		*aDataOutput        = json_object(dataValue);
		*aRepositoryOutput  = json_object(repositoryValue);
		*aPullRequestOutput = json_object(pullRequestValue);
		return { };
	}

	auto provider_request_valid(
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		const core::provider_snapshot_id                         aSnapshot,
		const github_paged_read_limits&                          aLimits,
		const github_provider_read_authority                     aReadAuthority) noexcept -> bool
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
		// RFC-BPR-0017. The signed-out reader, decided once. Two groups of
		// terms below describe an account and a probe, and a signed-out
		// read has neither; every other term still applies to it exactly
		// as written, and this one is false unless the whole synthesised
		// shape holds under the authority that is allowed to present it.
		const auto signedOut = aReadAuthority
				== github_provider_read_authority::anonymous_public
			&& signed_out_read_identity(aRequest);
		return limits_valid(aLimits) && aSnapshot.value() != 0U
			&& (!aRequest.AuthenticationSession || (aRequest.AuthenticationSession->value() != 0U && aReadAuthority != github_provider_read_authority::anonymous_public))
			&& application::detail::valid_context(context)
			&& application::valid(authority) && aRequest.Attempt.valid()
			// An anonymous read holds no account, so it holds no account
			// transport lease to present. Every other term below still
			// applies to it; only the lease is authority-specific, and the
			// authenticated authority keeps requiring it.
			&& (aReadAuthority
					== github_provider_read_authority::anonymous_public
				|| aRequest.AccountTransport != nullptr)
			// The reader. Named exactly, or named not at all: a
			// signed-out read carries an empty viewer, which is the
			// identity the accepted snapshot is required to carry back.
			&& (signedOut
				|| (aRequest.Viewer.Account.value() != 0U
					&& bounded_text(aRequest.Viewer.Node.value(),
									aLimits.Records.MaximumIdentityBytes,
									true)
					&& bounded_text(aRequest.Viewer.Login,
									aLimits.Records.MaximumLoginBytes,
									true)))
			&& aRequest.Viewer.Account == context.Account
			&& bounded_text(aRequest.Viewer.DisplayName,
							aLimits.Records.MaximumDisplayNameBytes)
			&& authority.Profile == context.Profile
			&& authority.Profile == aProfile.profile()
			&& authority.ProfileRevision == aProfile.revision()
			&& authority.Host == context.Host && authority.Host == aProfile.host()
			&& authority.Hostname.value() == aProfile.hostname()
			&& authority.ServerContract == aProfile.server_contract()
			&& authority.Authentication == aProfile.authentication_transport()
			&& (signedOut || application::valid(observation))
			&& evidence.Authority.Profile == authority.Profile
			&& evidence.Authority.ProfileRevision == authority.ProfileRevision
			&& evidence.Authority.Host == authority.Host
			&& evidence.Authority.Account == context.Account
			&& evidence.Authority.ServerContract == authority.ServerContract
			&& evidence.Authority.ProbeContractRevision
			== authority.ProbeContractRevision
			&& evidence.Revision == context.Capabilities
			// The probe. Observed within the freshness window and
			// projected from what it observed, or -- signed out -- never
			// observed at all, which `signed_out_read_identity` has
			// already checked term by term.
			&& (signedOut
				|| (evidence.ExpiresBefore > evidence.ObservedAt
					&& evidence.ExpiresBefore.value()
							- evidence.ObservedAt.value()
						<= application::
							maximum_github_capability_freshness_window
					&& evidence.Capabilities
						== application::projected_capabilities(observation)
					&& evidence.ReadShape
						== application::projected_read_shape(observation)));
	}

	auto parse_timeline_timestamp(const std::string_view aValue) -> std::optional<std::int64_t>
	{
		// REST's UTC second-resolution event timestamp; no locale or clock.
		if (aValue.size() != 20U || aValue[4] != '-' || aValue[7] != '-'
			|| aValue[10] != 'T' || aValue[13] != ':' || aValue[16] != ':' || aValue[19] != 'Z') {
			return std::nullopt;
		}
		const auto number = [aValue](const std::size_t aStart, const std::size_t aSize) -> std::optional<unsigned> {
			const auto part = aValue.substr(aStart, aSize);
			if (!std::ranges::all_of(part, [](const char aChar) { return aChar >= '0' && aChar <= '9'; })) {
				return std::nullopt;
			}
			unsigned   result = 0U;
			const auto parsed = std::from_chars(part.data(), part.data() + part.size(), result);
			return parsed.ec == std::errc{ } ? std::optional{ result } : std::nullopt;
		};
		const auto year = number(0U, 4U), month = number(5U, 2U), day = number(8U, 2U), hour = number(11U, 2U), minute = number(14U, 2U), second = number(17U, 2U);
		if (!year || !month || !day || !hour || !minute || !second || *year < 1970U || *hour > 23U || *minute > 59U || *second > 59U) {
			return std::nullopt;
		}
		const auto date = std::chrono::year{ static_cast<int>(*year) } / std::chrono::month{ *month } / std::chrono::day{ *day };
		if (!date.ok()) {
			return std::nullopt;
		}
		return std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::sys_days{ date } + std::chrono::hours{ *hour } + std::chrono::minutes{ *minute } + std::chrono::seconds{ *second }).time_since_epoch()).count();
	}

	auto parse_pull_ref(const page_json_value& aValue,
						const bool             aRepositoryRequired)
		-> std::optional<parsed_pull_ref>
	{
		if (!json_is_object(aValue)
			|| !has_required_keys(json_object(aValue), { "ref", "repo", "sha" })) {
			return std::nullopt;
		}
		const auto object = json_object(aValue);
		if (!nonempty_string(json_member(object, "ref"))
			|| !hexadecimal_oid(json_member(object, "sha"))) {
			return std::nullopt;
		}
		parsed_pull_ref result{
			.Ref        = text(json_member(object, "ref")),
			.Oid        = text(json_member(object, "sha")),
			.Repository = std::nullopt,
			.FullName   = std::nullopt,
		};
		const auto repository = json_member(object, "repo");
		if (json_is_null(repository)) {
			return aRepositoryRequired ? std::nullopt
									   : std::optional{ std::move(result) };
		}
		if (!json_is_object(repository)
			|| !has_required_keys(json_object(repository), { "full_name", "id" })
			|| !exact_integer(json_member(json_object(repository), "id"), true)
			|| !nonempty_string(json_member(json_object(repository), "full_name"))) {
			return std::nullopt;
		}
		result.Repository = core::repository_id{ integer(json_member(json_object(repository), "id")) };
		result.FullName   = text(json_member(json_object(repository), "full_name"));
		return result;
	}

	auto
	parse_pull_request(const parsed_page&                           aPage,
					   const core::pull_request_request_context&    aContext,
					   const application::github_repository_record& aRepository,
					   const application::github_read_limits&       aLimits)
		-> std::expected<parsed_pull_request, github_paged_read_failure>
	{
		if (!singleton_complete(aPage) || !json_is_object(aPage.Document)
			|| !has_required_keys(json_object(aPage.Document),
								  {
									  "base",
									  "body",
									  "changed_files",
									  "commits",
									  "draft",
									  "head",
									  "id",
									  "merged",
									  "node_id",
									  "number",
									  "state",
									  "title",
									  "user",
								  })) {
			return failed(application::github_provider_failure::malformed_response);
		}
		const auto object = json_object(aPage.Document);
		const auto base   = parse_pull_ref(json_member(object, "base"), true);
		const auto head   = parse_pull_ref(json_member(object, "head"), false);
		const auto author = optional_rest_actor(json_member(object, "user"));
		if (!exact_integer(json_member(object, "id"), true)
			|| !exact_integer(json_member(object, "number"), true)
			|| integer(json_member(object, "number")) > core::maximum_pull_request_number
			|| !exact_integer(json_member(object, "changed_files"))
			|| !exact_integer(json_member(object, "commits"), true)
			|| !nonempty_string(json_member(object, "node_id"))
			|| !nonempty_string(json_member(object, "title"))
			|| (!json_is_null(json_member(object, "body")) && !json_is_string(json_member(object, "body")))
			|| !one_of(json_member(object, "state"), { "open", "closed" })
			|| !json_is_bool(json_member(object, "draft")) || !json_is_bool(json_member(object, "merged"))
			|| !base.has_value() || !head.has_value() || !author.Valid
			|| (json_bool(json_member(object, "merged"))
				&& json_text(json_member(object, "state")) != QStringLiteral("closed"))) {
			return failed(application::github_provider_failure::malformed_response);
		}
		const auto changedFiles = integer(json_member(object, "changed_files"));
		const auto commits      = integer(json_member(object, "commits"));
		if (changedFiles > aLimits.MaximumFiles
			|| commits > std::numeric_limits<std::size_t>::max()) {
			return failed(application::github_provider_failure::response_limit);
		}
		const auto id     = core::pull_request_id{ integer(json_member(object, "id")) };
		const auto number = core::pull_request_number{
			static_cast<std::uint32_t>(integer(json_member(object, "number")))
		};
		const auto repositoryFullName = aRepository.Owner + "/" + aRepository.Name;
		if (number != aContext.Number || !base->Repository.has_value()
			|| *base->Repository != aRepository.Repository
			|| !base->FullName.has_value()
			|| !ascii_equal_fold(*base->FullName, repositoryFullName)
			|| (aContext.ExpectedPullRequest.has_value()
				&& *aContext.ExpectedPullRequest != id)) {
			return failed(application::github_provider_failure::malformed_response);
		}
		return parsed_pull_request{
			.Record = application::github_pull_request_record{
				.PullRequest         = id,
				.Repository          = aRepository.Repository,
				.Number              = number,
				.Node                = application::github_node_id{ text(json_member(object, "node_id")) },
				.Title               = text(json_member(object, "title")),
				.Body                = json_is_null(json_member(object, "body"))
					? std::string{ }
					: text(json_member(object, "body")),
				.State               = pull_request_state(json_text(json_member(object, "state")),
														  json_bool(json_member(object, "merged"))),
				.Draft               = json_bool(json_member(object, "draft")),
				.Author              = author.Actor,
				.BaseOid             = application::commit_oid{ base->Oid },
				.HeadOid             = application::commit_oid{ head->Oid },
				.BaseRef             = base->Ref,
				.HeadRef             = head->Ref,
				.ReportedCommitCount = static_cast<std::size_t>(commits),
			},
			.ChangedFiles = static_cast<std::size_t>(changedFiles),
			.Commits      = static_cast<std::size_t>(commits),
		};
	}

	auto parse_review(const page_json_value& aValue)
		-> std::expected<application::github_review_record,
						 github_paged_read_failure>
	{
		if (!json_is_object(aValue)
			|| !has_required_keys(json_object(aValue),
								  { "body",
									"commit_id",
									"id",
									"node_id",
									"state",
									"user" })) {
			return failed(application::github_provider_failure::malformed_response);
		}
		const auto object = json_object(aValue);
		const auto author = optional_rest_actor(json_member(object, "user"));
		if (!exact_integer(json_member(object, "id"), true)
			|| !nonempty_string(json_member(object, "node_id"))
			|| !json_is_string(json_member(object, "body")) || !author.Valid
			|| !one_of(json_member(object, "state"),
					   {
						   "PENDING",
						   "COMMENTED",
						   "APPROVED",
						   "CHANGES_REQUESTED",
						   "DISMISSED",
					   })
			|| (!json_is_null(json_member(object, "commit_id"))
				&& !hexadecimal_oid(json_member(object, "commit_id")))) {
			return failed(application::github_provider_failure::malformed_response);
		}
		std::optional<application::commit_oid> commit;
		if (!json_is_null(json_member(object, "commit_id"))) {
			commit = application::commit_oid{ text(json_member(object, "commit_id")) };
		}
		const auto observation = parse_comment_observation(aValue, false);
		if (!observation) {
			return std::unexpected(observation.error());
		}

		return application::github_review_record{
			.DatabaseId  = integer(json_member(object, "id")),
			.Review      = application::github_review_id{ text(json_member(object, "node_id")) },
			.Author      = author.Actor,
			.State       = review_state(json_text(json_member(object, "state"))),
			.Commit      = std::move(commit),
			.Body        = text(json_member(object, "body")),
			.Observation = *observation,
		};
	}

	auto parse_check(const page_json_value& aValue,
					 const std::string_view aHeadOid)
		-> std::expected<application::github_check_record,
						 github_paged_read_failure>
	{
		if (!json_is_object(aValue)
			|| !has_required_keys(json_object(aValue),
								  {
									  "conclusion",
									  "head_sha",
									  "name",
									  "node_id",
									  "status",
								  })) {
			return failed(application::github_provider_failure::malformed_response);
		}
		const auto object    = json_object(aValue);
		const auto completed = json_text(json_member(object, "status")) == QStringLiteral("completed");
		if (!nonempty_string(json_member(object, "node_id"))
			|| !nonempty_string(json_member(object, "name"))
			|| !hexadecimal_oid(json_member(object, "head_sha"))
			|| text(json_member(object, "head_sha")) != aHeadOid
			|| !one_of(json_member(object, "status"),
					   {
						   "queued",
						   "in_progress",
						   "completed",
						   "waiting",
						   "requested",
						   "pending",
					   })
			|| (!json_is_null(json_member(object, "conclusion"))
				&& !one_of(json_member(object, "conclusion"),
						   {
							   "success",
							   "failure",
							   "neutral",
							   "cancelled",
							   "skipped",
							   "timed_out",
							   "action_required",
							   "stale",
						   }))
			|| (completed != !json_is_null(json_member(object, "conclusion")))) {
			return failed(application::github_provider_failure::malformed_response);
		}
		auto result             = application::github_check_record{ };
		result.Check            = application::github_check_id{ text(json_member(object, "node_id")) };
		result.Name             = text(json_member(object, "name"));
		result.Status           = check_status(json_text(json_member(object, "status")));
		result.Conclusion       = check_conclusion(json_member(object, "conclusion"));
		result.EvaluatedOid     = application::commit_oid{ std::string{ aHeadOid } };
		const auto optionalText = [](const page_json_value& aField) { return json_is_string(aField) ? text(aField) : std::string{ }; };
		result.DetailsUrl       = optionalText(json_member(object, "details_url"));
		if (result.DetailsUrl.empty()) {
			result.DetailsUrl = optionalText(json_member(object, "html_url"));
		}
		if (const auto output = json_member(object, "output"); json_is_object(output)) {
			result.Description = optionalText(json_member(json_object(output), "title"));
			const auto summary = optionalText(json_member(json_object(output), "summary"));
			if (!summary.empty()) {
				result.Description += (result.Description.empty() ? "" : "\n") + summary;
			}
		}
		if (const auto app = json_member(object, "app"); json_is_object(app)) {
			result.Producer = optionalText(json_member(json_object(app), "slug"));
			if (exact_integer(json_member(json_object(app), "id"), true)) {
				result.AppId = integer(json_member(json_object(app), "id"));
			}
		}
		return result;
	}

	auto parse_commit_status(const page_json_value& aValue, const std::string_view aHeadOid)
		-> std::expected<application::github_check_record, github_paged_read_failure>
	{
		if (!json_is_object(aValue)) {
			return failed(application::github_provider_failure::malformed_response);
		}
		const auto object = json_object(aValue);
		if (!nonempty_string(json_member(object, "node_id")) || !nonempty_string(json_member(object, "context"))
			|| !one_of(json_member(object, "state"), { "pending", "success", "failure", "error" })) {
			return failed(application::github_provider_failure::malformed_response);
		}
		auto result            = application::github_check_record{ };
		result.Check           = application::github_check_id{ "status:" + text(json_member(object, "node_id")) };
		result.Name            = text(json_member(object, "context"));
		const auto state       = text(json_member(object, "state"));
		result.Status          = state == "pending" ? application::github_check_status::pending : application::github_check_status::completed;
		result.Conclusion      = state == "pending" ? application::github_check_conclusion::none : state == "success" ? application::github_check_conclusion::success
																													  : application::github_check_conclusion::failure;
		result.Source          = application::github_check_source::commit_status;
		result.StatusError     = state == "error";
		result.EvaluatedOid    = application::commit_oid{ std::string{ aHeadOid } };
		const auto description = json_member(object, "description");
		const auto target      = json_member(object, "target_url");
		if ((!json_is_null(description) && !json_is_string(description)) || (!json_is_null(target) && !json_is_string(target))) {
			return failed(application::github_provider_failure::malformed_response);
		}
		if (json_is_string(description)) {
			result.Description = text(description);
		}
		if (json_is_string(target)) {
			result.DetailsUrl = text(target);
		}
		if (const auto creator = json_member(object, "creator"); json_is_object(creator)) {
			const auto login = json_member(json_object(creator), "login");
			if (json_is_string(login)) {
				result.Producer = text(login);
			}
		}
		return result;
	}

	auto fatal_optional_read_failure(const github_paged_read_failure& aFailure) -> bool
	{
		if (std::holds_alternative<github_paged_read_cancelled>(aFailure)) {
			return true;
		}
		const auto* error = std::get_if<application::github_provider_error>(&aFailure);
		// Lost authentication or account binding invalidates the entire read,
		// even when only an optional explanation discovered it.
		return error && error->Failure == application::github_provider_failure::authentication;
	}

	auto observation_failure(const github_paged_read_failure& aFailure) -> observation
	{
		const auto* failure = std::get_if<application::github_provider_error>(&aFailure);
		if (!failure) {
			return observation::failed;
		}
		switch (failure->Failure) {
		case application::github_provider_failure::permission:
		case application::github_provider_failure::authentication:         return observation::permission;
		case application::github_provider_failure::api_version:
		case application::github_provider_failure::unsupported_capability: return observation::unsupported;
		case application::github_provider_failure::response_limit:
		case application::github_provider_failure::malformed_response:     return observation::incomplete;
		default:                                                           return observation::failed;
		}
	}

	auto merge_pull_object(const parsed_page& aPage, const application::github_pull_request_snapshot& aSnapshot)
		-> std::expected<page_json_object, github_paged_read_failure>
	{
		auto data = page_json_object{ }, repository = page_json_object{ }, pull = page_json_object{ };
		if (const auto identity = graphql_repository_and_pull_request(aPage, aSnapshot.Repository, aSnapshot.PullRequest, &data, &repository, &pull); !identity) {
			return std::unexpected{ identity.error() };
		}
		if (text(json_member(pull, "baseRefOid")) != aSnapshot.PullRequest.BaseOid.value() || text(json_member(pull, "baseRefName")) != aSnapshot.PullRequest.BaseRef) {
			return failed(application::github_provider_failure::malformed_response);
		}
		return pull;
	}

	auto apply_merge_fields(const page_json_object& aPull, application::github_pull_request_snapshot& aSnapshot)
		-> std::expected<void, github_paged_read_failure>
	{
		const auto state = json_member(aPull, "mergeStateStatus"), review = json_member(aPull, "reviewDecision"), automatic = json_member(aPull, "autoMergeRequest");
		if (!one_of(state, { "UNKNOWN", "BEHIND", "BLOCKED", "CLEAN", "DIRTY", "DRAFT", "HAS_HOOKS", "UNSTABLE" }) || (!json_is_null(review) && !one_of(review, { "APPROVED", "CHANGES_REQUESTED", "REVIEW_REQUIRED" })) || (!json_is_null(automatic) && (!json_is_object(automatic) || !has_exact_keys(json_object(automatic), { "enabledAt" })))) {
			return failed(application::github_provider_failure::malformed_response);
		}
		const auto value             = text(state);
		using assessment             = application::github_merge_assessment;
		aSnapshot.Merge.Assessment   = value == "BEHIND" ? assessment::behind : value == "BLOCKED" ? assessment::blocked
			: value == "CLEAN"                                                                     ? assessment::clean
			: value == "DIRTY"                                                                     ? assessment::conflicts
			: value == "DRAFT"                                                                     ? assessment::draft
			: value == "HAS_HOOKS"                                                                 ? assessment::hooks
			: value == "UNSTABLE"                                                                  ? assessment::unstable
																								   : assessment::unknown;
		using decision               = application::github_review_decision;
		aSnapshot.Merge.Review       = text(review) == "APPROVED" ? decision::approved : text(review) == "CHANGES_REQUESTED" ? decision::changes_requested
			: text(review) == "REVIEW_REQUIRED"                                                                              ? decision::required
																															 : decision::unknown;
		aSnapshot.Merge.AutoMerge    = !json_is_null(automatic);
		aSnapshot.Merge.Availability = observation::available;
		return { };
	}

	auto read_merge_assessment(production_read_execution& aExecution, server_contract aServer, const core::pull_request_request_context& aContext, application::github_pull_request_snapshot& aSnapshot)
		-> std::expected<void, github_paged_read_failure>
	{
		const auto page = aExecution.fetch(semantic_request_for(aServer, aContext, read_operation::get_merge_assessment));
		if (!page) {
			return std::unexpected{ page.error() };
		}
		const auto pull = merge_pull_object(*page, aSnapshot);
		if (!pull) {
			return std::unexpected{ pull.error() };
		}
		return apply_merge_fields(*pull, aSnapshot);
	}

	auto parse_pending_request(const page_json_value& aValue) -> std::expected<application::github_review_request_record, github_paged_read_failure>
	{
		if (!json_is_object(aValue)) {
			return failed(application::github_provider_failure::malformed_response);
		}
		const auto object = json_object(aValue);
		if (!has_exact_keys(object, { "id", "asCodeOwner", "requestedReviewer" }) || !nonempty_string(json_member(object, "id")) || !json_is_bool(json_member(object, "asCodeOwner"))) {
			return failed(application::github_provider_failure::malformed_response);
		}
		auto result        = application::github_review_request_record{ };
		result.Request     = text(json_member(object, "id"));
		result.AsCodeOwner = json_bool(json_member(object, "asCodeOwner"));
		const auto actor   = json_member(object, "requestedReviewer");
		if (json_is_null(actor)) {
			return result;
		}
		if (!json_is_object(actor) || !nonempty_string(json_member(json_object(actor), "__typename"))) {
			return failed(application::github_provider_failure::malformed_response);
		}
		const auto fields = json_object(actor);
		const auto kind   = text(json_member(fields, "__typename"));
		if (kind != "Team" && kind != "User") {
			return result;
		}
		if (!nonempty_string(json_member(fields, "id"))) {
			return result;
		}
		result.Actor = text(json_member(fields, "id"));
		result.Login = text(json_member(fields, kind == "Team" ? "slug" : "login"));
		if (result.Login.empty()) {
			return failed(application::github_provider_failure::malformed_response);
		}
		if (kind == "Team") {
			const auto org = json_member(fields, "organization");
			if (!json_is_object(org) || !nonempty_string(json_member(json_object(org), "login"))) {
				result.Actor.clear();
				result.Login.clear();
				return result;
			}
			result.Kind         = application::github_requested_actor_kind::team;
			result.Organization = text(json_member(json_object(org), "login"));
			result.Name         = text(json_member(fields, "name"));
		} else {
			result.Kind = application::github_requested_actor_kind::user;
		}
		return result;
	}

	auto read_pending_requests(production_read_execution& aExecution, server_contract aServer, const core::pull_request_request_context& aContext, application::github_pull_request_snapshot& aSnapshot, const github_paged_read_limits& aLimits)
		-> std::expected<void, github_paged_read_failure>
	{
		auto                         output  = std::vector<application::github_review_request_record>{ };
		auto                         cursors = std::unordered_set<std::string>{ }, requests = std::unordered_set<std::string>{ }, actors = std::unordered_set<std::string>{ };
		auto                         semantic = semantic_request_for(aServer, aContext, read_operation::list_review_requests);
		std::optional<std::uint64_t> expected;
		while (true) {
			const auto page = aExecution.fetch(semantic);
			if (!page) {
				return std::unexpected{ page.error() };
			}
			const auto pull = merge_pull_object(*page, aSnapshot);
			if (!pull) {
				return std::unexpected{ pull.error() };
			}
			const auto connection = parse_graphql_connection(json_member(*pull, "reviewRequests"), aLimits);
			if (!connection) {
				return std::unexpected{ connection.error() };
			}
			if ((expected && *expected != connection->TotalCount) || connection->TotalCount > 1000U) {
				return failed(application::github_provider_failure::response_limit);
			}
			expected = connection->TotalCount;
			for (const auto& node : connection->Nodes) {
				const auto record = parse_pending_request(node);
				if (!record) {
					return std::unexpected{ record.error() };
				}
				if (!requests.insert(record->Request).second || (!record->Actor.empty() && !actors.insert(record->Actor).second)) {
					return failed(application::github_provider_failure::malformed_response);
				}
				output.push_back(*record);
			}
			if (!connection->NextCursor) {
				if (output.size() != *expected) {
					return failed(application::github_provider_failure::malformed_response);
				}
				aSnapshot.ReviewRequests             = std::move(output);
				aSnapshot.ReviewRequestsAvailability = observation::available;
				return { };
			}
			if (semantic.Page == 10U || !cursors.insert(*connection->NextCursor).second || output.size() >= *expected) {
				return failed(application::github_provider_failure::response_limit);
			}
			semantic.ReviewCursor = *connection->NextCursor;
			++semantic.Page;
		}
	}

	auto
	read_checks(production_read_execution&                     aExecution,
				const validated_github_host_profile&           aProfile,
				semantic_read_request                          aSemantic,
				const std::string_view                         aHeadOid,
				std::vector<application::github_check_record>& aOutput,
				const github_paged_read_limits&                aLimits)
		-> std::expected<void, github_paged_read_failure>
	{
		std::optional<std::uint64_t> total;
		while (true) {
			auto page = aExecution.fetch(aSemantic);
			if (!page.has_value()) {
				return std::unexpected{ page.error() };
			}
			if (!json_is_object(page->Document)
				|| !has_required_keys(json_object(page->Document),
									  { "check_runs", "total_count" })) {
				return failed(
					application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(page->Document);
			if (!exact_integer(json_member(object, "total_count"))
				|| !json_is_array(json_member(object, "check_runs"))) {
				return failed(
					application::github_provider_failure::malformed_response);
			}
			const auto observedTotal = integer(json_member(object, "total_count"));
			if (observedTotal > aLimits.Records.MaximumChecks) {
				return failed(application::github_provider_failure::response_limit);
			}
			if (total.has_value() && *total != observedTotal) {
				return failed(
					application::github_provider_failure::malformed_response);
			}
			total              = observedTotal;
			const auto records = json_array(json_member(object, "check_runs"));
			const auto count   = static_cast<std::size_t>(records.size());
			if (count > aLimits.MaximumPageRecords
				|| count > aLimits.Records.MaximumChecks
				|| aOutput.size() > aLimits.Records.MaximumChecks - count) {
				return failed(application::github_provider_failure::response_limit);
			}
			aOutput.reserve(aOutput.size() + count);
			for (const auto& value : records) {
				auto check = parse_check(value, aHeadOid);
				if (!check.has_value()) {
					return std::unexpected{ check.error() };
				}
				auto budget = application::detail::github_text_budget{ aLimits.Records };
				if (application::detail::validate_check(*check, budget, aLimits.Records) != application::github_read_validation_error::none) {
					return failed(application::github_provider_failure::response_limit);
				}
				aOutput.push_back(std::move(*check));
			}
			if (aOutput.size() > observedTotal) {
				return failed(
					application::github_provider_failure::malformed_response);
			}
			const auto repository = aExecution.bound_repository();
			if (!repository.has_value()) {
				return failed(
					application::github_provider_failure::malformed_response);
			}
			const auto hasNext = rest_has_next_page(aProfile, *repository, *page, count);
			if (!hasNext.has_value()) {
				return std::unexpected{ hasNext.error() };
			}
			if (!*hasNext) {
				if (aOutput.size() != observedTotal) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				return { };
			}
			if (aSemantic.Page == std::numeric_limits<std::uint32_t>::max()) {
				return failed(application::github_provider_failure::response_limit);
			}
			++aSemantic.Page;
		}
	}
}
