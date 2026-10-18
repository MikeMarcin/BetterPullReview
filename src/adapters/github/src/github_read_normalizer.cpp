#include "github_read_contract.hpp"
#include "github_review_comment_grouping.hpp"

#include <glaze/json.hpp>

#include <algorithm>
#include <array>
#include <new>
#include <optional>
#include <string>
#include <string_view>
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
			const auto iterator = std::ranges::find(all_read_operations, aOperation);
			if (iterator == all_read_operations.end()) {
				return std::nullopt;
			}
			return static_cast<std::size_t>(
				std::distance(all_read_operations.begin(), iterator));
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

		/// Reports whether an admission can be normalized at all.
		///
		/// The seven operations named here are the ones every normalized read
		/// reads from unconditionally, so an admission missing one has no
		/// document to answer with. The two conditional operations are viewer
		/// identity and the GraphQL projection; the first must be replaced by
		/// a stated viewer when it is absent, because a session read model
		/// without a viewer is not a thing this normalizer may invent.
		[[nodiscard]] auto admission_is_valid(
			const github_read_admission& aAdmission) -> bool
		{
			constexpr std::array<read_operation, 7> required{
				read_operation::get_repository,
				read_operation::get_pull_request,
				read_operation::list_pull_request_files,
				read_operation::list_pull_request_commits,
				read_operation::list_reviews,
				read_operation::list_issue_comments,
				read_operation::list_checks,
			};
			const auto present = [&aAdmission](const read_operation aOperation) {
				return std::ranges::find(aAdmission.Operations, aOperation)
					!= aAdmission.Operations.end();
			};
			if (!std::ranges::all_of(required, present)) {
				return false;
			}
			if (!present(read_operation::search_pull_requests)) {
				return false;
			}
			if (
				!present(read_operation::get_viewer_identity)
				&& !aAdmission.Viewer.has_value()) {
				return false;
			}
			for (std::size_t index = 0U; index < aAdmission.Operations.size();
				 ++index) {
				const auto operation = aAdmission.Operations[index];
				if (std::ranges::find(all_read_operations, operation)
					== all_read_operations.end()) {
					return false;
				}
				for (std::size_t other = index + 1U;
					 other < aAdmission.Operations.size();
					 ++other) {
					if (aAdmission.Operations[other] == operation) {
						return false;
					}
				}
			}
			return true;
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

		// These private wire records extract already structurally validated responses.
		// Strict fixture validation remains responsible for field inventories,
		// nullable relationships and pagination completeness. Glaze additionally
		// validates skipped JSON and decodes integers without a floating-point step.
		struct normalization_json_options : glz::opts
		{
			// NOLINTNEXTLINE(readability-identifier-naming) -- Glaze option name.
			bool validate_skipped = true;
			// NOLINTNEXTLINE(readability-identifier-naming) -- Glaze option name.
			bool validate_trailing_whitespace = true;
			constexpr normalization_json_options()
			{
				null_terminated       = false;
				error_on_unknown_keys = false;
				error_on_missing_keys = true;
			}
		};

		struct json_rest_actor
		{
			std::string NodeId;
			std::string Login;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"node_id", &json_rest_actor::NodeId,
					"login", &json_rest_actor::Login);
			};
		};

		struct json_graphql_actor
		{
			std::string Id;
			std::string Login;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &json_graphql_actor::Id,
					"login", &json_graphql_actor::Login);
			};
		};

		struct json_viewer
		{
			std::uint64_t              Id = 0U;
			std::string                NodeId;
			std::string                Login;
			std::optional<std::string> Name;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &json_viewer::Id,
					"node_id", &json_viewer::NodeId,
					"login", &json_viewer::Login,
					"name", &json_viewer::Name);
			};
		};

		struct json_owner
		{
			std::string Login;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"login", &json_owner::Login);
			};
		};

		struct json_repository
		{
			std::uint64_t Id = 0U;
			std::string   NodeId;
			std::string   Name;
			std::string   FullName;
			json_owner    Owner;
			std::string   DefaultBranch;
			bool          Private = false;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &json_repository::Id,
					"node_id", &json_repository::NodeId,
					"name", &json_repository::Name,
					"full_name", &json_repository::FullName,
					"owner", &json_repository::Owner,
					"default_branch", &json_repository::DefaultBranch,
					"private", &json_repository::Private);
			};
		};

		struct json_ref_repository
		{
			std::uint64_t Id = 0U;
			std::string   FullName;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &json_ref_repository::Id,
					"full_name", &json_ref_repository::FullName);
			};
		};

		struct json_pull_ref
		{
			std::string                        Ref;
			std::string                        Sha;
			std::optional<json_ref_repository> Repo;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"ref", &json_pull_ref::Ref,
					"sha", &json_pull_ref::Sha,
					"repo", &json_pull_ref::Repo);
			};
		};

		struct json_pull_request
		{
			std::uint64_t                  Id     = 0U;
			std::uint32_t                  Number = 0U;
			std::string                    NodeId;
			std::string                    Title;
			std::optional<std::string>     Body;
			std::string                    State;
			bool                           Merged = false;
			bool                           Draft  = false;
			std::optional<json_rest_actor> User;
			json_pull_ref                  Base;
			json_pull_ref                  Head;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &json_pull_request::Id,
					"number", &json_pull_request::Number,
					"node_id", &json_pull_request::NodeId,
					"title", &json_pull_request::Title,
					"body", &json_pull_request::Body,
					"state", &json_pull_request::State,
					"merged", &json_pull_request::Merged,
					"draft", &json_pull_request::Draft,
					"user", &json_pull_request::User,
					"base", &json_pull_request::Base,
					"head", &json_pull_request::Head);
			};
		};

		struct json_file
		{
			std::string                Filename;
			std::optional<std::string> PreviousFilename;
			std::string                Status;
			std::uint64_t              Additions = 0U;
			std::uint64_t              Deletions = 0U;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"filename", &json_file::Filename,
					"previous_filename", &json_file::PreviousFilename,
					"status", &json_file::Status,
					"additions", &json_file::Additions,
					"deletions", &json_file::Deletions);
			};
		};

		struct json_commit_message
		{
			std::string Message;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"message", &json_commit_message::Message);
			};
		};

		struct json_commit
		{
			std::string                    Sha;
			std::string                    NodeId;
			std::optional<json_rest_actor> Author;
			json_commit_message            Commit;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"sha", &json_commit::Sha,
					"node_id", &json_commit::NodeId,
					"author", &json_commit::Author,
					"commit", &json_commit::Commit);
			};
		};

		struct json_review
		{
			std::uint64_t                  Id = 0U;
			std::string                    NodeId;
			std::optional<json_rest_actor> User;
			std::string                    State;
			std::optional<std::string>     CommitId;
			std::optional<std::string>     Body;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &json_review::Id,
					"node_id", &json_review::NodeId,
					"user", &json_review::User,
					"state", &json_review::State,
					"commit_id", &json_review::CommitId,
					"body", &json_review::Body);
			};
		};

		struct json_node
		{
			std::string Id;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &json_node::Id);
			};
		};

		struct json_graphql_comment
		{
			std::string                       Id;
			std::optional<json_node>          ReplyTo;
			std::optional<json_graphql_actor> Author;
			std::string                       BodyText;
			std::string                       Path;
			std::optional<std::uint32_t>      Line;
			std::optional<std::uint32_t>      OriginalLine;
			bool                              Outdated = false;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &json_graphql_comment::Id,
					"replyTo", &json_graphql_comment::ReplyTo,
					"author", &json_graphql_comment::Author,
					"bodyText", &json_graphql_comment::BodyText,
					"path", &json_graphql_comment::Path,
					"line", &json_graphql_comment::Line,
					"originalLine", &json_graphql_comment::OriginalLine,
					"outdated", &json_graphql_comment::Outdated);
			};
		};

		struct json_comment_connection
		{
			std::vector<json_graphql_comment> Nodes;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"nodes", &json_comment_connection::Nodes);
			};
		};

		struct json_thread
		{
			std::string                  Id;
			std::string                  SubjectType;
			std::string                  Path;
			std::optional<std::uint32_t> Line;
			std::optional<std::uint32_t> OriginalLine;
			std::string                  DiffSide;
			bool                         IsResolved         = false;
			bool                         IsOutdated         = false;
			bool                         ViewerCanResolve   = false;
			bool                         ViewerCanUnresolve = false;
			json_comment_connection      Comments;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &json_thread::Id,
					"subjectType", &json_thread::SubjectType,
					"path", &json_thread::Path,
					"line", &json_thread::Line,
					"originalLine", &json_thread::OriginalLine,
					"diffSide", &json_thread::DiffSide,
					"isResolved", &json_thread::IsResolved,
					"isOutdated", &json_thread::IsOutdated,
					"viewerCanResolve", &json_thread::ViewerCanResolve,
					"viewerCanUnresolve", &json_thread::ViewerCanUnresolve,
					"comments", &json_thread::Comments);
			};
		};

		struct json_thread_connection
		{
			std::vector<json_thread> Nodes;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"nodes", &json_thread_connection::Nodes);
			};
		};

		struct json_viewed_file
		{
			std::string Path;
			std::string ViewerViewedState;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"path", &json_viewed_file::Path,
					"viewerViewedState", &json_viewed_file::ViewerViewedState);
			};
		};

		struct json_file_connection
		{
			std::vector<json_viewed_file> Nodes;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"nodes", &json_file_connection::Nodes);
			};
		};

		struct json_graphql_pull
		{
			std::string            Id;
			std::string            HeadRefOid;
			json_file_connection   Files;
			json_thread_connection ReviewThreads;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &json_graphql_pull::Id,
					"headRefOid", &json_graphql_pull::HeadRefOid,
					"files", &json_graphql_pull::Files,
					"reviewThreads", &json_graphql_pull::ReviewThreads);
			};
		};

		struct json_graphql_repository
		{
			std::string       Id;
			json_graphql_pull PullRequest;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &json_graphql_repository::Id,
					"pullRequest", &json_graphql_repository::PullRequest);
			};
		};

		struct json_graphql_data
		{
			json_graphql_repository Repository;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"repository", &json_graphql_data::Repository);
			};
		};

		struct json_graphql_envelope
		{
			json_graphql_data Data;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"data", &json_graphql_envelope::Data);
			};
		};

		struct json_review_comment
		{
			std::uint64_t                  Id = 0U;
			std::optional<std::uint64_t>   InReplyToId;
			std::optional<json_rest_actor> User;
			std::string                    Body;
			std::string                    Path;
			// Missing coordinates normalize to invalid zero; explicit null is absent.
			std::optional<std::uint32_t> Line{ 0U };
			std::optional<std::uint32_t> OriginalLine{ 0U };
			// Position only determines nullness, not a value. Missing means current.
			std::optional<glz::raw_json> Position{ glz::raw_json{ "0" } };
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &json_review_comment::Id,
					"in_reply_to_id", &json_review_comment::InReplyToId,
					"user", &json_review_comment::User,
					"body", &json_review_comment::Body,
					"path", &json_review_comment::Path,
					"line", &json_review_comment::Line,
					"original_line", &json_review_comment::OriginalLine,
					"position", &json_review_comment::Position);
			};
		};

		struct json_issue_comment
		{
			std::string                    NodeId;
			std::optional<json_rest_actor> User;
			std::string                    Body;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"node_id", &json_issue_comment::NodeId,
					"user", &json_issue_comment::User,
					"body", &json_issue_comment::Body);
			};
		};

		struct json_check
		{
			std::string                NodeId;
			std::string                HeadSha;
			std::string                Name;
			std::string                Status;
			std::optional<std::string> Conclusion;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"node_id", &json_check::NodeId,
					"head_sha", &json_check::HeadSha,
					"name", &json_check::Name,
					"status", &json_check::Status,
					"conclusion", &json_check::Conclusion);
			};
		};

		struct json_checks
		{
			std::vector<json_check> CheckRuns;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"check_runs", &json_checks::CheckRuns);
			};
		};

		struct json_search_item
		{
			std::string   NodeId;
			std::uint32_t Number = 0U;
			std::string   Title;
			std::string   State;
			bool          Draft = false;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"node_id", &json_search_item::NodeId,
					"number", &json_search_item::Number,
					"title", &json_search_item::Title,
					"state", &json_search_item::State,
					"draft", &json_search_item::Draft);
			};
		};

		struct json_search
		{
			std::vector<json_search_item> Items;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"items", &json_search::Items);
			};
		};

		template <typename Record>
		[[nodiscard]] auto decode_record(Record& aRecord, const wire_response& aResponse) -> bool
		{
			return !glz::read<normalization_json_options{ }>(aRecord, aResponse.Body);
		}

		[[nodiscard]] auto optional_rest_actor(const std::optional<json_rest_actor>& aValue)
			-> std::optional<application::github_actor_record>
		{
			if (!aValue) {
				return std::nullopt;
			}
			return application::github_actor_record{ .Actor = application::github_actor_id{ aValue->NodeId }, .Login = aValue->Login };
		}
		[[nodiscard]] auto optional_graphql_actor(const std::optional<json_graphql_actor>& aValue)
			-> std::optional<application::github_actor_record>
		{
			if (!aValue) {
				return std::nullopt;
			}
			return application::github_actor_record{ .Actor = application::github_actor_id{ aValue->Id }, .Login = aValue->Login };
		}

		[[nodiscard]] auto full_name(
			const core::repository_locator& aRepository) -> std::string
		{
			return aRepository.Owner + "/" + aRepository.Name;
		}

		[[nodiscard]] auto pull_request_state(
			const std::string_view aValue,
			const bool             aMerged = false)
			-> application::github_pull_request_state
		{
			if (aMerged) {
				return application::github_pull_request_state::merged;
			}
			return aValue == "open"
				? application::github_pull_request_state::open
				: application::github_pull_request_state::closed;
		}

		[[nodiscard]] auto file_change(const std::string_view aValue)
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

		[[nodiscard]] auto viewed_state(const std::string_view aValue)
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

		[[nodiscard]] auto review_state(const std::string_view aValue)
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

		[[nodiscard]] auto review_thread_subject(const std::string_view aValue)
			-> application::github_review_thread_subject
		{
			return aValue == "FILE"
				? application::github_review_thread_subject::file
				: application::github_review_thread_subject::line;
		}

		[[nodiscard]] auto diff_side(const std::string_view aValue)
			-> std::optional<application::github_diff_side>
		{
			if (aValue == "LEFT") {
				return application::github_diff_side::left;
			}
			if (aValue == "RIGHT") {
				return application::github_diff_side::right;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto check_status(const std::string_view aValue)
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

		[[nodiscard]] auto check_conclusion(const std::optional<std::string>& aValue)
			-> application::github_check_conclusion
		{
			if (!aValue) {
				return application::github_check_conclusion::none;
			}
			const auto& conclusion = *aValue;
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
			case application::github_read_validation_error::relationship_file_previous_path:
			case application::github_read_validation_error::relationship_thread_comment_path:
			case application::github_read_validation_error::relationship_review_group_root:
			case application::github_read_validation_error::relationship_review_group_reply:
			case application::github_read_validation_error::relationship_actor_login:
			case application::github_read_validation_error::relationship_thread_reply:
			case application::github_read_validation_error::relationship_search_repository:
			case application::github_read_validation_error::relationship_search_target_identity:
			case application::github_read_validation_error::relationship_search_target_metadata:
			case application::github_read_validation_error::relationship_timeline_availability:
			case application::github_read_validation_error::relationship_timeline_event:
			case application::github_read_validation_error::relationship_review_comments_availability:
			case application::github_read_validation_error::relationship_review_threads_capability:
			case application::github_read_validation_error::relationship_file_viewed_capability:
			case application::github_read_validation_error::relationship_thread_permission_capability:
			case application::github_read_validation_error::relationship_search_availability:
				return contract_failure_code::unexpected_response_shape;
			}
			return contract_failure_code::unexpected_response_shape;
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
			case read_operation::list_pull_request_timeline:
			case read_operation::list_reviews:
			case read_operation::list_review_threads:
			case read_operation::list_issue_comments:
			case read_operation::list_review_comments:
			case read_operation::list_review_requests:
			case read_operation::get_merge_assessment:
			case read_operation::resolve_body_images:
			case read_operation::list_requested_reviewers:
				request.PullRequestNumber = aContext.PullRequestRequest.Number.value();
				break;
			case read_operation::get_viewer_identity:
			case read_operation::get_repository:
			case read_operation::list_commit_statuses:
			case read_operation::list_checks:
			case read_operation::search_pull_requests:
			case read_operation::list_branch_rules:
			case read_operation::get_branch_protection:
			case read_operation::search_personal_pull_requests:
			case read_operation::get_merge_base:
			case read_operation::get_team_membership:
				break;
			}
			if (aOperation == read_operation::list_checks || aOperation == read_operation::list_commit_statuses) {
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

	auto authenticated_read_admission() -> github_read_admission
	{
		return github_read_admission{
			.Operations = read_operation_catalog,
			.Shape      = {
				.ReviewThreads       = true,
				.ViewedFileState     = true,
				.ResolvePermission   = true,
				.UnresolvePermission = true,
			},
			.Viewer = std::nullopt,
		};
	}

	auto anonymous_read_admission(
		const application::github_viewer_record& aViewer)
		-> github_read_admission
	{
		return github_read_admission{
			.Operations = anonymous_read_operations,
			.Shape      = { },
			.Viewer     = aViewer,
		};
	}

	auto normalize_github_read(
		const github_read_normalization_context&  aContext,
		const std::span<const operation_response> aResponses,
		const github_read_admission&              aAdmission)
		-> normalized_read_result
	{
		try {
			if (!context_is_valid(aContext)) {
				return failed(contract_failure_code::identity_mismatch);
			}
			if (!admission_is_valid(aAdmission)) {
				return failed(contract_failure_code::unsupported_operation);
			}
			const auto admitted = [&aAdmission](const read_operation aOperation) {
				return std::ranges::find(aAdmission.Operations, aOperation)
					!= aAdmission.Operations.end();
			};
			if (aResponses.size() < aAdmission.Operations.size()) {
				return failed(contract_failure_code::recording_exhausted);
			}
			if (aResponses.size() > aAdmission.Operations.size()) {
				return failed(contract_failure_code::unexpected_operation);
			}

			struct response_records
			{
				json_viewer                      Viewer;
				json_repository                  Repository;
				json_pull_request                PullRequest;
				std::vector<json_file>           Files;
				std::vector<json_commit>         Commits;
				std::vector<json_review>         Reviews;
				json_graphql_envelope            Graphql;
				std::vector<json_review_comment> ReviewComments;
				std::vector<json_issue_comment>  IssueComments;
				json_checks                      Checks;
				json_search                      Search;
			} documents;
			std::array<bool, all_read_operations.size()> present{ };
			for (const auto& response : aResponses) {
				const auto index = operation_index(response.Operation);
				if (!index.has_value() || present[*index]
					|| !admitted(response.Operation)) {
					return failed(contract_failure_code::unexpected_operation);
				}
				const auto validation = validate_strict_fixture_response(
					response.Operation,
					response.Response);
				if (!validation.has_value()) {
					return std::unexpected{ validation.error() };
				}
				bool decoded = false;
				switch (response.Operation) {
				case read_operation::resolve_body_images:
				case read_operation::get_merge_assessment:
				case read_operation::list_review_requests:
				case read_operation::list_branch_rules:
				case read_operation::get_branch_protection:
				case read_operation::list_pull_request_timeline:
				case read_operation::search_personal_pull_requests:
				case read_operation::list_requested_reviewers:
				case read_operation::get_merge_base:
				case read_operation::get_team_membership:           return failed(contract_failure_code::unexpected_operation);
				case read_operation::get_viewer_identity:           decoded = decode_record(documents.Viewer, response.Response); break;
				case read_operation::get_repository:                decoded = decode_record(documents.Repository, response.Response); break;
				case read_operation::get_pull_request:              decoded = decode_record(documents.PullRequest, response.Response); break;
				case read_operation::list_pull_request_files:       decoded = decode_record(documents.Files, response.Response); break;
				case read_operation::list_pull_request_commits:     decoded = decode_record(documents.Commits, response.Response); break;
				case read_operation::list_reviews:                  decoded = decode_record(documents.Reviews, response.Response); break;
				case read_operation::list_review_threads:           decoded = decode_record(documents.Graphql, response.Response); break;
				case read_operation::list_review_comments:          decoded = decode_record(documents.ReviewComments, response.Response); break;
				case read_operation::list_issue_comments:           decoded = decode_record(documents.IssueComments, response.Response); break;
				case read_operation::list_commit_statuses:          return failed(contract_failure_code::unexpected_operation);
				case read_operation::list_checks:                   decoded = decode_record(documents.Checks, response.Response); break;
				case read_operation::search_pull_requests:          decoded = decode_record(documents.Search, response.Response); break;
				}
				if (!decoded) {
					return failed(contract_failure_code::unexpected_response_fields);
				}
				present[*index] = true;
			}
			for (const auto operation : aAdmission.Operations) {
				if (!present[*operation_index(operation)]) {
					return failed(contract_failure_code::recording_exhausted);
				}
			}

			const auto targetName = full_name(
				aContext.PullRequestRequest.Repository);

			// A read that does not reach `/user` has no viewer document, so
			// the viewer comes from the admission instead. Nothing is
			// invented for it: an admission that omits the operation without
			// naming a viewer is refused by `admission_is_valid` above.
			const auto viewerRecord = admitted(read_operation::get_viewer_identity)
				? application::github_viewer_record{
					  .Account = core::github_account_id{
						  documents.Viewer.Id },
					  .Node        = application::github_actor_id{ documents.Viewer.NodeId },
					  .Login       = documents.Viewer.Login,
					  .DisplayName = documents.Viewer.Name.value_or(""),
				  }
				: *aAdmission.Viewer;
			if (viewerRecord.Account != aContext.Account) {
				return failed(contract_failure_code::identity_mismatch);
			}

			normalized_github_read result;
			result.Session = {
				.Profile            = aContext.Profile,
				.Host               = aContext.Host,
				.Viewer             = viewerRecord,
				.CapabilityEvidence = {
					.Capabilities    = aContext.Capabilities,
					.Revision        = aContext.CapabilityRevision,
					.ReadShape       = aAdmission.Shape,
					.ContractVersion = 1U,
				},
				.RateLimits = aContext.RateLimits,
			};

			const auto& repository = documents.Repository;
			if (
				repository.Id != aContext.Repository.value()
				|| !ascii_equal_fold(
					repository.FullName,
					targetName)
				|| !ascii_equal_fold(
					repository.Name,
					aContext.PullRequestRequest.Repository.Name)
				|| !ascii_equal_fold(
					repository.Owner.Login,
					aContext.PullRequestRequest.Repository.Owner)) {
				return failed(contract_failure_code::identity_mismatch);
			}

			const auto& pullRequest    = documents.PullRequest;
			const auto& base           = pullRequest.Base;
			const auto& head           = pullRequest.Head;
			const auto& baseRepository = *base.Repo;
			if (
				pullRequest.Id
					!= aContext.PullRequest.value()
				|| pullRequest.Number
					!= aContext.PullRequestRequest.Number.value()
				|| baseRepository.Id
					!= aContext.Repository.value()
				|| !ascii_equal_fold(
					baseRepository.FullName,
					targetName)) {
				return failed(contract_failure_code::identity_mismatch);
			}
			const auto& headOid = head.Sha;

			result.PullRequest = {
				.Profile = aContext.Profile,
				.Host    = aContext.Host,
				.Account = aContext.Account,
				.Viewer  = {
					.Actor = viewerRecord.Node,
					.Login = viewerRecord.Login,
				},
				.Snapshot   = aContext.Snapshot,
				.Repository = {
					.Repository    = aContext.Repository,
					.Node          = application::github_node_id{ repository.NodeId },
					.Owner         = aContext.PullRequestRequest.Repository.Owner,
					.Name          = aContext.PullRequestRequest.Repository.Name,
					.DefaultBranch = repository.DefaultBranch,
					.Private       = repository.Private,
				},
				.PullRequest = {
					.PullRequest = aContext.PullRequest,
					.Repository  = aContext.Repository,
					.Number      = aContext.PullRequestRequest.Number,
					.Node        = application::github_node_id{ pullRequest.NodeId },
					.Title       = pullRequest.Title,
					.Body        = pullRequest.Body.value_or(""),
					.State       = pull_request_state(pullRequest.State, pullRequest.Merged),
					.Draft       = pullRequest.Draft,
					.Author      = optional_rest_actor(pullRequest.User),
					.BaseOid     = application::commit_oid{ base.Sha },
					.HeadOid     = application::commit_oid{ headOid },
					.BaseRef     = base.Ref,
					.HeadRef     = head.Ref,
				},
				.Files                      = { },
				.Commits                    = { },
				.Reviews                    = { },
				.ReviewThreadsAvailability  = admitted(read_operation::list_review_threads) ? application::github_collection_availability::available : application::github_collection_availability::unavailable,
				.ReviewThreads              = { },
				.ReviewCommentsAvailability = admitted(read_operation::list_review_comments) ? application::github_collection_availability::available : application::github_collection_availability::unavailable,
				.ReviewCommentGroups        = { },
				.IssueComments              = { },
				.Checks                     = { },
				.SearchResults              = { },
				.SearchAvailability         = admitted(read_operation::search_pull_requests) ? application::github_search_availability::available : application::github_search_availability::unavailable,
				.CapabilityEvidence         = {
					.Capabilities    = aContext.Capabilities,
					.Revision        = aContext.CapabilityRevision,
					.ReadShape       = aAdmission.Shape,
					.ContractVersion = 1U,
				},
				.RateLimits = aContext.RateLimits,
			};

			const auto& files = documents.Files;
			result.PullRequest.Files.reserve(
				static_cast<std::size_t>(files.size()));
			for (const auto& value : files) {
				const auto&                object = value;
				std::optional<std::string> previousPath;
				if (object.PreviousFilename.has_value()) {
					previousPath = *object.PreviousFilename;
				}
				result.PullRequest.Files.push_back(
					application::github_file_record{
						.Path         = object.Filename,
						.PreviousPath = std::move(previousPath),
						.Change       = file_change(object.Status),
						.Additions    = object.Additions,
						.Deletions    = object.Deletions,
						.ContentKind  = application::github_file_content_kind::unknown,
					});
			}

			const auto& commits = documents.Commits;
			result.PullRequest.Commits.reserve(
				static_cast<std::size_t>(commits.size()));
			for (const auto& value : commits) {
				const auto& object   = value;
				auto        headline = object.Commit.Message;
				if (const auto newline = headline.find('\n');
					newline != std::string::npos) {
					headline.resize(newline);
				}
				result.PullRequest.Commits.push_back(
					application::github_commit_record{
						.Oid = application::commit_oid{
							object.Sha },
						.Node     = application::github_node_id{ object.NodeId },
						.Author   = optional_rest_actor(object.Author),
						.Headline = std::move(headline),
					});
			}
			if (
				result.PullRequest.Commits.empty()
				|| result.PullRequest.Commits.back().Oid.value() != headOid) {
				return failed(contract_failure_code::identity_mismatch);
			}

			const auto& reviews = documents.Reviews;
			result.PullRequest.Reviews.reserve(
				static_cast<std::size_t>(reviews.size()));
			for (const auto& value : reviews) {
				const auto&                            object = value;
				std::optional<application::commit_oid> commit;
				if (object.CommitId.has_value()) {
					commit = application::commit_oid{
						*object.CommitId
					};
				}
				result.PullRequest.Reviews.push_back(
					application::github_review_record{
						.DatabaseId = object.Id,
						.Review     = application::github_review_id{
							object.NodeId },
						.Author = optional_rest_actor(object.User),
						.State  = review_state(object.State),
						.Commit = std::move(commit),
						.Body   = object.Body.value_or(""),
					});
			}

			// The GraphQL projection is the authenticated read's source of
			// viewed-file state and review threads. An admission that does
			// not reach it leaves both where they started: every file's
			// viewed state stays unavailable and the thread collection
			// stays unavailable and empty, which is what the snapshot's
			// all-false read shape requires of it.
			if (admitted(read_operation::list_review_threads)) {
				const auto& reviewThreadsDocument = documents.Graphql;
				const auto& graphqlRepository     = reviewThreadsDocument.Data.Repository;
				const auto& graphqlPullRequest    = graphqlRepository.PullRequest;
				if (
					graphqlRepository.Id
						!= repository.NodeId
					|| graphqlPullRequest.Id
						!= pullRequest.NodeId
					|| graphqlPullRequest.HeadRefOid != headOid) {
					return failed(contract_failure_code::identity_mismatch);
				}
				const auto& graphqlFiles = graphqlPullRequest.Files.Nodes;
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
					const auto& file  = fileValue;
					const auto  state = viewed_state(file.ViewerViewedState);
					if (!state.has_value()) {
						return failed(
							contract_failure_code::unexpected_response_shape);
					}
					const auto inserted = viewedStates.emplace(file.Path, *state)
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
				const auto& threads = reviewThreadsDocument.Data.Repository.PullRequest.ReviewThreads.Nodes;
				result.PullRequest.ReviewThreads.reserve(
					static_cast<std::size_t>(threads.size()));
				for (const auto& threadValue : threads) {
					const auto& thread = threadValue;
					const auto  side   = diff_side(thread.DiffSide);
					if (!side.has_value()) {
						return failed(contract_failure_code::unexpected_response_shape);
					}
					application::github_review_thread_record threadRecord{
						.Thread = application::github_thread_id{
							thread.Id },
						.Subject                   = review_thread_subject(thread.SubjectType),
						.Path                      = thread.Path,
						.Line                      = thread.SubjectType == "FILE" ? std::nullopt : thread.Line,
						.OriginalLine              = thread.SubjectType == "FILE" ? std::nullopt : thread.OriginalLine,
						.DiffSide                  = *side,
						.Resolved                  = thread.IsResolved,
						.Outdated                  = thread.IsOutdated,
						.ViewerResolvePermission   = thread.ViewerCanResolve ? application::github_permission_state::allowed : application::github_permission_state::denied,
						.ViewerUnresolvePermission = thread.ViewerCanUnresolve ? application::github_permission_state::allowed : application::github_permission_state::denied,
						.Comments                  = { },
					};
					const auto& comments = thread.Comments.Nodes;
					threadRecord.Comments.reserve(
						static_cast<std::size_t>(comments.size()));
					for (const auto& commentValue : comments) {
						const auto&                                   comment = commentValue;
						std::optional<application::github_comment_id> replyTo;
						if (comment.ReplyTo.has_value()) {
							replyTo = application::github_comment_id{
								comment.ReplyTo->Id
							};
						}
						threadRecord.Comments.push_back(
							application::github_review_comment_record{
								.Comment = application::github_comment_id{
									comment.Id },
								.ReplyTo      = std::move(replyTo),
								.Author       = optional_graphql_actor(comment.Author),
								.Body         = comment.BodyText,
								.Path         = comment.Path,
								.Line         = comment.Line,
								.OriginalLine = comment.OriginalLine,
								.Outdated     = comment.Outdated,
								.Deleted      = false,
							});
					}
					result.PullRequest.ReviewThreads.push_back(
						std::move(threadRecord));
				}
			}

			if (admitted(read_operation::list_review_comments)) {
				std::vector<application::github_review_comment_record> flat;
				const auto&                                            reviewComments = documents.ReviewComments;
				flat.reserve(static_cast<std::size_t>(reviewComments.size()));
				for (const auto& value : reviewComments) {
					const auto& object  = value;
					const auto  replyTo = object.InReplyToId;
					flat.push_back(
						application::github_review_comment_record{
							.Comment = application::github_comment_id{
								std::to_string(object.Id) },
							.ReplyTo      = replyTo.has_value() ? std::optional<application::github_comment_id>{ application::github_comment_id{ std::to_string(*replyTo) } } : std::nullopt,
							.Author       = optional_rest_actor(object.User),
							.Body         = object.Body,
							.Path         = object.Path,
							.Line         = object.Line,
							.OriginalLine = object.OriginalLine,
							.Outdated     = !object.Position.has_value(),
							.Deleted      = false,
						});
				}
				if (!group_review_comments(
						std::move(flat), result.PullRequest.ReviewCommentGroups)) {
					return failed(contract_failure_code::unexpected_response_shape);
				}
			}

			const auto& issueComments = documents.IssueComments;
			result.PullRequest.IssueComments.reserve(
				static_cast<std::size_t>(issueComments.size()));
			for (const auto& value : issueComments) {
				const auto& object = value;
				result.PullRequest.IssueComments.push_back(
					application::github_issue_comment_record{
						.Comment = application::github_comment_id{
							object.NodeId },
						.Author  = optional_rest_actor(object.User),
						.Body    = object.Body,
						.Deleted = false,
					});
			}

			const auto& checks = documents.Checks.CheckRuns;
			result.PullRequest.Checks.reserve(
				static_cast<std::size_t>(checks.size()));
			for (const auto& value : checks) {
				const auto& object = value;
				if (object.HeadSha != headOid) {
					return failed(contract_failure_code::identity_mismatch);
				}
				auto check       = application::github_check_record{ };
				check.Check      = application::github_check_id{ object.NodeId };
				check.Name       = object.Name;
				check.Status     = check_status(object.Status);
				check.Conclusion = check_conclusion(object.Conclusion);
				result.PullRequest.Checks.push_back(std::move(check));
			}

			const auto& searchResults = documents.Search.Items;
			result.PullRequest.SearchResults.reserve(
				static_cast<std::size_t>(searchResults.size()));
			for (const auto& value : searchResults) {
				const auto& object = value;
				result.PullRequest.SearchResults.push_back(
					application::github_search_result_record{
						.Repository      = aContext.Repository,
						.PullRequestNode = application::github_node_id{
							object.NodeId },
						.Number = core::pull_request_number{ object.Number },
						.Title  = object.Title,
						.State  = pull_request_state(object.State),
						.Draft  = object.Draft,
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
		const github_read_normalization_context& aContext,
		const github_read_admission&             aAdmission)
		-> normalized_read_result
	{
		try {
			if (!context_is_valid(aContext)) {
				return failed(contract_failure_code::identity_mismatch);
			}
			if (!admission_is_valid(aAdmission)) {
				return failed(contract_failure_code::unsupported_operation);
			}

			std::vector<operation_response> responses;
			responses.reserve(aAdmission.Operations.size());
			std::string headOid;
			for (const auto operation : aAdmission.Operations) {
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
					json_pull_request pull;
					if (!decode_record(pull, *response)) {
						return failed(contract_failure_code::unexpected_response_fields);
					}
					headOid = std::move(pull.Head.Sha);
				}
				responses.push_back(
					operation_response{
						.Operation = operation,
						.Response  = *response,
					});
			}
			return normalize_github_read(aContext, responses, aAdmission);
		} catch (const std::bad_alloc&) {
			return failed(contract_failure_code::response_limit);
		}
	}
}
