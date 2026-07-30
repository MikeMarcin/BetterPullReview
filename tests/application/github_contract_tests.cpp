#include "bpr/application/github_provider_port.hpp"

#include <concepts>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace
{
using namespace bpr;

static_assert(std::is_abstract_v<application::github_provider_port>);
static_assert(
    std::derived_from<
        application::github_provider_port,
        application::github_profile_port>);
static_assert(
    std::derived_from<
        application::github_provider_port,
        application::github_viewer_port>);
static_assert(
    std::derived_from<
        application::github_provider_port,
        application::github_capability_port>);
static_assert(
    std::derived_from<
        application::github_provider_port,
        application::github_pull_request_port>);
static_assert(
    std::variant_size_v<application::github_profile_completion> == 3U);
static_assert(
    std::variant_size_v<application::github_viewer_completion> == 3U);
static_assert(
    std::variant_size_v<application::github_capability_completion> == 3U);
static_assert(
    std::variant_size_v<application::github_pull_request_completion> == 3U);
static_assert(
    !std::is_convertible_v<std::string, application::github_node_id>);
static_assert(
    !std::same_as<
        application::github_node_id,
        application::github_comment_id>);
static_assert(
    !std::same_as<
        application::github_actor_id,
        application::github_node_id>);

auto expect(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

auto expect_error(
    const application::github_read_validation actual,
    const application::github_read_validation_error expected,
    const std::string_view message) -> void
{
    expect(actual.Error == expected, message);
}

auto session_context() -> core::github_session_request_context
{
    return {
        .Request = core::request_id{1},
        .ApplicationSession = core::application_session_id{2},
        .Profile = core::host_profile_id{3},
        .IdentityGeneration = core::github_identity_generation{4},
    };
}

auto pull_request_context() -> core::pull_request_request_context
{
    return {
        .Request = core::request_id{11},
        .ApplicationSession = core::application_session_id{12},
        .Profile = core::host_profile_id{13},
        .Host = core::git_host_id{14},
        .Account = core::github_account_id{15},
        .Repository = {.Owner = "openai", .Name = "better-pull-review"},
        .Number = core::pull_request_number{16},
        .ExpectedRepository = core::repository_id{17},
        .ExpectedPullRequest = core::pull_request_id{18},
        .Capabilities = core::capability_revision{19},
        .IdentityGeneration = core::github_identity_generation{20},
        .Generation = core::request_generation{21},
    };
}

auto capability_evidence(
    const core::capability_revision revision)
    -> application::github_capability_evidence
{
    return {
        .Capabilities =
            {
                .ReviewThreads =
                    core::github_capability_state::supported,
                .ViewedFileMutations =
                    core::github_capability_state::unsupported,
                .ResolveThread =
                    core::github_capability_state::supported,
                .UnresolveThread =
                    core::github_capability_state::unknown,
            },
        .Revision = revision,
        .ReadShape =
            {
                .ReviewThreads = true,
                .ViewedFileState = true,
                .ResolvePermission = true,
                .UnresolvePermission = true,
            },
        .ContractVersion = 1,
    };
}

[[nodiscard]] auto rate_limit(
    const application::github_rate_limit_resource resource)
    -> application::github_rate_limit_record
{
    return {
        .Resource = resource,
        .Limit = 5'000U,
        .Remaining = 4'999U,
        .ResetUnixSeconds = 2'000'000'000U,
        .SecondaryLimited = false,
    };
}

[[nodiscard]] auto rate_limits() -> application::github_rate_limit_state
{
    auto core =
        rate_limit(application::github_rate_limit_resource::core);
    core.Remaining = 4'900U;

    auto search =
        rate_limit(application::github_rate_limit_resource::search);
    search.Limit = 30U;
    search.Remaining = 29U;
    search.ResetUnixSeconds = 2'000'000'100U;

    auto graphql =
        rate_limit(application::github_rate_limit_resource::graphql);
    graphql.Remaining = 4'998U;
    graphql.ResetUnixSeconds = 2'000'000'200U;

    return {
        .Core = core,
        .Search = search,
        .Graphql = graphql,
    };
}

auto valid_session() -> application::github_session_read_model
{
    return {
        .Profile = core::host_profile_id{3},
        .Host = core::git_host_id{14},
        .Viewer =
            {
                .Account = core::github_account_id{15},
                .Node = application::github_actor_id{"reviewer-node"},
                .Login = "reviewer",
                .DisplayName = "Review Engineer",
            },
        .CapabilityEvidence =
            capability_evidence(core::capability_revision{1}),
        .RateLimits = rate_limits(),
    };
}

auto valid_snapshot() -> application::github_pull_request_snapshot
{
    const auto base_oid = application::commit_oid{
        "1111111111111111111111111111111111111111"};
    const auto head_oid = application::commit_oid{
        "2222222222222222222222222222222222222222"};
    const auto commit_oid = application::commit_oid{
        "3333333333333333333333333333333333333333"};

    return {
        .Profile = core::host_profile_id{13},
        .Host = core::git_host_id{14},
        .Account = core::github_account_id{15},
        .Viewer =
            {
                .Actor =
                    application::github_actor_id{"reviewer-node"},
                .Login = "reviewer",
            },
        .Snapshot = core::provider_snapshot_id{22},
        .Repository =
            {
                .Repository = core::repository_id{17},
                .Node = application::github_node_id{"repository-node"},
                .Owner = "openai",
                .Name = "better-pull-review",
                .DefaultBranch = "main",
                .Private = true,
            },
        .PullRequest =
            {
                .PullRequest = core::pull_request_id{18},
                .Repository = core::repository_id{17},
                .Number = core::pull_request_number{16},
                .Node = application::github_node_id{"pull-request-node"},
                .Title = "Normalize GitHub reads",
                .Body = "Bounded synthetic pull request body.",
                .State = application::github_pull_request_state::open,
                .Draft = false,
                .Author =
                    application::github_actor_record{
                        .Actor =
                            application::github_actor_id{"author-node"},
                        .Login = "author",
                    },
                .BaseOid = base_oid,
                .HeadOid = head_oid,
                .BaseRef = "main",
                .HeadRef = "normalized-reads",
            },
        .Files =
            {
                {
                    .Path = "src/github.cpp",
                    .PreviousPath = std::nullopt,
                    .Change = application::github_file_change::modified,
                    .Additions = 12,
                    .Deletions = 3,
                    .ViewedState =
                        application::github_file_viewed_state::viewed,
                },
            },
        .Commits =
            {
                {
                    .Oid = commit_oid,
                    .Node = application::github_node_id{"commit-node"},
                    .Author =
                        application::github_actor_record{
                            .Actor =
                                application::github_actor_id{
                                    "author-node"},
                            .Login = "author",
                        },
                    .Headline = "Add normalized read model",
                },
            },
        .Reviews =
            {
                {
                    .Review = application::github_review_id{"review-node"},
                    .Author =
                        application::github_actor_record{
                            .Actor =
                                application::github_actor_id{
                                    "reviewer-node"},
                            .Login = "reviewer",
                        },
                    .State = application::github_review_state::approved,
                    .Commit = commit_oid,
                    .Body = "Looks good.",
                },
            },
        .ReviewThreadsAvailability =
            application::github_collection_availability::available,
        .ReviewThreads =
            {
                {
                    .Thread = application::github_thread_id{"thread-node"},
                    .Subject =
                        application::github_review_thread_subject::line,
                    .Path = "src/github.cpp",
                    .Line = 42,
                    .Resolved = false,
                    .Outdated = false,
                    .ViewerResolvePermission =
                        application::github_permission_state::allowed,
                    .ViewerUnresolvePermission =
                        application::github_permission_state::denied,
                    .Comments =
                        {
                            {
                                .Comment =
                                    application::github_comment_id{
                                        "review-comment-node"},
                                .ReplyTo = std::nullopt,
                                .Author =
                                    application::github_actor_record{
                                        .Actor =
                                            application::github_actor_id{
                                                "reviewer-node"},
                                        .Login = "reviewer",
                                    },
                                .Body = "Please keep this bounded.",
                                .Path = "src/github.cpp",
                                .Line = 42,
                            },
                        },
                },
            },
        .IssueComments =
            {
                {
                    .Comment =
                        application::github_comment_id{
                            "issue-comment-node"},
                    .Author =
                        application::github_actor_record{
                            .Actor =
                                application::github_actor_id{
                                    "author-node"},
                            .Login = "author",
                        },
                    .Body = "Ready for review.",
                },
            },
        .Checks =
            {
                {
                    .Check = application::github_check_id{"check-node"},
                    .Name = "local-core-only",
                    .Status = application::github_check_status::completed,
                    .Conclusion =
                        application::github_check_conclusion::success,
                },
            },
        .SearchResults =
            {
                {
                    .Repository = core::repository_id{17},
                    .PullRequestNode =
                        application::github_node_id{"search-pr-node"},
                    .Number = core::pull_request_number{32},
                    .Title = "Another result",
                    .State = application::github_pull_request_state::open,
                },
            },
        .CapabilityEvidence =
            capability_evidence(core::capability_revision{19}),
        .RateLimits = rate_limits(),
    };
}

auto test_valid_models_and_identity_correlation() -> void
{
    const auto session = valid_session();
    const auto session_request = session_context();
    expect(
        application::validate(session, session_request).valid(),
        "valid session read model is accepted");

    auto wrong_session_context = session_request;
    wrong_session_context.Profile = core::host_profile_id{99};
    expect_error(
        application::validate(session, wrong_session_context),
        application::github_read_validation_error::identity_mismatch,
        "session profile mismatch is rejected");

    const auto snapshot = valid_snapshot();
    const auto request = pull_request_context();
    expect(
        application::validate(snapshot, request).valid(),
        "valid aggregate pull request snapshot is accepted");
    expect(
        snapshot.RateLimits.Core.has_value()
            && snapshot.RateLimits.Search.has_value()
            && snapshot.RateLimits.Graphql.has_value()
            && snapshot.RateLimits.Core->Resource
                == application::github_rate_limit_resource::core
            && snapshot.RateLimits.Core->Remaining == 4'900U
            && snapshot.RateLimits.Search->Resource
                == application::github_rate_limit_resource::search
            && snapshot.RateLimits.Search->Limit == 30U
            && snapshot.RateLimits.Search->Remaining == 29U
            && snapshot.RateLimits.Graphql->Resource
                == application::github_rate_limit_resource::graphql
            && snapshot.RateLimits.Graphql->Remaining == 4'998U,
        "snapshot preserves distinct core, search, and GraphQL rate limits");

    auto mismatch = snapshot;
    mismatch.Profile = core::host_profile_id{99};
    expect_error(
        application::validate(mismatch, request),
        application::github_read_validation_error::identity_mismatch,
        "profile mismatch is rejected");
    mismatch = snapshot;
    mismatch.Host = core::git_host_id{99};
    expect_error(
        application::validate(mismatch, request),
        application::github_read_validation_error::identity_mismatch,
        "host mismatch is rejected");
    mismatch = snapshot;
    mismatch.Account = core::github_account_id{99};
    expect_error(
        application::validate(mismatch, request),
        application::github_read_validation_error::identity_mismatch,
        "account mismatch is rejected");
    mismatch = snapshot;
    mismatch.PullRequest.Number = core::pull_request_number{99};
    expect_error(
        application::validate(mismatch, request),
        application::github_read_validation_error::identity_mismatch,
        "pull request number mismatch is rejected");
    mismatch = snapshot;
    mismatch.Repository.Owner = "different";
    expect_error(
        application::validate(mismatch, request),
        application::github_read_validation_error::identity_mismatch,
        "repository owner mismatch is rejected");
    mismatch = snapshot;
    mismatch.Repository.Name = "different";
    expect_error(
        application::validate(mismatch, request),
        application::github_read_validation_error::identity_mismatch,
        "repository name mismatch is rejected");
    mismatch = snapshot;
    mismatch.Repository.Repository = core::repository_id{99};
    expect_error(
        application::validate(mismatch, request),
        application::github_read_validation_error::identity_mismatch,
        "expected repository mismatch is rejected");
    mismatch = snapshot;
    mismatch.PullRequest.PullRequest = core::pull_request_id{99};
    expect_error(
        application::validate(mismatch, request),
        application::github_read_validation_error::identity_mismatch,
        "expected pull request mismatch is rejected");
    mismatch = snapshot;
    mismatch.CapabilityEvidence.Revision =
        core::capability_revision{99};
    expect_error(
        application::validate(mismatch, request),
        application::github_read_validation_error::identity_mismatch,
        "capability revision mismatch is rejected");

    auto case_only = snapshot;
    case_only.Repository.Owner = "OpenAI";
    case_only.Repository.Name = "Better-Pull-Review";
    expect(
        application::validate(case_only, request).valid(),
        "GitHub repository locator correlation is ASCII case-insensitive");

    auto file_subject = snapshot;
    file_subject.ReviewThreads.front().Subject =
        application::github_review_thread_subject::file;
    file_subject.ReviewThreads.front().Line.reset();
    file_subject.ReviewThreads.front().Comments.front().Line.reset();
    expect(
        application::validate(file_subject, request).valid(),
        "file-level review thread with no line is accepted");

    auto missing_line = snapshot;
    missing_line.ReviewThreads.front().Line.reset();
    expect_error(
        application::validate(missing_line, request),
        application::github_read_validation_error::invalid_line,
        "line-level review thread requires a line");

    auto unexpected_line = file_subject;
    unexpected_line.ReviewThreads.front().Line = 42;
    expect_error(
        application::validate(unexpected_line, request),
        application::github_read_validation_error::invalid_line,
        "file-level review thread rejects a line");

    auto maximum_line = snapshot;
    maximum_line.ReviewThreads.front().Line =
        application::maximum_github_line;
    maximum_line.ReviewThreads.front().Comments.front().Line =
        application::maximum_github_line;
    expect(
        application::validate(maximum_line, request).valid(),
        "GraphQL line at signed Int maximum is accepted");

    auto excessive_line = snapshot;
    excessive_line.ReviewThreads.front().Comments.front().Line =
        application::maximum_github_line + 1U;
    expect_error(
        application::validate(excessive_line, request),
        application::github_read_validation_error::invalid_line,
        "GraphQL line above signed Int range is rejected");

    auto missing_previous_path = snapshot;
    missing_previous_path.Files.front().Change =
        application::github_file_change::renamed;
    expect(
        application::validate(missing_previous_path, request).valid(),
        "renamed file may omit an unavailable previous path");

    auto copied_with_previous_path = snapshot;
    copied_with_previous_path.Files.front().Change =
        application::github_file_change::copied;
    copied_with_previous_path.Files.front().PreviousPath =
        "src/copied_file.cpp";
    expect(
        application::validate(copied_with_previous_path, request).valid(),
        "copied file may retain its available source path");

    auto unchanged = snapshot;
    unchanged.Files.front().Change =
        application::github_file_change::unchanged;
    expect(
        application::validate(unchanged, request).valid(),
        "unchanged is a valid GitHub file status");

    auto unexpected_previous_path = snapshot;
    unexpected_previous_path.Files.front().PreviousPath =
        "src/old_file.cpp";
    expect_error(
        application::validate(unexpected_previous_path, request),
        application::github_read_validation_error::invalid_relationship,
        "non-renamed file rejects a previous path");

    auto mismatched_comment_path = snapshot;
    mismatched_comment_path.ReviewThreads.front()
        .Comments.front()
        .Path = "src/other.cpp";
    expect_error(
        application::validate(mismatched_comment_path, request),
        application::github_read_validation_error::invalid_relationship,
        "review comment path must match its thread");
}

auto test_malformed_enums_fail_closed() -> void
{
    expect(
        application::valid(
            application::github_provider_failure::malformed_response),
        "known provider failure is valid");
    expect(
        !application::valid(
            static_cast<application::github_provider_failure>(255)),
        "malformed provider failure is rejected");

    const auto observed_rate_limits = rate_limits();
    auto core_exhausted = *observed_rate_limits.Core;
    core_exhausted.Remaining = 0U;
    auto primary_limited = observed_rate_limits;
    primary_limited.Core = core_exhausted;

    auto graphql_secondary = *observed_rate_limits.Graphql;
    graphql_secondary.SecondaryLimited = true;
    auto secondary_limited = observed_rate_limits;
    secondary_limited.Graphql = graphql_secondary;

    expect(
        application::valid(application::github_provider_error{
            .Failure =
                application::github_provider_failure::rate_limit,
            .RetryAfterSeconds =
                application::maximum_github_retry_after_seconds,
            .RateLimits = primary_limited,
            .RateLimitCause = core_exhausted,
        }),
        "bounded primary limit names the exact exhausted resource");
    expect(
        application::valid(application::github_provider_error{
            .Failure =
                application::github_provider_failure::unavailable,
            .RetryAfterSeconds = 30U,
            .RateLimits = observed_rate_limits,
            .RateLimitCause = std::nullopt,
        }),
        "unavailable failures may preserve all observed resource limits");
    expect(
        application::valid(application::github_provider_error{
            .Failure =
                application::github_provider_failure::rate_limit,
            .RetryAfterSeconds = 60U,
            .RateLimits = secondary_limited,
            .RateLimitCause = graphql_secondary,
        }),
        "secondary limit names the exact triggering GraphQL resource");
    expect(
        !application::valid(application::github_provider_error{
            .Failure =
                application::github_provider_failure::rate_limit,
            .RetryAfterSeconds = std::nullopt,
            .RateLimits = primary_limited,
            .RateLimitCause = std::nullopt,
        }),
        "rate-limit failures require an exact triggering record");
    expect(
        !application::valid(application::github_provider_error{
            .Failure = application::github_provider_failure::network,
            .RetryAfterSeconds = 1U,
            .RateLimits = {},
            .RateLimitCause = std::nullopt,
        }),
        "retry metadata is rejected for an ineligible failure");
    expect(
        !application::valid(application::github_provider_error{
            .Failure =
                application::github_provider_failure::rate_limit,
            .RetryAfterSeconds =
                application::maximum_github_retry_after_seconds + 1U,
            .RateLimits = primary_limited,
            .RateLimitCause = core_exhausted,
        }),
        "retry metadata above one day is rejected");
    expect(
        !application::valid(application::github_provider_error{
            .Failure =
                application::github_provider_failure::permission,
            .RetryAfterSeconds = std::nullopt,
            .RateLimits = secondary_limited,
            .RateLimitCause = graphql_secondary,
        }),
        "a triggering record cannot accompany a non-rate-limit failure");
    expect(
        !application::valid(application::github_provider_error{
            .Failure =
                application::github_provider_failure::rate_limit,
            .RetryAfterSeconds = std::nullopt,
            .RateLimits = observed_rate_limits,
            .RateLimitCause = *observed_rate_limits.Core,
        }),
        "a non-exhausted primary record cannot trigger rate limiting");

    auto mismatched_cause = core_exhausted;
    ++mismatched_cause.ResetUnixSeconds;
    expect(
        !application::valid(application::github_provider_error{
            .Failure =
                application::github_provider_failure::rate_limit,
            .RetryAfterSeconds = std::nullopt,
            .RateLimits = primary_limited,
            .RateLimitCause = mismatched_cause,
        }),
        "trigger metadata must equal its resource-keyed observation");

    auto wrong_resource_slot = observed_rate_limits;
    wrong_resource_slot.Core->Resource =
        application::github_rate_limit_resource::search;
    expect(
        !application::valid(application::github_provider_error{
            .Failure =
                application::github_provider_failure::unavailable,
            .RetryAfterSeconds = std::nullopt,
            .RateLimits = wrong_resource_slot,
            .RateLimitCause = std::nullopt,
        }),
        "a rate-limit record cannot occupy another resource's slot");

    auto wrong_secondary_cause = secondary_limited;
    wrong_secondary_cause.Core = core_exhausted;
    expect(
        !application::valid(application::github_provider_error{
            .Failure =
                application::github_provider_failure::rate_limit,
            .RetryAfterSeconds = std::nullopt,
            .RateLimits = wrong_secondary_cause,
            .RateLimitCause = core_exhausted,
        }),
        "a secondary-limited resource must be the named trigger");

    auto multiple_secondary_causes = secondary_limited;
    multiple_secondary_causes.Core->SecondaryLimited = true;
    expect(
        !application::valid(application::github_provider_error{
            .Failure =
                application::github_provider_failure::rate_limit,
            .RetryAfterSeconds = std::nullopt,
            .RateLimits = multiple_secondary_causes,
            .RateLimitCause = graphql_secondary,
        }),
        "one failure cannot claim multiple secondary-limit triggers");

    auto malformed_secondary = secondary_limited;
    malformed_secondary.Graphql->Remaining = 0U;
    expect(
        !application::valid(application::github_provider_error{
            .Failure =
                application::github_provider_failure::rate_limit,
            .RetryAfterSeconds = std::nullopt,
            .RateLimits = malformed_secondary,
            .RateLimitCause = *malformed_secondary.Graphql,
        }),
        "secondary limiting remains distinct from primary exhaustion");

    auto malformed_arithmetic = observed_rate_limits;
    malformed_arithmetic.Search->Remaining =
        malformed_arithmetic.Search->Limit + 1U;
    expect(
        !application::valid(application::github_provider_error{
            .Failure =
                application::github_provider_failure::unavailable,
            .RetryAfterSeconds = std::nullopt,
            .RateLimits = malformed_arithmetic,
            .RateLimitCause = std::nullopt,
        }),
        "provider errors reject malformed rate-limit arithmetic");
    expect(
        !application::valid(application::github_provider_error{
            .Failure =
                static_cast<application::github_provider_failure>(255),
            .RetryAfterSeconds = std::nullopt,
            .RateLimits = {},
            .RateLimitCause = std::nullopt,
        }),
        "a provider error rejects a malformed failure enum");

    const auto request = pull_request_context();
    auto snapshot = valid_snapshot();
    snapshot.PullRequest.State =
        static_cast<application::github_pull_request_state>(255);
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "malformed pull request state is rejected");

    snapshot = valid_snapshot();
    snapshot.Files.front().Change =
        static_cast<application::github_file_change>(255);
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "malformed file change is rejected");

    snapshot = valid_snapshot();
    snapshot.Files.front().Change =
        application::github_file_change::uninitialized;
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "uninitialized file change is rejected");

    snapshot = valid_snapshot();
    snapshot.Files.front().ContentKind =
        static_cast<application::github_file_content_kind>(255);
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "malformed file content kind is rejected");

    snapshot = valid_snapshot();
    snapshot.Files.front().ViewedState =
        static_cast<application::github_file_viewed_state>(255);
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "malformed viewed-file state is rejected");

    for(const auto viewed_state : {
            application::github_file_viewed_state::dismissed,
            application::github_file_viewed_state::unviewed,
            application::github_file_viewed_state::viewed,
        })
    {
        snapshot = valid_snapshot();
        snapshot.Files.front().ViewedState = viewed_state;
        expect(
            application::validate(snapshot, request).valid(),
            "every known viewed-file state is accepted");
    }
    snapshot = valid_snapshot();
    snapshot.CapabilityEvidence.ReadShape.ViewedFileState = false;
    snapshot.Files.front().ViewedState =
        application::github_file_viewed_state::unavailable;
    expect(
        application::validate(snapshot, request).valid(),
        "capability-omitted viewed state is explicitly unavailable");
    snapshot.Files.front().ViewedState =
        application::github_file_viewed_state::unviewed;
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_relationship,
        "an omitted viewed-state selection cannot become authoritative");

    snapshot = valid_snapshot();
    snapshot.Reviews.front().State =
        static_cast<application::github_review_state>(255);
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "malformed review state is rejected");

    snapshot = valid_snapshot();
    snapshot.Reviews.front().State =
        application::github_review_state::uninitialized;
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "uninitialized review state is rejected");

    snapshot = valid_snapshot();
    snapshot.ReviewThreads.front().Subject =
        static_cast<application::github_review_thread_subject>(255);
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "malformed review thread subject is rejected");

    snapshot = valid_snapshot();
    snapshot.ReviewThreads.front().ViewerResolvePermission =
        static_cast<application::github_permission_state>(255);
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "malformed thread permission is rejected");

    snapshot = valid_snapshot();
    snapshot.CapabilityEvidence.ReadShape.ResolvePermission = false;
    snapshot.ReviewThreads.front().ViewerResolvePermission =
        application::github_permission_state::unavailable;
    snapshot.CapabilityEvidence.Capabilities.ResolveThread =
        core::github_capability_state::unsupported;
    expect(
        application::validate(snapshot, request).valid(),
        "omitted resolve permission is distinct from denied");
    snapshot.ReviewThreads.front().ViewerResolvePermission =
        application::github_permission_state::denied;
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_relationship,
        "an omitted permission cannot be projected as denied");

    snapshot = valid_snapshot();
    snapshot.ReviewThreadsAvailability =
        application::github_collection_availability::unavailable;
    snapshot.ReviewThreads.clear();
    snapshot.CapabilityEvidence.ReadShape.ReviewThreads = false;
    snapshot.CapabilityEvidence.Capabilities.ReviewThreads =
        core::github_capability_state::unsupported;
    snapshot.CapabilityEvidence.Capabilities.ResolveThread =
        core::github_capability_state::unsupported;
    snapshot.CapabilityEvidence.Capabilities.UnresolveThread =
        core::github_capability_state::unsupported;
    expect(
        application::validate(snapshot, request).valid(),
        "capability-omitted thread collection is explicitly unavailable");
    snapshot.ReviewThreadsAvailability =
        application::github_collection_availability::available;
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_relationship,
        "selection omission cannot become an authoritative empty collection");

    snapshot = valid_snapshot();
    snapshot.Checks.front().Status =
        static_cast<application::github_check_status>(255);
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "malformed check status is rejected");

    snapshot = valid_snapshot();
    snapshot.Checks.front().Status =
        application::github_check_status::uninitialized;
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "uninitialized check status is rejected");

    snapshot = valid_snapshot();
    snapshot.Checks.front().Conclusion =
        static_cast<application::github_check_conclusion>(255);
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "malformed check conclusion is rejected");

    snapshot = valid_snapshot();
    snapshot.Checks.front().Conclusion =
        application::github_check_conclusion::uninitialized;
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "uninitialized check conclusion is rejected");

    snapshot = valid_snapshot();
    snapshot.Checks.front().Status =
        application::github_check_status::in_progress;
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "in-progress check cannot carry a completed conclusion");

    snapshot = valid_snapshot();
    snapshot.Checks.front().Conclusion =
        application::github_check_conclusion::none;
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "completed check requires a conclusion");

    snapshot = valid_snapshot();
    snapshot.SearchResults.front().State =
        static_cast<application::github_pull_request_state>(255);
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "malformed search result state is rejected");

    snapshot = valid_snapshot();
    snapshot.SearchResults.front().State =
        application::github_pull_request_state::closed;
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "selected open-only search rejects a closed result");

    snapshot = valid_snapshot();
    snapshot.CapabilityEvidence.Capabilities.ReviewThreads =
        static_cast<core::github_capability_state>(255);
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_enum,
        "malformed capability state is rejected");
}

