#include "bpr/core/home_discovery_plan.hpp"
#include "bpr/core/tour_agent_session.hpp"
#include "github_http_helpers.hpp"
#include "github_paged_read.hpp"
#include "github_paged_read_page_wire.hpp"
#include "github_paged_read_record_parsing.hpp"
#include "github_review_comment_grouping.hpp"

#include <QString>

#include <algorithm>
#include <cstddef>
#include <expected>
#include <functional>
#include <glaze/json.hpp>
#include <initializer_list>
#include <limits>
#include <map>
#include <new>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail {
	namespace {
		/// [[BPR-1900]]. The head lines of a file's patch hunks, as the
		/// record keeps them; absent with no patch or one it cannot read.
		[[nodiscard]] auto head_hunks(const std::optional<std::string>& aPatch) -> std::optional<std::vector<std::pair<std::uint32_t, std::uint32_t>>>
		{
			if (!aPatch) {
				return std::nullopt;
			}
			const auto hunks = core::parse_patch_head_hunks(*aPatch);
			if (!hunks) {
				return std::nullopt;
			}
			auto spans = std::vector<std::pair<std::uint32_t, std::uint32_t>>{ };
			spans.reserve(hunks->size());
			for (const auto& hunk : *hunks) {
				spans.emplace_back(hunk.First, hunk.Last);
			}
			return spans;
		}

		constexpr std::uint32_t maximum_graphql_sequence = 1'024U;

		[[nodiscard]] auto json_is_number(const page_json_value& aValue) noexcept -> bool
		{
			const auto tag = json_tag(aValue);
			return tag == '-' || (tag >= '0' && tag <= '9');
		}

		[[nodiscard]] auto file_change(const QString& aValue) noexcept
			-> application::github_file_change
		{
			if (aValue == QStringLiteral("added")) {
				return application::github_file_change::added;
			}
			if (aValue == QStringLiteral("modified")) {
				return application::github_file_change::modified;
			}
			if (aValue == QStringLiteral("removed")) {
				return application::github_file_change::removed;
			}
			if (aValue == QStringLiteral("renamed")) {
				return application::github_file_change::renamed;
			}
			if (aValue == QStringLiteral("copied")) {
				return application::github_file_change::copied;
			}
			if (aValue == QStringLiteral("unchanged")) {
				return application::github_file_change::unchanged;
			}
			return application::github_file_change::type_changed;
		}

		[[nodiscard]] auto viewed_state(const QString& aValue) noexcept
			-> std::optional<application::github_file_viewed_state>
		{
			if (aValue == QStringLiteral("DISMISSED")) {
				return application::github_file_viewed_state::dismissed;
			}
			if (aValue == QStringLiteral("UNVIEWED")) {
				return application::github_file_viewed_state::unviewed;
			}
			if (aValue == QStringLiteral("VIEWED")) {
				return application::github_file_viewed_state::viewed;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto diff_side(const page_json_value& aValue) noexcept
			-> std::optional<application::github_diff_side>
		{
			if (!json_is_string(aValue)) {
				return std::nullopt;
			}
			if (json_text(aValue) == QStringLiteral("LEFT")) {
				return application::github_diff_side::left;
			}
			if (json_text(aValue) == QStringLiteral("RIGHT")) {
				return application::github_diff_side::right;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto parse_graphql_comment(const page_json_value& aValue)
			-> std::expected<application::github_review_comment_record,
							 github_paged_read_failure>
		{
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue),
									  {
										  "author",
										  "bodyText",
										  "id",
										  "line",
										  "originalLine",
										  "outdated",
										  "path",
										  "replyTo",
									  })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto keys = std::vector<std::string_view>{ "author", "bodyText", "id", "line", "originalLine", "outdated", "path", "replyTo" };
			for (const auto key : { "body", "url", "updatedAt", "fullDatabaseId", "commit", "originalCommit", "startLine", "originalStartLine", "pullRequestReview" }) {
				if (!json_is_undefined(json_member(json_object(aValue), key))) {
					keys.emplace_back(key);
				}
			}
			if (!has_exact_keys(json_object(aValue), keys)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto observation = parse_comment_observation(aValue, true);
			if (!observation) {
				return std::unexpected(observation.error());
			}

			const auto comment      = json_object(aValue);
			const auto author       = optional_graphql_actor(json_member(comment, "author"));
			const auto line         = json_member(comment, "line");
			const auto originalLine = json_member(comment, "originalLine");
			const auto reply        = json_member(comment, "replyTo");
			if (!nonempty_string(json_member(comment, "id"))
				|| !json_is_string(json_member(comment, "bodyText"))
				|| !nonempty_string(json_member(comment, "path"))
				|| !json_is_bool(json_member(comment, "outdated")) || !author.Valid
				|| (!json_is_null(line) && !valid_line(line))
				|| (!json_is_null(originalLine) && !valid_line(originalLine))
				|| (!json_is_null(reply)
					&& (!json_is_object(reply) || !has_exact_keys(json_object(reply), { "id" })
						|| !nonempty_string(json_member(json_object(reply), "id"))))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			std::optional<application::github_comment_id> replyTo;
			if (json_is_object(reply)) {
				replyTo = application::github_comment_id{ text(json_member(json_object(reply), "id")) };
			}
			return application::github_review_comment_record{
				.Comment      = application::github_comment_id{ text(json_member(comment, "id")) },
				.ReplyTo      = std::move(replyTo),
				.Author       = author.Actor,
				.Body         = text(json_member(comment, "bodyText")),
				.Path         = text(json_member(comment, "path")),
				.Line         = optional_line(line),
				.OriginalLine = optional_line(originalLine),
				.Outdated     = json_bool(json_member(comment, "outdated")),
				.Deleted      = false,
				.Observation  = *observation,
			};
		}

		struct parsed_graphql_thread
		{
			application::github_review_thread_record Record;
			std::uint64_t                            TotalComments = 0U;
			std::optional<std::string>               NextCommentsCursor;
		};

		[[nodiscard]] auto
		parse_graphql_thread(const page_json_value&                aValue,
							 const application::github_read_shape& aShape,
							 const github_paged_read_limits&       aLimits)
			-> std::expected<parsed_graphql_thread, github_paged_read_failure>
		{
			if (!json_is_object(aValue)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			std::vector<std::string_view> keys{
				"comments",
				"diffSide",
				"id",
				"isOutdated",
				"isResolved",
				"line",
				"originalLine",
				"path",
				"subjectType",
			};
			if (aShape.ResolvePermission) {
				keys.emplace_back("viewerCanResolve");
			}
			if (aShape.UnresolvePermission) {
				keys.emplace_back("viewerCanUnresolve");
			}
			if (aShape.ReplyPermission) {
				keys.emplace_back("viewerCanReply");
			}
			const auto thread = json_object(aValue);
			if (!has_exact_keys(thread, keys)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto comments = parse_graphql_connection(json_member(thread, "comments"), aLimits);
			if (!comments.has_value()) {
				return std::unexpected{ comments.error() };
			}
			if (comments->TotalCount > aLimits.Records.MaximumCommentsPerThread
				|| comments->TotalCount > aLimits.Records.MaximumTotalComments) {
				return failed(application::github_provider_failure::response_limit);
			}
			const auto line         = json_member(thread, "line");
			const auto originalLine = json_member(thread, "originalLine");
			const auto side         = diff_side(json_member(thread, "diffSide"));
			const auto fileSubject  = json_text(json_member(thread, "subjectType")) == QStringLiteral("FILE");
			const auto lineSubject  = json_text(json_member(thread, "subjectType")) == QStringLiteral("LINE");
			if (!nonempty_string(json_member(thread, "id"))
				|| !json_is_bool(json_member(thread, "isOutdated"))
				|| !json_is_bool(json_member(thread, "isResolved"))
				|| !nonempty_string(json_member(thread, "path"))
				|| (!fileSubject && !lineSubject)
				|| !side.has_value()
				|| (!json_is_null(line) && !valid_line(line))
				|| (!json_is_null(originalLine) && !valid_line(originalLine))
				|| (fileSubject
					&& !(json_is_null(line) && json_is_null(originalLine))
					&& !(optional_line(line) == 1U && optional_line(originalLine) == 1U
						 && side == application::github_diff_side::right))
				|| (lineSubject && !json_bool(json_member(thread, "isOutdated"))
					&& json_is_null(line) && !json_is_null(originalLine))
				|| (aShape.ResolvePermission
					&& !json_is_bool(json_member(thread, "viewerCanResolve")))
				|| (aShape.UnresolvePermission
					&& !json_is_bool(json_member(thread, "viewerCanUnresolve")))
				|| (aShape.ReplyPermission
					&& !json_is_bool(json_member(thread, "viewerCanReply")))) {
				return failed(application::github_provider_failure::malformed_response);
			}

			application::github_review_thread_record record{
				.Thread                    = application::github_thread_id{ text(json_member(thread, "id")) },
				.Subject                   = fileSubject
					? application::github_review_thread_subject::file
					: application::github_review_thread_subject::line,
				.Path                      = text(json_member(thread, "path")),
				.Line                      = fileSubject ? std::nullopt : optional_line(line),
				.OriginalLine              = fileSubject ? std::nullopt : optional_line(originalLine),
				.DiffSide                  = *side,
				.Resolved                  = json_bool(json_member(thread, "isResolved")),
				.Outdated                  = json_bool(json_member(thread, "isOutdated")),
				.ViewerResolvePermission   = aShape.ResolvePermission
					? (json_bool(json_member(thread, "viewerCanResolve"))
						   ? application::github_permission_state::allowed
						   : application::github_permission_state::denied)
					: application::github_permission_state::unavailable,
				.ViewerUnresolvePermission = aShape.UnresolvePermission
					? (json_bool(json_member(thread, "viewerCanUnresolve"))
						   ? application::github_permission_state::allowed
						   : application::github_permission_state::denied)
					: application::github_permission_state::unavailable,
				// [[BPR-1746]]. GitHub's own word on whether this viewer
				// may reply here, which already weighs the lock.
				.ViewerReplyPermission = aShape.ReplyPermission
					? (json_bool(json_member(thread, "viewerCanReply"))
						   ? application::github_permission_state::allowed
						   : application::github_permission_state::denied)
					: application::github_permission_state::unavailable,
				.Comments              = { },
			};
			record.Comments.reserve(static_cast<std::size_t>(comments->TotalCount));
			for (const auto& commentValue : comments->Nodes) {
				auto comment = parse_graphql_comment(commentValue);
				if (!comment.has_value()) {
					return std::unexpected{ comment.error() };
				}
				if (comment->Path != record.Path
					|| (record.Subject == application::github_review_thread_subject::file
						&& (comment->Line.has_value()
							|| comment->OriginalLine.has_value()))) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				record.Comments.push_back(std::move(*comment));
			}
			return parsed_graphql_thread{
				.Record             = std::move(record),
				.TotalComments      = comments->TotalCount,
				.NextCommentsCursor = std::move(comments->NextCursor),
			};
		}

		struct thread_pagination_state
		{
			std::uint64_t                   TotalComments = 0U;
			std::optional<std::string>      NextCursor;
			std::unordered_set<std::string> SeenCursors;
		};

		[[nodiscard]] auto
		parse_repository(const parsed_page&                        aPage,
						 const core::pull_request_request_context& aContext)
			-> std::expected<application::github_repository_record,
							 github_paged_read_failure>
		{
			if (!singleton_complete(aPage) || !json_is_object(aPage.Document)
				|| !has_required_keys(json_object(aPage.Document),
									  {
										  "default_branch",
										  "full_name",
										  "id",
										  "name",
										  "node_id",
										  "owner",
										  "private",
									  })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aPage.Document);
			const auto owner  = json_member(object, "owner");
			if (!exact_integer(json_member(object, "id"), true)
				|| !nonempty_string(json_member(object, "name"))
				|| !nonempty_string(json_member(object, "full_name"))
				|| !nonempty_string(json_member(object, "node_id"))
				|| !json_is_bool(json_member(object, "private"))
				|| !nonempty_string(json_member(object, "default_branch")) || !json_is_object(owner)
				|| !has_required_keys(json_object(owner), { "login" })
				|| !nonempty_string(json_member(json_object(owner), "login"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto rawOwner    = text(json_member(json_object(owner), "login"));
			const auto rawName     = text(json_member(object, "name"));
			const auto rawFullName = text(json_member(object, "full_name"));
			if (!ascii_equal_fold(rawOwner, aContext.Repository.Owner)
				|| !ascii_equal_fold(rawName, aContext.Repository.Name)
				|| !ascii_equal_fold(rawFullName, rawOwner + "/" + rawName)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto id = core::repository_id{ integer(json_member(object, "id")) };
			if (aContext.ExpectedRepository.has_value()
				&& *aContext.ExpectedRepository != id) {
				return failed(application::github_provider_failure::malformed_response);
			}
			// [[BPR-1879]]. GitHub's `size`, in kibibytes, when it is a plain
			// count; anything else leaves it unknown and refuses nothing.
			auto size = std::optional<std::uint64_t>{ };
			if (exact_integer(json_member(object, "size"), false)) {
				size = integer(json_member(object, "size"));
			}
			return application::github_repository_record{
				.Repository    = id,
				.Node          = application::github_node_id{ text(json_member(object, "node_id")) },
				.Owner         = rawOwner,
				.Name          = rawName,
				.DefaultBranch = text(json_member(object, "default_branch")),
				.Private       = json_bool(json_member(object, "private")),
				.SizeKilobytes = size,
			};
		}

		[[nodiscard]] auto parse_file(const page_json_value& aValue)
			-> std::expected<application::github_file_record, github_paged_read_failure>
		{
			if (!json_is_object(aValue)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object      = json_object(aValue);
			const auto hasPrevious = object.contains("previous_filename");
			if (!has_required_keys(object,
								   { "additions", "deletions", "filename", "status" })
				|| !nonempty_string(json_member(object, "filename"))
				|| !one_of(json_member(object, "status"),
						   {
							   "added",
							   "modified",
							   "removed",
							   "renamed",
							   "copied",
							   "changed",
							   "unchanged",
						   })
				|| !exact_integer(json_member(object, "additions"))
				|| !exact_integer(json_member(object, "deletions"))
				|| (hasPrevious
					&& (!nonempty_string(json_member(object, "previous_filename"))
						|| (json_text(json_member(object, "status"))
								!= QStringLiteral("renamed")
							&& json_text(json_member(object, "status"))
								!= QStringLiteral("copied"))))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			std::optional<std::string> previousPath;
			if (hasPrevious) {
				previousPath = text(json_member(object, "previous_filename"));
			}
			return application::github_file_record{
				.Path         = text(json_member(object, "filename")),
				.PreviousPath = std::move(previousPath),
				.Change       = file_change(json_text(json_member(object, "status"))),
				.Additions    = integer(json_member(object, "additions")),
				.Deletions    = integer(json_member(object, "deletions")),
				.ContentKind  = application::github_file_content_kind::unknown,
				.ViewedState  = application::github_file_viewed_state::unavailable,
				// [[BPR-1900]]. Where the change is, for the tour's stops.
				.HeadHunks = object.contains("patch") && json_is_string(json_member(object, "patch")) ? head_hunks(std::optional<std::string>{ text(json_member(object, "patch")) }) : std::nullopt,
			};
		}

		[[nodiscard]] auto parse_commit(const page_json_value& aValue)
			-> std::expected<application::github_commit_record,
							 github_paged_read_failure>
		{
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue),
									  { "author", "commit", "node_id", "sha" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aValue);
			const auto author = optional_rest_actor(json_member(object, "author"));
			const auto commit = json_member(object, "commit");
			if (!hexadecimal_oid(json_member(object, "sha"))
				|| !nonempty_string(json_member(object, "node_id")) || !author.Valid
				|| !json_is_object(commit)
				|| !has_required_keys(json_object(commit), { "message" })
				|| !json_is_string(json_member(json_object(commit), "message"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto headline = text(json_member(json_object(commit), "message"));
			if (const auto newline = headline.find('\n'); newline != std::string::npos) {
				headline.resize(newline);
			}
			// RFC-BPR-0089. `parents` is read when the record carries it, and
			// then exactly: an array of objects each naming a full `sha`. A
			// record without the member leaves the parents unknown.
			auto       parents       = std::optional<std::vector<application::commit_oid>>{ };
			const auto parentsMember = json_member(object, "parents");
			if (!json_is_undefined(parentsMember)) {
				if (!json_is_array(parentsMember)) {
					return failed(application::github_provider_failure::malformed_response);
				}
				const auto values = json_array(parentsMember);
				if (values.size() > application::maximum_github_commit_parents) {
					return failed(application::github_provider_failure::malformed_response);
				}
				parents.emplace();
				parents->reserve(values.size());
				for (const auto& value : values) {
					if (!json_is_object(value) || !hexadecimal_oid(json_member(json_object(value), "sha"))) {
						return failed(application::github_provider_failure::malformed_response);
					}
					parents->emplace_back(text(json_member(json_object(value), "sha")));
				}
			}
			return application::github_commit_record{
				.Oid      = application::commit_oid{ text(json_member(object, "sha")) },
				.Node     = application::github_node_id{ text(json_member(object, "node_id")) },
				.Author   = author.Actor,
				.Headline = std::move(headline),
				.Parents  = std::move(parents),
			};
		}

		[[nodiscard]] auto parse_issue_comment(const page_json_value& aValue)
			-> std::expected<application::github_issue_comment_record,
							 github_paged_read_failure>
		{
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue), { "body", "node_id", "user" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aValue);
			const auto author = optional_rest_actor(json_member(object, "user"));
			if (!nonempty_string(json_member(object, "node_id"))
				|| !json_is_string(json_member(object, "body")) || !author.Valid) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto observation = parse_comment_observation(aValue, false);
			if (!observation) {
				return std::unexpected(observation.error());
			}

			return application::github_issue_comment_record{
				.Comment     = application::github_comment_id{ text(json_member(object, "node_id")) },
				.Author      = author.Actor,
				.Body        = text(json_member(object, "body")),
				.Deleted     = false,
				.Observation = *observation,
			};
		}

		/// Parses one entry of the REST inline review-comment listing.
		///
		/// The identity used here is the REST numeric id rendered as text
		/// rather than the node id, because `in_reply_to_id` names the numeric
		/// id and reply linkage is only checkable when both ends live in one
		/// identifier space. `position` is required and carries the anchor
		/// fact: the service returns null once the comment's line has left the
		/// diff, which is what outdated means on this surface. Nothing here
		/// reads resolution, viewer, viewed, or actionability, because the
		/// response carries none of them and inferring one is prohibited.
		[[nodiscard]] auto parse_review_comment(const page_json_value& aValue)
			-> std::expected<application::github_review_comment_record,
							 github_paged_read_failure>
		{
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue),
									  {
										  "body",
										  "id",
										  "path",
										  "position",
										  "user",
									  })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aValue);
			const auto author = optional_rest_actor(json_member(object, "user"));
			const auto reply  = json_member(object, "in_reply_to_id");
			if (!exact_integer(json_member(object, "id"), true) || !author.Valid
				|| !json_is_string(json_member(object, "body"))
				|| !nonempty_string(json_member(object, "path"))
				|| (!json_is_null(json_member(object, "position"))
					&& !exact_integer(json_member(object, "position"), true))
				|| (!json_is_undefined(json_member(object, "line"))
					&& !json_is_null(json_member(object, "line"))
					&& !exact_integer(json_member(object, "line"), true))
				|| (!json_is_undefined(json_member(object, "original_line"))
					&& !json_is_null(json_member(object, "original_line"))
					&& !exact_integer(json_member(object, "original_line"), true))
				|| (!json_is_undefined(reply) && !json_is_null(reply)
					&& !exact_integer(reply, true))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto replyTo = std::optional<application::github_comment_id>{ };
			if (json_is_number(reply)) {
				replyTo = application::github_comment_id{
					std::to_string(integer(reply))
				};
			}
			const auto observation = parse_comment_observation(aValue, false);
			if (!observation) {
				return std::unexpected(observation.error());
			}

			return application::github_review_comment_record{
				.Comment = application::github_comment_id{
					std::to_string(integer(json_member(object, "id"))) },
				.ReplyTo      = std::move(replyTo),
				.Author       = author.Actor,
				.Body         = text(json_member(object, "body")),
				.Path         = text(json_member(object, "path")),
				.Line         = optional_line(json_member(object, "line")),
				.OriginalLine = optional_line(json_member(object, "original_line")),
				.Outdated     = json_is_null(json_member(object, "position")),
				.Deleted      = false,
				.Observation  = *observation,
			};
		}

		[[nodiscard]] auto
		parse_search_result(const page_json_value&                       aValue,
							const application::github_repository_record& aRepository,
							const validated_github_host_profile&         aProfile)
			-> std::expected<application::github_search_result_record,
							 github_paged_read_failure>
		{
			if (!json_is_object(aValue)
				|| !has_required_keys(json_object(aValue),
									  { "draft",
										"html_url",
										"node_id",
										"number",
										"pull_request",
										"repository_url",
										"state",
										"title" })) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object      = json_object(aValue);
			const auto number      = uint32(json_member(object, "number"));
			const auto pullRequest = json_member(object, "pull_request");
			if (!number.has_value() || *number == 0U
				|| *number > core::maximum_pull_request_number
				|| !nonempty_string(json_member(object, "node_id"))
				|| !nonempty_string(json_member(object, "title"))
				|| !nonempty_string(json_member(object, "repository_url"))
				|| !nonempty_string(json_member(object, "html_url")) || !json_is_object(pullRequest)
				|| !has_required_keys(json_object(pullRequest), { "url" })
				|| !nonempty_string(json_member(json_object(pullRequest), "url"))
				|| !one_of(json_member(object, "state"), { "open" })
				|| !json_is_bool(json_member(object, "draft"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto expectedRepositoryUrl = std::string{ aProfile.rest_root() }
				+ "/repos/" + aRepository.Owner + "/"
				+ aRepository.Name;
			const auto expectedHtmlUrl = "https://" + std::string{ aProfile.hostname() } + "/" + aRepository.Owner
				+ "/" + aRepository.Name + "/pull/" + std::to_string(*number);
			const auto expectedPullRequestUrl = expectedRepositoryUrl + "/pulls/" + std::to_string(*number);
			if (text(json_member(object, "repository_url")) != expectedRepositoryUrl
				|| text(json_member(object, "html_url")) != expectedHtmlUrl
				|| text(json_member(json_object(pullRequest), "url"))
					!= expectedPullRequestUrl) {
				return failed(application::github_provider_failure::malformed_response);
			}
			return application::github_search_result_record{
				.Repository      = aRepository.Repository,
				.PullRequestNode = application::github_node_id{ text(json_member(object, "node_id")) },
				.Number          = core::pull_request_number{ *number },
				.Title           = text(json_member(object, "title")),
				.State           = application::github_pull_request_state::open,
				.Draft           = json_bool(json_member(object, "draft")),
			};
		}

		[[nodiscard]] auto
		perform_graphql_projection(production_read_execution&                 aExecution,
								   const server_contract                      aServer,
								   const core::pull_request_request_context&  aContext,
								   const application::github_read_shape&      aShape,
								   application::github_pull_request_snapshot& aSnapshot,
								   const github_paged_read_limits&            aLimits,
								   const bool                                 aPrioritizeConversation,
								   const auto&                                aBeforeViewedFiles)
			-> std::expected<void, github_paged_read_failure>
		{
			if ((aShape.ResolvePermission || aShape.UnresolvePermission || aShape.ReplyPermission || aShape.ConversationLock)
				&& !aShape.ReviewThreads) {
				return failed(application::github_provider_failure::malformed_response);
			}
			if (!aShape.ViewedFileState && !aShape.ReviewThreads) {
				aSnapshot.ReviewThreadsAvailability = application::github_collection_availability::unavailable;
				return { };
			}

			std::uint32_t sequence        = 1U;
			auto          initialSemantic = semantic_request_for(
				aServer, aContext, read_operation::list_review_threads, sequence);
			initialSemantic.GraphqlPage = github_graphql_page_request{
				.Kind        = github_graphql_page_kind::initial,
				.Shape       = aShape,
				.AfterCursor = { },
				.ThreadId    = { },
			};
			auto initial = aExecution.fetch(initialSemantic);
			if (!initial.has_value()) {
				return std::unexpected{ initial.error() };
			}
			page_json_object initialData;
			page_json_object initialRepository;
			page_json_object initialPullRequest;
			if (const auto identity = graphql_repository_and_pull_request(*initial,
																		  aSnapshot.Repository,
																		  aSnapshot.PullRequest,
																		  &initialData,
																		  &initialRepository,
																		  &initialPullRequest);
				!identity.has_value()) {
				return std::unexpected{ identity.error() };
			}
			std::vector<std::string_view> initialPullKeys{ "headRefOid", "id" };
			if (aShape.ConversationLock) {
				initialPullKeys.emplace_back("locked");
			}
			if (aShape.ViewedFileState) {
				initialPullKeys.emplace_back("files");
			}
			if (aShape.ReviewThreads) {
				initialPullKeys.emplace_back("reviewThreads");
			}
			if (!has_exact_keys(initialPullRequest, initialPullKeys)
				|| (aShape.ConversationLock && !json_is_bool(json_member(initialPullRequest, "locked")))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			// [[BPR-1746]]. The pull request's conversation lock.
			if (aShape.ConversationLock) {
				aSnapshot.ConversationLock = json_bool(json_member(initialPullRequest, "locked"))
					? application::github_conversation_lock::locked
					: application::github_conversation_lock::unlocked;
			}

			// [[BPR-1885]]. Viewed-state pages do not decide conversation
			// content. Keep their validation intact and run them independently.
			const auto readViewedFiles = [&]() -> std::expected<void, github_paged_read_failure> {
				if (!aShape.ViewedFileState) {
					return { };
				}
				auto files = parse_graphql_connection(
					json_member(initialPullRequest, "files"), aLimits);
				if (!files.has_value()) {
					return std::unexpected{ files.error() };
				}
				if (files->TotalCount > aLimits.Records.MaximumFiles
					|| files->TotalCount != aSnapshot.Files.size()) {
					return failed(
						files->TotalCount > aLimits.Records.MaximumFiles
							? application::github_provider_failure::response_limit
							: application::github_provider_failure::malformed_response);
				}
				std::unordered_map<std::string, application::github_file_viewed_state>
					viewed;
				viewed.reserve(static_cast<std::size_t>(files->TotalCount));
				const auto appendFileNodes = [&viewed](const page_json_array& aNodes)
					-> std::expected<void, github_paged_read_failure> {
					for (const auto& value : aNodes) {
						if (!json_is_object(value)
							|| !has_exact_keys(json_object(value),
											   { "path", "viewerViewedState" })
							|| !nonempty_string(json_member(json_object(value), "path"))) {
							return failed(application::github_provider_failure::
											  malformed_response);
						}
						const auto object = json_object(value);
						const auto state  = viewed_state(json_text(json_member(object, "viewerViewedState")));
						if (!state.has_value()
							|| !viewed.emplace(text(json_member(object, "path")), *state)
									.second) {
							return failed(application::github_provider_failure::
											  malformed_response);
						}
					}
					return { };
				};
				if (const auto append = appendFileNodes(files->Nodes);
					!append.has_value()) {
					return std::unexpected{ append.error() };
				}
				std::unordered_set<std::string> seenCursors;
				auto                            nextCursor = std::move(files->NextCursor);
				while (nextCursor.has_value()) {
					if (!seenCursors.insert(*nextCursor).second) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					if (sequence == maximum_graphql_sequence) {
						return failed(
							application::github_provider_failure::response_limit);
					}
					++sequence;
					auto semantic = semantic_request_for(
						aServer, aContext, read_operation::list_review_threads, sequence);
					semantic.GraphqlPage = github_graphql_page_request{
						.Kind        = github_graphql_page_kind::files,
						.Shape       = aShape,
						.AfterCursor = *nextCursor,
						.ThreadId    = { },
					};
					auto page = aExecution.fetch(semantic);
					if (!page.has_value()) {
						return std::unexpected{ page.error() };
					}
					page_json_object data;
					page_json_object repository;
					page_json_object pullRequest;
					if (const auto identity = graphql_repository_and_pull_request(*page,
																				  aSnapshot.Repository,
																				  aSnapshot.PullRequest,
																				  &data,
																				  &repository,
																				  &pullRequest);
						!identity.has_value()) {
						return std::unexpected{ identity.error() };
					}
					if (!has_exact_keys(
							pullRequest, { "files", "headRefOid", "id" })) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					auto connection = parse_graphql_connection(json_member(pullRequest, "files"), aLimits);
					if (!connection.has_value()) {
						return std::unexpected{ connection.error() };
					}
					if (connection->TotalCount != files->TotalCount) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					if (const auto append = appendFileNodes(connection->Nodes);
						!append.has_value()) {
						return std::unexpected{ append.error() };
					}
					if (viewed.size() > files->TotalCount) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					nextCursor = std::move(connection->NextCursor);
				}
				if (viewed.size() != files->TotalCount) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				for (auto& file : aSnapshot.Files) {
					const auto found = viewed.find(file.Path);
					if (found == viewed.end()) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					file.ViewedState = found->second;
					viewed.erase(found);
				}
				if (!viewed.empty()) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				return { };
			};
			if (!aPrioritizeConversation) {
				if (const auto files = readViewedFiles(); !files) {
					return std::unexpected{ files.error() };
				}
			}

			if (!aShape.ReviewThreads) {
				aSnapshot.ReviewThreadsAvailability = application::github_collection_availability::unavailable;
				return { };
			}

			auto threads = parse_graphql_connection(
				json_member(initialPullRequest, "reviewThreads"), aLimits);
			if (!threads.has_value()) {
				return std::unexpected{ threads.error() };
			}
			if (threads->TotalCount > aLimits.Records.MaximumReviewThreads) {
				return failed(application::github_provider_failure::response_limit);
			}
			aSnapshot.ReviewThreads.reserve(
				static_cast<std::size_t>(threads->TotalCount));
			std::vector<thread_pagination_state> pagination;
			pagination.reserve(static_cast<std::size_t>(threads->TotalCount));
			std::unordered_set<std::string> threadIds;
			threadIds.reserve(static_cast<std::size_t>(threads->TotalCount));
			std::size_t totalComments     = 0U;
			const auto  appendThreadNodes = [&](const page_json_array& aNodes)
				-> std::expected<void, github_paged_read_failure> {
				for (const auto& value : aNodes) {
					auto parsed = parse_graphql_thread(value, aShape, aLimits);
					if (!parsed.has_value()) {
						return std::unexpected{ parsed.error() };
					}
					const auto id = std::string{ parsed->Record.Thread.value() };
					if (!threadIds.insert(id).second) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					if (parsed->TotalComments > aLimits.Records.MaximumTotalComments
						|| totalComments
							> aLimits.Records.MaximumTotalComments
								- static_cast<std::size_t>(parsed->TotalComments)) {
						return failed(
							application::github_provider_failure::response_limit);
					}
					totalComments += static_cast<std::size_t>(parsed->TotalComments);
					thread_pagination_state state{
						.TotalComments = parsed->TotalComments,
						.NextCursor    = std::move(parsed->NextCommentsCursor),
						.SeenCursors   = { },
					};
					aSnapshot.ReviewThreads.push_back(std::move(parsed->Record));
					pagination.push_back(std::move(state));
				}
				return { };
			};
			if (const auto append = appendThreadNodes(threads->Nodes);
				!append.has_value()) {
				return std::unexpected{ append.error() };
			}

			std::unordered_set<std::string> threadCursors;
			auto                            nextThreadCursor = std::move(threads->NextCursor);
			while (nextThreadCursor.has_value()) {
				if (!threadCursors.insert(*nextThreadCursor).second) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				if (sequence == maximum_graphql_sequence) {
					return failed(application::github_provider_failure::response_limit);
				}
				++sequence;
				auto semantic = semantic_request_for(
					aServer, aContext, read_operation::list_review_threads, sequence);
				semantic.GraphqlPage = github_graphql_page_request{
					.Kind        = github_graphql_page_kind::threads,
					.Shape       = aShape,
					.AfterCursor = *nextThreadCursor,
					.ThreadId    = { },
				};
				auto page = aExecution.fetch(semantic);
				if (!page.has_value()) {
					return std::unexpected{ page.error() };
				}
				page_json_object data;
				page_json_object repository;
				page_json_object pullRequest;
				if (const auto identity = graphql_repository_and_pull_request(*page,
																			  aSnapshot.Repository,
																			  aSnapshot.PullRequest,
																			  &data,
																			  &repository,
																			  &pullRequest);
					!identity.has_value()) {
					return std::unexpected{ identity.error() };
				}
				if (!has_exact_keys(
						pullRequest, { "headRefOid", "id", "reviewThreads" })) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				auto connection = parse_graphql_connection(
					json_member(pullRequest, "reviewThreads"), aLimits);
				if (!connection.has_value()) {
					return std::unexpected{ connection.error() };
				}
				if (connection->TotalCount != threads->TotalCount) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				if (const auto append = appendThreadNodes(connection->Nodes);
					!append.has_value()) {
					return std::unexpected{ append.error() };
				}
				if (aSnapshot.ReviewThreads.size() > threads->TotalCount) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				nextThreadCursor = std::move(connection->NextCursor);
			}
			if (aSnapshot.ReviewThreads.size() != threads->TotalCount) {
				return failed(application::github_provider_failure::malformed_response);
			}

			for (std::size_t index = 0U; index < pagination.size(); ++index) {
				auto& state  = pagination[index];
				auto& thread = aSnapshot.ReviewThreads[index];
				while (state.NextCursor.has_value()) {
					if (!state.SeenCursors.insert(*state.NextCursor).second) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					if (sequence == maximum_graphql_sequence) {
						return failed(
							application::github_provider_failure::response_limit);
					}
					++sequence;
					auto semantic = semantic_request_for(
						aServer, aContext, read_operation::list_review_threads, sequence);
					semantic.GraphqlPage = github_graphql_page_request{
						.Kind        = github_graphql_page_kind::comments,
						.Shape       = aShape,
						.AfterCursor = *state.NextCursor,
						.ThreadId    = std::string{ thread.Thread.value() },
					};
					auto page = aExecution.fetch(semantic);
					if (!page.has_value()) {
						return std::unexpected{ page.error() };
					}
					if (!singleton_complete(*page) || !json_is_object(page->Document)
						|| !graphql_success_envelope(json_object(page->Document))) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					const auto dataValue = json_member(json_object(page->Document), "data");
					if (!json_is_object(dataValue)
						|| !has_exact_keys(json_object(dataValue),
										   { "node", "repository" })) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					const auto data            = json_object(dataValue);
					const auto repositoryValue = json_member(data, "repository");
					if (!json_is_object(repositoryValue)
						|| !has_exact_keys(json_object(repositoryValue),
										   { "id", "pullRequest" })
						|| text(json_member(json_object(repositoryValue), "id"))
							!= aSnapshot.Repository.Node.value()) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					const auto pullRequestValue = json_member(json_object(repositoryValue), "pullRequest");
					if (!json_is_object(pullRequestValue)
						|| !has_exact_keys(
							json_object(pullRequestValue), { "headRefOid", "id" })
						|| text(json_member(json_object(pullRequestValue), "id"))
							!= aSnapshot.PullRequest.Node.value()
						|| text(json_member(json_object(pullRequestValue), "headRefOid"))
							!= aSnapshot.PullRequest.HeadOid.value()) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					const auto nodeValue = json_member(data, "node");
					if (!json_is_object(nodeValue)
						|| !has_exact_keys(json_object(nodeValue), { "comments", "id" })
						|| text(json_member(json_object(nodeValue), "id"))
							!= thread.Thread.value()) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					auto connection = parse_graphql_connection(
						json_member(json_object(nodeValue), "comments"), aLimits);
					if (!connection.has_value()) {
						return std::unexpected{ connection.error() };
					}
					if (connection->TotalCount != state.TotalComments) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					for (const auto& value : connection->Nodes) {
						auto comment = parse_graphql_comment(value);
						if (!comment.has_value()) {
							return std::unexpected{ comment.error() };
						}
						if (comment->Path != thread.Path
							|| (thread.Subject
									== application::github_review_thread_subject::file
								&& (comment->Line.has_value()
									|| comment->OriginalLine.has_value()))) {
							return failed(application::github_provider_failure::
											  malformed_response);
						}
						thread.Comments.push_back(std::move(*comment));
					}
					if (thread.Comments.size() > state.TotalComments) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					state.NextCursor = std::move(connection->NextCursor);
				}
				if (thread.Comments.size() != state.TotalComments) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
			}
			aSnapshot.ReviewThreadsAvailability = application::github_collection_availability::available;
			if (aPrioritizeConversation) {
				if (const auto files = aBeforeViewedFiles(); !files) {
					return files;
				}
				return readViewedFiles();
			}
			return { };
		}

		[[nodiscard]] auto read_rest_pending_requests(production_read_execution& aExecution, server_contract aServer, const core::pull_request_request_context& aContext, application::github_pull_request_snapshot& aSnapshot) -> std::expected<void, github_paged_read_failure>
		{
			const auto page = aExecution.fetch(semantic_request_for(aServer, aContext, read_operation::list_requested_reviewers));
			if (!page) {
				return std::unexpected{ page.error() };
			}
			if (!singleton_complete(*page) || !json_is_object(page->Document)) {
				return failed(application::github_provider_failure::response_limit);
			}
			const auto object = json_object(page->Document);
			if (!json_is_array(json_member(object, "users")) || !json_is_array(json_member(object, "teams"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto records = std::vector<application::github_review_request_record>{ };
			auto seen    = std::unordered_set<std::string>{ };
			for (const auto& key : { "users", "teams" }) {
				for (const auto& actor : json_array(json_member(object, key))) {
					if (!json_is_object(actor)) {
						return failed(application::github_provider_failure::malformed_response);
					}
					const auto fields = json_object(actor);
					auto       result = application::github_review_request_record{ };
					result.Actor      = text(json_member(fields, "node_id"));
					result.Login      = text(json_member(fields, std::string_view{ key } == "users" ? "login" : "slug"));
					if (result.Actor.empty() || result.Login.empty() || !seen.insert(result.Actor).second) {
						return failed(application::github_provider_failure::malformed_response);
					}
					result.Request = "rest:" + result.Actor;
					result.Kind    = std::string_view{ key } == "users" ? application::github_requested_actor_kind::user : application::github_requested_actor_kind::team;
					if (result.Kind == application::github_requested_actor_kind::team) {
						result.Organization = aSnapshot.Repository.Owner;
						result.Name         = text(json_member(fields, "name"));
					}
					records.push_back(std::move(result));
				}
			}
			aSnapshot.ReviewRequests              = std::move(records);
			aSnapshot.ReviewRequestActorsObserved = true;
			// REST establishes actor identities, never code-owner origin. Keep the
			// richer connection's actual availability beside this usable roster.
			return { };
		}

		[[nodiscard]] auto parse_required_checks(const page_json_value& aValue, const std::string_view aAppKey) -> std::expected<std::vector<application::github_required_check>, github_paged_read_failure>
		{
			if (!json_is_array(aValue)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto checks = std::vector<application::github_required_check>{ };
			for (const auto& value : json_array(aValue)) {
				if (!json_is_object(value) || !nonempty_string(json_member(json_object(value), "context"))) {
					return failed(application::github_provider_failure::malformed_response);
				}
				auto check     = application::github_required_check{ };
				check.Context  = text(json_member(json_object(value), "context"));
				const auto app = json_member(json_object(value), aAppKey);
				if (!json_is_null(app) && !json_is_undefined(app)) {
					// Classic -1 permits any app, as does a null integration.
					std::int64_t appNumber = 0;
					if (glz::read<page_json_options{ }>(appNumber, app.Json) || appNumber != -1) {
						if (!exact_integer(app, true)) {
							return failed(application::github_provider_failure::malformed_response);
						}
						check.AppId = integer(app);
					}
				}
				checks.push_back(std::move(check));
			}
			return checks;
		}

		[[nodiscard]] auto parse_branch_rule(const page_json_value& aValue) -> std::expected<application::github_branch_requirement, github_paged_read_failure>
		{
			if (!json_is_object(aValue)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto object = json_object(aValue);
			if (!nonempty_string(json_member(object, "type")) || !one_of(json_member(object, "ruleset_source_type"), { "Repository", "Organization", "Enterprise" }) || !nonempty_string(json_member(object, "ruleset_source")) || !exact_integer(json_member(object, "ruleset_id"), true)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto result       = application::github_branch_requirement{ };
			result.Type       = text(json_member(object, "type"));
			result.Source     = text(json_member(object, "ruleset_source"));
			result.SourceKind = text(json_member(object, "ruleset_source_type"));
			result.RulesetId  = integer(json_member(object, "ruleset_id"));
			const auto params = json_object(json_member(object, "parameters"));
			if (result.Type == "pull_request") {
				const auto minimum = json_member(params, "required_approving_review_count");
				if (!exact_integer(minimum) || integer(minimum) > 100U || !json_is_bool(json_member(params, "require_code_owner_review")) || !json_is_bool(json_member(params, "require_last_push_approval")) || !json_is_bool(json_member(params, "dismiss_stale_reviews_on_push"))) {
					return failed(application::github_provider_failure::malformed_response);
				}
				result.ApprovalMinimum     = static_cast<std::uint32_t>(integer(minimum));
				result.CodeOwners          = json_bool(json_member(params, "require_code_owner_review"));
				result.LastPushApproval    = json_bool(json_member(params, "require_last_push_approval"));
				result.DismissStaleReviews = json_bool(json_member(params, "dismiss_stale_reviews_on_push"));
				if (json_is_bool(json_member(params, "required_review_thread_resolution")) && json_bool(json_member(params, "required_review_thread_resolution"))) {
					result.Type = "pull_request_and_conversation_resolution";
				}
				const auto teams = json_member(params, "required_reviewers");
				if (!json_is_undefined(teams)) {
					if (!json_is_array(teams)) {
						return failed(application::github_provider_failure::malformed_response);
					}
					for (const auto& team : json_array(teams)) {
						const auto fields = json_object(team), reviewer = json_object(json_member(fields, "reviewer"));
						const auto count = json_member(fields, "minimum_approvals");
						if (text(json_member(reviewer, "type")) != "Team" || !exact_integer(json_member(reviewer, "id"), true) || !exact_integer(count) || integer(count) > 100U) {
							return failed(application::github_provider_failure::malformed_response);
						}
						result.RequiredTeams.emplace_back(integer(json_member(reviewer, "id")), static_cast<std::uint32_t>(integer(count)));
					}
				}
			} else if (result.Type == "required_status_checks") {
				const auto checks = parse_required_checks(json_member(params, "required_status_checks"), "integration_id");
				if (!checks) {
					return std::unexpected{ checks.error() };
				}
				if (!json_is_bool(json_member(params, "strict_required_status_checks_policy"))) {
					return failed(application::github_provider_failure::malformed_response);
				}
				result.Checks       = *checks;
				result.StrictChecks = json_bool(json_member(params, "strict_required_status_checks_policy"));
			}
			return result;
		}

		[[nodiscard]] auto read_classic_protection(production_read_execution& aExecution, server_contract aServer, const core::pull_request_request_context& aContext, application::github_pull_request_snapshot& aSnapshot) -> std::expected<void, github_paged_read_failure>
		{
			auto semantic      = semantic_request_for(aServer, aContext, read_operation::get_branch_protection);
			semantic.BranchRef = aSnapshot.PullRequest.BaseRef;
			const auto page    = aExecution.fetch(semantic);
			if (!page) {
				return std::unexpected{ page.error() };
			}
			if (!singleton_complete(*page) || !json_is_object(page->Document)) {
				return failed(application::github_provider_failure::malformed_response);
			}
			const auto fields = json_object(page->Document);
			if (!nonempty_string(json_member(fields, "url"))) {
				return failed(application::github_provider_failure::malformed_response);
			}
			auto       records = std::vector<application::github_branch_requirement>{ };
			const auto reviews = json_member(fields, "required_pull_request_reviews");
			if (!json_is_null(reviews) && !json_is_undefined(reviews)) {
				const auto params  = json_object(reviews);
				const auto minimum = json_member(params, "required_approving_review_count");
				if (!json_is_object(reviews) || !exact_integer(minimum) || integer(minimum) > 100U || !json_is_bool(json_member(params, "require_code_owner_reviews")) || !json_is_bool(json_member(params, "dismiss_stale_reviews"))) {
					return failed(application::github_provider_failure::malformed_response);
				}
				auto rule                = application::github_branch_requirement{ };
				rule.Type                = "pull_request";
				rule.ApprovalMinimum     = static_cast<std::uint32_t>(integer(minimum));
				rule.CodeOwners          = json_bool(json_member(params, "require_code_owner_reviews"));
				rule.DismissStaleReviews = json_bool(json_member(params, "dismiss_stale_reviews"));
				rule.LastPushApproval    = json_bool(json_member(params, "require_last_push_approval"));
				records.push_back(std::move(rule));
			}
			const auto status = json_member(fields, "required_status_checks");
			if (!json_is_null(status) && !json_is_undefined(status)) {
				const auto params = json_object(status);
				const auto checks = parse_required_checks(json_member(params, "checks"), "app_id");
				if (!checks || !json_is_bool(json_member(params, "strict"))) {
					return failed(application::github_provider_failure::malformed_response);
				}
				auto rule         = application::github_branch_requirement{ };
				rule.Type         = "required_status_checks";
				rule.Checks       = *checks;
				rule.StrictChecks = json_bool(json_member(params, "strict"));
				records.push_back(std::move(rule));
			}
			for (const auto& type : { "required_conversation_resolution", "required_signatures", "required_linear_history" }) {
				const auto value = json_member(fields, type);
				if (json_is_object(value) && json_bool(json_member(json_object(value), "enabled"))) {
					auto rule = application::github_branch_requirement{ };
					rule.Type = type;
					records.push_back(std::move(rule));
				}
			}
			for (auto& record : records) {
				record.Source     = aSnapshot.PullRequest.BaseRef;
				record.SourceKind = "Classic branch protection";
			}
			aSnapshot.BranchPolicy.Protection             = std::move(records);
			aSnapshot.BranchPolicy.ProtectionAvailability = observation::available;
			return { };
		}

		[[nodiscard]] auto read_merge_requirements(production_read_execution& aExecution, const validated_github_host_profile& aProfile, server_contract aServer, const core::pull_request_request_context& aContext, application::github_pull_request_snapshot& aSnapshot, const github_paged_read_limits& aLimits, const std::function<void()>& aAssessmentReady, const std::function<void()>& aPeopleReady) -> std::expected<void, github_paged_read_failure>
		{
			const auto assessment = read_merge_assessment(aExecution, aServer, aContext, aSnapshot);
			if (!assessment) {
				if (fatal_optional_read_failure(assessment.error())) {
					return std::unexpected{ assessment.error() };
				}
				aSnapshot.Merge.Availability = observation_failure(assessment.error());
			}
			aAssessmentReady();
			const auto requests = read_pending_requests(aExecution, aServer, aContext, aSnapshot, aLimits);
			if (!requests) {
				if (fatal_optional_read_failure(requests.error())) {
					return std::unexpected{ requests.error() };
				}
				aSnapshot.ReviewRequestsAvailability = observation_failure(requests.error());
				const auto fallback                  = read_rest_pending_requests(aExecution, aServer, aContext, aSnapshot);
				if (!fallback && fatal_optional_read_failure(fallback.error())) {
					return std::unexpected{ fallback.error() };
				}
			}
			const auto validCollection = [&](const application::github_pull_request_snapshot& aRecords) {
				auto budget = application::detail::github_text_budget{ aLimits.Records };
				return application::detail::validate_merge_observations(aRecords, budget, aLimits.Records) == application::github_read_validation_error::none;
			};
			auto records                                = application::github_pull_request_snapshot{ };
			records.ReviewRequestsAvailability          = observation::available;
			records.BranchPolicy.RulesAvailability      = observation::available;
			records.BranchPolicy.ProtectionAvailability = observation::available;

			records.Viewer             = aSnapshot.Viewer;
			records.PullRequest.Author = aSnapshot.PullRequest.Author;
			// Only authors participate in review-request consistency; do not
			// copy review bodies while the remaining policy reads are pending.
			records.Reviews.reserve(aSnapshot.Reviews.size());
			for (const auto& review : aSnapshot.Reviews) {
				auto author   = application::github_review_record{ };
				author.Author = review.Author;
				records.Reviews.push_back(std::move(author));
			}
			records.ReviewRequests = aSnapshot.ReviewRequests;
			if (!validCollection(records)) {
				aSnapshot.ReviewRequests.clear();
				aSnapshot.ReviewRequestsAvailability  = observation::incomplete;
				aSnapshot.ReviewRequestActorsObserved = false;
			}
			aPeopleReady();
			auto semantic      = semantic_request_for(aServer, aContext, read_operation::list_branch_rules);
			semantic.BranchRef = aSnapshot.PullRequest.BaseRef;
			auto       rules   = std::vector<application::github_branch_requirement>{ };
			const auto read    = read_rest_array_collection(aExecution, aProfile, semantic, rules, 1000U, aLimits.MaximumPageRecords, parse_branch_rule);
			if (read) {
				aSnapshot.BranchPolicy.Rules             = std::move(rules);
				aSnapshot.BranchPolicy.RulesAvailability = observation::available;
			} else {
				if (fatal_optional_read_failure(read.error())) {
					return std::unexpected{ read.error() };
				}
				aSnapshot.BranchPolicy.RulesAvailability = observation_failure(read.error());
			}
			const auto protection = read_classic_protection(aExecution, aServer, aContext, aSnapshot);
			if (!protection) {
				if (fatal_optional_read_failure(protection.error())) {
					return std::unexpected{ protection.error() };
				}
				aSnapshot.BranchPolicy.ProtectionAvailability = observation_failure(protection.error());
			}
			records.ReviewRequests.clear();
			records.BranchPolicy.Rules = aSnapshot.BranchPolicy.Rules;
			if (!validCollection(records)) {
				aSnapshot.BranchPolicy.Rules.clear();
				aSnapshot.BranchPolicy.RulesAvailability = observation::incomplete;
			}
			records.BranchPolicy.Rules.clear();
			records.BranchPolicy.Protection = aSnapshot.BranchPolicy.Protection;
			if (!validCollection(records)) {
				aSnapshot.BranchPolicy.Protection.clear();
				aSnapshot.BranchPolicy.ProtectionAvailability = observation::incomplete;
			}
			return { };
		}

		[[nodiscard]] auto read_search_results(
			production_read_execution&                             aExecution,
			const validated_github_host_profile&                   aProfile,
			semantic_read_request                                  aSemantic,
			const application::github_repository_record&           aRepository,
			std::vector<application::github_search_result_record>& aOutput,
			const github_paged_read_limits&                        aLimits)
			-> std::expected<application::github_search_availability, github_paged_read_failure>
		{
			std::optional<std::uint64_t> total;
			while (true) {
				auto page = aExecution.fetch(aSemantic);
				if (!page.has_value()) {
					return std::unexpected{ page.error() };
				}
				if (!json_is_object(page->Document)
					|| !has_required_keys(
						json_object(page->Document),
						{ "incomplete_results", "items", "total_count" })) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto object = json_object(page->Document);
				if (!exact_integer(json_member(object, "total_count"))
					|| !json_is_array(json_member(object, "items"))
					|| !json_is_bool(json_member(object, "incomplete_results"))
					|| json_bool(json_member(object, "incomplete_results"))) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto observedTotal = integer(json_member(object, "total_count"));
				if (total.has_value() && *total != observedTotal) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto resultLimited = observedTotal > aLimits.Records.MaximumSearchResults;
				total                    = observedTotal;
				const auto records       = json_array(json_member(object, "items"));
				const auto count         = static_cast<std::size_t>(records.size());
				if (count > aLimits.MaximumPageRecords
					|| count > aLimits.Records.MaximumSearchResults
					|| aOutput.size() > aLimits.Records.MaximumSearchResults - count) {
					return failed(application::github_provider_failure::response_limit);
				}
				aOutput.reserve(aOutput.size() + count);
				for (const auto& value : records) {
					auto result = parse_search_result(value, aRepository, aProfile);
					if (!result.has_value()) {
						return std::unexpected{ result.error() };
					}
					aOutput.push_back(std::move(*result));
				}
				if (aOutput.size() > observedTotal) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto boundRepository = aExecution.bound_repository();
				if (!boundRepository.has_value()
					|| *boundRepository != aRepository.Repository) {
					return failed(
						application::github_provider_failure::malformed_response);
				}
				const auto hasNext = rest_has_next_page(aProfile, *boundRepository, *page, count);
				if (!hasNext.has_value()) {
					return std::unexpected{ hasNext.error() };
				}
				if (!*hasNext) {
					if (aOutput.size() != observedTotal) {
						return failed(
							application::github_provider_failure::malformed_response);
					}
					return application::github_search_availability::available;
				}
				if (resultLimited) {
					// Keep this bounded page until the existing snapshot validator
					// has checked its fields and cross-collection identities.
					return application::github_search_availability::result_limit;
				}
				if (aSemantic.Page == std::numeric_limits<std::uint32_t>::max()) {
					return failed(application::github_provider_failure::response_limit);
				}
				++aSemantic.Page;
			}
		}

		[[nodiscard]] auto validation_failure(
			const application::github_read_validation_error aError) noexcept
			-> application::github_provider_failure
		{
			switch (aError) {
			case application::github_read_validation_error::text_too_long:
			case application::github_read_validation_error::collection_too_large:
			case application::github_read_validation_error::allocation_limit:
			case application::github_read_validation_error::total_text_too_large:
				return application::github_provider_failure::response_limit;
			case application::github_read_validation_error::none:
			case application::github_read_validation_error::invalid_context:
			case application::github_read_validation_error::invalid_limits:
			case application::github_read_validation_error::missing_identity:
			case application::github_read_validation_error::identity_mismatch:
			case application::github_read_validation_error::missing_required_text:
			case application::github_read_validation_error::invalid_text:
			case application::github_read_validation_error::invalid_repository:
			case application::github_read_validation_error::invalid_pull_request_number:
			case application::github_read_validation_error::invalid_commit_oid:
			case application::github_read_validation_error::invalid_line:
			case application::github_read_validation_error::invalid_enum:
			case application::github_read_validation_error::invalid_capability_revision:
			case application::github_read_validation_error::invalid_rate_limit:
			case application::github_read_validation_error::duplicate_identity:
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
				return application::github_provider_failure::malformed_response;
			}
			return application::github_provider_failure::malformed_response;
		}
	}

	auto perform_paged_github_read(
		github_wire_transport_port&                              aTransport,
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		const core::provider_snapshot_id                         aSnapshotId,
		const std::stop_token                                    aStop,
		const github_paged_read_limits&                          aLimits,
		const github_provider_read_authority                     aReadAuthority,
		github_paged_response_observer*                          aObserver) noexcept -> github_paged_read_result
	{
		auto observedRateLimits = application::github_rate_limit_state{ };
		try {
			if (!provider_request_valid(
					aProfile, aRequest, aSnapshotId, aLimits, aReadAuthority)) {
				return failed(
					application::github_provider_failure::malformed_response);
			}
			if (aStop.stop_requested()) {
				return cancelled();
			}
			const auto server = server_contract_for(aProfile);
			if (!server.has_value()) {
				return failed(
					application::github_provider_failure::malformed_response);
			}

			production_read_execution execution{ aTransport,
												 aProfile,
												 aRequest.Context,
												 aLimits,
												 aStop,
												 observedRateLimits, aRequest.AuthenticationSession, aObserver };

			auto repositoryPage = execution.fetch(semantic_request_for(
				*server, aRequest.Context, read_operation::get_repository));
			if (!repositoryPage.has_value()) {
				return std::unexpected{ repositoryPage.error() };
			}
			auto repository = parse_repository(*repositoryPage, aRequest.Context);
			if (!repository.has_value()) {
				return std::unexpected{ execution.enrich(repository.error()) };
			}
			if (!execution.bind_repository(repository->Repository)) {
				return execution.terminal_failure(
					application::github_provider_failure::malformed_response);
			}

			auto pullRequestPage = execution.fetch(semantic_request_for(
				*server, aRequest.Context, read_operation::get_pull_request));
			if (!pullRequestPage.has_value()) {
				return std::unexpected{ pullRequestPage.error() };
			}
			auto pullRequest = parse_pull_request(
				*pullRequestPage, aRequest.Context, *repository, aLimits.Records);
			if (!pullRequest.has_value()) {
				return std::unexpected{ execution.enrich(pullRequest.error()) };
			}

			const auto expectedFiles   = pullRequest->ChangedFiles;
			const auto expectedCommits = std::min(pullRequest->Commits, application::maximum_github_pull_request_endpoint_commits);
			// An observation is independently bounded and validated. It grants no
			// snapshot authority; the same complete read and final audit still run.
			auto overviewBudget = application::detail::github_text_budget{ aLimits.Records };
			if (aObserver != nullptr && !aStop.stop_requested()
				&& application::detail::validate_repository(*repository, overviewBudget, aLimits.Records) == application::github_read_validation_error::none
				&& application::detail::validate_pull_request(pullRequest->Record, overviewBudget, aLimits.Records) == application::github_read_validation_error::none) {
				aObserver->overview_observed({ aRequest.Context, *repository, pullRequest->Record, expectedFiles, std::nullopt, std::nullopt });
			}
			application::github_pull_request_snapshot result{
				.Profile = aRequest.ProfileAuthority.Profile,
				.Host    = aRequest.ProfileAuthority.Host,
				.Account = aRequest.Viewer.Account,
				.Viewer  = application::github_actor_record{
					.Actor = aRequest.Viewer.Node,
					.Login = aRequest.Viewer.Login,
				},
				.Snapshot                   = aSnapshotId,
				.Repository                 = std::move(*repository),
				.PullRequest                = std::move(pullRequest->Record),
				.Files                      = { },
				.Commits                    = { },
				.Reviews                    = { },
				.ReviewThreadsAvailability  = application::github_collection_availability::unavailable,
				.ReviewThreads              = { },
				.ReviewCommentsAvailability = application::github_collection_availability::unavailable,
				.ReviewCommentGroups        = { },
				.IssueComments              = { },
				.Checks                     = { },
				.SearchResults              = { },
				.CapabilityEvidence         = application::github_capability_evidence{
					.Capabilities    = aRequest.CapabilityEvidence.Capabilities,
					.Revision        = aRequest.CapabilityEvidence.Revision,
					.ReadShape       = aRequest.CapabilityEvidence.ReadShape,
					.ContractVersion = 1U,
				},
				.RateLimits = { },
			};

			// [[BPR-1890]]. A refresh can use its conversation without waiting
			// for the independent file catalogue. Other reads keep their order.
			const auto prioritizeConversation = aObserver != nullptr
				&& aReadAuthority == github_provider_read_authority::authenticated
				&& aRequest.Context.ExpectedPullRequest.has_value()
				&& aRequest.CapabilityEvidence.ReadShape.ReviewThreads;
			const auto readFiles = [&]() -> std::expected<void, github_paged_read_failure> {
				if (const auto files = read_rest_array_collection(
						execution,
						aProfile,
						semantic_request_for(*server,
											 aRequest.Context,
											 read_operation::list_pull_request_files),
						result.Files,
						aLimits.Records.MaximumFiles,
						aLimits.MaximumPageRecords,
						[](const page_json_value& aValue) { return parse_file(aValue); });
					!files.has_value()) {
					return std::unexpected{ execution.enrich(files.error()) };
				}
				if (result.Files.size() != expectedFiles) {
					return execution.terminal_failure(
						application::github_provider_failure::malformed_response);
				}
				// [[BPR-1842]]. The file list is the first part a reviewer reads, so
				// it is observed as soon as it is complete, again without authority.
				if (aObserver != nullptr && !aStop.stop_requested()) {
					aObserver->overview_observed({ aRequest.Context, result.Repository, result.PullRequest, expectedFiles, result.Files, std::nullopt });
				}
				return { };
			};
			if (!prioritizeConversation) {
				if (const auto files = readFiles(); !files) {
					return std::unexpected{ files.error() };
				}
			}

			const auto readCommits = [&]() -> std::expected<void, github_paged_read_failure> {
				if (const auto commits = read_rest_array_collection(
						execution,
						aProfile,
						semantic_request_for(*server,
											 aRequest.Context,
											 read_operation::list_pull_request_commits),
						result.Commits,
						aLimits.Records.MaximumCommits,
						aLimits.MaximumPageRecords,
						[](const page_json_value& aValue) { return parse_commit(aValue); });
					!commits.has_value()) {
					return std::unexpected{ execution.enrich(commits.error()) };
				}
				if (result.Commits.size() != expectedCommits || result.Commits.empty()
					|| result.Commits.back().Oid != result.PullRequest.HeadOid) {
					return execution.terminal_failure(
						application::github_provider_failure::malformed_response);
				}
				return { };
			};
			if (!prioritizeConversation) {
				if (const auto commits = readCommits(); !commits) {
					return std::unexpected{ commits.error() };
				}
			}

			if (const auto reviews = read_rest_array_collection(
					execution,
					aProfile,
					semantic_request_for(
						*server, aRequest.Context, read_operation::list_reviews),
					result.Reviews,
					aLimits.Records.MaximumReviews,
					aLimits.MaximumPageRecords,
					[](const page_json_value& aValue) { return parse_review(aValue); });
				!reviews.has_value()) {
				return std::unexpected{ execution.enrich(reviews.error()) };
			}

			const auto readDiscussion = [&]() -> std::expected<void, github_paged_read_failure> {
				if (const auto comments = read_rest_array_collection(
						execution, aProfile,
						semantic_request_for(*server, aRequest.Context, read_operation::list_issue_comments),
						result.IssueComments, aLimits.Records.MaximumIssueComments,
						aLimits.MaximumPageRecords,
						[](const page_json_value& aValue) { return parse_issue_comment(aValue); });
					!comments.has_value()) {
					return std::unexpected{ execution.enrich(comments.error()) };
				}
				// [[BPR-1893]]. PR-wide comments are independently readable too;
				// their observation neither repeats nor delays the inline threads.
				if (prioritizeConversation && !aStop.stop_requested()) {
					aObserver->overview_observed({ .Context       = aRequest.Context,
												   .Repository    = result.Repository,
												   .PullRequest   = result.PullRequest,
												   .ReportedFiles = expectedFiles,
												   .Files         = std::nullopt,
												   .Conversation  = std::nullopt,
												   .Discussion    = application::github_pull_request_overview_observation::discussion{ .IssueComments = result.IssueComments } });
				}
				return { };
			};

			// [[BPR-1890]]. Publish the complete refresh conversation before
			// reading the catalogue. Viewed-state validation then has those
			// files, under the same execution and budget. No new authority.
			const auto beforeViewedFiles = [&]() -> std::expected<void, github_paged_read_failure> {
				if (aObserver != nullptr && !aStop.stop_requested() && aRequest.Context.ExpectedPullRequest.has_value()
					&& result.ReviewThreadsAvailability == application::github_collection_availability::available) {
					aObserver->overview_observed({ .Context       = aRequest.Context,
												   .Repository    = result.Repository,
												   .PullRequest   = result.PullRequest,
												   .ReportedFiles = expectedFiles,
												   .Files         = std::nullopt,
												   .Conversation  = application::github_pull_request_overview_observation::conversation{
													   .Reviews                   = result.Reviews,
													   .ReviewThreadsAvailability = result.ReviewThreadsAvailability,
													   .ReviewThreads             = result.ReviewThreads,
													   .ConversationLock          = result.ConversationLock } });
				}
				if (const auto discussion = readDiscussion(); !discussion) {
					return discussion;
				}
				return readFiles();
			};
			if (const auto graphql = perform_graphql_projection(execution,
																*server,
																aRequest.Context,
																aRequest.CapabilityEvidence.ReadShape,
																result,
																aLimits, prioritizeConversation, beforeViewedFiles);
				!graphql.has_value()) {
				return std::unexpected{ execution.enrich(graphql.error()) };
			}

			if (prioritizeConversation) {
				if (const auto commits = readCommits(); !commits) {
					return std::unexpected{ commits.error() };
				}
			}

			// The inline review conversation is read over REST only under the
			// signed-out authority. An authenticated read already has the
			// GraphQL threads, which carry resolution the REST listing cannot;
			// issuing both would put the same conversation in two places and
			// would change the recorded authenticated wire shape. The
			// availability field stays `unavailable` on that path, which is
			// the truthful answer for a collection that was never asked for.
			if (aReadAuthority
				== github_provider_read_authority::anonymous_public) {
				std::vector<application::github_review_comment_record> flat;
				if (const auto reviewComments = read_rest_array_collection(
						execution,
						aProfile,
						semantic_request_for(*server,
											 aRequest.Context,
											 read_operation::list_review_comments),
						flat,
						aLimits.Records.MaximumTotalComments,
						aLimits.MaximumPageRecords,
						[](const page_json_value& aValue) {
							return parse_review_comment(aValue);
						});
					!reviewComments.has_value()) {
					return std::unexpected{ execution.enrich(reviewComments.error()) };
				}
				if (!group_review_comments(std::move(flat),
										   result.ReviewCommentGroups)) {
					return execution.terminal_failure(
						application::github_provider_failure::malformed_response);
				}
				result.ReviewCommentsAvailability = application::github_collection_availability::available;
			}

			if (!prioritizeConversation) {
				if (const auto discussion = readDiscussion(); !discussion) {
					return std::unexpected{ discussion.error() };
				}
			}

			// Finish mandatory content before optional results can spend the
			// remaining page/byte budget. Exhaustion then qualifies only details.
			auto searchSemantic = semantic_request_for(
				*server, aRequest.Context, read_operation::search_pull_requests);
			searchSemantic.SearchTerms = "is:open";
			const auto search          = read_search_results(execution,
															 aProfile,
															 std::move(searchSemantic),
															 result.Repository,
															 result.SearchResults,
															 aLimits);
			if (!search.has_value()) {
				return std::unexpected{ execution.enrich(search.error()) };
			}
			auto checksSemantic = semantic_request_for(
				*server, aRequest.Context, read_operation::list_checks);
			checksSemantic.HeadOid = std::string{ result.PullRequest.HeadOid.value() };
			if (const auto checks = read_checks(execution,
												aProfile,
												std::move(checksSemantic),
												result.PullRequest.HeadOid.value(),
												result.Checks,
												aLimits);
				!checks.has_value()) {
				if (fatal_optional_read_failure(checks.error())) {
					return std::unexpected{ checks.error() };
				}
				result.Checks.clear();
				result.ChecksAvailability = application::github_collection_availability::unavailable;
			}

			auto statusesSemantic    = semantic_request_for(*server, aRequest.Context, read_operation::list_commit_statuses);
			statusesSemantic.HeadOid = std::string{ result.PullRequest.HeadOid.value() };
			auto       statuses      = std::vector<application::github_check_record>{ };
			const auto statusRead    = read_rest_array_collection(execution, aProfile, std::move(statusesSemantic), statuses,
																  aLimits.Records.MaximumChecks, aLimits.MaximumPageRecords, [&](const page_json_value& aValue) { return parse_commit_status(aValue, result.PullRequest.HeadOid.value()); });
			if (statusRead) {
				// The statuses endpoint is newest first. One context has one current
				// value; old attempts never enter current totals, including across pages.
				auto contexts = std::unordered_set<std::string>{ };
				for (auto& status : statuses) {
					if (contexts.insert(status.Name).second) {
						result.Checks.push_back(std::move(status));
					}
				}
				result.StatusesAvailability = application::github_collection_availability::available;
			} else if (fatal_optional_read_failure(statusRead.error())) {
				return std::unexpected{ statusRead.error() };
			}

			// [[BPR-1895]]. Completed results can be read before independent
			// merge rules and reviewer details. This carries no review authority.
			if (aObserver != nullptr && !aStop.stop_requested() && !aRequest.Context.ExpectedPullRequest
				&& aReadAuthority == github_provider_read_authority::authenticated) {
				aObserver->overview_observed({ .Context = aRequest.Context, .Repository = result.Repository, .PullRequest = result.PullRequest, .ReportedFiles = expectedFiles, .Files = std::nullopt, .Conversation = std::nullopt, .Discussion = std::nullopt, .Checks = application::github_pull_request_overview_observation::checks{ .Records = result.Checks, .Runs = result.ChecksAvailability, .Statuses = result.StatusesAvailability } });
			}

			if (aReadAuthority != github_provider_read_authority::anonymous_public) {
				// Optional explanation has a separate small aggregate budget. It
				// cannot exhaust essential PR reads or change their diagnostic origin.
				auto requirementLimits                          = aLimits;
				requirementLimits.MaximumTotalPages             = std::min(requirementLimits.MaximumTotalPages, std::size_t{ 32 });
				requirementLimits.MaximumAggregateResponseBytes = std::min(requirementLimits.MaximumAggregateResponseBytes, std::size_t{ 8U * 1024U * 1024U });
				production_read_execution requirementExecution{ aTransport, aProfile, aRequest.Context, requirementLimits, aStop, observedRateLimits, aRequest.AuthenticationSession, nullptr };
				if (!requirementExecution.bind_repository(result.Repository.Repository)) {
					return execution.terminal_failure(application::github_provider_failure::malformed_response);
				}
				const auto observe = [&](application::github_pull_request_overview_observation aObservation) {
					if (aObserver != nullptr && !aStop.stop_requested() && !aRequest.Context.ExpectedPullRequest
						&& aReadAuthority == github_provider_read_authority::authenticated) {
						aObservation.Context       = aRequest.Context;
						aObservation.Repository    = result.Repository;
						aObservation.PullRequest   = result.PullRequest;
						aObservation.ReportedFiles = expectedFiles;
						aObserver->overview_observed(std::move(aObservation));
					}
				};
				const auto assessmentReady = [&] {
					auto observation  = application::github_pull_request_overview_observation{ };
					observation.Merge = result.Merge;
					observe(std::move(observation));
				};
				const auto peopleReady = [&] {
					if (aObserver == nullptr || aStop.stop_requested() || aRequest.Context.ExpectedPullRequest
						|| aReadAuthority != github_provider_read_authority::authenticated) {
						return;
					}
					auto  observation   = application::github_pull_request_overview_observation{ };
					auto& people        = observation.People.emplace();
					people.Viewer       = result.Viewer;
					people.Requests     = result.ReviewRequests;
					people.Availability = result.ReviewRequestsAvailability;
					if (result.ReviewThreadsAvailability == application::github_collection_availability::available) {
						for (const auto& thread : result.ReviewThreads) {
							for (const auto& comment : thread.Comments) {
								people.Commenters.push_back({ comment.Comment, comment.Author });
							}
						}
					}
					for (const auto& comment : result.IssueComments) {
						people.Commenters.push_back({ comment.Comment, comment.Author });
					}
					for (const auto& review : result.Reviews) {
						people.Reviews.push_back({ review.DatabaseId, review.Review, review.Author, review.State });
					}
					observe(std::move(observation));
				};
				const auto requirements = read_merge_requirements(requirementExecution, aProfile, *server, aRequest.Context, result, requirementLimits, assessmentReady, peopleReady);
				if (!requirements) {
					return std::unexpected{ requirements.error() };
				}
			}

			// Received rows participate in ordinary validation even when their
			// search will be refused. No staged snapshot is published here.
			result.SearchAvailability = application::github_search_availability::available;

			if (aStop.stop_requested()) {
				return cancelled();
			}
			// The authority the read ran under, carried on the snapshot it
			// produced. The application layer validates the reader against
			// this field rather than against the request, so a signed-out read
			// that forgot to say so would be judged by the authenticated rule
			// and refused for holding no account.
			result.Mode       = aReadAuthority
					== github_provider_read_authority::anonymous_public
				? core::github_access_mode::public_preview
				: core::github_access_mode::authenticated;
			result.RateLimits = execution.rate_limits();
			auto validation   = application::validate(result, aRequest.Context, aLimits.Records);
			if (!validation.valid() && (validation.Error == application::github_read_validation_error::total_text_too_large || validation.Error == application::github_read_validation_error::collection_too_large)) {
				// Prefer accessible requests when optional policy details exhaust
				// the combined snapshot bound. Essential PR content stays usable.
				result.BranchPolicy.Rules.clear();
				result.BranchPolicy.Protection.clear();
				result.BranchPolicy.RulesAvailability      = observation::incomplete;
				result.BranchPolicy.ProtectionAvailability = observation::incomplete;
				validation                                 = application::validate(result, aRequest.Context, aLimits.Records);
				if (!validation.valid()) {
					result.ReviewRequests.clear();
					result.ReviewRequestsAvailability  = observation::incomplete;
					result.ReviewRequestActorsObserved = false;
					validation                         = application::validate(result, aRequest.Context, aLimits.Records);
				}
			}
			if (!validation.valid() && !result.Checks.empty()) {
				// Optional result detail cannot spend away otherwise readable PR facts.
				result.Checks.clear();
				result.ChecksAvailability   = application::github_collection_availability::unavailable;
				result.StatusesAvailability = application::github_collection_availability::unavailable;
				validation                  = application::validate(result, aRequest.Context, aLimits.Records);
			}
			if (!validation.valid()) {
				if (aObserver != nullptr) {
					aObserver->validation_failed(validation);
				}
				return execution.terminal_failure(
					validation_failure(validation.Error));
			}
			if (*search == application::github_search_availability::result_limit) {
				result.SearchResults.clear();
			}
			result.SearchAvailability = *search;
			return result;
		} catch (const std::bad_alloc&) {
			return failed(application::github_provider_failure::response_limit,
						  std::nullopt,
						  observedRateLimits);
		} catch (...) {
			return failed(application::github_provider_failure::malformed_response,
						  std::nullopt,
						  observedRateLimits);
		}
	}
}
