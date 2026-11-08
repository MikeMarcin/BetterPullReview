#include "bpr/application/review_mutation_coordinator.hpp"
#include "bpr/core/comment_edit.hpp"
#include "bpr/core/reviewer_request.hpp"
#include "github_http_helpers.hpp"
#include "github_json_preflight.hpp"
#include "github_mutation_contract.hpp"
#include "github_mutation_contract_support.hpp"
#include "github_paged_read.hpp"
#include "review_comment_permalink.hpp"

#include <QByteArray>
#include <QString>

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <expected>
#include <glaze/json.hpp>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail {
	namespace {
		/// [[BPR-1804]]. The people who may review the pull request named by
		/// its node, as GitHub's picker lists them: its suggestions, then the
		/// repository's assignable users matching the query; with the author,
		/// the open requests and the submitted reviews that say where each
		/// stands. Users only: teams are not asked for here.
		constexpr auto observe_reviewers_document = std::string_view{
			"query ObserveReviewers($pullRequest:ID!,$query:String!){node(id:$pullRequest){... on PullRequest{"
			"id databaseId number author{login} repository{id databaseId nameWithOwner assignableUsers(query:$query,first:20){nodes{id login name}}} "
			"suggestedReviewers{isAuthor isCommenter reviewer{id login name}} "
			"reviewRequests(first:100){nodes{requestedReviewer{__typename ... on User{id login}}}} "
			"latestReviews(first:100){nodes{author{login}}}}}}"
		};

		/// [[BPR-1804]]. One review requested, added to the open requests
		/// (`union:true`). GitHub answers success without adding someone who
		/// may not review, so the answer's requests are what is believed.
		constexpr auto request_reviews_document = std::string_view{
			"mutation RequestReviews($pullRequest:ID!,$user:ID!){requestReviews(input:{pullRequestId:$pullRequest,userIds:[$user],union:true}){"
			"pullRequest{id databaseId number author{login} repository{id databaseId nameWithOwner} "
			"reviewRequests(first:100){nodes{requestedReviewer{__typename ... on User{id login}}}} "
			"latestReviews(first:100){nodes{author{login}}}}}}"
		};

		/// [[BPR-1852]]. The pull request a draft-state change reaches, named
		/// by its node as the accepted read gave it, with the identities that
		/// bind it to the designated pull request, its draft state, whether it
		/// is open, and GitHub's word on whether this viewer may update it.
		/// The two writes answer with the same selection.
		constexpr auto observe_draft_state_document = std::string_view{
			"query ObserveDraftState($pullRequest:ID!){node(id:$pullRequest){... on PullRequest{"
			"id databaseId number isDraft state viewerCanUpdate repository{id databaseId nameWithOwner}}}}"
		};

		constexpr auto mark_ready_for_review_document = std::string_view{
			"mutation MarkReadyForReview($pullRequest:ID!){markPullRequestReadyForReview(input:{pullRequestId:$pullRequest}){"
			"pullRequest{id databaseId number isDraft state viewerCanUpdate repository{id databaseId nameWithOwner}}}}"
		};

		constexpr auto convert_to_draft_document = std::string_view{
			"mutation ConvertToDraft($pullRequest:ID!){convertPullRequestToDraft(input:{pullRequestId:$pullRequest}){"
			"pullRequest{id databaseId number isDraft state viewerCanUpdate repository{id databaseId nameWithOwner}}}}"
		};

		/// [[BPR-1775]]. One of the viewer's published comments, named by its
		/// node, of one of the three kinds that can be edited, with its body,
		/// GitHub's word on whether this viewer may update it, and the pull
		/// request it belongs to (as a reaction subject is, [[BPR-1741]]).
		constexpr auto observe_comment_document = std::string_view{
			"query ObserveComment($subject:ID!){node(id:$subject){__typename id "
			"... on PullRequestReviewComment{body viewerCanUpdate viewerDidAuthor updatedAt pullRequest{id databaseId number repository{id databaseId nameWithOwner}}} "
			"... on IssueComment{body viewerCanUpdate viewerDidAuthor updatedAt pullRequest{id databaseId number repository{id databaseId nameWithOwner}}} "
			"... on PullRequestReview{body viewerCanUpdate viewerDidAuthor updatedAt pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}"
			"}}"
		};

		/// [[BPR-1775]]. The update each kind takes. A published review
		/// comment takes the mutation a pending one does ([[BPR-1368]]), under
		/// its own document name and answer.
		constexpr auto edit_review_comment_document = std::string_view{
			"mutation EditReviewComment($subject:ID!,$body:String!){updatePullRequestReviewComment(input:{pullRequestReviewCommentId:$subject,body:$body}){"
			"pullRequestReviewComment{__typename id body viewerCanUpdate viewerDidAuthor updatedAt pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}}}"
		};

		constexpr auto edit_issue_comment_document = std::string_view{
			"mutation EditIssueComment($subject:ID!,$body:String!){updateIssueComment(input:{id:$subject,body:$body}){"
			"issueComment{__typename id body viewerCanUpdate viewerDidAuthor updatedAt pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}}}"
		};

		constexpr auto edit_review_body_document = std::string_view{
			"mutation EditReviewBody($subject:ID!,$body:String!){updatePullRequestReview(input:{pullRequestReviewId:$subject,body:$body}){"
			"pullRequestReview{__typename id body viewerCanUpdate viewerDidAuthor updatedAt pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}}}"
		};

		[[nodiscard]] constexpr auto comment_edit_document(const core::comment_edit_kind aKind) noexcept -> std::string_view
		{
			switch (aKind) {
			case core::comment_edit_kind::review_comment:       return edit_review_comment_document;
			case core::comment_edit_kind::pull_request_comment: return edit_issue_comment_document;
			case core::comment_edit_kind::review_body:          return edit_review_body_document;
			}
			return { };
		}

		[[nodiscard]] constexpr auto comment_edit_operation(const core::comment_edit_kind aKind) noexcept -> review_mutation_operation
		{
			switch (aKind) {
			case core::comment_edit_kind::review_comment:       return review_mutation_operation::edit_review_comment;
			case core::comment_edit_kind::pull_request_comment: return review_mutation_operation::edit_issue_comment;
			case core::comment_edit_kind::review_body:          return review_mutation_operation::edit_review_body;
			}
			return review_mutation_operation::edit_review_comment;
		}

		/// [[BPR-0896]]. The read that reconciles a reply. A reply is written
		/// by GraphQL, so its own path is the GraphQL root and no REST
		/// collection can be derived from it; what it can be reconciled
		/// against is the thread it landed in. The side lives on the thread,
		/// the database identity is a string-encoded `BigInt`, and the
		/// author's identities exist only on the `User` fragment.
		constexpr auto observe_thread_comments_document = std::string_view{
			"query ObserveThreadComments($thread:ID!){node(id:$thread){... on PullRequestReviewThread{"
			"id diffSide pullRequest{id databaseId number repository{id databaseId nameWithOwner}} "
			"comments(first:100){pageInfo{hasNextPage} nodes{id fullDatabaseId body path line commit{oid} "
			"viewerDidAuthor author{login ... on User{databaseId id}}}}}}}"
		};

		/// [[BPR-1345]]. One line comment a review carries: GitHub's `path`,
		/// `line` and `side`, and for a range `start_line` and `start_side`,
		/// in sorted key order. Absent members are skipped.
		struct review_submission_comment_json
		{
			std::string_view                Body;
			std::uint64_t                   Line = 0U;
			std::string_view                Path;
			std::string_view                Side;
			std::optional<std::uint64_t>    StartLine;
			std::optional<std::string_view> StartSide;

			struct glaze
			{
				static constexpr auto value = glz::object(
					"body", &review_submission_comment_json::Body,
					"line", &review_submission_comment_json::Line,
					"path", &review_submission_comment_json::Path,
					"side", &review_submission_comment_json::Side,
					"start_line", &review_submission_comment_json::StartLine,
					"start_side", &review_submission_comment_json::StartSide);
			};
		};

		struct review_submission_json
		{
			std::string_view Body;
			/// [[BPR-1345]]. Present only when the review carries line
			/// comments, so a summary-only review's bytes are unchanged.
			std::optional<std::vector<review_submission_comment_json>> Comments;
			std::string_view                                           CommitId;
			std::string_view                                           Event;

			struct glaze
			{
				static constexpr auto value = glz::object(
					"body", &review_submission_json::Body,
					"comments", &review_submission_json::Comments,
					"commit_id", &review_submission_json::CommitId,
					"event", &review_submission_json::Event);
			};
		};

		struct recovery_repository_json
		{
			std::uint64_t Id = 0U;
			std::string   NodeId;
			std::string   FullName;
			struct glaze
			{
				static constexpr auto value = glz::object("id", &recovery_repository_json::Id, "node_id", &recovery_repository_json::NodeId, "full_name", &recovery_repository_json::FullName);
			};
		};

		struct recovery_base_json
		{
			recovery_repository_json Repository;
			struct glaze
			{
				static constexpr auto value = glz::object("repo", &recovery_base_json::Repository);
			};
		};

		struct recovery_target_json
		{
			std::uint64_t      Id = 0U;
			std::string        NodeId;
			std::uint64_t      Number = 0U;
			recovery_base_json Base;
			struct glaze
			{
				static constexpr auto value = glz::object("id", &recovery_target_json::Id, "node_id", &recovery_target_json::NodeId, "number", &recovery_target_json::Number, "base", &recovery_target_json::Base);
			};
		};

		struct unsupported_error_json
		{
			std::string Resource, Field, Code, Message;
			struct glaze
			{
				static constexpr auto value = glz::object("resource", &unsupported_error_json::Resource, "field", &unsupported_error_json::Field, "code", &unsupported_error_json::Code, "message", &unsupported_error_json::Message);
			};
		};

		struct unsupported_response_json
		{
			std::string                         Message;
			std::vector<unsupported_error_json> Errors;
			struct glaze
			{
				static constexpr auto value = glz::object("message", &unsupported_response_json::Message, "errors", &unsupported_response_json::Errors);
			};
		};

		struct recorded_unsupported_response_json
		{
			std::string                         Message, DocumentationUrl, Status;
			std::vector<unsupported_error_json> Errors;
			struct glaze
			{
				static constexpr auto value = glz::object("message", &recorded_unsupported_response_json::Message, "documentation_url", &recorded_unsupported_response_json::DocumentationUrl, "status", &recorded_unsupported_response_json::Status, "errors", &recorded_unsupported_response_json::Errors);
			};
		};

		/// \brief [[BPR-1736]] (D4). GitHub.com's refusal, its documentation URL
		/// and status optional and unconstrained.
		struct general_unsupported_response_json
		{
			std::string                         Message;
			std::optional<std::string>          DocumentationUrl;
			std::optional<std::string>          Status;
			std::vector<unsupported_error_json> Errors;
			struct glaze
			{
				static constexpr auto value = glz::object("message", &general_unsupported_response_json::Message, "documentation_url", &general_unsupported_response_json::DocumentationUrl,
														  "status", &general_unsupported_response_json::Status, "errors", &general_unsupported_response_json::Errors);
			};
		};

		// [[BPR-1775]]. A comment edit's envelope, outbound. Its observation
		// reuses the reaction subject's (`subject` alone).
		struct comment_edit_variables_json
		{
			std::string_view Subject;
			std::string_view Body;
			struct glaze
			{
				static constexpr auto value = glz::object("subject", &comment_edit_variables_json::Subject, "body", &comment_edit_variables_json::Body);
			};
		};

		struct comment_edit_query_json
		{
			std::string_view            Query;
			comment_edit_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object("query", &comment_edit_query_json::Query, "variables", &comment_edit_query_json::Variables);
			};
		};

		struct reply_author_json
		{
			std::string Login;
			struct glaze
			{
				static constexpr auto value = glz::object("login", &reply_author_json::Login);
			};
		};

		struct reply_comment_json
		{
			std::string   Id;
			std::uint64_t DatabaseId = 0U;
			std::string   Body;
			std::string   Path;
			// [[BPR-1745]]. Null for a reply to an outdated thread.
			std::optional<std::uint64_t> Line;
			reply_commit_json            Commit;
			reply_author_json            Author;
			bool                         ViewerDidAuthor = false;
			thread_pull_request_json     PullRequest;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &reply_comment_json::Id,
					"databaseId", &reply_comment_json::DatabaseId,
					"body", &reply_comment_json::Body,
					"path", &reply_comment_json::Path,
					"line", &reply_comment_json::Line,
					"commit", &reply_comment_json::Commit,
					"author", &reply_comment_json::Author,
					"viewerDidAuthor", &reply_comment_json::ViewerDidAuthor,
					"pullRequest", &reply_comment_json::PullRequest);
			};
		};

		struct reply_payload_json
		{
			reply_comment_json Comment;
			struct glaze
			{
				static constexpr auto value = glz::object("comment", &reply_payload_json::Comment);
			};
		};

		struct reply_data_json
		{
			reply_payload_json Reply;
			struct glaze
			{
				static constexpr auto value = glz::object("addPullRequestReviewThreadReply", &reply_data_json::Reply);
			};
		};

		struct reply_response_json
		{
			reply_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &reply_response_json::Data);
			};
		};

		struct resolution_thread_json
		{
			std::string              Id;
			bool                     IsResolved = false;
			thread_pull_request_json PullRequest;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &resolution_thread_json::Id,
					"isResolved", &resolution_thread_json::IsResolved,
					"pullRequest", &resolution_thread_json::PullRequest);
			};
		};

		// [[BPR-0879]]. The pre-flight answer: `node(id:)` narrowed to a review
		// thread carries exactly the fields a resolution answer does.
		struct thread_node_data_json
		{
			std::variant<resolution_thread_json, std::nullptr_t> Node;
			struct glaze
			{
				static constexpr auto value = glz::object("node", &thread_node_data_json::Node);
			};
		};

		struct thread_node_response_json
		{
			thread_node_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &thread_node_response_json::Data);
			};
		};

		// [[BPR-1741]]. A reaction subject as an answer names it. The
		// groups are kept raw and read by the same reader `ListReactions`
		// uses, so a reaction answer is held to one rule.
		struct reaction_subject_json
		{
			std::string              Typename;
			std::string              Id;
			bool                     ViewerCanReact = false;
			glz::raw_json            ReactionGroups;
			thread_pull_request_json PullRequest;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"__typename", &reaction_subject_json::Typename,
					"id", &reaction_subject_json::Id,
					"viewerCanReact", &reaction_subject_json::ViewerCanReact,
					"reactionGroups", &reaction_subject_json::ReactionGroups,
					"pullRequest", &reaction_subject_json::PullRequest);
			};
		};

		struct reaction_node_data_json
		{
			std::variant<reaction_subject_json, std::nullptr_t> Node;
			struct glaze
			{
				static constexpr auto value = glz::object("node", &reaction_node_data_json::Node);
			};
		};

		struct reaction_node_response_json
		{
			reaction_node_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &reaction_node_response_json::Data);
			};
		};

		struct reaction_content_json
		{
			std::string Content;
			struct glaze
			{
				static constexpr auto value = glz::object("content", &reaction_content_json::Content);
			};
		};

		struct reaction_payload_json
		{
			reaction_content_json Reaction;
			reaction_subject_json Subject;
			struct glaze
			{
				static constexpr auto value = glz::object("reaction", &reaction_payload_json::Reaction, "subject", &reaction_payload_json::Subject);
			};
		};

		struct add_reaction_data_json
		{
			reaction_payload_json Add;
			struct glaze
			{
				static constexpr auto value = glz::object("addReaction", &add_reaction_data_json::Add);
			};
		};

		struct add_reaction_response_json
		{
			add_reaction_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &add_reaction_response_json::Data);
			};
		};

		struct remove_reaction_data_json
		{
			reaction_payload_json Remove;
			struct glaze
			{
				static constexpr auto value = glz::object("removeReaction", &remove_reaction_data_json::Remove);
			};
		};

		struct remove_reaction_response_json
		{
			remove_reaction_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &remove_reaction_response_json::Data);
			};
		};

		// [[BPR-1774]]. The pull request as a description answer names it.
		struct description_pull_request_json
		{
			std::string            Id;
			std::uint64_t          DatabaseId = 0U;
			std::uint64_t          Number     = 0U;
			std::string            Body;
			bool                   ViewerCanUpdate = false;
			std::string            UpdatedAt;
			thread_repository_json Repository;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &description_pull_request_json::Id,
					"databaseId", &description_pull_request_json::DatabaseId,
					"number", &description_pull_request_json::Number,
					"body", &description_pull_request_json::Body,
					"viewerCanUpdate", &description_pull_request_json::ViewerCanUpdate,
					"updatedAt", &description_pull_request_json::UpdatedAt,
					"repository", &description_pull_request_json::Repository);
			};
		};

		struct description_node_data_json
		{
			std::variant<description_pull_request_json, std::nullptr_t> Node;
			struct glaze
			{
				static constexpr auto value = glz::object("node", &description_node_data_json::Node);
			};
		};

		struct description_node_response_json
		{
			description_node_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &description_node_response_json::Data);
			};
		};

		struct description_payload_json
		{
			description_pull_request_json PullRequest;
			struct glaze
			{
				static constexpr auto value = glz::object("pullRequest", &description_payload_json::PullRequest);
			};
		};

		struct description_update_data_json
		{
			description_payload_json Update;
			struct glaze
			{
				static constexpr auto value = glz::object("updatePullRequest", &description_update_data_json::Update);
			};
		};

		struct description_update_response_json
		{
			description_update_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &description_update_response_json::Data);
			};
		};

		// [[BPR-1804]]. Reviewer envelopes, outbound.
		struct reviewer_observation_variables_json
		{
			std::string_view PullRequest;
			std::string_view Query;
			struct glaze
			{
				static constexpr auto value = glz::object("pullRequest", &reviewer_observation_variables_json::PullRequest, "query", &reviewer_observation_variables_json::Query);
			};
		};

		struct reviewer_observation_query_json
		{
			std::string_view                    Query;
			reviewer_observation_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object("query", &reviewer_observation_query_json::Query, "variables", &reviewer_observation_query_json::Variables);
			};
		};

		struct reviewer_write_variables_json
		{
			std::string_view PullRequest;
			std::string_view User;
			struct glaze
			{
				static constexpr auto value = glz::object("pullRequest", &reviewer_write_variables_json::PullRequest, "user", &reviewer_write_variables_json::User);
			};
		};

		struct reviewer_write_query_json
		{
			std::string_view              Query;
			reviewer_write_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object("query", &reviewer_write_query_json::Query, "variables", &reviewer_write_query_json::Variables);
			};
		};

		// [[BPR-1804]]. Reviewer answers, inbound. A name, an author and a
		// requested reviewer may be null; a team request carries no user
		// fields.
		struct reviewer_user_json
		{
			std::string                Id;
			std::string                Login;
			std::optional<std::string> Name;
			struct glaze
			{
				static constexpr auto value = glz::object("id", &reviewer_user_json::Id, "login", &reviewer_user_json::Login, "name", &reviewer_user_json::Name);
			};
		};

		struct reviewer_user_nodes_json
		{
			std::vector<reviewer_user_json> Nodes;
			struct glaze
			{
				static constexpr auto value = glz::object("nodes", &reviewer_user_nodes_json::Nodes);
			};
		};

		struct reviewer_suggestion_json
		{
			bool                              IsAuthor    = false;
			bool                              IsCommenter = false;
			std::optional<reviewer_user_json> Reviewer;
			struct glaze
			{
				static constexpr auto value = glz::object("isAuthor", &reviewer_suggestion_json::IsAuthor, "isCommenter", &reviewer_suggestion_json::IsCommenter, "reviewer", &reviewer_suggestion_json::Reviewer);
			};
		};

		struct reviewer_repository_json
		{
			std::string              Id;
			std::uint64_t            DatabaseId = 0U;
			std::string              NameWithOwner;
			reviewer_user_nodes_json AssignableUsers;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &reviewer_repository_json::Id,
					"databaseId", &reviewer_repository_json::DatabaseId,
					"nameWithOwner", &reviewer_repository_json::NameWithOwner,
					"assignableUsers", &reviewer_repository_json::AssignableUsers);
			};
		};

		/// The pull request as `ObserveReviewers` names it.
		struct reviewer_pull_request_json
		{
			std::string                           Id;
			std::uint64_t                         DatabaseId = 0U;
			std::uint64_t                         Number     = 0U;
			std::optional<reviewer_login_json>    Author;
			reviewer_repository_json              Repository;
			std::vector<reviewer_suggestion_json> SuggestedReviewers;
			reviewer_request_nodes_json           ReviewRequests;
			reviewer_review_nodes_json            LatestReviews;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &reviewer_pull_request_json::Id,
					"databaseId", &reviewer_pull_request_json::DatabaseId,
					"number", &reviewer_pull_request_json::Number,
					"author", &reviewer_pull_request_json::Author,
					"repository", &reviewer_pull_request_json::Repository,
					"suggestedReviewers", &reviewer_pull_request_json::SuggestedReviewers,
					"reviewRequests", &reviewer_pull_request_json::ReviewRequests,
					"latestReviews", &reviewer_pull_request_json::LatestReviews);
			};
		};

		struct reviewer_node_data_json
		{
			std::variant<reviewer_pull_request_json, std::nullptr_t> Node;
			struct glaze
			{
				static constexpr auto value = glz::object("node", &reviewer_node_data_json::Node);
			};
		};

		struct reviewer_node_response_json
		{
			reviewer_node_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &reviewer_node_response_json::Data);
			};
		};

		// [[BPR-1852]]. A draft-state change's envelope, outbound; the read
		// and both writes take the pull request's node alone.
		struct draft_state_variables_json
		{
			std::string_view PullRequest;
			struct glaze
			{
				static constexpr auto value = glz::object("pullRequest", &draft_state_variables_json::PullRequest);
			};
		};

		struct draft_state_query_json
		{
			std::string_view           Query;
			draft_state_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object("query", &draft_state_query_json::Query, "variables", &draft_state_query_json::Variables);
			};
		};

		/// \brief [[BPR-1804]]. One person as GitHub listed them, or none
		/// when the entry is not a person who may be asked.
		[[nodiscard]] auto reviewer_person(const reviewer_user_json& aUser) -> std::optional<core::reviewer_candidate>
		{
			const auto name = aUser.Name.value_or(std::string{ });
			if (!reviewer_login(aUser.Login) || aUser.Id.empty() || aUser.Id.size() > 256U || name.size() > 512U) {
				return std::nullopt;
			}
			return core::reviewer_candidate{ .NodeId = aUser.Id, .Login = aUser.Login, .Name = name };
		}

		/// [[BPR-1774]]. The description, held to the designated pull request
		/// by every identity the target binds.
		[[nodiscard]] auto description_observation(
			const description_pull_request_json&           aPullRequest,
			const application::review_description_request& aExpected)
			-> review_mutation_parse_result<review_description_observation>
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
			return review_description_observation{ .Body = aPullRequest.Body, .UpdatedAt = aPullRequest.UpdatedAt, .ViewerCanUpdate = aPullRequest.ViewerCanUpdate };
		}

		/// [[BPR-1741]]. The subject's reactions, held to the
		/// `ListReactions` reader: a subject of one of the three kinds, the
		/// identity asked, in the designated pull request.
		[[nodiscard]] auto reaction_subject_observation(
			const reaction_subject_json&                aSubject,
			const application::review_reaction_request& aExpected)
			-> review_mutation_parse_result<review_reaction_subject_observation>
		{
			if ((aSubject.Typename != "PullRequestReviewComment" && aSubject.Typename != "IssueComment" && aSubject.Typename != "PullRequestReview")
				|| aSubject.Id != aExpected.Subject || !aExpected.LiveBinding.has_value()
				|| !binds_designated_pull_request(aSubject.PullRequest, aExpected.LiveBinding->Target)) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			auto object = std::string{ R"({"viewerCanReact":)" };
			object += aSubject.ViewerCanReact ? "true" : "false";
			object += R"(,"reactionGroups":)";
			object += aSubject.ReactionGroups.str.empty() ? std::string_view{ "null" } : std::string_view{ aSubject.ReactionGroups.str };
			object += "}";
			auto summary = parse_reaction_summary(object);
			if (!summary.has_value()) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			return review_reaction_subject_observation{ .Subject = aSubject.Id, .Reactions = std::move(*summary) };
		}

		struct resolution_payload_json
		{
			resolution_thread_json Thread;
			struct glaze
			{
				static constexpr auto value = glz::object("thread", &resolution_payload_json::Thread);
			};
		};

		struct resolve_data_json
		{
			resolution_payload_json Resolve;
			struct glaze
			{
				static constexpr auto value = glz::object("resolveReviewThread", &resolve_data_json::Resolve);
			};
		};

		struct unresolve_data_json
		{
			resolution_payload_json Unresolve;
			struct glaze
			{
				static constexpr auto value = glz::object("unresolveReviewThread", &unresolve_data_json::Unresolve);
			};
		};

		struct resolve_response_json
		{
			resolve_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &resolve_response_json::Data);
			};
		};

		struct unresolve_response_json
		{
			unresolve_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &unresolve_response_json::Data);
			};
		};

		struct target_head_json
		{
			std::string Sha;
			std::string Ref;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"sha", &target_head_json::Sha,
					"ref", &target_head_json::Ref);
			};
		};

		struct target_base_json
		{
			std::string              Sha;
			std::string              Ref;
			recovery_repository_json Repository;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"sha", &target_base_json::Sha,
					"ref", &target_base_json::Ref,
					"repo", &target_base_json::Repository);
			};
		};

		struct target_response_json
		{
			std::uint64_t    Id = 0U;
			std::string      NodeId;
			std::uint64_t    Number = 0U;
			std::string      State;
			target_head_json Head;
			target_base_json Base;
			/// [[BPR-1572]]. The commit count GitHub reports, read for a LEFT
			/// line's stacked-base rule.
			std::optional<std::uint64_t> Commits;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &target_response_json::Id,
					"node_id", &target_response_json::NodeId,
					"number", &target_response_json::Number,
					"state", &target_response_json::State,
					"head", &target_response_json::Head,
					"base", &target_response_json::Base,
					"commits", &target_response_json::Commits);
			};
		};

		[[nodiscard]] auto valid_submission(const review_submission_request& aRequest)
			-> bool
		{
			if (!application::valid(aRequest.Target)
				|| !application::valid(aRequest.ActingIdentity)
				|| !submission_names(aRequest.Disposition).has_value()
				|| aRequest.Body.size() > maximum_review_mutation_body_bytes) {
				return false;
			}
			const auto texts = std::array<std::string_view, 7>{
				aRequest.Body, aRequest.Target.RepositoryNodeId,
				aRequest.Target.PullRequestNodeId, aRequest.Target.BaseRef,
				aRequest.Target.HeadRef, aRequest.ActingIdentity.NodeId,
				aRequest.ActingIdentity.Login
			};
			// [[BPR-1345]]. Comment needs no summary when the review carries
			// line comments; each of them needs words, and every path is text.
			return std::ranges::all_of(texts, utf8_round_trip)
				&& core::review_comments_rejection(aRequest.Body, aRequest.Comments, true) == core::review_submission_rejection::none
				&& std::ranges::all_of(aRequest.Comments, [](const core::review_submission_comment& aComment) { return utf8_round_trip(aComment.Path) && utf8_round_trip(aComment.Body); })
				&& (!core::summary_required(aRequest.Disposition, aRequest.Comments)
					|| !QString::fromUtf8(aRequest.Body.data(), static_cast<qsizetype>(aRequest.Body.size())).trimmed().isEmpty());
		}

		// [[BPR-0657]]. A reply answered by GraphQL: the same identity checks a
		// REST comment gets, over the fields the mutation returns.
		[[nodiscard]] auto reply_observation(const reply_comment_json&                   aComment,
											 const application::review_mutation_request& aExpected)
			-> review_mutation_parse_result<review_mutation_comment_observation>
		{
			const auto     commit            = core::parse_git_commit_oid(aComment.Commit.Oid);
			constexpr auto maximumGithubLine = static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1U;
			const auto&    binding           = *aExpected.LiveBinding;
			// [[BPR-1745]]. A reply bound to its outdated thread is
			// answered with no line, on the commit the thread was written on
			// (live on BPR-Test#133, 2026-10-22). A line in that answer is not
			// the reply asked for; neither is a missing one for a reply on a
			// line.
			const auto threadBound = application::thread_bound_binding(binding.Anchor);
			if (aComment.Id.empty() || aComment.DatabaseId == 0U || !commit
				|| (aComment.Line.has_value() && (*aComment.Line == 0U || *aComment.Line > maximumGithubLine))) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			if (threadBound ? aComment.Line.has_value() : !aComment.Line.has_value()) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			const auto semanticLine = threadBound ? binding.Anchor.Semantic.Line : static_cast<std::uint32_t>(*aComment.Line - 1U);
			// [[BPR-0879]]. The thread identity was free; the pull request the
			// server says it answered is not.
			if (aComment.Path != binding.Anchor.Path || *commit != binding.Anchor.Commit
				|| semanticLine != binding.Anchor.Semantic.Line
				|| aComment.Body != aExpected.Proposal.Body || !aComment.ViewerDidAuthor
				|| aComment.Author.Login != binding.ActingIdentity.Login
				|| !binds_designated_pull_request(aComment.PullRequest, binding.Target)) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			// [[BPR-0816]]. Name the nested type, as in comment_observation.
			return review_mutation_comment_observation{
				.Receipt    = application::review_mutation_receipt{ .ServerCommentId = aComment.Id, .Duplicate = false },
				.DatabaseId = aComment.DatabaseId,
				.NodeId     = aComment.Id,
				.Anchor     = binding.Anchor,
				.Body       = aComment.Body,
				.Author     = binding.ActingIdentity,
			};
		}

		/// [[BPR-1367]]. The page a collection request names, or zero; the
		/// canonical request compiled for it must then match it exactly.
		[[nodiscard]] auto collection_page(const std::string_view aPath) -> std::uint32_t
		{
			const auto at = aPath.rfind("&page=");
			if (at == std::string_view::npos) {
				return 0U;
			}
			const auto digits = aPath.substr(at + 6U);
			auto       page   = std::uint32_t{ 0U };
			const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), page);
			return parsed.ec == std::errc{ } && parsed.ptr == digits.data() + digits.size() ? page : 0U;
		}

		/// \brief [[BPR-1852]]. One draft-state document for the designated
		/// pull request, bound as a description edit is.
		[[nodiscard]] auto compile_draft_state_document(
			const application::review_draft_state_request& aRequest,
			const application::github_server_contract      aServer,
			const review_mutation_operation                aOperation,
			const std::string_view                         aDocument)
			-> review_mutation_request_result
		{
			if (!valid_server(aServer) || !aRequest.LiveBinding.has_value()
				|| !application::valid(aRequest.LiveBinding->Target)
				|| !application::valid(aRequest.LiveBinding->ActingIdentity)
				|| aRequest.LiveBinding->Target.ServerContract != aServer
				|| aRequest.Target != aRequest.LiveBinding->Target.Semantic
				|| aRequest.ActingIdentity != aRequest.LiveBinding->ActingIdentity.Account
				|| !graphql_token(aRequest.LiveBinding->Target.PullRequestNodeId)
				|| (aRequest.Change != core::draft_state_change::ready_for_review && aRequest.Change != core::draft_state_change::convert_to_draft)) {
				return failed(review_mutation_contract_failure::invalid_request);
			}
			auto headers = common_headers();
			headers.push_back({ .Name = "Content-Type", .Value = "application/json" });
			auto body = glz::write<review_json_options{ }>(draft_state_query_json{
				.Query     = aDocument,
				.Variables = { .PullRequest = aRequest.LiveBinding->Target.PullRequestNodeId },
			});
			if (!body.has_value() || body->empty() || body->size() > maximum_review_mutation_body_bytes) {
				return failed(review_mutation_contract_failure::invalid_request);
			}
			return review_mutation_wire_request{
				.Semantic  = aRequest,
				.Server    = aServer,
				.Operation = aOperation,
				.Method    = http_method::post,
				.Path      = std::string{ graphql_path(aServer) },
				.Headers   = std::move(headers),
				.Body      = std::move(*body),
			};
		}

		// Owner decision de5b5b6d, mandate r3. This evidence describes one
		// immutable PR pair, not a service-wide interpretation of the message.
		[[nodiscard]] auto recorded_target_matches(const application::review_mutation_target_binding& aTarget) -> bool
		{
			return aTarget.Hostname.value() == "github.com"
				&& aTarget.ServerContract == application::github_server_contract::github_com_2022_11_28
				&& aTarget.Repository.Owner == "MikeMarcin" && aTarget.Repository.Name == "BPR-Test"
				&& aTarget.RepositoryDatabaseId == 1'328'277'631U && aTarget.RepositoryNodeId == "R_kgDOTyvofw"
				&& aTarget.BaseRepositoryDatabaseId == 1'328'277'631U
				&& aTarget.Number.value() == 2U && aTarget.PullRequestDatabaseId == 4'650'736'621U
				&& aTarget.PullRequestNodeId == "PR_kwDOTyvof88AAAABFTSb7Q" && aTarget.Open
				&& core::canonical_hex(aTarget.BaseOid).view() == "a939a85a5525994fe9e9cb4c973ad9362ed427c7"
				&& core::canonical_hex(aTarget.Semantic.ExpectedHead).view() == "61e8dbcd1c3940f6df0fa720bec5485dc25d4a3a"
				&& aTarget.BaseRef == "codex/rfc0022-fixture-base-20260926"
				&& aTarget.HeadRef == "codex/rfc0022-live-comments-20260926";
		}

		[[nodiscard]] auto recorded_actor_matches(const application::review_mutation_acting_identity& aActor) -> bool
		{
			return aActor.DatabaseId == 7'605'339U && aActor.NodeId == "MDQ6VXNlcjc2MDUzMzk=" && aActor.Login == "MikeMarcin";
		}

		[[nodiscard]] auto recorded_anchor_matches(const application::review_mutation_request& aRequest) -> bool
		{
			if (!aRequest.LiveBinding || !aRequest.Source || !aRequest.Source->Endpoint
				|| !recorded_target_matches(aRequest.LiveBinding->Target) || !recorded_actor_matches(aRequest.LiveBinding->ActingIdentity)) {
				return false;
			}
			const auto& source   = *aRequest.Source;
			const auto& endpoint = *source.Endpoint;
			const auto& anchor   = aRequest.LiveBinding->Anchor;
			// m4-live-anchor-eligibility.json records complete provider patches:
			// both are @@ -7,7 +7,7 @@ with exactly one addition/deletion.
			// Their fixed base/head and content-addressed blobs bind that proof;
			// changing any of them requires a new reviewed evidence contract.
			const auto blob      = core::canonical_hex(endpoint.blob());
			const bool knownBlob = (anchor.Path == "off_hunk_fixture.md" && blob.view() == "828d5c87c20a0f9d74c498e04c36b32d2948b7c5")
				|| (anchor.Path == "off_hunk_fixture.txt" && blob.view() == "79fe8e95cf056dd629442c686696851f26631115");
			return knownBlob && source.LineCount == 80U && anchor.Semantic.Side == core::diff_side::right
				&& anchor.Semantic.Line < source.LineCount && (anchor.Semantic.Line < 6U || anchor.Semantic.Line >= 13U)
				&& source.Placement != core::review_placement::legacy_line
				&& endpoint.request().Diff.Comparison.Left.Commit == application::comparison_base(aRequest.LiveBinding->Target)
				&& endpoint.request().File.Change == application::workbench_file_change_kind::modified
				&& compile_review_permalink_body({ .Target = aRequest.LiveBinding->Target, .Endpoint = source.Endpoint, .Comparison = source.Comparison, .LineCount = source.LineCount }, anchor.Semantic, "Source validation").has_value();
		}

		[[nodiscard]] auto recorded_response_metadata_valid(const review_mutation_wire_response& aResponse) -> bool
		{
			// Native HTTPS supplies only normalized metadata after complete TLS,
			// content-length/encoding and duplicate-header validation. Require the
			// complete core rate tuple again at this authority boundary.
			if (aResponse.Headers.size() != 5U) {
				return false;
			}
			auto headers = std::map<std::string, std::string_view>{ };
			for (const auto& header : aResponse.Headers) {
				auto name = header.Name;
				std::ranges::transform(name, name.begin(), [](const unsigned char aByte) { return static_cast<char>(std::tolower(aByte)); });
				if (!headers.emplace(std::move(name), header.Value).second) {
					return false;
				}
			}
			const auto number = [&headers](const char* aName) -> std::optional<std::uint64_t> {
				const auto found = headers.find(aName);
				if (found == headers.end() || found->second.empty()) {
					return std::nullopt;
				}
				auto       value  = std::uint64_t{ };
				const auto text   = found->second;
				const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
				return parsed.ec == std::errc{ } && parsed.ptr == text.data() + text.size() ? std::optional{ value } : std::nullopt;
			};
			const auto type     = headers.find("content-type");
			const auto resource = headers.find("x-ratelimit-resource");
			const auto limit = number("x-ratelimit-limit"), remaining = number("x-ratelimit-remaining"), reset = number("x-ratelimit-reset");
			return type != headers.end() && json_media_type(type->second)
				&& resource != headers.end() && resource->second == "core"
				&& limit && remaining && reset && *limit > 0U && *remaining > 0U && *remaining <= *limit && *reset > 0U;
		}

		/// Whether an `errors` entry of GitHub's refusal, a string or an
		/// object's `message`, reads any of `aWords` (lower case), compared
		/// without ASCII case. Bounded; any other body is false.
		[[nodiscard]] auto refusal_reads(const std::string_view aBody, const std::span<const std::string_view> aWords) -> bool
		{
			auto       envelope = diagnostic_json_object{ };
			const auto body     = std::string{ aBody };
			if (body.size() > 4096U || !json_preflight{ body }.run() || glz::read<review_json_options{ }>(envelope, body)) {
				return false;
			}
			const auto found = envelope.find("errors");
			if (found == envelope.end()) {
				return false;
			}
			const auto reads = [aWords](std::string aText) {
				std::ranges::transform(aText, aText.begin(), [](const unsigned char aValue) { return static_cast<char>(std::tolower(aValue)); });
				return std::ranges::any_of(aWords, [&aText](const std::string_view aWord) { return aText.find(aWord) != std::string::npos; });
			};
			auto texts = std::vector<std::string>{ };
			if (!glz::read<review_json_options{ }>(texts, found->second.str)) {
				return texts.size() <= 8U && std::ranges::any_of(texts, reads);
			}
			auto errors = std::vector<diagnostic_json_object>{ };
			if (glz::read<review_json_options{ }>(errors, found->second.str) || errors.size() > 8U) {
				return false;
			}
			return std::ranges::any_of(errors, [&reads](const diagnostic_json_object& aError) {
				const auto message = aError.find("message");
				auto       text    = std::string{ };
				if (message == aError.end() || glz::read<review_json_options{ }>(text, message->second.str)) {
					return false;
				}
				// GitHub's single-comment form names the field apart from the words.
				const auto field = aError.find("field");
				auto       named = std::string{ };
				if (field != aError.end() && !glz::read<review_json_options{ }>(named, field->second.str) && named.ends_with("line")) {
					text = "line " + text;
				}
				return reads(std::move(text));
			});
		}
	}

	auto compile_reviewer_request(
		const application::review_reviewer_request& aRequest,
		const application::github_server_contract   aServer)
		-> review_mutation_request_result
	{
		// [[BPR-1804]]. Bound to the target and the actor as a description
		// edit is, naming the pull request by the accepted read's node. A
		// search carries only its query; a request only its person.
		const auto search = aRequest.Operation == application::review_reviewer_operation::search;
		if (!valid_server(aServer) || !aRequest.LiveBinding.has_value()
			|| !application::valid(aRequest.LiveBinding->Target)
			|| !application::valid(aRequest.LiveBinding->ActingIdentity)
			|| aRequest.LiveBinding->Target.ServerContract != aServer
			|| aRequest.Target != aRequest.LiveBinding->Target.Semantic
			|| aRequest.ActingIdentity != aRequest.LiveBinding->ActingIdentity.Account
			|| !graphql_token(aRequest.LiveBinding->Target.PullRequestNodeId)
			|| (search && (!aRequest.User.empty() || !aRequest.Login.empty() || aRequest.Query.size() > core::maximum_reviewer_query_bytes || core::normalize_reviewer_query(aRequest.Query) != aRequest.Query || !utf8_round_trip(aRequest.Query)))
			|| (!search && (!aRequest.Query.empty() || !graphql_token(aRequest.User) || !reviewer_login(aRequest.Login)))) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		auto headers = common_headers();
		headers.push_back({ .Name = "Content-Type", .Value = "application/json" });
		auto body = search
			? glz::write<review_json_options{ }>(reviewer_observation_query_json{
				  .Query     = observe_reviewers_document,
				  .Variables = { .PullRequest = aRequest.LiveBinding->Target.PullRequestNodeId, .Query = aRequest.Query },
			  })
			: glz::write<review_json_options{ }>(reviewer_write_query_json{
				  .Query     = request_reviews_document,
				  .Variables = { .PullRequest = aRequest.LiveBinding->Target.PullRequestNodeId, .User = aRequest.User },
			  });
		if (!body.has_value() || body->empty() || body->size() > maximum_review_mutation_body_bytes) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		return review_mutation_wire_request{
			.Semantic  = aRequest,
			.Server    = aServer,
			.Operation = search ? review_mutation_operation::observe_reviewers : review_mutation_operation::request_reviews,
			.Method    = http_method::post,
			.Path      = std::string{ graphql_path(aServer) },
			.Headers   = std::move(headers),
			.Body      = std::move(*body),
		};
	}

	auto compile_reviewer_observation_request(
		const application::review_reviewer_request& aRequest,
		const application::github_server_contract   aServer)
		-> review_mutation_request_result
	{
		// The write's own guards, then the search for the person's login.
		if (aRequest.Operation != application::review_reviewer_operation::request || !compile_reviewer_request(aRequest, aServer).has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		auto observation      = aRequest;
		observation.Operation = application::review_reviewer_operation::search;
		observation.Query     = aRequest.Login;
		observation.User.clear();
		observation.Login.clear();
		return compile_reviewer_request(observation, aServer);
	}

	auto compile_draft_state_request(
		const application::review_draft_state_request& aRequest,
		const application::github_server_contract      aServer)
		-> review_mutation_request_result
	{
		// [[BPR-1852]]. The change asked names its own mutation.
		return aRequest.Change == core::draft_state_change::convert_to_draft
			? compile_draft_state_document(aRequest, aServer, review_mutation_operation::convert_pull_request_to_draft, convert_to_draft_document)
			: compile_draft_state_document(aRequest, aServer, review_mutation_operation::mark_pull_request_ready_for_review, mark_ready_for_review_document);
	}

	auto compile_draft_state_observation_request(
		const application::review_draft_state_request& aRequest,
		const application::github_server_contract      aServer)
		-> review_mutation_request_result
	{
		return compile_draft_state_document(aRequest, aServer, review_mutation_operation::observe_pull_request_draft_state, observe_draft_state_document);
	}

	auto compile_comment_edit_request(
		const application::review_comment_edit_request& aRequest,
		const application::github_server_contract       aServer)
		-> review_mutation_request_result
	{
		// [[BPR-1775]]. A comment edit binds the target and the actor, as a
		// description edit does, and names the comment by its node.
		if (!valid_server(aServer) || !aRequest.LiveBinding.has_value()
			|| !application::valid(aRequest.LiveBinding->Target)
			|| !application::valid(aRequest.LiveBinding->ActingIdentity)
			|| aRequest.LiveBinding->Target.ServerContract != aServer
			|| aRequest.Target != aRequest.LiveBinding->Target.Semantic
			|| aRequest.ActingIdentity != aRequest.LiveBinding->ActingIdentity.Account
			|| !graphql_token(aRequest.Subject) || comment_edit_document(aRequest.Kind).empty()
			|| !utf8_round_trip(aRequest.Body) || !utf8_round_trip(aRequest.Original)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		auto headers = common_headers();
		headers.push_back({ .Name = "Content-Type", .Value = "application/json" });
		auto body = glz::write<review_json_options{ }>(comment_edit_query_json{
			.Query     = comment_edit_document(aRequest.Kind),
			.Variables = { .Subject = aRequest.Subject, .Body = aRequest.Body },
		});
		if (!body.has_value() || body->empty() || body->size() > maximum_description_request_bytes) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		return review_mutation_wire_request{
			.Semantic  = aRequest,
			.Server    = aServer,
			.Operation = comment_edit_operation(aRequest.Kind),
			.Method    = http_method::post,
			.Path      = std::string{ graphql_path(aServer) },
			.Headers   = std::move(headers),
			.Body      = std::move(*body),
		};
	}

	auto compile_comment_observation_request(
		const application::review_comment_edit_request& aRequest,
		const application::github_server_contract       aServer)
		-> review_mutation_request_result
	{
		// The mutation's own guards with a read document in its place. A text
		// too long to send is still observed, so the read names the refusal.
		auto bounded = aRequest;
		bounded.Body.clear();
		auto observation = compile_comment_edit_request(bounded, aServer);
		if (!observation.has_value()) {
			return observation;
		}
		auto body = glz::write<review_json_options{ }>(reaction_subject_query_json{
			.Query     = observe_comment_document,
			.Variables = { .Subject = aRequest.Subject },
		});
		if (!body.has_value() || body->empty() || body->size() > maximum_review_mutation_body_bytes) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		observation->Semantic  = aRequest;
		observation->Operation = review_mutation_operation::observe_comment_for_edit;
		observation->Body      = std::move(*body);
		return observation;
	}

	auto compile_review_submission_request(
		const review_submission_request&          aRequest,
		const application::github_server_contract aServer)
		-> review_mutation_request_result
	{
		if (!valid_server(aServer) || aServer != aRequest.Target.ServerContract
			|| !valid_submission(aRequest)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		const auto commit = core::canonical_hex(aRequest.Target.Semantic.ExpectedHead);
		auto       object = review_submission_json{
			.Body     = aRequest.Body,
			.CommitId = commit.view(),
			.Event    = submission_names(aRequest.Disposition)->Event,
		};
		if (!aRequest.Comments.empty()) {
			auto& comments = object.Comments.emplace();
			for (const auto& comment : aRequest.Comments) {
				const auto side  = comment.Side == core::diff_side::left ? std::string_view{ "LEFT" } : std::string_view{ "RIGHT" };
				auto       value = review_submission_comment_json{ .Body = comment.Body, .Line = std::uint64_t{ comment.Line } + 1U, .Path = comment.Path, .Side = side };
				if (comment.StartLine) {
					value.StartLine = std::uint64_t{ *comment.StartLine } + 1U;
					value.StartSide = side;
				}
				comments.push_back(value);
			}
		}
		auto bytes = glz::write<review_json_options{ }>(object);
		if (!bytes.has_value() || bytes->size() > maximum_review_mutation_body_bytes) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		auto headers = common_headers();
		headers.push_back({ .Name = "Content-Type", .Value = "application/json" });
		return review_mutation_wire_request{
			.Semantic  = aRequest,
			.Server    = aServer,
			.Operation = review_mutation_operation::create_review,
			.Method    = http_method::post,
			.Path      = pull_request_path(aRequest.Target, aServer) + "/reviews",
			.Headers   = std::move(headers),
			.Body      = std::move(*bytes),
		};
	}

	auto compile_review_submission_observation_request(const review_submission_request& aRequest, const application::github_server_contract aServer) -> review_mutation_request_result
	{
		auto result = compile_review_submission_request(aRequest, aServer);
		if (!result) {
			return result;
		}
		result->Operation = review_mutation_operation::observe_reviews;
		result->Method    = http_method::get;
		result->Path += "?per_page=100";
		result->Headers = common_headers();
		result->Body.clear();
		return result;
	}

	auto compile_review_comment_observation_request(
		const application::review_mutation_request& aRequest,
		const application::github_server_contract   aServer)
		-> review_mutation_request_result
	{
		auto mutation = compile_review_comment_request(aRequest, aServer);
		if (!mutation.has_value()) {
			return std::unexpected(mutation.error());
		}
		// [[BPR-0896]]. A reply is written by GraphQL, so its path is the
		// GraphQL root and no REST collection can be derived from it. It is
		// reconciled through the thread it landed in: the write's own
		// compiled request with a read document in its place, as the thread
		// pre-flight is.
		if (aRequest.Proposal.ReplyToThread.has_value()) {
			auto body = glz::write<review_json_options{ }>(resolution_query_json{
				.Query     = observe_thread_comments_document,
				.Variables = { .Thread = *aRequest.Proposal.ReplyToThread },
			});
			if (!body.has_value() || body->empty()
				|| body->size() > maximum_review_mutation_body_bytes) {
				return failed(review_mutation_contract_failure::invalid_request);
			}
			mutation->Operation = review_mutation_operation::observe_review_thread_comments;
			mutation->Body      = std::move(*body);
			return mutation;
		}
		return review_mutation_wire_request{
			.Semantic  = aRequest,
			.Server    = aServer,
			.Operation = review_mutation_operation::observe_review_comments,
			.Method    = http_method::get,
			.Path      = mutation->Path + "?per_page=100",
			.Headers   = common_headers(),
			.Body      = { },
		};
	}

	auto compile_review_comment_collection_request(const application::review_mutation_target_binding& aTarget, const application::github_server_contract aServer, const std::uint32_t aPage) -> review_mutation_request_result
	{
		if (!valid_server(aServer) || !application::valid(aTarget) || aTarget.ServerContract != aServer || aPage == 0U || aPage > maximum_review_comment_collection_pages) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		return review_mutation_wire_request{
			.Semantic  = aTarget,
			.Server    = aServer,
			.Operation = review_mutation_operation::observe_review_comments,
			.Method    = http_method::get,
			.Path      = pull_request_path(aTarget, aServer) + "/comments" + staged_page_suffix(aPage),
			.Headers   = common_headers(),
		};
	}

	auto validate_outbound_review_mutation_request(
		const review_mutation_wire_request& aRequest)
		-> std::expected<void, review_mutation_contract_failure>
	{
		try {
			auto canonical = review_mutation_request_result{ failed(
				review_mutation_contract_failure::invalid_request) };
			std::visit(
				[&canonical, &aRequest](const auto& aSemantic) {
					using semantic_type = std::remove_cvref_t<decltype(aSemantic)>;
					if constexpr (std::same_as<semantic_type, application::review_mutation_target_binding>) {
						canonical = aRequest.Operation == review_mutation_operation::observe_review_comments
							? compile_review_comment_collection_request(aSemantic, aRequest.Server, collection_page(aRequest.Path))
							: aRequest.Operation == review_mutation_operation::observe_merge_base
							? compile_merge_base_observation_request(aSemantic, aRequest.Server)
							: aRequest.Operation == review_mutation_operation::observe_recorded_merge_base
							? compile_merge_base_observation_request(aSemantic, aRequest.Server, merge_base_observation_basis::recorded_base)
							: aRequest.Operation == review_mutation_operation::observe_base_ancestry
							? compile_merge_base_observation_request(aSemantic, aRequest.Server, merge_base_observation_basis::base_ancestry)
							: aRequest.Operation == review_mutation_operation::observe_server_meta
							? compile_server_meta_request(aSemantic, aRequest.Server)
							: compile_target_observation_request(aSemantic, aRequest.Server);
					} else if constexpr (std::same_as<semantic_type, application::review_mutation_acting_identity>) {
						canonical = compile_viewer_observation_request(aSemantic, aRequest.Server);
					} else if constexpr (std::same_as<semantic_type, application::staged_review_request>) {
						canonical = compile_staged_review_request(aSemantic);
					} else if constexpr (std::same_as<semantic_type, review_submission_request>) {
						canonical = aRequest.Operation == review_mutation_operation::observe_reviews
							? compile_review_submission_observation_request(aSemantic, aRequest.Server)
							: compile_review_submission_request(aSemantic, aRequest.Server);
					} else if constexpr (std::same_as<semantic_type, application::review_reaction_request>) {
						canonical = aRequest.Operation == review_mutation_operation::observe_reaction_subject
							? compile_reaction_subject_observation_request(aSemantic, aRequest.Server)
							: compile_reaction_request(aSemantic, aRequest.Server);
					} else if constexpr (std::same_as<semantic_type, application::review_description_request>) {
						canonical = aRequest.Operation == review_mutation_operation::observe_pull_request_description
							? compile_description_observation_request(aSemantic, aRequest.Server)
							: compile_description_request(aSemantic, aRequest.Server);
					} else if constexpr (std::same_as<semantic_type, application::review_reviewer_request>) {
						canonical = compile_reviewer_request(aSemantic, aRequest.Server);
					} else if constexpr (std::same_as<semantic_type, application::review_draft_state_request>) {
						canonical = aRequest.Operation == review_mutation_operation::observe_pull_request_draft_state
							? compile_draft_state_observation_request(aSemantic, aRequest.Server)
							: compile_draft_state_request(aSemantic, aRequest.Server);
					} else if constexpr (std::same_as<semantic_type, application::review_comment_edit_request>) {
						canonical = aRequest.Operation == review_mutation_operation::observe_comment_for_edit
							? compile_comment_observation_request(aSemantic, aRequest.Server)
							: compile_comment_edit_request(aSemantic, aRequest.Server);
					} else if constexpr (std::same_as<semantic_type, application::review_thread_resolution_request>) {

						canonical = aRequest.Operation
								== review_mutation_operation::observe_review_thread
							? compile_thread_target_observation_request(aSemantic, aRequest.Server)
							: compile_thread_resolution_request(aSemantic, aRequest.Server);
					} else if (aRequest.Operation
								   == review_mutation_operation::create_review_comment
							   || aRequest.Operation
								   == review_mutation_operation::reply_review_thread) {
						canonical = compile_review_comment_request(aSemantic, aRequest.Server);
					} else if (aRequest.Operation
								   == review_mutation_operation::observe_review_comments
							   || aRequest.Operation
								   == review_mutation_operation::observe_review_thread_comments) {
						// [[BPR-0896]]. Both spellings recompile from the
						// semantic: a reply claiming the REST list, or a root
						// claiming the thread, differs from its canonical form.
						canonical = compile_review_comment_observation_request(
							aSemantic, aRequest.Server);
					}
				},
				aRequest.Semantic);
			if (!canonical.has_value() || *canonical != aRequest) {
				return failed(review_mutation_contract_failure::invalid_request);
			}
			return { };
		} catch (...) {
			return failed(review_mutation_contract_failure::response_limit);
		}
	}

	auto parse_review_submission_recovery_target(const review_mutation_wire_response& aResponse, const application::review_mutation_target_binding& aExpected) -> review_mutation_parse_result<void>
	{
		if (!application::valid(aExpected)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse) || has_header(aResponse, "location")) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		if (aResponse.Body.size() > maximum_review_mutation_response_bytes) {
			return failed(review_mutation_contract_failure::response_limit);
		}
		if (!json_preflight{ aResponse.Body }.run()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		recovery_target_json target;
		if (glz::read<review_json_options{ }>(target, aResponse.Body)) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		const auto& repository = target.Base.Repository;
		if (target.Id != aExpected.PullRequestDatabaseId || target.NodeId != aExpected.PullRequestNodeId || target.Number != aExpected.Number.value() || repository.Id != aExpected.RepositoryDatabaseId || repository.Id != aExpected.BaseRepositoryDatabaseId || repository.NodeId != aExpected.RepositoryNodeId || repository.FullName != aExpected.Repository.Owner + "/" + aExpected.Repository.Name) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return { };
	}

	auto parse_target_observation(
		const review_mutation_wire_response&               aResponse,
		const application::review_mutation_target_binding& aExpected)
		-> review_mutation_parse_result<review_mutation_target_observation>
	{
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto response = document<target_response_json>(aResponse);
		if (!response || response->Id == 0U || response->Number == 0U || response->Base.Repository.Id == 0U) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		const auto head = core::parse_git_commit_oid(response->Head.Sha);
		const auto base = core::parse_git_commit_oid(response->Base.Sha);
		if (!head || !base) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		const auto& repository = response->Base.Repository;
		if (response->Id != aExpected.PullRequestDatabaseId || response->NodeId != aExpected.PullRequestNodeId
			// [[BPR-1826]]. The state the binding was read in: an open
			// binding refuses a pull request closed since, and a closed one a
			// pull request reopened since (REST says "closed" when merged).
			|| response->Number != aExpected.Number.value() || (response->State == "open") != aExpected.Open
			|| (response->State != "open" && response->State != "closed")
			|| repository.Id != aExpected.RepositoryDatabaseId || repository.Id != aExpected.BaseRepositoryDatabaseId
			|| repository.NodeId != aExpected.RepositoryNodeId
			|| repository.FullName != aExpected.Repository.Owner + "/" + aExpected.Repository.Name
			|| *base != aExpected.BaseOid || response->Base.Ref != aExpected.BaseRef || response->Head.Ref != aExpected.HeadRef) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		auto observed                  = aExpected;
		observed.Semantic.ExpectedHead = *head;
		return review_mutation_target_observation{ .Target = std::move(observed), .Commits = response->Commits };
	}

	auto written_target(const review_mutation_wire_request& aRequest)
		-> std::optional<application::review_mutation_target_binding>
	{
		const auto exchange = review_mutation_exchange_for(aRequest.Operation);
		if (!exchange || !exchange->Witnessed) {
			return std::nullopt;
		}
		return std::visit(
			[](const auto& aSemantic) -> std::optional<application::review_mutation_target_binding> {
				using semantic_type = std::remove_cvref_t<decltype(aSemantic)>;
				if constexpr (std::same_as<semantic_type, review_submission_request>) {
					return aSemantic.Target;
				} else if constexpr (std::same_as<semantic_type, application::staged_review_request>) {
					return aSemantic.Binding.Target;
				} else if constexpr (std::same_as<semantic_type, application::review_mutation_request>) {
					return aSemantic.LiveBinding ? std::optional{ aSemantic.LiveBinding->Target } : std::nullopt;
				} else {
					return std::nullopt;
				}
			},
			aRequest.Semantic);
	}

	auto target_recorded_base_moved(
		const review_mutation_wire_response&               aResponse,
		const application::review_mutation_target_binding& aExpected) -> bool
	{
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return false;
		}
		const auto response = document<target_response_json>(aResponse);
		const auto base     = response ? core::parse_git_commit_oid(response->Base.Sha) : std::nullopt;
		if (!base || *base == aExpected.BaseOid) {
			return false;
		}
		auto moved    = aExpected;
		moved.BaseOid = *base;
		return parse_target_observation(aResponse, moved).has_value();
	}

	auto parse_viewer_observation(
		const review_mutation_wire_response&                aResponse,
		const application::review_mutation_acting_identity& aExpected)
		-> review_mutation_parse_result<review_mutation_viewer_observation>
	{
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto response = document<review_author_json>(aResponse);
		if (!response || response->Id == 0U) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		if (response->Id != aExpected.DatabaseId || response->NodeId != aExpected.NodeId || response->Login != aExpected.Login) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return review_mutation_viewer_observation{ .Identity = aExpected };
	}

	auto recorded_off_hunk_contract(const live_review_mutation_configuration& aConfiguration) -> std::optional<off_hunk_unsupported_contract>
	{
		if (!valid(aConfiguration) || aConfiguration.CredentialContext.Authority.BprProfile.value() != 2U
			|| !recorded_target_matches(aConfiguration.Target) || !recorded_actor_matches(aConfiguration.ActingIdentity)) {
			return std::nullopt;
		}
		return off_hunk_unsupported_contract{ .Resource = "PullRequestReviewComment", .Field = "pull_request_review_thread.line", .Code = "custom", .Message = "could not be resolved", .RecordedDesignatedScope = true };
	}

	auto refusal_names_unplaceable_line(const std::string_view aBody) -> bool
	{
		static constexpr auto words = std::array<std::string_view, 3>{ "line must be part of the diff", "line could not be resolved", "diff hunk can't be blank" };
		return refusal_reads(aBody, words);
	}

	auto refusal_names_own_pull_request(const std::string_view aBody) -> bool
	{
		// [[BPR-1853]]. GitHub's words, probed on MikeMarcin/BPR-Test#43.
		static constexpr auto words = std::array<std::string_view, 2>{ "can not approve your own pull request", "can not request changes on your own pull request" };
		return refusal_reads(aBody, words);
	}

	auto general_off_hunk_contract(const application::github_server_contract aServer) -> std::optional<off_hunk_unsupported_contract>
	{
		if (aServer != application::github_server_contract::github_com_2022_11_28) {
			return std::nullopt;
		}
		return off_hunk_unsupported_contract{ .Server = aServer, .Resource = "PullRequestReviewComment", .Field = "pull_request_review_thread.line", .Code = "custom", .Message = "could not be resolved", .General = true };
	}

	auto refusal_names_comment_line(const review_mutation_wire_response& aResponse) -> bool
	{
		auto       envelope = diagnostic_json_object{ };
		const auto body     = aResponse.Body;
		if (aResponse.Status != 422 || body.size() > 4096U || !json_preflight{ body }.run() || glz::read<review_json_options{ }>(envelope, body)) {
			return false;
		}
		const auto found  = envelope.find("errors");
		auto       errors = std::vector<diagnostic_json_object>{ };
		if (found == envelope.end() || glz::read<review_json_options{ }>(errors, found->second.str) || errors.size() > 8U) {
			return false;
		}
		return std::ranges::any_of(errors, [](const diagnostic_json_object& aError) {
			const auto field = aError.find("field");
			auto       named = std::string{ };
			return field != aError.end() && !glz::read<review_json_options{ }>(named, field->second.str)
				&& (named == "line" || named == "pull_request_review_thread.line");
		});
	}

	auto definitive_off_hunk_unsupported(const review_mutation_wire_response& aResponse,
										 const review_mutation_wire_request& aRequest, const off_hunk_unsupported_contract& aContract) -> bool
	{
		const auto* semantic = std::get_if<application::review_mutation_request>(&aRequest.Semantic);
		if (aResponse.Status != 422 || !response_headers_valid(aResponse)
			|| has_header(aResponse, "Retry-After") || has_header(aResponse, "Location")
			|| aRequest.Operation != review_mutation_operation::create_review_comment
			|| aContract.Operation != aRequest.Operation || aContract.Server != aRequest.Server
			|| aContract.ApiRevision != rest_version || aContract.ClassifierRevision != off_hunk_classifier_revision
			|| !semantic || semantic->Representation != core::review_publication_representation::native_line
			|| semantic->Proposal.ReplyToThread || !validate_outbound_review_mutation_request(aRequest)
			|| !bounded_text(aContract.Resource, 128U) || !bounded_text(aContract.Field, 128U)
			|| !bounded_text(aContract.Code, 128U) || !bounded_text(aContract.Message, 512U)) {
			return false;
		}
		if (aResponse.Body.size() > 4096U || !json_preflight{ aResponse.Body }.run()) {
			return false;
		}
		// [[BPR-1736]] (D4). GitHub.com's measured shape, any repository: the
		// exact tuple, any documentation URL and status, no other member.
		if (aContract.General) {
			auto           parsed  = general_unsupported_response_json{ };
			constexpr auto options = [] { auto value = review_json_options{ }; value.error_on_unknown_keys = true; return value; }();
			if (glz::read<options>(parsed, aResponse.Body) || parsed.Message != "Validation Failed" || parsed.Errors.size() != 1U) {
				return false;
			}
			const auto& error = parsed.Errors.front();
			return error.Resource == aContract.Resource && error.Field == aContract.Field && error.Code == aContract.Code && error.Message == aContract.Message;
		}
		if (aContract.RecordedDesignatedScope) {
			auto           parsed  = recorded_unsupported_response_json{ };
			constexpr auto options = [] { auto value = review_json_options{ }; value.error_on_unknown_keys = true; return value; }();
			if (!recorded_anchor_matches(*semantic) || !recorded_response_metadata_valid(aResponse)
				|| glz::read<options>(parsed, aResponse.Body) || parsed.Message != "Validation Failed"
				|| parsed.DocumentationUrl != "https://docs.github.com/rest/pulls/comments#create-a-review-comment-for-a-pull-request"
				|| parsed.Status != "422" || parsed.Errors.size() != 1U) {
				return false;
			}
			const auto& error = parsed.Errors.front();
			return error.Resource == aContract.Resource && error.Field == aContract.Field && error.Code == aContract.Code && error.Message == aContract.Message;
		}
		auto           parsed  = unsupported_response_json{ };
		constexpr auto options = [] { auto value = review_json_options{}; value.error_on_unknown_keys = true; return value; }();
		if (glz::read<options>(parsed, aResponse.Body) || parsed.Message != "Validation Failed" || parsed.Errors.size() != 1U) {
			return false;
		}
		const auto& error = parsed.Errors.front();
		return error.Resource == aContract.Resource && error.Field == aContract.Field
			&& error.Code == aContract.Code && error.Message == aContract.Message;
	}

	auto parse_review_comment_response(
		const review_mutation_wire_response&        aResponse,
		const application::review_mutation_request& aExpected)
		-> review_mutation_parse_result<review_mutation_comment_observation>
	{
		// [[BPR-0657]]. A reply comes back from GraphQL as 200 with an
		// envelope; a new root comment from REST as 201 with the comment.
		if (aExpected.Proposal.ReplyToThread.has_value()) {
			if (aResponse.Status != 200 || !response_headers_valid(aResponse) || !aExpected.LiveBinding) {
				return failed(review_mutation_contract_failure::invalid_status);
			}
			const auto response = document<reply_response_json>(aResponse);
			if (!response) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			return reply_observation(response->Data.Reply.Comment, aExpected);
		}
		if (aResponse.Status != 201 || !response_headers_valid(aResponse) || !aExpected.LiveBinding) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto response = document<comment_response_json>(aResponse);
		if (!response) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		return comment_observation(*response, aExpected);
	}

	auto parse_thread_target_observation(
		const review_mutation_wire_response&                 aResponse,
		const application::review_thread_resolution_request& aExpected)
		-> review_mutation_parse_result<review_thread_target_observation>
	{
		if (!aExpected.LiveBinding.has_value()
			|| !compile_thread_target_observation_request(
					aExpected, aExpected.LiveBinding->Target.ServerContract)
					.has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		// A null node is a valid answer when the target is no longer visible.
		// Keep that refusal distinct from an invalid server response.
		const auto response = document<thread_node_response_json>(aResponse);
		if (!response.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		if (std::holds_alternative<std::nullptr_t>(response->Data.Node)) {
			return failed(review_mutation_contract_failure::target_unavailable);
		}
		const auto& thread = std::get<resolution_thread_json>(response->Data.Node);
		if (thread.Id != aExpected.Thread
			|| !binds_designated_pull_request(
				thread.PullRequest, aExpected.LiveBinding->Target)) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return review_thread_target_observation{
			.Thread   = thread.Id,
			.Resolved = thread.IsResolved,
		};
	}

	auto parse_thread_resolution_response(
		const review_mutation_wire_response&                 aResponse,
		const application::review_thread_resolution_request& aExpected)
		-> review_mutation_parse_result<review_thread_resolution_observation>
	{
		if (!compile_thread_resolution_request(aExpected, aExpected.LiveBinding ? aExpected.LiveBinding->Target.ServerContract : application::github_server_contract::github_com_2022_11_28).has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		auto thread = std::optional<resolution_thread_json>{ };
		if (aExpected.Resolve) {
			if (const auto response = document<resolve_response_json>(aResponse); response) {
				thread = response->Data.Resolve.Thread;
			}
		} else if (const auto response = document<unresolve_response_json>(aResponse); response) {
			thread = response->Data.Unresolve.Thread;
		}
		if (!thread.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		// [[BPR-0879]]. The second, cheap check: the write's own answer names
		// the pull request it changed. It cannot refuse a write that already
		// happened, which is why the pre-flight observation exists, but it
		// catches anything the pre-flight could not see.
		if (thread->Id != aExpected.Thread || thread->IsResolved != aExpected.Resolve
			|| !aExpected.LiveBinding.has_value()
			|| !binds_designated_pull_request(
				thread->PullRequest, aExpected.LiveBinding->Target)) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return review_thread_resolution_observation{
			.Receipt = { .Thread = thread->Id, .Resolved = thread->IsResolved },
		};
	}

	auto parse_reaction_subject_observation(
		const review_mutation_wire_response&        aResponse,
		const application::review_reaction_request& aExpected)
		-> review_mutation_parse_result<review_reaction_subject_observation>
	{
		if (!aExpected.LiveBinding.has_value()
			|| !compile_reaction_subject_observation_request(aExpected, aExpected.LiveBinding->Target.ServerContract).has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto response = document<reaction_node_response_json>(aResponse);
		if (!response.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		if (std::holds_alternative<std::nullptr_t>(response->Data.Node)) {
			return failed(review_mutation_contract_failure::target_unavailable);
		}
		return reaction_subject_observation(std::get<reaction_subject_json>(response->Data.Node), aExpected);
	}

	auto parse_reaction_response(
		const review_mutation_wire_response&        aResponse,
		const application::review_reaction_request& aExpected)
		-> review_mutation_parse_result<review_reaction_subject_observation>
	{
		if (!aExpected.LiveBinding.has_value() || !compile_reaction_request(aExpected, aExpected.LiveBinding->Target.ServerContract).has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		auto payload = std::optional<reaction_payload_json>{ };
		if (aExpected.Add) {
			if (auto response = document<add_reaction_response_json>(aResponse); response) {
				payload = std::move(response->Data.Add);
			}
		} else if (auto response = document<remove_reaction_response_json>(aResponse); response) {
			payload = std::move(response->Data.Remove);
		}
		if (!payload.has_value() || payload->Reaction.Content != core::reaction_graphql_name(aExpected.Content)) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		auto observed = reaction_subject_observation(payload->Subject, aExpected);
		if (!observed.has_value()) {
			return observed;
		}
		// The write's own answer must show the state it asked for.
		const auto* const group = core::find_reaction_group(observed->Reactions, aExpected.Content);
		if ((group != nullptr && group->ViewerReacted) != aExpected.Add) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return observed;
	}

	auto parse_description_observation(
		const review_mutation_wire_response&           aResponse,
		const application::review_description_request& aExpected)
		-> review_mutation_parse_result<review_description_observation>
	{
		if (!aExpected.LiveBinding.has_value()
			|| !compile_description_observation_request(aExpected, aExpected.LiveBinding->Target.ServerContract).has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto response = document<description_node_response_json>(aResponse, maximum_description_response_bytes);
		if (!response.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		if (std::holds_alternative<std::nullptr_t>(response->Data.Node)) {
			return failed(review_mutation_contract_failure::target_unavailable);
		}
		return description_observation(std::get<description_pull_request_json>(response->Data.Node), aExpected);
	}

	auto parse_description_response(
		const review_mutation_wire_response&           aResponse,
		const application::review_description_request& aExpected)
		-> review_mutation_parse_result<review_description_observation>
	{
		if (!aExpected.LiveBinding.has_value() || !compile_description_request(aExpected, aExpected.LiveBinding->Target.ServerContract).has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto response = document<description_update_response_json>(aResponse, maximum_description_response_bytes);
		if (!response.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		auto observed = description_observation(response->Data.Update.PullRequest, aExpected);
		if (!observed.has_value()) {
			return observed;
		}
		// The write's own answer must carry the body it asked for.
		if (observed->Body != aExpected.Body) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return observed;
	}

	auto parse_reviewer_observation(
		const review_mutation_wire_response&        aResponse,
		const application::review_reviewer_request& aExpected)
		-> review_mutation_parse_result<core::reviewer_search_answer>
	{
		if (!aExpected.LiveBinding.has_value() || aExpected.Operation != application::review_reviewer_operation::search
			|| !compile_reviewer_request(aExpected, aExpected.LiveBinding->Target.ServerContract).has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto response = document<reviewer_node_response_json>(aResponse);
		if (!response.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		if (std::holds_alternative<std::nullptr_t>(response->Data.Node)) {
			return failed(review_mutation_contract_failure::target_unavailable);
		}
		const auto& pull       = std::get<reviewer_pull_request_json>(response->Data.Node);
		const auto  repository = thread_repository_json{ .Id = pull.Repository.Id, .DatabaseId = pull.Repository.DatabaseId, .NameWithOwner = pull.Repository.NameWithOwner };
		if (!reviewer_binds(pull.Id, pull.DatabaseId, pull.Number, repository, aExpected)) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		auto answer = reviewer_standing_facts(pull.Author, pull.ReviewRequests, pull.LatestReviews);
		if (!answer.has_value()) {
			return answer;
		}
		if (pull.SuggestedReviewers.size() > maximum_reviewer_list_items || pull.Repository.AssignableUsers.Nodes.size() > maximum_reviewer_matches) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		for (const auto& suggestion : pull.SuggestedReviewers) {
			if (!suggestion.Reviewer.has_value() || suggestion.IsAuthor) {
				continue;
			}
			auto person = reviewer_person(*suggestion.Reviewer);
			if (!person) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			person->Suggested = true;
			person->Commenter = suggestion.IsCommenter;
			answer->Suggested.push_back(std::move(*person));
		}
		for (const auto& user : pull.Repository.AssignableUsers.Nodes) {
			auto person = reviewer_person(user);
			if (!person) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			answer->Matches.push_back(std::move(*person));
		}
		return answer;
	}
}