auto test_default_collection_and_field_bounds() -> void
{
    const application::github_read_limits defaults;
    expect(defaults.MaximumTitleBytes == 1'024U, "title limit is 1 KiB");
    expect(defaults.MaximumFiles == 3'000U, "file limit is 3,000");
    expect(
        defaults.MaximumCommits == 250U,
        "pull-request commit limit matches the provider endpoint ceiling");
    expect(
        defaults.MaximumReviews == 10'000U
            && defaults.MaximumChecks == 10'000U,
        "general list limits are 10,000");
    expect(
        defaults.MaximumIssueComments == 10'000U,
        "issue comment limit is explicit");
    expect(
        defaults.MaximumSearchResults == 1'000U,
        "search result limit is 1,000");
    expect(
        defaults.MaximumTotalTextBytes == 16U * 1'024U * 1'024U,
        "normalized text limit is 16 MiB");

    const auto request = pull_request_context();
    auto snapshot = valid_snapshot();
    snapshot.PullRequest.Title.assign(defaults.MaximumTitleBytes, 't');
    expect(
        application::validate(snapshot, request).valid(),
        "title at default N is accepted");
    snapshot.PullRequest.Title.push_back('t');
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::text_too_long,
        "title at default N+1 is rejected");

    snapshot = valid_snapshot();
    snapshot.Files.clear();
    snapshot.Files.reserve(defaults.MaximumFiles + 1U);
    for(std::size_t index = 0; index < defaults.MaximumFiles; ++index)
    {
        snapshot.Files.push_back(application::github_file_record{
            .Path = "file-" + std::to_string(index),
            .PreviousPath = std::nullopt,
            .Change = application::github_file_change::modified,
            .ViewedState =
                application::github_file_viewed_state::unviewed,
        });
    }
    expect(
        application::validate(snapshot, request).valid(),
        "files at default N are accepted");
    snapshot.Files.push_back(application::github_file_record{
        .Path = "file-over-limit",
        .PreviousPath = std::nullopt,
        .Change = application::github_file_change::modified,
        .ViewedState = application::github_file_viewed_state::unviewed,
    });
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::collection_too_large,
        "files at default N+1 are rejected");

    snapshot = valid_snapshot();
    snapshot.SearchResults.clear();
    snapshot.SearchResults.reserve(defaults.MaximumSearchResults + 1U);
    for(std::size_t index = 0; index < defaults.MaximumSearchResults;
        ++index)
    {
        snapshot.SearchResults.push_back(
            application::github_search_result_record{
                .Repository = core::repository_id{17},
                .PullRequestNode =
                    application::github_node_id{
                        "search-pr-" + std::to_string(index)},
                .Number = core::pull_request_number{
                    static_cast<std::uint32_t>(index + 1'000U)},
                .Title = "result",
            });
    }
    expect(
        application::validate(snapshot, request).valid(),
        "search results at default N are accepted");
    snapshot.SearchResults.push_back(
        application::github_search_result_record{
            .Repository = core::repository_id{20'000},
            .PullRequestNode =
                application::github_node_id{"search-over-limit"},
            .Number = core::pull_request_number{20'000},
            .Title = "over limit",
        });
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::collection_too_large,
        "search results at default N+1 are rejected");
}

auto test_duplicate_identity_text_budget_and_rate_limit() -> void
{
    const auto request = pull_request_context();
    auto snapshot = valid_snapshot();
    snapshot.Files.push_back(snapshot.Files.front());
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::duplicate_identity,
        "duplicate file path identity is rejected");

    snapshot = valid_snapshot();
    auto duplicate_commit = snapshot.Commits.front();
    duplicate_commit.Node =
        application::github_node_id{"different-commit-node"};
    snapshot.Commits.push_back(std::move(duplicate_commit));
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::duplicate_identity,
        "duplicate commit OID is rejected");

    snapshot = valid_snapshot();
    snapshot.IssueComments.front().Comment =
        snapshot.ReviewThreads.front().Comments.front().Comment;
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::duplicate_identity,
        "duplicate comment identity across collections is rejected");

    snapshot = valid_snapshot();
    snapshot.Reviews.front().Review =
        application::github_review_id{
            std::string{snapshot.Repository.Node.value()}};
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::duplicate_identity,
        "GitHub node identity cannot collide across entity kinds");

    snapshot = valid_snapshot();
    snapshot.IssueComments.front().Author->Actor =
        application::github_actor_id{
            std::string{snapshot.Repository.Node.value()}};
    snapshot.IssueComments.front().Author->Login = "repository-alias";
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::duplicate_identity,
        "actor node identity cannot collide with another entity kind");

    snapshot = valid_snapshot();
    snapshot.SearchResults.front().PullRequestNode =
        snapshot.Commits.front().Node;
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::duplicate_identity,
        "non-target search node cannot collide with another entity kind");

    snapshot = valid_snapshot();
    snapshot.ReviewThreads.front().Comments.front().ReplyTo =
        application::github_comment_id{"missing-parent"};
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_relationship,
        "dangling review-comment reply is rejected");

    snapshot = valid_snapshot();
    snapshot.SearchResults.front().Repository = core::repository_id{99};
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_relationship,
        "repo-scoped search result cannot cross repository identity");

    snapshot = valid_snapshot();
    snapshot.SearchResults.front().PullRequestNode =
        snapshot.PullRequest.Node;
    snapshot.SearchResults.front().Number =
        core::pull_request_number{17};
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_relationship,
        "target search node cannot carry another PR number");

    snapshot = valid_snapshot();
    snapshot.SearchResults.front().PullRequestNode =
        application::github_node_id{"different-pr-node"};
    snapshot.SearchResults.front().Number =
        snapshot.PullRequest.Number;
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_relationship,
        "target search number cannot carry another PR node");

    snapshot = valid_snapshot();
    snapshot.SearchResults.front().PullRequestNode =
        snapshot.PullRequest.Node;
    snapshot.SearchResults.front().Number =
        snapshot.PullRequest.Number;
    snapshot.SearchResults.front().Title = "contradictory title";
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_relationship,
        "target search title must match pull-request metadata");

    snapshot = valid_snapshot();
    snapshot.SearchResults.front().PullRequestNode =
        snapshot.PullRequest.Node;
    snapshot.SearchResults.front().Number =
        snapshot.PullRequest.Number;
    snapshot.SearchResults.front().Draft =
        !snapshot.PullRequest.Draft;
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_relationship,
        "target search draft state must match pull-request metadata");

    snapshot = valid_snapshot();
    snapshot.SearchResults.front().PullRequestNode =
        application::github_node_id{"search-pr-3002"};
    snapshot.SearchResults.front().Number =
        core::pull_request_number{18};
    auto duplicate_search_number = snapshot.SearchResults.front();
    duplicate_search_number.PullRequestNode =
        application::github_node_id{"search-pr-3003"};
    snapshot.SearchResults.push_back(std::move(duplicate_search_number));
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::duplicate_identity,
        "repository-scoped search numbers are unique");

    snapshot = valid_snapshot();
    snapshot.IssueComments.front().Author->Login = "different-author";
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_relationship,
        "same actor ID cannot carry conflicting nonempty logins");

    snapshot = valid_snapshot();
    snapshot.Reviews.front().Author->Login = "viewer-alias";
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_relationship,
        "viewer node and login seed snapshot actor correlation");

    snapshot = valid_snapshot();
    snapshot.IssueComments.front().Author->Login.clear();
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::missing_required_text,
        "present actor requires a display login");

    snapshot = valid_snapshot();
    snapshot.IssueComments.front().Author->Actor =
        application::github_actor_id{"Author-node"};
    snapshot.IssueComments.front().Author->Login = "different-author";
    expect(
        application::validate(snapshot, request).valid(),
        "actor IDs remain case-sensitive");

    snapshot = valid_snapshot();
    snapshot.PullRequest.Author.reset();
    snapshot.Commits.front().Author.reset();
    snapshot.Reviews.front().Author.reset();
    snapshot.ReviewThreads.front().Comments.front().Author.reset();
    snapshot.IssueComments.front().Author.reset();
    expect(
        application::validate(snapshot, request).valid(),
        "nullable authors remain valid across every normalized record");

    auto session = valid_session();
    session.Viewer.Node = application::github_actor_id{"n"};
    session.Viewer.Login = "l";
    session.Viewer.DisplayName = "123456";
    auto limits = application::github_read_limits{};
    limits.MaximumTotalTextBytes = 8U;
    expect(
        application::validate(session, session_context(), limits).valid(),
        "total normalized text at N is accepted");
    session.Viewer.DisplayName.push_back('7');
    expect_error(
        application::validate(session, session_context(), limits),
        application::github_read_validation_error::total_text_too_large,
        "total normalized text at N+1 is rejected");

    snapshot = valid_snapshot();
    snapshot.RateLimits.Graphql->Remaining =
        snapshot.RateLimits.Graphql->Limit + 1U;
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::invalid_rate_limit,
        "malformed GraphQL rate-limit arithmetic is rejected");

    snapshot = valid_snapshot();
    snapshot.PullRequest.Author = application::github_actor_record{};
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::missing_identity,
        "present author identity cannot be empty");

    snapshot = valid_snapshot();
    snapshot.PullRequest.Author->Login.assign(
        application::github_read_limits{}.MaximumLoginBytes + 1U,
        'a');
    expect_error(
        application::validate(snapshot, request),
        application::github_read_validation_error::text_too_long,
        "actor login is bounded by the normalized login limit");

    auto relaxed_limits = application::github_read_limits{};
    ++relaxed_limits.MaximumBodyBytes;
    expect_error(
        application::validate(valid_snapshot(), request, relaxed_limits),
        application::github_read_validation_error::invalid_limits,
        "callers cannot relax a security ceiling");

    auto invalid_text = valid_snapshot();
    invalid_text.IssueComments.front().Body = "before";
    invalid_text.IssueComments.front().Body.push_back('\0');
    invalid_text.IssueComments.front().Body += "after";
    expect_error(
        application::validate(invalid_text, request),
        application::github_read_validation_error::invalid_text,
        "embedded NUL text cannot cross the normalized boundary");
}

