#include "github_read_contract.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <new>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bpr::adapters::github::detail
{
namespace
{
[[nodiscard]] auto failed(const contract_failure_code code)
    -> std::unexpected<contract_failure>
{
    return std::unexpected{contract_failure{.Code = code}};
}

[[nodiscard]] auto operation_index(const read_operation operation) noexcept
    -> std::optional<std::size_t>
{
    const auto iterator =
        std::ranges::find(read_operation_catalog, operation);
    if(iterator == read_operation_catalog.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(
        std::distance(read_operation_catalog.begin(), iterator));
}

[[nodiscard]] auto ascii_equal_fold(
    const std::string_view left,
    const std::string_view right) noexcept -> bool
{
    if(left.size() != right.size())
    {
        return false;
    }
    for(std::size_t index = 0U; index < left.size(); ++index)
    {
        const auto fold = [](const char value) {
            return value >= 'A' && value <= 'Z'
                ? static_cast<char>(value - 'A' + 'a')
                : value;
        };
        if(fold(left[index]) != fold(right[index]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr auto valid_capability(
    const core::github_capability_state state) noexcept -> bool
{
    switch(state)
    {
    case core::github_capability_state::unknown:
    case core::github_capability_state::supported:
    case core::github_capability_state::unsupported:
        return true;
    }
    return false;
}

[[nodiscard]] auto context_is_valid(
    const github_read_normalization_context& context) noexcept -> bool
{
    const auto server_valid =
        context.Server == server_contract::github_com
        || context.Server == server_contract::ghes_3_18;
    const auto& session = context.SessionRequest;
    const auto& pull_request = context.PullRequestRequest;
    return server_valid && context.Profile.value() != 0U
        && context.Host.value() != 0U && context.Account.value() != 0U
        && context.Repository.value() != 0U
        && context.PullRequest.value() != 0U
        && context.Snapshot.value() != 0U
        && context.CapabilityRevision.value() != 0U
        && context.SearchTerms == "is:open"
        && session.Request.value() != 0U
        && session.ApplicationSession.value() != 0U
        && session.Profile.value() != 0U
        && session.IdentityGeneration.value() != 0U
        && pull_request.Request.value() != 0U
        && pull_request.ApplicationSession.value() != 0U
        && pull_request.Profile.value() != 0U
        && pull_request.Host.value() != 0U
        && pull_request.Account.value() != 0U
        && core::valid(pull_request.Repository)
        && pull_request.Number.valid()
        && pull_request.Capabilities.value() != 0U
        && pull_request.IdentityGeneration.value() != 0U
        && pull_request.Generation.value() != 0U
        && context.Profile == session.Profile
        && context.Profile == pull_request.Profile
        && context.Host == pull_request.Host
        && context.Account == pull_request.Account
        && context.CapabilityRevision == pull_request.Capabilities
        && session.ApplicationSession
            == pull_request.ApplicationSession
        && session.IdentityGeneration
            == pull_request.IdentityGeneration
        && (
            !pull_request.ExpectedRepository.has_value()
            || *pull_request.ExpectedRepository == context.Repository)
        && (
            !pull_request.ExpectedPullRequest.has_value()
            || *pull_request.ExpectedPullRequest == context.PullRequest)
        && valid_capability(context.Capabilities.ReviewThreads)
        && valid_capability(context.Capabilities.ViewedFileMutations)
        && valid_capability(context.Capabilities.ResolveThread)
        && valid_capability(context.Capabilities.UnresolveThread)
        && context.RateLimit.Remaining <= context.RateLimit.Limit;
}

[[nodiscard]] auto parse_document(const wire_response& response)
    -> QJsonDocument
{
    QJsonParseError error;
    return QJsonDocument::fromJson(
        QByteArray{
            response.Body.data(),
            static_cast<qsizetype>(response.Body.size()),
        },
        &error);
}

[[nodiscard]] auto text(const QJsonValue& value) -> std::string
{
    const auto bytes = value.toString().toUtf8();
    return {
        bytes.constData(),
        static_cast<std::size_t>(bytes.size()),
    };
}

[[nodiscard]] auto integer(const QJsonValue& value) noexcept
    -> std::uint64_t
{
    return static_cast<std::uint64_t>(value.toDouble());
}

[[nodiscard]] auto uint32(const QJsonValue& value)
    -> std::optional<std::uint32_t>
{
    const auto number = integer(value);
    if(number > std::numeric_limits<std::uint32_t>::max())
    {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(number);
}

[[nodiscard]] auto optional_line(
    const QJsonValue& preferred,
    const QJsonValue& fallback) -> std::optional<std::uint32_t>
{
    const auto selected = preferred.isNull() ? fallback : preferred;
    if(selected.isNull())
    {
        return std::nullopt;
    }
    return uint32(selected);
}

[[nodiscard]] auto optional_rest_actor(const QJsonValue& value)
    -> std::optional<application::github_actor_record>
{
    if(value.isNull())
    {
        return std::nullopt;
    }
    const auto object = value.toObject();
    return application::github_actor_record{
        .Actor = application::github_actor_id{
            text(object.value("node_id"))},
        .Login = text(object.value("login")),
    };
}

[[nodiscard]] auto optional_graphql_actor(const QJsonValue& value)
    -> std::optional<application::github_actor_record>
{
    if(value.isNull())
    {
        return std::nullopt;
    }
    const auto object = value.toObject();
    return application::github_actor_record{
        .Actor = application::github_actor_id{
            text(object.value("id"))},
        .Login = text(object.value("login")),
    };
}

[[nodiscard]] auto full_name(
    const core::repository_locator& repository) -> std::string
{
    return repository.Owner + "/" + repository.Name;
}

[[nodiscard]] auto pull_request_state(
    const QString& value,
    const bool merged = false)
    -> application::github_pull_request_state
{
    if(merged)
    {
        return application::github_pull_request_state::merged;
    }
    return value == "open"
        ? application::github_pull_request_state::open
        : application::github_pull_request_state::closed;
}

[[nodiscard]] auto file_change(const QString& value)
    -> application::github_file_change
{
    if(value == "added")
    {
        return application::github_file_change::added;
    }
    if(value == "modified")
    {
        return application::github_file_change::modified;
    }
    if(value == "removed")
    {
        return application::github_file_change::removed;
    }
    if(value == "renamed")
    {
        return application::github_file_change::renamed;
    }
    if(value == "copied")
    {
        return application::github_file_change::copied;
    }
    if(value == "unchanged")
    {
        return application::github_file_change::unchanged;
    }
    return application::github_file_change::type_changed;
}

[[nodiscard]] auto viewed_state(const QString& value)
    -> std::optional<application::github_file_viewed_state>
{
    if(value == "DISMISSED")
    {
        return application::github_file_viewed_state::dismissed;
    }
    if(value == "UNVIEWED")
    {
        return application::github_file_viewed_state::unviewed;
    }
    if(value == "VIEWED")
    {
        return application::github_file_viewed_state::viewed;
    }
    return std::nullopt;
}

[[nodiscard]] auto review_state(const QString& value)
    -> application::github_review_state
{
    if(value == "PENDING")
    {
        return application::github_review_state::pending;
    }
    if(value == "COMMENTED")
    {
        return application::github_review_state::commented;
    }
    if(value == "APPROVED")
    {
        return application::github_review_state::approved;
    }
    if(value == "CHANGES_REQUESTED")
    {
        return application::github_review_state::changes_requested;
    }
    return application::github_review_state::dismissed;
}

[[nodiscard]] auto review_thread_subject(const QString& value)
    -> application::github_review_thread_subject
{
    return value == "FILE"
        ? application::github_review_thread_subject::file
        : application::github_review_thread_subject::line;
}

[[nodiscard]] auto check_status(const QString& value)
    -> application::github_check_status
{
    if(value == "queued")
    {
        return application::github_check_status::queued;
    }
    if(value == "in_progress")
    {
        return application::github_check_status::in_progress;
    }
    if(value == "completed")
    {
        return application::github_check_status::completed;
    }
    if(value == "waiting")
    {
        return application::github_check_status::waiting;
    }
    if(value == "requested")
    {
        return application::github_check_status::requested;
    }
    return application::github_check_status::pending;
}

[[nodiscard]] auto check_conclusion(const QJsonValue& value)
    -> application::github_check_conclusion
{
    if(value.isNull())
    {
        return application::github_check_conclusion::none;
    }
    const auto conclusion = value.toString();
    if(conclusion == "success")
    {
        return application::github_check_conclusion::success;
    }
    if(conclusion == "failure")
    {
        return application::github_check_conclusion::failure;
    }
    if(conclusion == "neutral")
    {
        return application::github_check_conclusion::neutral;
    }
    if(conclusion == "cancelled")
    {
        return application::github_check_conclusion::cancelled;
    }
    if(conclusion == "skipped")
    {
        return application::github_check_conclusion::skipped;
    }
    if(conclusion == "timed_out")
    {
        return application::github_check_conclusion::timed_out;
    }
    if(conclusion == "action_required")
    {
        return application::github_check_conclusion::action_required;
    }
    return application::github_check_conclusion::stale;
}

[[nodiscard]] auto validation_failure(
    const application::github_read_validation_error error)
    -> contract_failure_code
{
    switch(error)
    {
    case application::github_read_validation_error::identity_mismatch:
    case application::github_read_validation_error::missing_identity:
    case application::github_read_validation_error::invalid_context:
        return contract_failure_code::identity_mismatch;
    case application::github_read_validation_error::duplicate_identity:
        return contract_failure_code::duplicate_identity;
    case application::github_read_validation_error::text_too_long:
    case application::github_read_validation_error::collection_too_large:
    case application::github_read_validation_error::allocation_limit:
    case application::github_read_validation_error::total_text_too_large:
        return contract_failure_code::response_limit;
    case application::github_read_validation_error::none:
    case application::github_read_validation_error::invalid_limits:
    case application::github_read_validation_error::missing_required_text:
    case application::github_read_validation_error::invalid_text:
    case application::github_read_validation_error::invalid_repository:
    case application::github_read_validation_error::
        invalid_pull_request_number:
    case application::github_read_validation_error::invalid_commit_oid:
    case application::github_read_validation_error::invalid_line:
    case application::github_read_validation_error::invalid_enum:
    case application::github_read_validation_error::
        invalid_capability_revision:
    case application::github_read_validation_error::invalid_rate_limit:
    case application::github_read_validation_error::invalid_relationship:
        return contract_failure_code::unexpected_response_shape;
    }
    return contract_failure_code::unexpected_response_shape;
}

[[nodiscard]] auto review_threads_array(const QJsonDocument& document)
    -> QJsonArray
{
    return document.object()
        .value("data")
        .toObject()
        .value("repository")
        .toObject()
        .value("pullRequest")
        .toObject()
        .value("reviewThreads")
        .toObject()
        .value("nodes")
        .toArray();
}

[[nodiscard]] auto semantic_request(
    const github_read_normalization_context& context,
    const read_operation operation,
    const std::string_view head_oid) -> semantic_read_request
{
    semantic_read_request request{
        .Server = context.Server,
        .Operation = operation,
        .Owner = {},
        .Repository = {},
        .PullRequestNumber = 0U,
        .HeadOid = {},
        .SearchTerms = {},
        .Page = 1U,
    };
    if(operation != read_operation::get_viewer_identity)
    {
        request.Owner = context.PullRequestRequest.Repository.Owner;
        request.Repository = context.PullRequestRequest.Repository.Name;
    }
    switch(operation)
    {
    case read_operation::get_pull_request:
    case read_operation::list_pull_request_files:
    case read_operation::list_pull_request_commits:
    case read_operation::list_reviews:
    case read_operation::list_review_threads:
    case read_operation::list_issue_comments:
        request.PullRequestNumber =
            context.PullRequestRequest.Number.value();
        break;
    case read_operation::get_viewer_identity:
    case read_operation::get_repository:
    case read_operation::list_checks:
    case read_operation::search_pull_requests:
        break;
    }
    if(operation == read_operation::list_checks)
    {
        request.HeadOid = head_oid;
    }
    if(operation == read_operation::search_pull_requests)
    {
        request.SearchTerms = context.SearchTerms;
    }
    return request;
}
}

auto normalize_github_read(
    const github_read_normalization_context& context,
    const std::span<const operation_response> responses)
    -> normalized_read_result
{
    try
    {
        if(!context_is_valid(context))
        {
            return failed(contract_failure_code::identity_mismatch);
        }
        if(responses.size() < read_operation_catalog.size())
        {
            return failed(contract_failure_code::recording_exhausted);
        }
        if(responses.size() > read_operation_catalog.size())
        {
            return failed(contract_failure_code::unexpected_operation);
        }

        std::array<QJsonDocument, read_operation_catalog.size()> documents;
        std::array<bool, read_operation_catalog.size()> present{};
        for(const auto& response : responses)
        {
            const auto index = operation_index(response.Operation);
            if(!index.has_value() || present[*index])
            {
                return failed(contract_failure_code::unexpected_operation);
            }
            const auto validation = validate_strict_fixture_response(
                response.Operation,
                response.Response);
            if(!validation.has_value())
            {
                return std::unexpected{validation.error()};
            }
            documents[*index] = parse_document(response.Response);
            present[*index] = true;
        }
        if(std::ranges::find(present, false) != present.end())
        {
            return failed(contract_failure_code::recording_exhausted);
        }

        const auto document = [&documents](const read_operation operation)
            -> const QJsonDocument& {
            return documents[*operation_index(operation)];
        };
        const auto target_name = full_name(
            context.PullRequestRequest.Repository);

        const auto viewer = document(
            read_operation::get_viewer_identity).object();
        if(integer(viewer.value("id")) != context.Account.value())
        {
            return failed(contract_failure_code::identity_mismatch);
        }

        normalized_github_read result;
        result.Session = {
            .Profile = context.Profile,
            .Host = context.Host,
            .Viewer =
                {
                    .Account = context.Account,
                    .Node = application::github_actor_id{
                        text(viewer.value("node_id"))},
                    .Login = text(viewer.value("login")),
                    .DisplayName = text(viewer.value("name")),
                },
            .CapabilityEvidence =
                {
                    .Capabilities = context.Capabilities,
                    .Revision = context.CapabilityRevision,
                    .ContractVersion = 1U,
                },
            .RateLimit = context.RateLimit,
        };

        const auto repository = document(
            read_operation::get_repository).object();
        if(
            integer(repository.value("id")) != context.Repository.value()
            || !ascii_equal_fold(
                text(repository.value("full_name")),
                target_name)
            || !ascii_equal_fold(
                text(repository.value("name")),
                context.PullRequestRequest.Repository.Name)
            || !ascii_equal_fold(
                text(repository.value("owner").toObject().value("login")),
                context.PullRequestRequest.Repository.Owner))
        {
            return failed(contract_failure_code::identity_mismatch);
        }

        const auto pull_request = document(
            read_operation::get_pull_request).object();
        const auto base = pull_request.value("base").toObject();
        const auto head = pull_request.value("head").toObject();
        const auto base_repository = base.value("repo").toObject();
        if(
            integer(pull_request.value("id"))
                != context.PullRequest.value()
            || integer(pull_request.value("number"))
                != context.PullRequestRequest.Number.value()
            || integer(base_repository.value("id"))
                != context.Repository.value()
            || !ascii_equal_fold(
                text(base_repository.value("full_name")),
                target_name))
        {
            return failed(contract_failure_code::identity_mismatch);
        }
        const auto head_oid = text(head.value("sha"));

        result.PullRequest = {
            .Profile = context.Profile,
            .Host = context.Host,
            .Account = context.Account,
            .Viewer =
                {
                    .Actor = application::github_actor_id{
                        text(viewer.value("node_id"))},
                    .Login = text(viewer.value("login")),
                },
            .Snapshot = context.Snapshot,
            .Repository =
                {
                    .Repository = context.Repository,
                    .Node = application::github_node_id{
                        text(repository.value("node_id"))},
                    .Owner = context.PullRequestRequest.Repository.Owner,
                    .Name = context.PullRequestRequest.Repository.Name,
                    .DefaultBranch =
                        text(repository.value("default_branch")),
                    .Private = repository.value("private").toBool(),
                },
            .PullRequest =
                {
                    .PullRequest = context.PullRequest,
                    .Repository = context.Repository,
                    .Number = context.PullRequestRequest.Number,
                    .Node = application::github_node_id{
                        text(pull_request.value("node_id"))},
                    .Title = text(pull_request.value("title")),
                    .Body = text(pull_request.value("body")),
                    .State = pull_request_state(
                        pull_request.value("state").toString(),
                        pull_request.value("merged").toBool()),
                    .Draft = pull_request.value("draft").toBool(),
                    .Author =
                        optional_rest_actor(pull_request.value("user")),
                    .BaseOid = application::commit_oid{
                        text(base.value("sha"))},
                    .HeadOid = application::commit_oid{head_oid},
                    .BaseRef = text(base.value("ref")),
                    .HeadRef = text(head.value("ref")),
                },
            .Files = {},
            .Commits = {},
            .Reviews = {},
            .ReviewThreads = {},
            .IssueComments = {},
            .Checks = {},
            .SearchResults = {},
            .CapabilityEvidence =
                {
                    .Capabilities = context.Capabilities,
                    .Revision = context.CapabilityRevision,
                    .ContractVersion = 1U,
                },
            .RateLimit = context.RateLimit,
        };

        const auto files = document(
            read_operation::list_pull_request_files).array();
        result.PullRequest.Files.reserve(
            static_cast<std::size_t>(files.size()));
        for(const auto& value : files)
        {
            const auto object = value.toObject();
            std::optional<std::string> previous_path;
            if(object.contains("previous_filename"))
            {
                previous_path = text(object.value("previous_filename"));
            }
            result.PullRequest.Files.push_back(
                application::github_file_record{
                    .Path = text(object.value("filename")),
                    .PreviousPath = std::move(previous_path),
                    .Change =
                        file_change(object.value("status").toString()),
                    .Additions = integer(object.value("additions")),
                    .Deletions = integer(object.value("deletions")),
                    .ContentKind =
                        application::github_file_content_kind::unknown,
                });
        }

        const auto commits = document(
            read_operation::list_pull_request_commits).array();
        result.PullRequest.Commits.reserve(
            static_cast<std::size_t>(commits.size()));
        for(const auto& value : commits)
        {
            const auto object = value.toObject();
            auto headline =
                text(object.value("commit").toObject().value("message"));
            if(const auto newline = headline.find('\n');
               newline != std::string::npos)
            {
                headline.resize(newline);
            }
            result.PullRequest.Commits.push_back(
                application::github_commit_record{
                    .Oid = application::commit_oid{
                        text(object.value("sha"))},
                    .Node = application::github_node_id{
                        text(object.value("node_id"))},
                    .Author = optional_rest_actor(object.value("author")),
                    .Headline = std::move(headline),
                });
        }
        if(
            result.PullRequest.Commits.empty()
            || result.PullRequest.Commits.back().Oid.value() != head_oid)
        {
            return failed(contract_failure_code::identity_mismatch);
        }

        const auto reviews =
            document(read_operation::list_reviews).array();
        result.PullRequest.Reviews.reserve(
            static_cast<std::size_t>(reviews.size()));
        for(const auto& value : reviews)
        {
            const auto object = value.toObject();
            std::optional<application::commit_oid> commit;
            if(!object.value("commit_id").isNull())
            {
                commit = application::commit_oid{
                    text(object.value("commit_id"))};
            }
            result.PullRequest.Reviews.push_back(
                application::github_review_record{
                    .Review = application::github_review_id{
                        text(object.value("node_id"))},
                    .Author = optional_rest_actor(object.value("user")),
                    .State =
                        review_state(object.value("state").toString()),
                    .Commit = std::move(commit),
                    .Body = text(object.value("body")),
                });
        }

        const auto& review_threads_document =
            document(read_operation::list_review_threads);
        const auto graphql_repository = review_threads_document.object()
                                            .value("data")
                                            .toObject()
                                            .value("repository")
                                            .toObject();
        const auto graphql_pull_request =
            graphql_repository.value("pullRequest").toObject();
        if(
            text(graphql_repository.value("id"))
                != text(repository.value("node_id"))
            || text(graphql_pull_request.value("id"))
                != text(pull_request.value("node_id")))
        {
            return failed(contract_failure_code::identity_mismatch);
        }
        const auto graphql_files = graphql_pull_request.value("files")
                                       .toObject()
                                       .value("nodes")
                                       .toArray();
        if(
            static_cast<std::size_t>(graphql_files.size())
            != result.PullRequest.Files.size())
        {
            return failed(contract_failure_code::identity_mismatch);
        }
        std::unordered_map<
            std::string,
            application::github_file_viewed_state>
            viewed_states;
        viewed_states.reserve(
            static_cast<std::size_t>(graphql_files.size()));
        for(const auto& file_value : graphql_files)
        {
            const auto file = file_value.toObject();
            const auto state =
                viewed_state(file.value("viewerViewedState").toString());
            if(!state.has_value())
            {
                return failed(
                    contract_failure_code::unexpected_response_shape);
            }
            const auto inserted =
                viewed_states
                    .emplace(text(file.value("path")), *state)
                    .second;
            if(!inserted)
            {
                return failed(contract_failure_code::duplicate_identity);
            }
        }
        for(auto& file : result.PullRequest.Files)
        {
            const auto iterator = viewed_states.find(file.Path);
            if(iterator == viewed_states.end())
            {
                return failed(contract_failure_code::identity_mismatch);
            }
            file.ViewedState = iterator->second;
            viewed_states.erase(iterator);
        }
        if(!viewed_states.empty())
        {
            return failed(contract_failure_code::identity_mismatch);
        }
        const auto threads = review_threads_array(review_threads_document);
        result.PullRequest.ReviewThreads.reserve(
            static_cast<std::size_t>(threads.size()));
        for(const auto& thread_value : threads)
        {
            const auto thread = thread_value.toObject();
            application::github_review_thread_record thread_record{
                .Thread = application::github_thread_id{
                    text(thread.value("id"))},
                .Subject =
                    review_thread_subject(
                        thread.value("subjectType").toString()),
                .Path = text(thread.value("path")),
                .Line = optional_line(
                    thread.value("line"),
                    thread.value("originalLine")),
                .Resolved = thread.value("isResolved").toBool(),
                .Outdated = thread.value("isOutdated").toBool(),
                .ViewerCanResolve =
                    thread.value("viewerCanResolve").toBool(),
                .ViewerCanUnresolve =
                    thread.value("viewerCanUnresolve").toBool(),
                .Comments = {},
            };
            const auto comments = thread.value("comments")
                                      .toObject()
                                      .value("nodes")
                                      .toArray();
            thread_record.Comments.reserve(
                static_cast<std::size_t>(comments.size()));
            for(const auto& comment_value : comments)
            {
                const auto comment = comment_value.toObject();
                std::optional<application::github_comment_id> reply_to;
                if(!comment.value("replyTo").isNull())
                {
                    reply_to = application::github_comment_id{
                        text(
                            comment.value("replyTo")
                                .toObject()
                                .value("id"))};
                }
                thread_record.Comments.push_back(
                    application::github_review_comment_record{
                        .Comment = application::github_comment_id{
                            text(comment.value("id"))},
                        .ReplyTo = std::move(reply_to),
                        .Author = optional_graphql_actor(
                            comment.value("author")),
                        .Body = text(comment.value("bodyText")),
                        .Path = text(comment.value("path")),
                        .Line = optional_line(
                            comment.value("line"),
                            comment.value("originalLine")),
                        .Outdated = comment.value("outdated").toBool(),
                        .Deleted = false,
                    });
            }
            result.PullRequest.ReviewThreads.push_back(
                std::move(thread_record));
        }

        const auto issue_comments =
            document(read_operation::list_issue_comments).array();
        result.PullRequest.IssueComments.reserve(
            static_cast<std::size_t>(issue_comments.size()));
        for(const auto& value : issue_comments)
        {
            const auto object = value.toObject();
            result.PullRequest.IssueComments.push_back(
                application::github_issue_comment_record{
                    .Comment = application::github_comment_id{
                        text(object.value("node_id"))},
                    .Author = optional_rest_actor(object.value("user")),
                    .Body = text(object.value("body")),
                    .Deleted = false,
                });
        }

        const auto checks = document(read_operation::list_checks)
                                .object()
                                .value("check_runs")
                                .toArray();
        result.PullRequest.Checks.reserve(
            static_cast<std::size_t>(checks.size()));
        for(const auto& value : checks)
        {
            const auto object = value.toObject();
            if(text(object.value("head_sha")) != head_oid)
            {
                return failed(contract_failure_code::identity_mismatch);
            }
            result.PullRequest.Checks.push_back(
                application::github_check_record{
                    .Check = application::github_check_id{
                        text(object.value("node_id"))},
                    .Name = text(object.value("name")),
                    .Status =
                        check_status(object.value("status").toString()),
                    .Conclusion =
                        check_conclusion(object.value("conclusion")),
                });
        }

        const auto search_results = document(
            read_operation::search_pull_requests)
                                        .object()
                                        .value("items")
                                        .toArray();
        result.PullRequest.SearchResults.reserve(
            static_cast<std::size_t>(search_results.size()));
        for(const auto& value : search_results)
        {
            const auto object = value.toObject();
            const auto number = uint32(object.value("number"));
            if(!number.has_value())
            {
                return failed(
                    contract_failure_code::unexpected_response_shape);
            }
            result.PullRequest.SearchResults.push_back(
                application::github_search_result_record{
                    .Repository = context.Repository,
                    .PullRequestNode = application::github_node_id{
                        text(object.value("node_id"))},
                    .Number = core::pull_request_number{*number},
                    .Title = text(object.value("title")),
                    .State = pull_request_state(
                        object.value("state").toString()),
                    .Draft = object.value("draft").toBool(),
                });
        }

        const auto session_validation = application::validate(
            result.Session,
            context.SessionRequest,
            context.Limits);
        if(!session_validation.valid())
        {
            return failed(validation_failure(session_validation.Error));
        }
        const auto pull_request_validation = application::validate(
            result.PullRequest,
            context.PullRequestRequest,
            context.Limits);
        if(!pull_request_validation.valid())
        {
            return failed(
                validation_failure(pull_request_validation.Error));
        }
        return result;
    }
    catch(const std::bad_alloc&)
    {
        return failed(contract_failure_code::response_limit);
    }
}

auto perform_normalized_github_read(
    github_transport& transport,
    const github_read_normalization_context& context)
    -> normalized_read_result
{
    try
    {
        if(!context_is_valid(context))
        {
            return failed(contract_failure_code::identity_mismatch);
        }

        std::vector<operation_response> responses;
        responses.reserve(read_operation_catalog.size());
        std::string head_oid;
        for(const auto operation : read_operation_catalog)
        {
            const auto semantic =
                semantic_request(context, operation, head_oid);
            const auto request = compile_read_request(semantic);
            if(!request.has_value())
            {
                return std::unexpected{request.error()};
            }
            const auto response = transport.perform(*request);
            if(!response.has_value())
            {
                return std::unexpected{response.error()};
            }

            if(operation == read_operation::get_pull_request)
            {
                const auto validation = validate_strict_fixture_response(
                    operation,
                    *response);
                if(!validation.has_value())
                {
                    return std::unexpected{validation.error()};
                }
                head_oid = text(
                    parse_document(*response)
                        .object()
                        .value("head")
                        .toObject()
                        .value("sha"));
            }
            responses.push_back(
                operation_response{
                    .Operation = operation,
                    .Response = *response,
                });
        }
        return normalize_github_read(context, responses);
    }
    catch(const std::bad_alloc&)
    {
        return failed(contract_failure_code::response_limit);
    }
}
}
