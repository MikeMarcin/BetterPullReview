#include "bpr/application/review_mutation_coordinator.hpp"
#include "bpr/core/comment_edit.hpp"
#include "bpr/core/reviewer_request.hpp"
#include "github_json_preflight.hpp"
#include "github_mutation_contract.hpp"
#include "github_mutation_contract_support.hpp"
#include "github_paged_read.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <expected>
#include <functional>
#include <glaze/json.hpp>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail {
	namespace {
		struct review_response_json
		{
			std::uint64_t      Id = 0U;
			std::string        NodeId;
			std::string        CommitId;
			std::string        Body;
			std::string        State;
			std::string        SubmittedAt;
			std::string        PullRequestUrl;
			review_author_json User;

			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &review_response_json::Id,
					"node_id", &review_response_json::NodeId,
					"commit_id", &review_response_json::CommitId,
					"body", &review_response_json::Body,
					"state", &review_response_json::State,
					"submitted_at", &review_response_json::SubmittedAt,
					"pull_request_url", &review_response_json::PullRequestUrl,
					"user", &review_response_json::User);
			};
		};

		struct review_list_json
		{
			std::uint64_t                     Id = 0U;
			std::string                       NodeId;
			std::string                       State;
			std::string                       PullRequestUrl;
			std::optional<std::string>        Body;
			std::optional<std::string>        CommitId;
			std::optional<std::string>        SubmittedAt;
			std::optional<review_author_json> User;
			struct glaze
			{
				static constexpr auto value = glz::object("id", &review_list_json::Id, "node_id", &review_list_json::NodeId, "state", &review_list_json::State, "pull_request_url", &review_list_json::PullRequestUrl, "body", &review_list_json::Body, "commit_id", &review_list_json::CommitId, "submitted_at", &review_list_json::SubmittedAt, "user", &review_list_json::User);
			};
		};

		/// The pull request as `RequestReviews` answers it.
		struct reviewer_written_pull_request_json
		{
			std::string                        Id;
			std::uint64_t                      DatabaseId = 0U;
			std::uint64_t                      Number     = 0U;
			std::optional<reviewer_login_json> Author;
			thread_repository_json             Repository;
			reviewer_request_nodes_json        ReviewRequests;
			reviewer_review_nodes_json         LatestReviews;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &reviewer_written_pull_request_json::Id,
					"databaseId", &reviewer_written_pull_request_json::DatabaseId,
					"number", &reviewer_written_pull_request_json::Number,
					"author", &reviewer_written_pull_request_json::Author,
					"repository", &reviewer_written_pull_request_json::Repository,
					"reviewRequests", &reviewer_written_pull_request_json::ReviewRequests,
					"latestReviews", &reviewer_written_pull_request_json::LatestReviews);
			};
		};

		struct reviewer_payload_json
		{
			reviewer_written_pull_request_json PullRequest;
			struct glaze
			{
				static constexpr auto value = glz::object("pullRequest", &reviewer_payload_json::PullRequest);
			};
		};

		struct reviewer_write_data_json
		{
			std::variant<reviewer_payload_json, std::nullptr_t> Request;
			struct glaze
			{
				static constexpr auto value = glz::object("requestReviews", &reviewer_write_data_json::Request);
			};
		};

		struct reviewer_write_response_json
		{
			reviewer_write_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &reviewer_write_response_json::Data);
			};
		};

		// [[BPR-1852]]. The pull request as a draft-state answer names it.
		struct draft_state_pull_request_json
		{
			std::string            Id;
			std::uint64_t          DatabaseId = 0U;
			std::uint64_t          Number     = 0U;
			bool                   IsDraft    = false;
			std::string            State;
			bool                   ViewerCanUpdate = false;
			thread_repository_json Repository;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &draft_state_pull_request_json::Id,
					"databaseId", &draft_state_pull_request_json::DatabaseId,
					"number", &draft_state_pull_request_json::Number,
					"isDraft", &draft_state_pull_request_json::IsDraft,
					"state", &draft_state_pull_request_json::State,
					"viewerCanUpdate", &draft_state_pull_request_json::ViewerCanUpdate,
					"repository", &draft_state_pull_request_json::Repository);
			};
		};

		struct draft_state_node_data_json
		{
			std::variant<draft_state_pull_request_json, std::nullptr_t> Node;
			struct glaze
			{
				static constexpr auto value = glz::object("node", &draft_state_node_data_json::Node);
			};
		};

		struct draft_state_node_response_json
		{
			draft_state_node_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &draft_state_node_response_json::Data);
			};
		};

		struct draft_state_payload_json
		{
			draft_state_pull_request_json PullRequest;
			struct glaze
			{
				static constexpr auto value = glz::object("pullRequest", &draft_state_payload_json::PullRequest);
			};
		};

		struct ready_for_review_data_json
		{
			std::variant<draft_state_payload_json, std::nullptr_t> Change;
			struct glaze
			{
				static constexpr auto value = glz::object("markPullRequestReadyForReview", &ready_for_review_data_json::Change);
			};
		};

		struct ready_for_review_response_json
		{
			ready_for_review_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &ready_for_review_response_json::Data);
			};
		};

		struct convert_to_draft_data_json
		{
			std::variant<draft_state_payload_json, std::nullptr_t> Change;
			struct glaze
			{
				static constexpr auto value = glz::object("convertPullRequestToDraft", &convert_to_draft_data_json::Change);
			};
		};

		struct convert_to_draft_response_json
		{
			convert_to_draft_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &convert_to_draft_response_json::Data);
			};
		};

		/// [[BPR-1852]]. The draft state, held to the designated pull request
		/// by every identity the target binds; a state GitHub does not name is
		/// malformed.
		[[nodiscard]] auto draft_state_observation(
			const draft_state_pull_request_json&           aPullRequest,
			const application::review_draft_state_request& aExpected)
			-> review_mutation_parse_result<review_draft_state_observation>
		{
			const auto bound = thread_pull_request_json{
				.Id         = aPullRequest.Id,
				.DatabaseId = aPullRequest.DatabaseId,
				.Number     = aPullRequest.Number,
				.Repository = aPullRequest.Repository,
			};
			if (!aExpected.LiveBinding.has_value() || !binds_designated_pull_request(bound, aExpected.LiveBinding->Target)) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			if (aPullRequest.State != "OPEN" && aPullRequest.State != "CLOSED" && aPullRequest.State != "MERGED") {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			return review_draft_state_observation{ .Draft = aPullRequest.IsDraft, .Open = aPullRequest.State == "OPEN", .ViewerCanUpdate = aPullRequest.ViewerCanUpdate };
		}

		struct comment_node_data_json
		{
			std::variant<comment_subject_json, std::nullptr_t> Node;
			struct glaze
			{
				static constexpr auto value = glz::object("node", &comment_node_data_json::Node);
			};
		};

		struct comment_node_response_json
		{
			comment_node_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &comment_node_response_json::Data);
			};
		};

		struct review_comment_payload_json
		{
			comment_subject_json Comment;
			struct glaze
			{
				static constexpr auto value = glz::object("pullRequestReviewComment", &review_comment_payload_json::Comment);
			};
		};

		struct issue_comment_payload_json
		{
			comment_subject_json Comment;
			struct glaze
			{
				static constexpr auto value = glz::object("issueComment", &issue_comment_payload_json::Comment);
			};
		};

		struct review_body_payload_json
		{
			comment_subject_json Comment;
			struct glaze
			{
				static constexpr auto value = glz::object("pullRequestReview", &review_body_payload_json::Comment);
			};
		};

		struct edit_review_comment_data_json
		{
			review_comment_payload_json Update;
			struct glaze
			{
				static constexpr auto value = glz::object("updatePullRequestReviewComment", &edit_review_comment_data_json::Update);
			};
		};

		struct edit_issue_comment_data_json
		{
			issue_comment_payload_json Update;
			struct glaze
			{
				static constexpr auto value = glz::object("updateIssueComment", &edit_issue_comment_data_json::Update);
			};
		};

		struct edit_review_body_data_json
		{
			review_body_payload_json Update;
			struct glaze
			{
				static constexpr auto value = glz::object("updatePullRequestReview", &edit_review_body_data_json::Update);
			};
		};

		template <typename Data>
		struct comment_edit_response_json
		{
			Data Payload;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &comment_edit_response_json::Payload);
			};
		};

		struct thread_comment_json
		{
			std::string                               Id;
			std::optional<std::string>                FullDatabaseId;
			std::string                               Body;
			std::string                               Path;
			std::optional<std::uint64_t>              Line;
			std::optional<reply_commit_json>          Commit;
			bool                                      ViewerDidAuthor = false;
			std::optional<thread_comment_author_json> Author;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &thread_comment_json::Id,
					"fullDatabaseId", &thread_comment_json::FullDatabaseId,
					"body", &thread_comment_json::Body,
					"path", &thread_comment_json::Path,
					"line", &thread_comment_json::Line,
					"commit", &thread_comment_json::Commit,
					"viewerDidAuthor", &thread_comment_json::ViewerDidAuthor,
					"author", &thread_comment_json::Author);
			};
		};

		struct thread_comments_page_json
		{
			bool HasNextPage = false;
			struct glaze
			{
				static constexpr auto value = glz::object("hasNextPage", &thread_comments_page_json::HasNextPage);
			};
		};

		struct thread_comments_json
		{
			thread_comments_page_json        PageInfo;
			std::vector<thread_comment_json> Nodes;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"pageInfo", &thread_comments_json::PageInfo,
					"nodes", &thread_comments_json::Nodes);
			};
		};

		struct thread_comments_node_json
		{
			std::string              Id;
			std::string              DiffSide;
			thread_pull_request_json PullRequest;
			thread_comments_json     Comments;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &thread_comments_node_json::Id,
					"diffSide", &thread_comments_node_json::DiffSide,
					"pullRequest", &thread_comments_node_json::PullRequest,
					"comments", &thread_comments_node_json::Comments);
			};
		};

		struct thread_comments_data_json
		{
			std::variant<thread_comments_node_json, std::nullptr_t> Node;
			struct glaze
			{
				static constexpr auto value = glz::object("node", &thread_comments_data_json::Node);
			};
		};

		struct thread_comments_response_json
		{
			thread_comments_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &thread_comments_response_json::Data);
			};
		};

		/// [[BPR-1538]]. The structural bounds for one page of the review list
		/// or the review comment collection: 100 objects of about 62 values
		/// each are 6,200 values, over the ordinary 4,096.
		constexpr auto list_preflight_limits = json_preflight_limits{ .Values = maximum_review_mutation_list_values };

		[[nodiscard]] auto review_observation(review_response_json aResponse, const review_submission_request& aExpected) -> review_mutation_parse_result<review_submission_observation>
		{
			const auto commit = core::parse_git_commit_oid(aResponse.CommitId);
			if (aResponse.Id == 0U || aResponse.User.Id == 0U
				|| !bounded_text(aResponse.NodeId, 256U) || !commit.has_value()
				|| !submitted_timestamp(aResponse.SubmittedAt)) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			const auto& target      = aExpected.Target;
			const auto  authority   = target.ServerContract == application::github_server_contract::github_com_2022_11_28
				? std::string{ "api.github.com" }
				: std::string{ target.Hostname.value() };
			const auto  expectedUrl = "https://" + authority + pull_request_path(target, target.ServerContract);
			if (aResponse.PullRequestUrl != expectedUrl || *commit != target.Semantic.ExpectedHead
				|| aResponse.Body != aExpected.Body || aResponse.State != submission_names(aExpected.Disposition)->State
				|| aResponse.User.Id != aExpected.ActingIdentity.DatabaseId
				|| aResponse.User.NodeId != aExpected.ActingIdentity.NodeId
				|| aResponse.User.Login != aExpected.ActingIdentity.Login) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			return review_submission_observation{
				.ReviewDatabaseId = aResponse.Id,
				.ReviewNodeId     = std::move(aResponse.NodeId),
				.Target           = target,
				.Author           = aExpected.ActingIdentity,
				.Disposition      = aExpected.Disposition,
				.Body             = std::move(aResponse.Body),
				.SubmittedAt      = std::move(aResponse.SubmittedAt),
			};
		}

		// Only constructed public vocabulary leaves this parser. Missing, null,
		// empty, non-string and unknown values remain distinct observations.
		[[nodiscard]] auto diagnostic_text(const diagnostic_json_object& aObject, const std::string_view aKey, std::initializer_list<std::string_view> aAllowed) -> std::string
		{
			const auto found = aObject.find(aKey);
			if (found == aObject.end()) {
				return "absent";
			}
			if (found->second.str == "null") {
				return "null";
			}
			auto value = std::string{ };
			if (glz::read<review_json_options{ }>(value, found->second.str)) {
				return "non_string";
			}
			if (value.empty()) {
				return "empty";
			}
			for (const auto word : aAllowed) {
				if (value == word) {
					return std::string{ word };
				}
			}
			if (aKey == "message") {
				for (const auto subject : { "line", "Line", "pull_request_review_thread.line" }) {
					for (const auto quote : { "", "'", "\"", "`" }) {
						for (const auto predicate : { " must be part of the diff", " must be a part of the diff", " is not part of the diff", " is not a part of the diff" }) {
							for (const auto period : { "", "." }) {
								auto recognized = std::string{ quote } + subject + quote + predicate + period;
								if (value == recognized) {
									return recognized;
								}
							}
						}
					}
				}
			}
			return "omitted";
		}

		[[nodiscard]] auto unknown_diagnostic_members(const diagnostic_json_object& aObject, std::initializer_list<std::string_view> aKnown) -> std::size_t
		{
			return static_cast<std::size_t>(std::ranges::count_if(aObject, [&](const auto& aMember) { return std::ranges::find(aKnown, aMember.first) == aKnown.end(); }));
		}

		[[nodiscard]] auto rejection_diagnostic(const std::string& aBody) -> std::string
		{
			auto envelope = diagnostic_json_object{ };
			if (aBody.size() > 4096U || !json_preflight{ aBody }.run() || glz::read<review_json_options{ }>(envelope, aBody)) {
				return " rejection=omitted_invalid_or_limit";
			}
			auto result = std::string{ " envelope=" } + diagnostic_text(envelope, "message", { "Validation Failed" });
			result += " envelope_unknown=" + std::to_string(unknown_diagnostic_members(envelope, { "message", "errors", "documentation_url", "status" }));
			result += " documentation_url=" + diagnostic_text(envelope, "documentation_url", { "https://docs.github.com/rest/pulls/comments#create-a-review-comment-for-a-pull-request", "https://docs.github.com/en/rest/pulls/comments#create-a-review-comment-for-a-pull-request" });
			result += " status=" + diagnostic_text(envelope, "status", { "422" });
			const auto found = envelope.find("errors");
			if (found == envelope.end()) {
				return result + " errors=absent";
			}
			if (found->second.str == "null") {
				return result + " errors=null";
			}
			auto errors = std::vector<diagnostic_json_object>{ };
			if (glz::read<review_json_options{ }>(errors, found->second.str)) {
				return result + " errors=invalid_shape";
			}
			if (errors.size() > 8U) {
				return result + " errors=omitted_limit";
			}
			result += " errors=" + std::to_string(errors.size());
			for (const auto& error : errors) {
				result += " error_unknown=" + std::to_string(unknown_diagnostic_members(error, { "resource", "field", "code", "message" }));
				result += " resource=" + diagnostic_text(error, "resource", { "PullRequestReviewComment", "PullRequestReviewThread", "PullRequestReview" });
				result += " field=" + diagnostic_text(error, "field", { "line", "position", "path", "commit_id", "side", "subject_type", "pull_request_review_thread", "pull_request_review_thread.line", "pull_request_review_thread.start_line" });
				result += " code=" + diagnostic_text(error, "code", { "invalid", "missing", "missing_field", "custom", "unprocessable", "already_exists" });
				result += " message=" + diagnostic_text(error, "message", { "could not be resolved", "could not be resolved.", "must be part of the diff", "must be part of the diff.", "is not part of the diff", "is not part of the diff." });
			}
			return result;
		}

		// [[BPR-0896]]. GraphQL `BigInt` is a string-encoded integer; the
		// schema encodes it so because the value may exceed a 32-bit `Int`.
		// Only the canonical decimal spelling is admitted here, and a bare
		// JSON number is refused at decode time, so a 32-bit `databaseId`
		// can never pass for the full identity.
		[[nodiscard]] auto big_int(const std::string_view aText) noexcept
			-> std::optional<std::uint64_t>
		{
			if (aText.empty() || aText.size() > 20U
				|| (aText.size() > 1U && aText.front() == '0')
				|| !std::ranges::all_of(aText, [](const unsigned char aCharacter) {
					   return std::isdigit(aCharacter) != 0;
				   })) {
				return std::nullopt;
			}
			auto value              = std::uint64_t{ };
			const auto [end, error] = std::from_chars(aText.data(), aText.data() + aText.size(), value);
			if (error != std::errc{ } || end != aText.data() + aText.size()) {
				return std::nullopt;
			}
			return value;
		}

		// [[BPR-0896]]. One comment of the thread a reply landed in, held to
		// the identity a REST comment (comment_observation) and the reply's
		// own answer (reply_observation) are held to. The thread holds its
		// root and any earlier replies, by anyone and of any age, so a
		// comment that is not ours must be free to be anything the schema
		// allows (an identical one of ours is exact and falls in the
		// baseline the new reply is told apart from): an
		// outdated one (`line` and `commit` null), one by a Bot, Mannequin or
		// Organization (no `User` fragment) or one without a database
		// identity is a tolerated non-match, never a malformed answer. Null
		// `line` and `commit` are not mapped to `originalLine` and
		// `originalCommit`: the reply's own response check already demands a
		// live line and commit for our comment, so demanding the same of an
		// exact receipt is consistent; the original position is a stale one
		// that would be compared against the anchor the app read; and what
		// `originalCommit` means for a reply is not established by the
		// schema. The consequence is that a reply of ours that goes outdated
		// between the write and a later reconciliation is not recognised as
		// an exact receipt by this observation: its attempt stays unresolved,
		// which is the safe direction, since a known attempt is never
		// resubmitted and no stale position is ever confirmed as ours.
		[[nodiscard]] auto thread_comment_observation(
			const thread_comment_json&                  aComment,
			const std::string_view                      aDiffSide,
			const application::review_mutation_request& aExpected)
			-> review_mutation_parse_result<review_mutation_comment_observation>
		{
			constexpr auto maximumGithubLine = static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1U;
			const auto     databaseId        = aComment.FullDatabaseId ? big_int(*aComment.FullDatabaseId) : std::optional<std::uint64_t>{ };
			const auto     commit            = aComment.Commit ? core::parse_git_commit_oid(aComment.Commit->Oid) : std::optional<core::git_commit_oid>{ };
			// A present field spelled outside its scalar's vocabulary is a
			// malformed answer; an absent one is a comment that is not ours.
			if ((aComment.FullDatabaseId.has_value() && !databaseId.has_value())
				|| (aComment.Commit.has_value() && !commit.has_value())
				|| (aComment.Line.has_value() && (*aComment.Line == 0U || *aComment.Line > maximumGithubLine))) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			const auto& author  = aComment.Author;
			const auto& binding = *aExpected.LiveBinding;
			// [[BPR-1745]]. A reply bound to its outdated thread has no
			// line, and is ours on the thread's own path and commit; the thread
			// observed is already the one asked for, so its side says nothing
			// more. A reply on a line keeps every check it had.
			const auto threadBound = application::thread_bound_binding(binding.Anchor);
			if (!databaseId.has_value() || *databaseId == 0U || !commit.has_value()
				|| (threadBound ? aComment.Line.has_value() : !aComment.Line.has_value()) || !author.has_value()
				|| !author->DatabaseId.has_value() || *author->DatabaseId == 0U
				|| !author->Id.has_value()) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			const auto expectedSide = binding.Anchor.Semantic.Side == core::diff_side::left ? "LEFT" : "RIGHT";
			const auto semanticLine = threadBound ? binding.Anchor.Semantic.Line : static_cast<std::uint32_t>(*aComment.Line - 1U);
			if (aComment.Path != binding.Anchor.Path || *commit != binding.Anchor.Commit
				|| (!threadBound && aDiffSide != expectedSide) || semanticLine != binding.Anchor.Semantic.Line
				|| aComment.Body != aExpected.Proposal.Body || !aComment.ViewerDidAuthor
				|| *author->DatabaseId != binding.ActingIdentity.DatabaseId
				|| *author->Id != binding.ActingIdentity.NodeId
				|| author->Login != binding.ActingIdentity.Login) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			// [[BPR-0816]]. Name the nested type, as in comment_observation.
			return review_mutation_comment_observation{
				.Receipt    = application::review_mutation_receipt{ .ServerCommentId = aComment.Id, .Duplicate = false },
				.DatabaseId = *databaseId,
				.NodeId     = aComment.Id,
				.Anchor     = binding.Anchor,
				.Body       = aComment.Body,
				.Author     = binding.ActingIdentity,
			};
		}

		/// Server-identity order for both lists: a projection that repeats an
		/// identity is refused, exact receipts are deduplicated.
		[[nodiscard]] auto ordered_reconciliation(review_mutation_server_reconciliation aResult)
			-> review_mutation_parse_result<review_mutation_server_reconciliation>
		{
			std::ranges::sort(aResult.Projection, { }, &application::review_comment_projection::ServerId);
			if (std::ranges::adjacent_find(
					aResult.Projection,
					{ },
					&application::review_comment_projection::ServerId)
				!= aResult.Projection.end()) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			std::ranges::sort(
				aResult.ExactReceipts,
				{ },
				&application::review_mutation_receipt::ServerCommentId);
			const auto duplicates = std::ranges::unique(
				aResult.ExactReceipts,
				{ },
				&application::review_mutation_receipt::ServerCommentId);
			aResult.ExactReceipts.erase(duplicates.begin(), duplicates.end());
			return aResult;
		}

		// [[BPR-0896]]. The ObserveThreadComments envelope, reduced to the
		// same reconciliation a REST comment list yields. `malformed_response`
		// is reserved for a structurally invalid envelope; a comment that is
		// merely not ours is projected and tolerated.
		[[nodiscard]] auto parse_thread_comment_observations(
			const review_mutation_wire_response&        aResponse,
			const application::review_mutation_request& aExpected)
			-> review_mutation_parse_result<review_mutation_server_reconciliation>
		{
			if (aResponse.Status != 200 || !response_headers_valid(aResponse)
				|| !aExpected.LiveBinding.has_value()) {
				return failed(review_mutation_contract_failure::invalid_status);
			}
			// A null node is an unavailable target, distinct from an invalid
			// envelope. Neither answer authorizes a write or reconciliation.
			const auto response = document<thread_comments_response_json>(aResponse);
			if (!response.has_value()) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			if (std::holds_alternative<std::nullptr_t>(response->Data.Node)) {
				return failed(review_mutation_contract_failure::target_unavailable);
			}
			const auto& thread = std::get<thread_comments_node_json>(response->Data.Node);
			if (thread.Id != *aExpected.Proposal.ReplyToThread
				|| !binds_designated_pull_request(
					thread.PullRequest, aExpected.LiveBinding->Target)) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			if ((thread.DiffSide != "LEFT" && thread.DiffSide != "RIGHT")
				|| thread.Comments.Nodes.size() > 100U) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			// A thread with a further page is as incomplete as a REST list
			// with a `Link` header, and is refused the same way.
			if (thread.Comments.PageInfo.HasNextPage) {
				return failed(review_mutation_contract_failure::invalid_status);
			}
			auto result = review_mutation_server_reconciliation{ };
			result.Projection.reserve(thread.Comments.Nodes.size());
			for (const auto& comment : thread.Comments.Nodes) {
				if (!bounded_text(comment.Id, 256U) || comment.Body.size() > maximum_review_mutation_response_bytes || !utf8_round_trip(comment.Body)) {
					return failed(review_mutation_contract_failure::malformed_response);
				}
				result.Projection.push_back(application::review_comment_projection{
					.ServerId = comment.Id,
					.Body     = comment.Body,
				});
				const auto exact = thread_comment_observation(comment, thread.DiffSide, aExpected);
				if (exact) {
					result.ExactReceipts.push_back(exact->Receipt);
				} else if (exact.error() != review_mutation_contract_failure::identity_mismatch) {
					return failed(exact.error());
				}
			}
			return ordered_reconciliation(std::move(result));
		}
	}

	auto parse_reviewer_response(
		const review_mutation_wire_response&        aResponse,
		const application::review_reviewer_request& aExpected)
		-> review_mutation_parse_result<core::reviewer_search_answer>
	{
		if (!aExpected.LiveBinding.has_value() || aExpected.Operation != application::review_reviewer_operation::request
			|| !compile_reviewer_request(aExpected, aExpected.LiveBinding->Target.ServerContract).has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto response = document<reviewer_write_response_json>(aResponse);
		if (!response.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		// A null payload is GraphQL's refusal of the mutation: nothing written.
		if (std::holds_alternative<std::nullptr_t>(response->Data.Request)) {
			return failed(review_mutation_contract_failure::unexpected_body);
		}
		const auto& pull = std::get<reviewer_payload_json>(response->Data.Request).PullRequest;
		if (!reviewer_binds(pull.Id, pull.DatabaseId, pull.Number, pull.Repository, aExpected)) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return reviewer_standing_facts(pull.Author, pull.ReviewRequests, pull.LatestReviews);
	}

	auto parse_draft_state_observation(
		const review_mutation_wire_response&           aResponse,
		const application::review_draft_state_request& aExpected)
		-> review_mutation_parse_result<review_draft_state_observation>
	{
		if (!aExpected.LiveBinding.has_value()
			|| !compile_draft_state_observation_request(aExpected, aExpected.LiveBinding->Target.ServerContract).has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto response = document<draft_state_node_response_json>(aResponse);
		if (!response.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		if (std::holds_alternative<std::nullptr_t>(response->Data.Node)) {
			return failed(review_mutation_contract_failure::target_unavailable);
		}
		return draft_state_observation(std::get<draft_state_pull_request_json>(response->Data.Node), aExpected);
	}

	auto parse_draft_state_response(
		const review_mutation_wire_response&           aResponse,
		const application::review_draft_state_request& aExpected)
		-> review_mutation_parse_result<review_draft_state_observation>
	{
		if (!aExpected.LiveBinding.has_value() || !compile_draft_state_request(aExpected, aExpected.LiveBinding->Target.ServerContract).has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		// Each change is answered under its own mutation's name.
		auto payload = std::optional<std::variant<draft_state_payload_json, std::nullptr_t>>{ };
		if (aExpected.Change == core::draft_state_change::convert_to_draft) {
			if (auto response = document<convert_to_draft_response_json>(aResponse); response.has_value()) {
				payload = std::move(response->Data.Change);
			}
		} else if (auto response = document<ready_for_review_response_json>(aResponse); response.has_value()) {
			payload = std::move(response->Data.Change);
		}
		if (!payload.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		// A null payload is GraphQL's refusal of the mutation: nothing written.
		if (std::holds_alternative<std::nullptr_t>(*payload)) {
			return failed(review_mutation_contract_failure::unexpected_body);
		}
		return draft_state_observation(std::get<draft_state_payload_json>(*payload).PullRequest, aExpected);
	}

	auto parse_comment_observation(
		const review_mutation_wire_response&            aResponse,
		const application::review_comment_edit_request& aExpected)
		-> review_mutation_parse_result<review_description_observation>
	{
		if (!aExpected.LiveBinding.has_value()
			|| !compile_comment_observation_request(aExpected, aExpected.LiveBinding->Target.ServerContract).has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto response = document<comment_node_response_json>(aResponse, maximum_description_response_bytes);
		if (!response.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		if (std::holds_alternative<std::nullptr_t>(response->Data.Node)) {
			return failed(review_mutation_contract_failure::target_unavailable);
		}
		return comment_observation(std::get<comment_subject_json>(response->Data.Node), aExpected);
	}

	auto parse_comment_edit_response(
		const review_mutation_wire_response&            aResponse,
		const application::review_comment_edit_request& aExpected)
		-> review_mutation_parse_result<review_description_observation>
	{
		if (!aExpected.LiveBinding.has_value() || !compile_comment_edit_request(aExpected, aExpected.LiveBinding->Target.ServerContract).has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		auto comment = std::optional<comment_subject_json>{ };
		switch (aExpected.Kind) {
		case core::comment_edit_kind::review_comment:
			if (auto response = document<comment_edit_response_json<edit_review_comment_data_json>>(aResponse, maximum_description_response_bytes); response.has_value()) {
				comment = std::move(response->Payload.Update.Comment);
			}
			break;
		case core::comment_edit_kind::pull_request_comment:
			if (auto response = document<comment_edit_response_json<edit_issue_comment_data_json>>(aResponse, maximum_description_response_bytes); response.has_value()) {
				comment = std::move(response->Payload.Update.Comment);
			}
			break;
		case core::comment_edit_kind::review_body:
			if (auto response = document<comment_edit_response_json<edit_review_body_data_json>>(aResponse, maximum_description_response_bytes); response.has_value()) {
				comment = std::move(response->Payload.Update.Comment);
			}
			break;
		}
		if (!comment.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		auto observed = comment_observation(*comment, aExpected);
		if (!observed.has_value()) {
			return observed;
		}
		// The write's own answer must carry the body it asked for.
		if (observed->Body != aExpected.Body) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return observed;
	}

	auto parse_review_submission_response(
		const review_mutation_wire_response& aResponse,
		const review_submission_request&     aExpected)
		-> review_mutation_parse_result<review_submission_observation>
	{
		if (!compile_review_submission_request(aExpected, aExpected.Target.ServerContract).has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (has_header(aResponse, "location") || (aResponse.Status >= 300 && aResponse.Status < 400)) {
			return failed(review_mutation_contract_failure::redirect);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		if (aResponse.Body.size() > maximum_review_mutation_response_bytes) {
			return failed(review_mutation_contract_failure::response_limit);
		}
		if (!json_preflight{ aResponse.Body }.run()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		auto response = review_response_json{ };
		if (glz::read<review_json_options{ }>(response, aResponse.Body)) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		return review_observation(std::move(response), aExpected);
	}

	auto parse_review_submission_observations(const review_mutation_wire_response& aResponse, const review_submission_request& aExpected) -> review_mutation_parse_result<review_submission_reconciliation>
	{
		if (!compile_review_submission_observation_request(aExpected, aExpected.Target.ServerContract)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse) || has_header(aResponse, "location") || has_header(aResponse, "link")) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		// [[BPR-1538]]. A full page of real reviews, not the ordinary bound.
		if (aResponse.Body.size() > maximum_review_mutation_list_response_bytes) {
			return failed(review_mutation_contract_failure::response_limit);
		}
		if (!json_preflight{ aResponse.Body, list_preflight_limits }.run()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		std::vector<review_list_json> reviews;
		if (glz::read<review_json_options{ }>(reviews, aResponse.Body) || reviews.size() > 100U) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		review_submission_reconciliation result;
		const auto                       authority   = aExpected.Target.ServerContract == application::github_server_contract::github_com_2022_11_28 ? std::string{ "api.github.com" } : std::string{ aExpected.Target.Hostname.value() };
		const auto                       expectedUrl = "https://" + authority + pull_request_path(aExpected.Target, aExpected.Target.ServerContract);
		for (auto& review : reviews) {
			if (review.Id == 0U || !bounded_text(review.NodeId, 256U) || review.PullRequestUrl != expectedUrl) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			result.ReviewIds.emplace_back(review.Id);
			if (review.State == "PENDING") {
				if (review.SubmittedAt) {
					return failed(review_mutation_contract_failure::malformed_response);
				}
				continue;
			}
			if (review.State != "DISMISSED" && review.State != "COMMENTED" && review.State != "APPROVED" && review.State != "CHANGES_REQUESTED") {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			if (!review.SubmittedAt || !submitted_timestamp(*review.SubmittedAt)) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			if (review.State == "DISMISSED" || !review.User || !review.Body || !review.CommitId) {
				// [[BPR-1715]]. One that could be the request's own is
				// not proof that the request created nothing.
				if (!review.User || review.User->Id == aExpected.ActingIdentity.DatabaseId || review.User->Login == aExpected.ActingIdentity.Login) {
					result.UncertainReviews.emplace_back(review.Id);
				}
				continue;
			}
			const auto exact = review_observation(review_response_json{
													  .Id = review.Id, .NodeId = std::move(review.NodeId), .CommitId = std::move(*review.CommitId), .Body = std::move(*review.Body), .State = std::move(review.State), .SubmittedAt = std::move(*review.SubmittedAt), .PullRequestUrl = std::move(review.PullRequestUrl), .User = std::move(*review.User) },
												  aExpected);

			if (exact) {
				result.ExactReviews.push_back(*exact);
			} else if (exact.error() != review_mutation_contract_failure::identity_mismatch) {
				return failed(exact.error());
			}
		}
		std::ranges::sort(result.ReviewIds);
		std::ranges::sort(result.UncertainReviews);
		if (std::ranges::adjacent_find(result.ReviewIds) != result.ReviewIds.end()) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return result;
	}

	auto parse_review_comment_collection(const review_mutation_wire_response& aResponse, const application::review_mutation_target_binding& aTarget, const std::uint32_t aPage)
		-> review_mutation_parse_result<review_comment_collection_page>
	{
		const auto wire = compile_review_comment_collection_request(aTarget, aTarget.ServerContract, aPage);
		if (!wire) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (has_header(aResponse, "location") || (aResponse.Status >= 300 && aResponse.Status < 400)) {
			return failed(review_mutation_contract_failure::redirect);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		// [[BPR-1538]]. A real page of 100 comments is larger than the
		// ordinary bound (380 KB on MikeMarcin/BPR-Test#70).
		if (aResponse.Body.size() > maximum_review_mutation_list_response_bytes) {
			return failed(review_mutation_contract_failure::response_limit);
		}
		// [[BPR-1367]]. Every page is read; a collection longer than the
		// bounded number of pages is refused, never read in part.
		// [[BPR-1540]]. Read one page past the bound so that a list longer
		// than the bound is told apart from a next link that is not this
		// list's, and both from a body that is not GitHub's list.
		const auto next = paged_next(aResponse, aTarget, wire->Path, aPage, maximum_review_comment_collection_pages + 1U, true);
		if (!next) {
			return failed(review_mutation_contract_failure::unexpected_headers);
		}
		if (*next && aPage >= maximum_review_comment_collection_pages) {
			return failed(review_mutation_contract_failure::collection_limit);
		}
		auto comments = std::optional<std::vector<comment_response_json>>{ };
		if (!aResponse.Body.empty() && json_preflight{ aResponse.Body, list_preflight_limits }.run()) {
			comments.emplace();
			if (glz::read<review_json_options{ }>(*comments, aResponse.Body)) {
				comments.reset();
			}
		}
		if (!comments || comments->size() > 100U || (*next && comments->empty())) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		auto page = review_comment_collection_page{ .HasNext = *next };
		for (const auto& comment : *comments) {
			// Anyone's comment is listed: outdated (lines null, sides kept), on
			// a file (no side) or a range from one side to the other are all
			// well formed. A tour's or a review's own comments are held to
			// more where they are matched.
			const auto sideName   = [](const std::optional<std::string>& aSide) { return !aSide || *aSide == "LEFT" || *aSide == "RIGHT"; };
			const auto lineNumber = [](const std::optional<std::uint64_t>& aLine) { return !aLine || (*aLine != 0U && *aLine <= std::uint64_t{ std::numeric_limits<std::uint32_t>::max() } + 1U); };
			if (comment.Id == 0U || !bounded_text(comment.NodeId, 256U) || !utf8_round_trip(comment.Path) || !utf8_round_trip(comment.Body)
				|| comment.Body.size() > maximum_review_mutation_body_bytes || !sideName(comment.Side) || !sideName(comment.StartSide)
				|| !lineNumber(comment.Line) || !lineNumber(comment.StartLine) || !lineNumber(comment.OriginalLine) || !lineNumber(comment.OriginalStartLine)
				|| (comment.Line && comment.StartLine && comment.StartSide == comment.Side && *comment.StartLine >= *comment.Line)
				|| std::ranges::any_of(page.Comments, [&comment](const review_comment_collection_entry& aSeen) { return aSeen.Id == comment.Id || aSeen.NodeId == comment.NodeId; })) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			const auto side         = [](const std::optional<std::string>& aSide) { return aSide == std::optional<std::string>{ "LEFT" } ? core::diff_side::left : core::diff_side::right; };
			auto       entry        = review_comment_collection_entry{ };
			entry.Id                = comment.Id;
			entry.NodeId            = comment.NodeId;
			entry.WebUrl            = comment.WebUrl.value_or(std::string{ });
			entry.PullRequestUrl    = comment.PullRequestUrl.value_or(std::string{ });
			entry.Review            = comment.ReviewId.value_or(0U);
			entry.Path              = comment.Path;
			entry.Side              = side(comment.Side);
			entry.Line              = comment.Line ? std::optional{ static_cast<std::uint32_t>(*comment.Line - 1U) } : std::nullopt;
			entry.StartLine         = comment.StartLine ? std::optional{ static_cast<std::uint32_t>(*comment.StartLine - 1U) } : std::nullopt;
			entry.StartSide         = comment.StartSide ? std::optional{ side(comment.StartSide) } : std::nullopt;
			entry.Body              = comment.Body;
			entry.CommitId          = comment.CommitId;
			entry.OriginalCommitId  = comment.OriginalCommitId.value_or(std::string{ });
			entry.OriginalLine      = comment.OriginalLine ? std::optional{ static_cast<std::uint32_t>(*comment.OriginalLine - 1U) } : std::nullopt;
			entry.OriginalStartLine = comment.OriginalStartLine ? std::optional{ static_cast<std::uint32_t>(*comment.OriginalStartLine - 1U) } : std::nullopt;
			entry.Author            = comment.User.Id;
			entry.AuthorNodeId      = comment.User.NodeId;
			entry.Login             = comment.User.Login;
			entry.OnLine            = comment.SubjectType.value_or("line") == "line" && comment.Side.has_value();
			entry.Reply             = comment.InReplyToId.has_value();
			page.Comments.push_back(std::move(entry));
		}
		return page;
	}

	auto staged_review_comments(const std::vector<review_comment_collection_entry>& aCollection, const application::staged_review_request& aExpected)
		-> review_mutation_parse_result<std::vector<application::staged_review_comment>>
	{
		if (!aExpected.Review.value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		auto result = std::vector<application::staged_review_comment>{ };
		for (const auto& entry : aCollection) {
			if (entry.Review != aExpected.Review.value()) {
				continue;
			}
			// Held to exactly what the per-review list's comment is held to,
			// with the lines and side the collection reports.
			const auto sideName = [](const core::diff_side aSide) { return std::string{ aSide == core::diff_side::left ? "LEFT" : "RIGHT" }; };
			auto       json     = staged_comment_json{ };
			json.Id             = entry.Id;
			json.Review         = entry.Review;
			json.NodeId         = entry.NodeId;
			json.WebUrl         = entry.WebUrl;
			json.PullRequestUrl = entry.PullRequestUrl;
			json.Body           = entry.Body;
			json.Path           = entry.Path;
			json.Commit         = entry.CommitId;
			json.User           = { .Id = entry.Author, .NodeId = entry.AuthorNodeId, .Login = entry.Login };
			json.Line           = entry.Line ? std::optional{ std::uint64_t{ *entry.Line } + 1U } : std::nullopt;
			json.StartLine      = entry.StartLine ? std::optional{ std::uint64_t{ *entry.StartLine } + 1U } : std::nullopt;
			json.Reply          = entry.Reply ? std::optional<std::uint64_t>{ 1U } : std::nullopt;
			json.Side           = entry.OnLine ? std::optional{ sideName(entry.Side) } : std::nullopt;
			json.StartSide      = entry.StartSide ? std::optional{ sideName(*entry.StartSide) } : std::nullopt;
			json.Subject        = std::string{ entry.OnLine ? "line" : "file" };
			auto value          = staged_comment_entity(json, aExpected);
			if (!value) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			result.push_back(std::move(*value));
		}
		return result;
	}

	auto parse_review_comment_observations(
		const review_mutation_wire_response&        aResponse,
		const application::review_mutation_request& aExpected)
		-> review_mutation_parse_result<review_mutation_server_reconciliation>
	{
		// [[BPR-0896]]. A reply is reconciled through its thread's GraphQL
		// answer; a new root comment through the REST comment list.
		if (aExpected.Proposal.ReplyToThread.has_value()) {
			return parse_thread_comment_observations(aResponse, aExpected);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)
			|| has_header(aResponse, "Link") || !aExpected.LiveBinding.has_value()) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto comments = document<std::vector<comment_response_json>>(aResponse);
		if (!comments || comments->size() > 100U) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		auto result = review_mutation_server_reconciliation{ };
		result.Projection.reserve(comments->size());
		for (const auto& comment : *comments) {
			if (!bounded_text(comment.NodeId, 256U) || comment.Body.size() > maximum_review_mutation_response_bytes || !utf8_round_trip(comment.Body)) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			result.Projection.push_back(application::review_comment_projection{
				.ServerId = comment.NodeId,
				.Body     = comment.Body,
			});
			const auto exact = comment_observation(comment, aExpected);
			if (exact) {
				result.ExactReceipts.push_back(exact->Receipt);
			} else if (exact.error() != review_mutation_contract_failure::identity_mismatch) {
				return failed(exact.error());
			}
		}
		return ordered_reconciliation(std::move(result));
	}

	auto valid(const live_review_mutation_configuration& aConfiguration) noexcept -> bool
	{
		return valid_identity(aConfiguration) && application::valid(aConfiguration.Target);
	}

	auto valid_identity(const live_review_mutation_configuration& aConfiguration) noexcept -> bool
	{
		return application::valid(aConfiguration.CredentialContext)
			&& application::valid_identity(aConfiguration.Target)
			&& application::valid(aConfiguration.ActingIdentity)
			&& aConfiguration.CredentialContext.Authority.Host
			== aConfiguration.Target.Semantic.Host
			&& aConfiguration.CredentialContext.Authority.Hostname
			== aConfiguration.Target.Hostname
			&& aConfiguration.CredentialContext.Authority.ServerContract
			== aConfiguration.Target.ServerContract
			&& aConfiguration.CredentialContext.Authority.Account
			== aConfiguration.ActingIdentity.Account;
	}

	review_mutation_session::review_mutation_session(
		github_mutation_wire_transport&        aTransport,
		live_review_mutation_configuration     aConfiguration,
		application::review_mutation_transport aLease,
		std::unique_ptr<application::review_mutation_recovery_port>
													 aRecovery,
		std::optional<off_hunk_unsupported_contract> aUnsupportedContract, application::review_publication_lineage_port* aLineage, std::shared_ptr<application::diagnostic_log_port> aLog) noexcept
		: Transport{ &aTransport }
		, Configuration{ std::move(aConfiguration) }
		, Lease{ std::move(aLease) }
		, Recovery{ std::move(aRecovery) }
		, UnsupportedContract{ std::move(aUnsupportedContract) }
		, Lineage{ aLineage }
		, Log{ std::move(aLog) }
	{
	}

	auto review_comment_diagnostic(const review_mutation_wire_request& aRequest, const github_mutation_wire_completion& aCompletion) noexcept -> application::diagnostic_event
	{
		auto event = application::diagnostic_event{ .Operation = application::diagnostic_operation::review_comment, .Outcome = application::diagnostic_outcome::failed };
		try {
			const auto* semantic = std::get_if<application::review_mutation_request>(&aRequest.Semantic);
			auto        detail   = std::string{ "create_review_comment representation=" };
			detail += semantic && semantic->Representation == core::review_publication_representation::file_comment_with_permalink ? "file" : "native";
			if (const auto* success = std::get_if<github_mutation_wire_success>(&aCompletion)) {
				const auto& response         = success->Response;
				event.HttpStatus             = response.Status >= 100 && response.Status <= 599 ? static_cast<std::uint16_t>(response.Status) : 0U;
				event.Outcome                = application::diagnostic_outcome::response_received;
				event.TransportFailureSource = application::diagnostic_transport_failure_source::http_response;
				detail += response_headers_valid(response) ? " metadata=valid" : " metadata=invalid";
				if (response.Status == 422) {
					detail += rejection_diagnostic(response.Body);
				}
			} else if (const auto* error = std::get_if<github_mutation_wire_error>(&aCompletion)) {
				event.HttpStatus             = error->HttpStatus;
				event.TransportFailureSource = error->TransportFailureSource;
				detail += " failure=";
				detail += application::name(error->Failure);
				constexpr std::array<std::string_view, 15> causes{ "authentication", "permission", "network", "api_version", "unsupported_capability", "rate_limit", "certificate", "redirect", "timeout", "process_startup", "cli_unavailable", "vault", "malformed_response", "response_limit", "unavailable" };
				if (error->ProviderFailure) {
					const auto index = static_cast<std::size_t>(*error->ProviderFailure);
					detail += " provider_failure=";
					detail += index < causes.size() ? causes[index] : "invalid";
				}
				detail += error->Transmission == github_mutation_transmission::indeterminate ? " transmission=indeterminate" : " transmission=not_sent";
			} else {
				event.Outcome = application::diagnostic_outcome::cancelled;
				detail += " cancelled";
			}
			if (detail.size() > event.Summary.Text.size()) {
				event.Summary.State = application::diagnostic_summary_state::omitted_limit;
				return event;
			}
			std::ranges::copy(detail, event.Summary.Text.begin());
			event.Summary.Size  = static_cast<std::uint16_t>(detail.size());
			event.Summary.State = application::diagnostic_summary_state::sanitized;
		} catch (...) {
			event.Summary = { .State = application::diagnostic_summary_state::omitted_failure };
		}
		return event;
	}

	auto review_mutation_session::perform(
		review_mutation_wire_request         aRequest,
		const std::stop_token                aStop,
		github_mutation_transmission_witness aWitness)
		-> std::expected<review_mutation_wire_response, application::review_mutation_failure>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(application::review_mutation_failure::cancelled);
		}
		// [[BPR-1826]]. A closed or merged pull request's connection performs
		// too: every request was compiled by its own compiler, and only a
		// thread's resolution and the reads it needs compile for one.
		if (Transport == nullptr || Lease == nullptr || !Lease->active()
			|| !valid_identity(Configuration)) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		const auto operation         = aRequest.Operation;
		const auto contextGeneration = Log ? Log->context_generation() : 0U;
		const auto wireRequest       = github_mutation_wire_request{
			.Context = Configuration.CredentialContext,
			.Wire    = std::move(aRequest),
		};
		auto completion = Transport->perform_review_mutation(wireRequest, aStop, std::move(aWitness));
		if (Log && operation == review_mutation_operation::create_review_comment) {
			auto event              = review_comment_diagnostic(wireRequest.Wire, completion);
			event.Request           = Configuration.CredentialContext.Request.value();
			event.ContextGeneration = contextGeneration;
			static_cast<void>(Log->record(event));
		}
		// [[BPR-1735]]. The status of a refusal the transport carries as an
		// error (native HTTPS admits no 4xx answer for every operation).
		LastErrorStatus = 0U;
		if (auto* error = std::get_if<github_mutation_wire_error>(&completion)) {
			LastErrorStatus = error->HttpStatus;
		}
		if (auto* success = std::get_if<github_mutation_wire_success>(&completion)) {
			return std::move(success->Response);
		}
		if (auto* error = std::get_if<github_mutation_wire_error>(&completion)) {
			return std::unexpected(error->Failure);
		}
		return std::unexpected(application::review_mutation_failure::cancelled);
	}

	auto review_mutation_session::observe_exact(const std::stop_token aStop, const bool aLeftLine)
		-> std::expected<void, application::review_mutation_failure>
	{
		const auto viewerRequest = compile_viewer_observation_request(
			Configuration.ActingIdentity,
			Configuration.CredentialContext.Authority.ServerContract);
		if (!viewerRequest.has_value()) {
			return std::unexpected(map_contract_failure(viewerRequest.error()));
		}
		auto viewerResponse = perform(*viewerRequest, aStop);
		if (!viewerResponse.has_value()) {
			return std::unexpected(viewerResponse.error());
		}
		const auto viewer = parse_viewer_observation(*viewerResponse, Configuration.ActingIdentity);
		if (!viewer.has_value()) {
			return std::unexpected(map_contract_failure(viewer.error()));
		}

		// Viewer identity is checked first so no other network operation widens
		// the target-revalidation window. Target/head is the last observation
		// before the POST. A LEFT line's merge-base reads follow it
		// ([[BPR-1572]]): one compare when the base branch names the comparison
		// base, or, when it names another, two more; on both paths the target
		// is then read again, last. A transport that checks the account with
		// reads of its own before the POST (the GitHub CLI account wrapper)
		// reads the target once more after them.
		const auto targetRequest = compile_target_observation_request(
			Configuration.Target,
			Configuration.CredentialContext.Authority.ServerContract);
		if (!targetRequest.has_value()) {
			return std::unexpected(map_contract_failure(targetRequest.error()));
		}
		auto targetResponse = perform(*targetRequest, aStop);
		if (!targetResponse.has_value()) {
			return std::unexpected(targetResponse.error());
		}
		const auto target = parse_target_observation(*targetResponse, Configuration.Target);
		if (!target.has_value()) {
			return std::unexpected(map_contract_failure(target.error()));
		}
		if (target->Target.Semantic.ExpectedHead
			!= Configuration.Target.Semantic.ExpectedHead) {
			return std::unexpected(application::review_mutation_failure::stale_head);
		}
		if (!aLeftLine) {
			return { };
		}
		// [[BPR-1560]], [[BPR-1572]]. A LEFT line is numbered in the merge
		// base GitHub compares the pull request from. The base branch's merge
		// base naming it settles it. Otherwise GitHub still numbers in it only
		// while it is the recorded base's merge base, GitHub's commit count is
		// that pair's, and the base branch only moved forward from the recorded
		// base (to the tip its answer resolved); the pull request is then read
		// again and must be unchanged, so a recorded base GitHub replaced while
		// these were asked is not taken for the one they describe.
		const auto observe = [&](const application::review_mutation_target_binding& aBinding, const merge_base_observation_basis aBasis)
			-> std::expected<merge_base_observation, application::review_mutation_failure> {
			const auto request = compile_merge_base_observation_request(aBinding, Configuration.CredentialContext.Authority.ServerContract, aBasis);
			if (!request.has_value()) {
				return std::unexpected(map_contract_failure(request.error()));
			}
			auto response = perform(*request, aStop);
			if (!response.has_value()) {
				return std::unexpected(response.error());
			}
			const auto observed = parse_merge_base_observation(*response, aBinding, aBasis);
			if (!observed.has_value()) {
				return std::unexpected(map_contract_failure(observed.error()));
			}
			return *observed;
		};
		const auto branch = observe(Configuration.Target, merge_base_observation_basis::base_branch);
		if (!branch.has_value()) {
			return std::unexpected(branch.error());
		}
		const auto agrees   = branch->MergeBase == application::comparison_base(Configuration.Target);
		auto       recorded = std::optional<merge_base_observation>{ };
		auto       ancestry = std::optional<merge_base_observation>{ };
		if (!agrees) {
			auto recordedAnswer = observe(Configuration.Target, merge_base_observation_basis::recorded_base);
			if (!recordedAnswer.has_value()) {
				return std::unexpected(recordedAnswer.error());
			}
			recorded            = *recordedAnswer;
			auto ancestryAnswer = observe(merge_base_ancestry_binding(Configuration.Target, branch->BaseCommit), merge_base_observation_basis::base_ancestry);
			if (!ancestryAnswer.has_value()) {
				return std::unexpected(ancestryAnswer.error());
			}
			ancestry = *ancestryAnswer;
		}
		// The pull request read again, the last read before the write, on
		// either path, and classified as the first read is; a recorded base
		// GitHub replaced under the merge-base reads is the target moving.
		auto again = perform(*targetRequest, aStop);
		if (!again.has_value()) {
			return std::unexpected(again.error());
		}
		const auto confirmed = parse_target_observation(*again, Configuration.Target);
		if (!confirmed.has_value()) {
			if (target_recorded_base_moved(*again, Configuration.Target)) {
				return std::unexpected(application::review_mutation_failure::target_moved);
			}
			return std::unexpected(map_contract_failure(confirmed.error()));
		}
		if (confirmed->Target.Semantic.ExpectedHead != Configuration.Target.Semantic.ExpectedHead) {
			return std::unexpected(application::review_mutation_failure::stale_head);
		}
		if (!agrees && (confirmed->Commits != target->Commits || !left_lines_numbered_in_comparison_base(Configuration.Target, *branch, recorded, ancestry, target->Commits))) {
			return std::unexpected(application::review_mutation_failure::target_moved);
		}
		return { };
	}

	auto review_mutation_session::observe_thread_target(
		const application::review_thread_resolution_request& aRequest,
		const std::stop_token                                aStop)
		-> std::expected<
			review_thread_target_observation,
			application::review_mutation_failure>
	{
		const auto wire = compile_thread_target_observation_request(
			aRequest, Configuration.CredentialContext.Authority.ServerContract);
		if (!wire.has_value()) {
			return std::unexpected(map_contract_failure(wire.error()));
		}
		auto response = perform(*wire, aStop, { });
		if (!response.has_value()) {
			return std::unexpected(response.error());
		}
		auto observed = parse_thread_target_observation(*response, aRequest);
		if (!observed.has_value()) {
			return std::unexpected(map_contract_failure(observed.error()));
		}
		return std::move(*observed);
	}

	auto review_mutation_session::reconcile(
		const application::review_mutation_request& aRequest,
		const std::stop_token                       aStop)
		-> std::expected<
			review_mutation_server_reconciliation,
			application::review_mutation_failure>
	{
		const auto wire = compile_review_comment_observation_request(
			aRequest,
			Configuration.CredentialContext.Authority.ServerContract);
		if (!wire.has_value()) {
			return std::unexpected(map_contract_failure(wire.error()));
		}
		auto response = perform(*wire, aStop);
		if (!response.has_value()) {
			return std::unexpected(response.error());
		}
		auto parsed = parse_review_comment_observations(*response, aRequest);
		if (!parsed.has_value()) {
			return std::unexpected(map_contract_failure(parsed.error()));
		}
		return std::move(*parsed);
	}

	auto review_mutation_session::observe_target(const std::stop_token aStop)
		-> std::expected<core::review_mutation_target, application::review_mutation_failure>
	{
		try {
			std::scoped_lock                lock{ Mutex };
			github_mutation_operation_scope operation{ Transport, aStop };
			if (!operation.valid()) {
				return std::unexpected(operation.failure());
			}
			const auto targetRequest = compile_target_observation_request(
				Configuration.Target,
				Configuration.CredentialContext.Authority.ServerContract);
			if (!targetRequest.has_value()) {
				return std::unexpected(map_contract_failure(targetRequest.error()));
			}
			auto targetResponse = perform(*targetRequest, aStop);
			if (!targetResponse.has_value()) {
				return std::unexpected(targetResponse.error());
			}
			const auto target = parse_target_observation(*targetResponse, Configuration.Target);
			if (!target.has_value()) {
				return std::unexpected(map_contract_failure(target.error()));
			}
			const auto viewerRequest = compile_viewer_observation_request(
				Configuration.ActingIdentity,
				Configuration.CredentialContext.Authority.ServerContract);
			if (!viewerRequest.has_value()) {
				return std::unexpected(map_contract_failure(viewerRequest.error()));
			}
			auto viewerResponse = perform(*viewerRequest, aStop);
			if (!viewerResponse.has_value()) {
				return std::unexpected(viewerResponse.error());
			}
			const auto viewer = parse_viewer_observation(*viewerResponse, Configuration.ActingIdentity);
			if (!viewer.has_value()) {
				return std::unexpected(map_contract_failure(viewer.error()));
			}
			return target->Target.Semantic;
		} catch (...) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
	}

	auto review_mutation_session::submit(
		const application::review_mutation_request& aRequest,
		const std::stop_token                       aStop)
		-> std::expected<application::review_mutation_receipt, application::review_mutation_failure>
	{
		return submit_and_reconcile(aRequest, aStop).Result;
	}

	auto review_mutation_session::resolve_thread(
		const application::review_thread_resolution_request& aRequest,
		const std::stop_token                                aStop)
		-> std::expected<application::review_thread_resolution_receipt, application::review_mutation_failure>
	{
		try {
			std::scoped_lock                lock{ Mutex };
			github_mutation_operation_scope operation{ Transport, aStop };
			if (!operation.valid()) {
				return std::unexpected(operation.failure());
			}
			if (aStop.stop_requested()) {
				return std::unexpected(application::review_mutation_failure::cancelled);
			}
			if (Lease == nullptr || !Lease->active()) {
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			// [[BPR-1826]]. On a closed or merged pull request too.
			if (!valid_identity(Configuration) || !application::valid_for(Configuration.Target, core::pull_request_change::thread_resolution)
				|| !aRequest.LiveBinding.has_value()
				|| aRequest.LiveBinding->Target != Configuration.Target
				|| aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity) {
				return std::unexpected(application::review_mutation_failure::undesignated_target);
			}
			const auto bound = observe_thread_target(aRequest, aStop);
			if (!bound.has_value()) {
				return std::unexpected(bound.error());
			}
			// [[BPR-1741]]. A thread GitHub already shows in the state
			// asked is not written again.
			if (bound->Resolved == aRequest.Resolve) {
				return application::review_thread_resolution_receipt{ .Thread = bound->Thread, .Resolved = bound->Resolved };
			}
			const auto wire = compile_thread_resolution_request(
				aRequest, Configuration.CredentialContext.Authority.ServerContract);
			if (!wire.has_value()) {
				return std::unexpected(map_contract_failure(wire.error()));
			}
			auto       failure  = application::review_mutation_failure::unavailable;
			const auto response = perform(*wire, aStop, { });
			if (response.has_value()) {
				const auto observed = parse_thread_resolution_response(*response, aRequest);
				if (observed.has_value()) {
					return observed->Receipt;
				}
				if (observed.error() == review_mutation_contract_failure::identity_mismatch) {
					return std::unexpected(map_contract_failure(observed.error()));
				}
				failure = map_contract_failure(observed.error());
			} else {
				failure = response.error();
			}
			if (failure == application::review_mutation_failure::cancelled || aStop.stop_requested()) {
				return std::unexpected(application::review_mutation_failure::cancelled);
			}
			// [[BPR-1741]]. An ambiguous outcome is confirmed by one read
			// of the thread's state, never by resolving it again.
			const auto after = observe_thread_target(aRequest, aStop);
			if (after.has_value() && after->Resolved == aRequest.Resolve) {
				return application::review_thread_resolution_receipt{ .Thread = after->Thread, .Resolved = after->Resolved };
			}
			return std::unexpected(after.has_value() ? application::review_mutation_failure::not_confirmed : failure);

		} catch (const std::bad_alloc&) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
	}

	auto review_mutation_session::observe_reaction_subject(
		const application::review_reaction_request& aRequest,
		const std::stop_token                       aStop)
		-> std::expected<review_reaction_subject_observation, application::review_mutation_failure>
	{
		const auto wire = compile_reaction_subject_observation_request(aRequest, Configuration.CredentialContext.Authority.ServerContract);
		if (!wire.has_value()) {
			return std::unexpected(map_contract_failure(wire.error()));
		}
		auto response = perform(*wire, aStop, { });
		if (!response.has_value()) {
			return std::unexpected(response.error());
		}
		auto observed = parse_reaction_subject_observation(*response, aRequest);
		if (!observed.has_value()) {
			return std::unexpected(map_contract_failure(observed.error()));
		}
		return std::move(*observed);
	}
}