class compiling_provider final : public application::github_provider_port
{
public:
    void resolve_profile(
        application::github_profile_request request,
        application::github_profile_completion_handler completion) override
    {
        LastProfile = request;
        completion(application::github_profile_cancelled{
            .Request = request,
        });
    }

    void cancel_profile(application::github_profile_request request) override
    {
        LastProfileCancellation = request;
    }

    void observe_viewer(
        application::github_viewer_submission submission,
        application::github_viewer_completion_handler completion) override
    {
        LastViewer = submission.Request;
        completion(application::github_viewer_cancelled{
            .Request = submission.Request,
        });
    }

    void cancel_viewer(application::github_viewer_request request) override
    {
        LastViewerCancellation = request;
    }

    void probe_capabilities(
        application::github_capability_request request,
        application::github_capability_completion_handler completion)
        override
    {
        LastCapability = request;
        completion(application::github_capability_cancelled{
            .Request = request,
        });
    }

    void cancel_capabilities(
        application::github_capability_request request) override
    {
        LastCapabilityCancellation = request;
    }

    void fetch_pull_request(
        application::github_pull_request_provider_request request,
        application::github_pull_request_completion_handler completion)
        override
    {
        LastPullRequest = request;
        completion(application::github_pull_request_cancelled{
            .Request = request,
        });
    }

