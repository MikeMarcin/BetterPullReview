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

namespace bpr::adapters::github::detail {
	namespace {
		[[nodiscard]] auto failed(const contract_failure_code aCode)
			-> std::unexpected<contract_failure>
		{
			return std::unexpected{ contract_failure{ .Code = aCode } };
		}

		[[nodiscard]] auto operation_index(const read_operation aOperation) noexcept
			-> std::optional<std::size_t>
		{
			const auto iterator = std::ranges::find(read_operation_catalog, aOperation);
			if (iterator == read_operation_catalog.end()) {
				return std::nullopt;
			}
			return static_cast<std::size_t>(
				std::distance(read_operation_catalog.begin(), iterator));
		}

		[[nodiscard]] auto ascii_equal_fold(
			const std::string_view aLeft,
			const std::string_view aRight) noexcept -> bool
		{
			if (aLeft.size() != aRight.size()) {
				return false;
			}
			for (std::size_t index = 0U; index < aLeft.size(); ++index) {
				const auto fold = [](const char aValue) {
					return aValue >= 'A' && aValue <= 'Z'
						? static_cast<char>(aValue - 'A' + 'a')
						: aValue;
				};
				if (fold(aLeft[index]) != fold(aRight[index])) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] constexpr auto valid_capability(
			const core::github_capability_state aState) noexcept -> bool
		{
			switch (aState) {
			case core::github_capability_state::unknown:
			case core::github_capability_state::supported:
			case core::github_capability_state::unsupported:
				return true;
			}
			return false;
		}

		[[nodiscard]] auto context_is_valid(
			const github_read_normalization_context& aContext) noexcept -> bool
		{
			const auto serverValid = aContext.Server == server_contract::github_com
				|| aContext.Server == server_contract::ghes_3_18;
			const auto& session     = aContext.SessionRequest;
			const auto& pullRequest = aContext.PullRequestRequest;
			return serverValid && aContext.Profile.value() != 0U
				&& aContext.Host.value() != 0U && aContext.Account.value() != 0U
				&& aContext.Repository.value() != 0U
				&& aContext.PullRequest.value() != 0U
				&& aContext.Snapshot.value() != 0U
				&& aContext.CapabilityRevision.value() != 0U
				&& aContext.SearchTerms == "is:open"
				&& session.Request.value() != 0U
				&& session.ApplicationSession.value() != 0U
				&& session.Profile.value() != 0U
				&& session.IdentityGeneration.value() != 0U
				&& pullRequest.Request.value() != 0U
				&& pullRequest.ApplicationSession.value() != 0U
				&& pullRequest.Profile.value() != 0U
				&& pullRequest.Host.value() != 0U
				&& pullRequest.Account.value() != 0U
				&& core::valid(pullRequest.Repository)
				&& pullRequest.Number.valid()
				&& pullRequest.Capabilities.value() != 0U
				&& pullRequest.IdentityGeneration.value() != 0U
				&& pullRequest.Generation.value() != 0U
				&& aContext.Profile == session.Profile
				&& aContext.Profile == pullRequest.Profile
				&& aContext.Host == pullRequest.Host
				&& aContext.Account == pullRequest.Account
				&& aContext.CapabilityRevision == pullRequest.Capabilities
				&& session.ApplicationSession
				== pullRequest.ApplicationSession
				&& session.IdentityGeneration
				== pullRequest.IdentityGeneration
				&& (!pullRequest.ExpectedRepository.has_value()
					|| *pullRequest.ExpectedRepository == aContext.Repository)
				&& (!pullRequest.ExpectedPullRequest.has_value()
					|| *pullRequest.ExpectedPullRequest == aContext.PullRequest)
				&& valid_capability(aContext.Capabilities.ReviewThreads)
				&& valid_capability(aContext.Capabilities.ViewedFileMutations)
				&& valid_capability(aContext.Capabilities.ResolveThread)
				&& valid_capability(aContext.Capabilities.UnresolveThread)
				&& application::valid(aContext.RateLimits);
		}

		[[nodiscard]] auto parse_document(const wire_response& aResponse)
			-> QJsonDocument
		{
			QJsonParseError error;
			return QJsonDocument::fromJson(
				QByteArray{
					aResponse.Body.data(),
					static_cast<qsizetype>(aResponse.Body.size()),
				},
				&error);
		}

		[[nodiscard]] auto text(const QJsonValue& aValue) -> std::string
		{
			const auto bytes = aValue.toString().toUtf8();
			return {
				bytes.constData(),
				static_cast<std::size_t>(bytes.size()),
			};
		}

		[[nodiscard]] auto integer(const QJsonValue& aValue) noexcept
			-> std::uint64_t
		{
			return static_cast<std::uint64_t>(aValue.toDouble());
		}

		[[nodiscard]] auto uint32(const QJsonValue& aValue)
			-> std::optional<std::uint32_t>
		{
			const auto number = integer(aValue);
			if (number > std::numeric_limits<std::uint32_t>::max()) {
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(number);
		}

		[[nodiscard]] auto optional_line(const QJsonValue& aValue)
			-> std::optional<std::uint32_t>
		{
			if (aValue.isNull()) {
				return std::nullopt;
			}
			return uint32(aValue);
		}

		[[nodiscard]] auto optional_rest_actor(const QJsonValue& aValue)
			-> std::optional<application::github_actor_record>
		{
			if (aValue.isNull()) {
				return std::nullopt;
			}
			const auto object = aValue.toObject();
			return application::github_actor_record{
				.Actor = application::github_actor_id{
					text(object.value("node_id")) },
				.Login = text(object.value("login")),
			};
		}

		[[nodiscard]] auto optional_graphql_actor(const QJsonValue& aValue)
			-> std::optional<application::github_actor_record>
		{
			if (aValue.isNull()) {
				return std::nullopt;
			}
			const auto object = aValue.toObject();
			return application::github_actor_record{
				.Actor = application::github_actor_id{
					text(object.value("id")) },
				.Login = text(object.value("login")),
			};
		}

		[[nodiscard]] auto full_name(
			const core::repository_locator& aRepository) -> std::string
		{
			return aRepository.Owner + "/" + aRepository.Name;
		}

		[[nodiscard]] auto pull_request_state(
			const QString& aValue,
			const bool     aMerged = false)
			-> application::github_pull_request_state
		{
			if (aMerged) {
				return application::github_pull_request_state::merged;
			}
			return aValue == "open"
				? application::github_pull_request_state::open
				: application::github_pull_request_state::closed;
		}

		[[nodiscard]] auto file_change(const QString& aValue)
			-> application::github_file_change
		{
			if (aValue == "added") {
				return application::github_file_change::added;
			}
			if (aValue == "modified") {
				return application::github_file_change::modified;
			}
			if (aValue == "removed") {
				return application::github_file_change::removed;
			}
			if (aValue == "renamed") {
				return application::github_file_change::renamed;
			}
			if (aValue == "copied") {
				return application::github_file_change::copied;
			}
			if (aValue == "unchanged") {
				return application::github_file_change::unchanged;
			}
			return application::github_file_change::type_changed;
		}

		[[nodiscard]] auto viewed_state(const QString& aValue)
			-> std::optional<application::github_file_viewed_state>
		{
			if (aValue == "DISMISSED") {
				return application::github_file_viewed_state::dismissed;
			}
			if (aValue == "UNVIEWED") {
				return application::github_file_viewed_state::unviewed;
			}
			if (aValue == "VIEWED") {
				return application::github_file_viewed_state::viewed;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto review_state(const QString& aValue)
			-> application::github_review_state
		{
			if (aValue == "PENDING") {
				return application::github_review_state::pending;
			}
			if (aValue == "COMMENTED") {
				return application::github_review_state::commented;
			}
			if (aValue == "APPROVED") {
				return application::github_review_state::approved;
			}
			if (aValue == "CHANGES_REQUESTED") {
				return application::github_review_state::changes_requested;
			}
			return application::github_review_state::dismissed;
		}

		[[nodiscard]] auto review_thread_subject(const QString& aValue)
			-> application::github_review_thread_subject
		{
			return aValue == "FILE"
				? application::github_review_thread_subject::file
				: application::github_review_thread_subject::line;
		}

		[[nodiscard]] auto diff_side(const QString& aValue)
			-> std::optional<application::github_diff_side>
		{
			if (aValue == QStringLiteral("LEFT")) {
				return application::github_diff_side::left;
			}
			if (aValue == QStringLiteral("RIGHT")) {
				return application::github_diff_side::right;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto check_status(const QString& aValue)
			-> application::github_check_status
		{
			if (aValue == "queued") {
				return application::github_check_status::queued;
			}
			if (aValue == "in_progress") {
				return application::github_check_status::in_progress;
			}
			if (aValue == "completed") {
				return application::github_check_status::completed;
			}
			if (aValue == "waiting") {
				return application::github_check_status::waiting;
			}
			if (aValue == "requested") {
				return application::github_check_status::requested;
			}
			return application::github_check_status::pending;
		}

		[[nodiscard]] auto check_conclusion(const QJsonValue& aValue)
			-> application::github_check_conclusion
		{
			if (aValue.isNull()) {
				return application::github_check_conclusion::none;
			}
			const auto conclusion = aValue.toString();
			if (conclusion == "success") {
				return application::github_check_conclusion::success;
			}
			if (conclusion == "failure") {
				return application::github_check_conclusion::failure;
			}
			if (conclusion == "neutral") {
				return application::github_check_conclusion::neutral;
			}
			if (conclusion == "cancelled") {
				return application::github_check_conclusion::cancelled;
			}
			if (conclusion == "skipped") {
				return application::github_check_conclusion::skipped;
			}
			if (conclusion == "timed_out") {
				return application::github_check_conclusion::timed_out;
			}
			if (conclusion == "action_required") {
				return application::github_check_conclusion::action_required;
			}
			return application::github_check_conclusion::stale;
		}

		[[nodiscard]] auto validation_failure(
			const application::github_read_validation_error aError)
			-> contract_failure_code
		{
			switch (aError) {
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

		[[nodiscard]] auto review_threads_array(const QJsonDocument& aDocument)
			-> QJsonArray
		{
			return aDocument.object()
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
			const github_read_normalization_context& aContext,
			const read_operation                     aOperation,
			const std::string_view                   aHeadOid) -> semantic_read_request
		{
			semantic_read_request request{
				.Server            = aContext.Server,
				.Operation         = aOperation,
				.Owner             = { },
				.Repository        = { },
				.PullRequestNumber = 0U,
				.HeadOid           = { },
				.SearchTerms       = { },
				.Page              = 1U,
				.GraphqlPage       = std::nullopt,
			};
			if (aOperation != read_operation::get_viewer_identity) {
				request.Owner      = aContext.PullRequestRequest.Repository.Owner;
				request.Repository = aContext.PullRequestRequest.Repository.Name;
			}
			switch (aOperation) {
			case read_operation::get_pull_request:
			case read_operation::list_pull_request_files:
			case read_operation::list_pull_request_commits:
			case read_operation::list_reviews:
			case read_operation::list_review_threads:
			case read_operation::list_issue_comments:
				request.PullRequestNumber = aContext.PullRequestRequest.Number.value();
				break;
			case read_operation::get_viewer_identity:
			case read_operation::get_repository:
			case read_operation::list_checks:
			case read_operation::search_pull_requests:
				break;
			}
			if (aOperation == read_operation::list_checks) {
				request.HeadOid = aHeadOid;
			}
			if (aOperation == read_operation::list_review_threads) {
				request.GraphqlPage = github_graphql_page_request{
					.Kind  = github_graphql_page_kind::initial,
					.Shape = {
						.ReviewThreads       = true,
						.ViewedFileState     = true,
						.ResolvePermission   = true,
						.UnresolvePermission = true,
					},
					.AfterCursor = { },
					.ThreadId    = { },
				};
			}
			if (aOperation == read_operation::search_pull_requests) {
				request.SearchTerms = aContext.SearchTerms;
			}
			return request;
		}
	}

	auto normalize_github_read(
		const github_read_normalization_context&  aContext,
		const std::span<const operation_response> aResponses)
		-> normalized_read_result
	{
		try {
			if (!context_is_valid(aContext)) {
				return failed(contract_failure_code::identity_mismatch);
			}
			if (aResponses.size() < read_operation_catalog.size()) {
				return failed(contract_failure_code::recording_exhausted);
			}
			if (aResponses.size() > read_operation_catalog.size()) {
				return failed(contract_failure_code::unexpected_operation);
			}

			std::array<QJsonDocument, read_operation_catalog.size()> documents;
			std::array<bool, read_operation_catalog.size()>          present{ };
			for (const auto& response : aResponses) {
				const auto index = operation_index(response.Operation);
				if (!index.has_value() || present[*index]) {
					return failed(contract_failure_code::unexpected_operation);
				}
				const auto validation = validate_strict_fixture_response(
					response.Operation,
					response.Response);
				if (!validation.has_value()) {
					return std::unexpected{ validation.error() };
				}
				documents[*index] = parse_document(response.Response);
				present[*index]   = true;
			}
			if (std::ranges::find(present, false) != present.end()) {
				return failed(contract_failure_code::recording_exhausted);
			}

			const auto document = [&documents](const read_operation aOperation)
				-> const QJsonDocument& {
				return documents[*operation_index(aOperation)];
			};
			const auto targetName = full_name(
				aContext.PullRequestRequest.Repository);

			const auto viewer = document(
									read_operation::get_viewer_identity)
									.object();
			if (integer(viewer.value("id")) != aContext.Account.value()) {
				return failed(contract_failure_code::identity_mismatch);
			}

			normalized_github_read result;
			result.Session = {
				.Profile = aContext.Profile,
				.Host    = aContext.Host,
				.Viewer  = {
					.Account = aContext.Account,
					.Node    = application::github_actor_id{
						text(viewer.value("node_id")) },
					.Login       = text(viewer.value("login")),
					.DisplayName = text(viewer.value("name")),
				},
				.CapabilityEvidence = {
					.Capabilities = aContext.Capabilities,
					.Revision     = aContext.CapabilityRevision,
					.ReadShape    = {
						.ReviewThreads       = true,
						.ViewedFileState     = true,
						.ResolvePermission   = true,
						.UnresolvePermission = true,
					},
					.ContractVersion = 1U,
				},
				.RateLimits = aContext.RateLimits,
			};

			const auto repository = document(
										read_operation::get_repository)
										.object();
			if (
				integer(repository.value("id")) != aContext.Repository.value()
				|| !ascii_equal_fold(
					text(repository.value("full_name")),
					targetName)
				|| !ascii_equal_fold(
					text(repository.value("name")),
					aContext.PullRequestRequest.Repository.Name)
				|| !ascii_equal_fold(
					text(repository.value("owner").toObject().value("login")),
					aContext.PullRequestRequest.Repository.Owner)) {
				return failed(contract_failure_code::identity_mismatch);
			}

			const auto pullRequest    = document(
											read_operation::get_pull_request)
											.object();
			const auto base           = pullRequest.value("base").toObject();
			const auto head           = pullRequest.value("head").toObject();
			const auto baseRepository = base.value("repo").toObject();
			if (
				integer(pullRequest.value("id"))
					!= aContext.PullRequest.value()
				|| integer(pullRequest.value("number"))
					!= aContext.PullRequestRequest.Number.value()
				|| integer(baseRepository.value("id"))
					!= aContext.Repository.value()
				|| !ascii_equal_fold(
					text(baseRepository.value("full_name")),
					targetName)) {
				return failed(contract_failure_code::identity_mismatch);
			}
			const auto headOid = text(head.value("sha"));

			result.PullRequest = {
				.Profile = aContext.Profile,
				.Host    = aContext.Host,
				.Account = aContext.Account,
				.Viewer  = {
					.Actor = application::github_actor_id{
						text(viewer.value("node_id")) },
					.Login = text(viewer.value("login")),
				},
				.Snapshot   = aContext.Snapshot,
				.Repository = {
					.Repository    = aContext.Repository,
					.Node          = application::github_node_id{ text(repository.value("node_id")) },
					.Owner         = aContext.PullRequestRequest.Repository.Owner,
					.Name          = aContext.PullRequestRequest.Repository.Name,
					.DefaultBranch = text(repository.value("default_branch")),
					.Private       = repository.value("private").toBool(),
				},
				.PullRequest = {
					.PullRequest = aContext.PullRequest,
					.Repository  = aContext.Repository,
					.Number      = aContext.PullRequestRequest.Number,
					.Node        = application::github_node_id{ text(pullRequest.value("node_id")) },
					.Title       = text(pullRequest.value("title")),
					.Body        = text(pullRequest.value("body")),
					.State       = pull_request_state(pullRequest.value("state").toString(), pullRequest.value("merged").toBool()),
					.Draft       = pullRequest.value("draft").toBool(),
					.Author      = optional_rest_actor(pullRequest.value("user")),
					.BaseOid     = application::commit_oid{ text(base.value("sha")) },
					.HeadOid     = application::commit_oid{ headOid },
					.BaseRef     = text(base.value("ref")),
					.HeadRef     = text(head.value("ref")),
				},
				.Files                     = { },
				.Commits                   = { },
				.Reviews                   = { },
				.ReviewThreadsAvailability = application::github_collection_availability::available,
				.ReviewThreads             = { },
				.IssueComments             = { },
				.Checks                    = { },
				.SearchResults             = { },
				.CapabilityEvidence        = {
					.Capabilities = aContext.Capabilities,
					.Revision     = aContext.CapabilityRevision,
					.ReadShape    = {
						.ReviewThreads       = true,
						.ViewedFileState     = true,
						.ResolvePermission   = true,
						.UnresolvePermission = true,
					},
					.ContractVersion = 1U,
				},
				.RateLimits = aContext.RateLimits,
			};

			const auto files = document(
								   read_operation::list_pull_request_files)
								   .array();
			result.PullRequest.Files.reserve(
				static_cast<std::size_t>(files.size()));
			for (const auto& value : files) {
				const auto                 object = value.toObject();
				std::optional<std::string> previousPath;
				if (object.contains("previous_filename")) {
					previousPath = text(object.value("previous_filename"));
				}
				result.PullRequest.Files.push_back(
					application::github_file_record{
						.Path         = text(object.value("filename")),
						.PreviousPath = std::move(previousPath),
						.Change       = file_change(object.value("status").toString()),
						.Additions    = integer(object.value("additions")),
						.Deletions    = integer(object.value("deletions")),
						.ContentKind  = application::github_file_content_kind::unknown,
					});
			}

			const auto commits = document(
									 read_operation::list_pull_request_commits)
									 .array();
			result.PullRequest.Commits.reserve(
				static_cast<std::size_t>(commits.size()));
			for (const auto& value : commits) {
				const auto object   = value.toObject();
				auto       headline = text(object.value("commit").toObject().value("message"));
				if (const auto newline = headline.find('\n');
					newline != std::string::npos) {
					headline.resize(newline);
				}
				result.PullRequest.Commits.push_back(
					application::github_commit_record{
						.Oid = application::commit_oid{
							text(object.value("sha")) },
						.Node     = application::github_node_id{ text(object.value("node_id")) },
						.Author   = optional_rest_actor(object.value("author")),
						.Headline = std::move(headline),
					});
			}
			if (
				result.PullRequest.Commits.empty()
				|| result.PullRequest.Commits.back().Oid.value() != headOid) {
				return failed(contract_failure_code::identity_mismatch);
			}

			const auto reviews = document(read_operation::list_reviews).array();
			result.PullRequest.Reviews.reserve(
				static_cast<std::size_t>(reviews.size()));
			for (const auto& value : reviews) {
				const auto                             object = value.toObject();
				std::optional<application::commit_oid> commit;
				if (!object.value("commit_id").isNull()) {
					commit = application::commit_oid{
						text(object.value("commit_id"))
					};
				}
				result.PullRequest.Reviews.push_back(
					application::github_review_record{
						.DatabaseId = integer(object.value("id")),
						.Review     = application::github_review_id{
							text(object.value("node_id")) },
						.Author = optional_rest_actor(object.value("user")),
						.State  = review_state(object.value("state").toString()),
						.Commit = std::move(commit),
						.Body   = text(object.value("body")),
					});
			}

			const auto& reviewThreadsDocument = document(read_operation::list_review_threads);
			const auto  graphqlRepository     = reviewThreadsDocument.object()
													.value("data")
													.toObject()
													.value("repository")
													.toObject();
			const auto  graphqlPullRequest    = graphqlRepository.value("pullRequest").toObject();
			if (
				text(graphqlRepository.value("id"))
					!= text(repository.value("node_id"))
				|| text(graphqlPullRequest.value("id"))
					!= text(pullRequest.value("node_id"))
				|| text(graphqlPullRequest.value("headRefOid")) != headOid) {
				return failed(contract_failure_code::identity_mismatch);
			}
			const auto graphqlFiles = graphqlPullRequest.value("files")
										  .toObject()
										  .value("nodes")
										  .toArray();
			if (
				static_cast<std::size_t>(graphqlFiles.size())
				!= result.PullRequest.Files.size()) {
				return failed(contract_failure_code::identity_mismatch);
			}
			std::unordered_map<
				std::string,
				application::github_file_viewed_state>
				viewedStates;
			viewedStates.reserve(
				static_cast<std::size_t>(graphqlFiles.size()));
			for (const auto& fileValue : graphqlFiles) {
				const auto file  = fileValue.toObject();
				const auto state = viewed_state(file.value("viewerViewedState").toString());
				if (!state.has_value()) {
					return failed(
						contract_failure_code::unexpected_response_shape);
				}
				const auto inserted = viewedStates
										  .emplace(text(file.value("path")), *state)
										  .second;
				if (!inserted) {
					return failed(contract_failure_code::duplicate_identity);
				}
			}
			for (auto& file : result.PullRequest.Files) {
				const auto iterator = viewedStates.find(file.Path);
				if (iterator == viewedStates.end()) {
					return failed(contract_failure_code::identity_mismatch);
				}
				file.ViewedState = iterator->second;
				viewedStates.erase(iterator);
			}
			if (!viewedStates.empty()) {
				return failed(contract_failure_code::identity_mismatch);
			}
			const auto threads = review_threads_array(reviewThreadsDocument);
			result.PullRequest.ReviewThreads.reserve(
				static_cast<std::size_t>(threads.size()));
			for (const auto& threadValue : threads) {
				const auto thread = threadValue.toObject();
				const auto side   = diff_side(thread.value("diffSide").toString());
				if (!side.has_value()) {
					return failed(contract_failure_code::unexpected_response_shape);
				}
				application::github_review_thread_record threadRecord{
					.Thread = application::github_thread_id{
						text(thread.value("id")) },
					.Subject                   = review_thread_subject(thread.value("subjectType").toString()),
					.Path                      = text(thread.value("path")),
					.Line                      = optional_line(thread.value("line")),
					.OriginalLine              = optional_line(thread.value("originalLine")),
					.DiffSide                  = *side,
					.Resolved                  = thread.value("isResolved").toBool(),
					.Outdated                  = thread.value("isOutdated").toBool(),
					.ViewerResolvePermission   = thread.value("viewerCanResolve").toBool() ? application::github_permission_state::allowed : application::github_permission_state::denied,
					.ViewerUnresolvePermission = thread.value("viewerCanUnresolve").toBool() ? application::github_permission_state::allowed : application::github_permission_state::denied,
					.Comments                  = { },
				};
				const auto comments = thread.value("comments")
										  .toObject()
										  .value("nodes")
										  .toArray();
				threadRecord.Comments.reserve(
					static_cast<std::size_t>(comments.size()));
				for (const auto& commentValue : comments) {
					const auto                                    comment = commentValue.toObject();
					std::optional<application::github_comment_id> replyTo;
					if (!comment.value("replyTo").isNull()) {
						replyTo = application::github_comment_id{
							text(
								comment.value("replyTo")
									.toObject()
									.value("id"))
						};
					}
					threadRecord.Comments.push_back(
						application::github_review_comment_record{
							.Comment = application::github_comment_id{
								text(comment.value("id")) },
							.ReplyTo      = std::move(replyTo),
							.Author       = optional_graphql_actor(comment.value("author")),
							.Body         = text(comment.value("bodyText")),
							.Path         = text(comment.value("path")),
							.Line         = optional_line(comment.value("line")),
							.OriginalLine = optional_line(comment.value("originalLine")),
							.Outdated     = comment.value("outdated").toBool(),
							.Deleted      = false,
						});
				}
				result.PullRequest.ReviewThreads.push_back(
					std::move(threadRecord));
			}

			const auto issueComments = document(read_operation::list_issue_comments).array();
			result.PullRequest.IssueComments.reserve(
				static_cast<std::size_t>(issueComments.size()));
			for (const auto& value : issueComments) {
				const auto object = value.toObject();
				result.PullRequest.IssueComments.push_back(
					application::github_issue_comment_record{
						.Comment = application::github_comment_id{
							text(object.value("node_id")) },
						.Author  = optional_rest_actor(object.value("user")),
						.Body    = text(object.value("body")),
						.Deleted = false,
					});
			}

			const auto checks = document(read_operation::list_checks)
									.object()
									.value("check_runs")
									.toArray();
			result.PullRequest.Checks.reserve(
				static_cast<std::size_t>(checks.size()));
			for (const auto& value : checks) {
				const auto object = value.toObject();
				if (text(object.value("head_sha")) != headOid) {
					return failed(contract_failure_code::identity_mismatch);
				}
				result.PullRequest.Checks.push_back(
					application::github_check_record{
						.Check = application::github_check_id{
							text(object.value("node_id")) },
						.Name       = text(object.value("name")),
						.Status     = check_status(object.value("status").toString()),
						.Conclusion = check_conclusion(object.value("conclusion")),
					});
			}

			const auto searchResults = document(
										   read_operation::search_pull_requests)
										   .object()
										   .value("items")
										   .toArray();
			result.PullRequest.SearchResults.reserve(
				static_cast<std::size_t>(searchResults.size()));
			for (const auto& value : searchResults) {
				const auto object = value.toObject();
				const auto number = uint32(object.value("number"));
				if (!number.has_value()) {
					return failed(
						contract_failure_code::unexpected_response_shape);
				}
				result.PullRequest.SearchResults.push_back(
					application::github_search_result_record{
						.Repository      = aContext.Repository,
						.PullRequestNode = application::github_node_id{
							text(object.value("node_id")) },
						.Number = core::pull_request_number{ *number },
						.Title  = text(object.value("title")),
						.State  = pull_request_state(object.value("state").toString()),
						.Draft  = object.value("draft").toBool(),
					});
			}

			const auto sessionValidation = application::validate(
				result.Session,
				aContext.SessionRequest,
				aContext.Limits);
			if (!sessionValidation.valid()) {
				return failed(validation_failure(sessionValidation.Error));
			}
			const auto pullRequestValidation = application::validate(
				result.PullRequest,
				aContext.PullRequestRequest,
				aContext.Limits);
			if (!pullRequestValidation.valid()) {
				return failed(
					validation_failure(pullRequestValidation.Error));
			}
			return result;
		} catch (const std::bad_alloc&) {
			return failed(contract_failure_code::response_limit);
		}
	}

	auto perform_normalized_github_read(
		github_transport&                        aTransport,
		const github_read_normalization_context& aContext)
		-> normalized_read_result
	{
		try {
			if (!context_is_valid(aContext)) {
				return failed(contract_failure_code::identity_mismatch);
			}

			std::vector<operation_response> responses;
			responses.reserve(read_operation_catalog.size());
			std::string headOid;
			for (const auto operation : read_operation_catalog) {
				const auto semantic = semantic_request(aContext, operation, headOid);
				const auto request  = compile_read_request(semantic);
				if (!request.has_value()) {
					return std::unexpected{ request.error() };
				}
				const auto response = aTransport.perform(*request);
				if (!response.has_value()) {
					return std::unexpected{ response.error() };
				}

				if (operation == read_operation::get_pull_request) {
					const auto validation = validate_strict_fixture_response(
						operation,
						*response);
					if (!validation.has_value()) {
						return std::unexpected{ validation.error() };
					}
					headOid = text(
						parse_document(*response)
							.object()
							.value("head")
							.toObject()
							.value("sha"));
				}
				responses.push_back(
					operation_response{
						.Operation = operation,
						.Response  = *response,
					});
			}
			return normalize_github_read(aContext, responses);
		} catch (const std::bad_alloc&) {
			return failed(contract_failure_code::response_limit);
		}
	}
}