    void cancel_pull_request(
        application::github_pull_request_provider_request request) override
    {
        LastPullRequestCancellation = std::move(request);
    }

    application::github_profile_request LastProfile;
    application::github_profile_request LastProfileCancellation;
    application::github_viewer_request LastViewer;
    application::github_viewer_request LastViewerCancellation;
    application::github_capability_request LastCapability;
    application::github_capability_request LastCapabilityCancellation;
    application::github_pull_request_provider_request LastPullRequest;
    application::github_pull_request_provider_request
        LastPullRequestCancellation;
};

auto test_completion_contexts_are_exact() -> void
{
    const application::github_profile_authority authority{
        .Profile = core::host_profile_id{3},
        .ProfileRevision = application::github_profile_revision{4},
        .Host = core::git_host_id{14},
        .Hostname =
            application::github_canonical_hostname{"github.com"},
        .ServerContract =
            application::github_server_contract::github_com_2022_11_28,
        .Authentication =
            application::github_authentication_transport::github_cli,
        .ProbeContractRevision =
            application::github_probe_contract_revision{5},
    };
    const application::github_profile_request profile_request{
        .Context = session_context(),
        .Attempt = application::github_provider_attempt{1U},
    };
    const application::github_viewer_request viewer_request{
        .Context = session_context(),
        .Authority = authority,
        .Attempt = application::github_provider_attempt{1U},
    };
    const application::github_capability_request capability_request{
        .Context = session_context(),
        .ProfileAuthority = authority,
        .Viewer = valid_session().Viewer,
        .AccountTransport = {},
        .Attempt = application::github_provider_attempt{1U},
    };
    const application::github_pull_request_provider_request pull_request{
        .Context = pull_request_context(),
        .ProfileAuthority = authority,
        .Viewer = valid_session().Viewer,
        .CapabilityEvidence = {},
        .AccountTransport = {},
        .Attempt = application::github_provider_attempt{1U},
    };
    compiling_provider provider;

    application::github_profile_completion profile_completion;
    provider.resolve_profile(
        profile_request,
        [&](auto completion) {
            profile_completion = std::move(completion);
        });
    expect(
        std::get<application::github_profile_cancelled>(
            profile_completion)
                .Request
            == profile_request,
        "profile cancellation returns the exact phase request");

    application::github_pull_request_completion pull_completion;
    provider.fetch_pull_request(
        pull_request,
        [&](auto completion) {
            pull_completion = std::move(completion);
        });
    expect(
        std::get<application::github_pull_request_cancelled>(
            pull_completion)
                .Request
            == pull_request,
        "pull request cancellation returns the exact request context");

    const auto success =
        application::github_pull_request_completion{
            application::github_pull_request_success{
                .Request = pull_request,
                .Snapshot = valid_snapshot(),
            }};
    expect(
        std::get<application::github_pull_request_success>(success).Request
            == pull_request,
        "success alternative retains the exact request context");

    auto failure_rate_limits = rate_limits();
    auto failure_cause = *failure_rate_limits.Core;
    failure_cause.Remaining = 0U;
    failure_rate_limits.Core = failure_cause;
    const auto failure =
        application::github_pull_request_completion{
            application::github_pull_request_error{
                .Request = pull_request,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::rate_limit,
                        .RetryAfterSeconds = 30,
                        .RateLimits = failure_rate_limits,
                        .RateLimitCause = failure_cause,
                    },
            }};
    const auto& pull_request_error =
        std::get<application::github_pull_request_error>(failure);
    expect(
        pull_request_error.Request == pull_request
            && application::valid(pull_request_error.Error),
        "failure alternative retains exact context and a valid trigger");

    provider.cancel_profile(profile_request);
    provider.cancel_viewer(viewer_request);
    provider.cancel_capabilities(capability_request);
    provider.cancel_pull_request(pull_request);
    expect(
        provider.LastProfileCancellation == profile_request
            && provider.LastViewerCancellation == viewer_request
            && provider.LastCapabilityCancellation == capability_request
            && provider.LastPullRequestCancellation == pull_request,
        "every phase cancellation retains its complete typed identity");
}
}

auto main() -> int
{
    test_valid_models_and_identity_correlation();
    test_malformed_enums_fail_closed();
    test_default_collection_and_field_bounds();
    test_duplicate_identity_text_budget_and_rate_limit();
    test_completion_contexts_are_exact();
    return EXIT_SUCCESS;
}
