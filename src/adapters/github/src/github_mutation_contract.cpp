#include "github_mutation_contract.hpp"

#include "github_http_helpers.hpp"
#include "github_json_preflight.hpp"
#include "review_comment_permalink.hpp"

#include "bpr/application/review_mutation_coordinator.hpp"
#include "bpr/core/sha256.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <initializer_list>

#include <glaze/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <concepts>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace bpr::adapters::github::detail {
	namespace {
		constexpr std::string_view rest_version = "2022-11-28";

		[[nodiscard]] auto failed(const review_mutation_contract_failure aFailure)
			-> std::unexpected<review_mutation_contract_failure>
		{
			return std::unexpected{ aFailure };
		}

		[[nodiscard]] auto bounded_text(
			const std::string_view aText,
			const std::size_t      aMaximum) noexcept -> bool
		{
			return !aText.empty() && aText.size() <= aMaximum
				&& std::ranges::all_of(aText, [](const unsigned char aCharacter) {
					   return aCharacter >= 0x20U && aCharacter != 0x7FU;
				   });
		}

		[[nodiscard]] constexpr auto valid_server(
			const application::github_server_contract aServer) noexcept -> bool
		{
			return aServer == application::github_server_contract::github_com_2022_11_28
				|| aServer == application::github_server_contract::ghes_3_18_2022_11_28;
		}

		[[nodiscard]] constexpr auto rest_prefix(
			const application::github_server_contract aServer) noexcept -> std::string_view
		{
			return aServer == application::github_server_contract::ghes_3_18_2022_11_28
				? std::string_view{ "/api/v3" }
				: std::string_view{ };
		}

		/// [[BPR-0657]]. GitHub's GraphQL root: beside the REST root on
		/// github.com, under `/api` on GitHub Enterprise Server.
		[[nodiscard]] constexpr auto graphql_path(
			const application::github_server_contract aServer) noexcept -> std::string_view
		{
			return aServer == application::github_server_contract::ghes_3_18_2022_11_28
				? std::string_view{ "/api/graphql" }
				: std::string_view{ "/graphql" };
		}

		/// The operation names are the ones the read contract reserves as
		/// mutations, so a document that reaches the read transport is refused
		/// by name.
		///
		/// [[BPR-0879]]. Every document that names a thread also selects the
		/// pull request that thread belongs to. A thread node identity is the
		/// one free identifier the product hands to a write, and without this
		/// selection nothing on the wire says which pull request it reaches.
		constexpr auto reply_document = std::string_view{
			"mutation ReplyToThread($thread:ID!,$body:String!){"
			"addPullRequestReviewThreadReply(input:{pullRequestReviewThreadId:$thread,body:$body}){"
			"comment{id databaseId body path line commit{oid} author{login} viewerDidAuthor "
			"pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}}}"
		};
		constexpr auto resolve_document = std::string_view{
			"mutation ResolveThread($thread:ID!){resolveReviewThread(input:{threadId:$thread}){"
			"thread{id isResolved pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}}}"
		};
		constexpr auto unresolve_document = std::string_view{
			"mutation UnresolveThread($thread:ID!){unresolveReviewThread(input:{threadId:$thread}){"
			"thread{id isResolved pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}}}"
		};
		/// [[BPR-0879]]. The read that precedes a thread write. Detecting a
		/// foreign write in the answer is not refusing one, so the server is
		/// asked which pull request the thread belongs to first. A thread
		/// cannot migrate between pull requests, so the extra round trip
		/// races with nothing.
		constexpr auto observe_thread_document = std::string_view{
			"query ObserveThread($thread:ID!){node(id:$thread){... on PullRequestReviewThread{"
			"id isResolved pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}}}"
		};
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

		/// [[BPR-1368]]. A tour stop's body edited while its review is
		/// pending. GitHub's REST edit and read of such a comment
		/// (`/pulls/comments/{id}`) answer 404; this mutation edits it
		/// (MikeMarcin/BPR-Test#45). The answer names the comment's database
		/// identity, its review and its pull request, so the node ID the
		/// request was addressed by is tied back to the REST identities the
		/// tour holds. `fullDatabaseId` is a string-encoded `BigInt`.
		constexpr auto update_review_comment_document = std::string_view{
			"mutation UpdateReviewComment($comment:ID!,$body:String!){"
			"updatePullRequestReviewComment(input:{pullRequestReviewCommentId:$comment,body:$body}){"
			"pullRequestReviewComment{id fullDatabaseId url body state viewerDidAuthor "
			"author{login ... on User{databaseId id}} pullRequestReview{id fullDatabaseId} "
			"pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}}}"
		};

		[[nodiscard]] auto graphql_token(const std::string_view aValue) noexcept -> bool
		{
			return !aValue.empty() && aValue.size() <= 256U
				&& std::ranges::all_of(aValue, [](const char aCharacter) {
					   const auto byte = static_cast<unsigned char>(aCharacter);
					   return byte >= 0x21U && byte <= 0x7eU && aCharacter != '"' && aCharacter != '\\';
				   });
		}

		[[nodiscard]] auto utf8_round_trip(const std::string_view aText) -> bool
		{
			const auto decoded = QString::fromUtf8(
				aText.data(), static_cast<qsizetype>(aText.size()));
			const auto encoded = decoded.toUtf8();
			return static_cast<std::size_t>(encoded.size()) == aText.size()
				&& std::equal(encoded.begin(), encoded.end(), aText.begin());
		}

		[[nodiscard]] auto common_headers() -> std::vector<http_header>
		{
			return {
				{ .Name = "Accept", .Value = "application/vnd.github+json" },
				{ .Name = "User-Agent", .Value = "BetterPullReview/0.1" },
				{ .Name = "X-GitHub-Api-Version", .Value = std::string{ rest_version } },
			};
		}

		[[nodiscard]] auto pull_request_path(
			const application::review_mutation_target_binding& aTarget,
			const application::github_server_contract          aServer)
			-> std::string
		{
			return std::string{ rest_prefix(aServer) } + "/repos/"
				+ percent_encode(aTarget.Repository.Owner) + "/"
				+ percent_encode(aTarget.Repository.Name) + "/pulls/"
				+ std::to_string(aTarget.Number.value());
		}

		struct review_submission_names
		{
			std::string_view Event;
			std::string_view State;
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

		struct review_author_json
		{
			std::uint64_t Id = 0U;
			std::string   NodeId;
			std::string   Login;

			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &review_author_json::Id,
					"node_id", &review_author_json::NodeId,
					"login", &review_author_json::Login);
			};
		};

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
		struct comment_submission_json
		{
			std::string_view Body;
			std::string_view CommitId;
			std::uint64_t    Line = 0U;
			std::string_view Path;
			std::string_view Side;
			/// [[BPR-1212]]. GitHub's multi-line fields, written only for a
			/// range: absent members are skipped, so a single line's bytes are
			/// exactly what they were. Keys stay in the sorted order of Qt's
			/// compact JSON that the durable fingerprints were sealed with.
			std::optional<std::uint64_t>    StartLine;
			std::optional<std::string_view> StartSide;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"body", &comment_submission_json::Body,
					"commit_id", &comment_submission_json::CommitId,
					"line", &comment_submission_json::Line,
					"path", &comment_submission_json::Path,
					"side", &comment_submission_json::Side,
					"start_line", &comment_submission_json::StartLine,
					"start_side", &comment_submission_json::StartSide);
			};
		};

		struct file_comment_submission_json
		{
			std::string Body;
			std::string CommitId;
			std::string Path;
			std::string SubjectType = "file";
			struct glaze
			{
				static constexpr auto value = glz::object("body", &file_comment_submission_json::Body, "commit_id", &file_comment_submission_json::CommitId, "path", &file_comment_submission_json::Path, "subject_type", &file_comment_submission_json::SubjectType);
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

		struct comment_response_json
		{
			std::uint64_t                Id = 0U;
			std::string                  NodeId;
			std::string                  Path;
			std::string                  CommitId;
			std::optional<std::string>   OriginalCommitId;
			std::optional<std::string>   Side;
			std::optional<std::uint64_t> Line;
			std::optional<std::uint64_t> OriginalLine;
			std::optional<std::string>   SubjectType;
			std::optional<std::uint64_t> StartLine;
			/// [[BPR-1538]]. The first line of a range where it was written.
			std::optional<std::uint64_t> OriginalStartLine;
			std::optional<std::string>   StartSide;
			std::optional<std::uint64_t> InReplyToId;
			std::string                  Body;
			review_author_json           User;
			/// [[BPR-1345]]. The review the comment belongs to.
			std::optional<std::uint64_t> ReviewId;
			/// [[BPR-1367]]. Where a tour's own comment is checked.
			std::optional<std::string> WebUrl;
			std::optional<std::string> PullRequestUrl;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"pull_request_review_id", &comment_response_json::ReviewId,
					"html_url", &comment_response_json::WebUrl,
					"pull_request_url", &comment_response_json::PullRequestUrl,
					"id", &comment_response_json::Id,
					"node_id", &comment_response_json::NodeId,
					"path", &comment_response_json::Path,
					"commit_id", &comment_response_json::CommitId,
					"original_commit_id", &comment_response_json::OriginalCommitId,
					"side", &comment_response_json::Side,
					"line", &comment_response_json::Line,
					"original_line", &comment_response_json::OriginalLine,
					"subject_type", &comment_response_json::SubjectType,
					"start_line", &comment_response_json::StartLine,
					"original_start_line", &comment_response_json::OriginalStartLine,
					"start_side", &comment_response_json::StartSide,
					"in_reply_to_id", &comment_response_json::InReplyToId,
					"body", &comment_response_json::Body,
					"user", &comment_response_json::User);
			};
		};

		// [[BPR-0657]]. The GraphQL envelopes, outbound and inbound.
		struct reply_variables_json
		{
			std::string_view Thread;
			std::string_view Body;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"thread", &reply_variables_json::Thread,
					"body", &reply_variables_json::Body);
			};
		};
		struct reply_query_json
		{
			std::string_view     Query;
			reply_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"query", &reply_query_json::Query,
					"variables", &reply_query_json::Variables);
			};
		};
		struct resolution_variables_json
		{
			std::string_view Thread;
			struct glaze
			{
				static constexpr auto value = glz::object("thread", &resolution_variables_json::Thread);
			};
		};
		struct resolution_query_json
		{
			std::string_view          Query;
			resolution_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"query", &resolution_query_json::Query,
					"variables", &resolution_query_json::Variables);
			};
		};
		// [[BPR-1368]]. The pending stop edit's envelope, outbound.
		struct update_comment_variables_json
		{
			std::string_view Comment;
			std::string_view Body;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"comment", &update_comment_variables_json::Comment,
					"body", &update_comment_variables_json::Body);
			};
		};
		struct update_comment_query_json
		{
			std::string_view              Query;
			update_comment_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"query", &update_comment_query_json::Query,
					"variables", &update_comment_query_json::Variables);
			};
		};
		struct reply_commit_json
		{
			std::string Oid;
			struct glaze
			{
				static constexpr auto value = glz::object("oid", &reply_commit_json::Oid);
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
		// [[BPR-0879]]. The pull request a thread-addressed write landed in, as
		// the server names it: node identities and database identities, which
		// no rename can move, beside the human-readable spelling.
		struct thread_repository_json
		{
			std::string   Id;
			std::uint64_t DatabaseId = 0U;
			std::string   NameWithOwner;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &thread_repository_json::Id,
					"databaseId", &thread_repository_json::DatabaseId,
					"nameWithOwner", &thread_repository_json::NameWithOwner);
			};
		};
		struct thread_pull_request_json
		{
			std::string            Id;
			std::uint64_t          DatabaseId = 0U;
			std::uint64_t          Number     = 0U;
			thread_repository_json Repository;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &thread_pull_request_json::Id,
					"databaseId", &thread_pull_request_json::DatabaseId,
					"number", &thread_pull_request_json::Number,
					"repository", &thread_pull_request_json::Repository);
			};
		};
		[[nodiscard]] auto binds_designated_pull_request(
			const thread_pull_request_json&                    aPullRequest,
			const application::review_mutation_target_binding& aTarget) -> bool
		{
			return aPullRequest.Id == aTarget.PullRequestNodeId
				&& aPullRequest.DatabaseId == aTarget.PullRequestDatabaseId
				&& aPullRequest.Number == aTarget.Number.value()
				&& aPullRequest.Repository.Id == aTarget.RepositoryNodeId
				&& aPullRequest.Repository.DatabaseId == aTarget.RepositoryDatabaseId
				&& aPullRequest.Repository.NameWithOwner
				== aTarget.Repository.Owner + "/" + aTarget.Repository.Name;
		}
		struct reply_comment_json
		{
			std::string              Id;
			std::uint64_t            DatabaseId = 0U;
			std::string              Body;
			std::string              Path;
			std::uint64_t            Line = 0U;
			reply_commit_json        Commit;
			reply_author_json        Author;
			bool                     ViewerDidAuthor = false;
			thread_pull_request_json PullRequest;
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
		// [[BPR-0896]]. The thread a reply landed in, with its comments, as
		// the reconciliation observes it. `line`, `commit` and
		// `fullDatabaseId` are nullable on the wire and the author's
		// identities exist only on the `User` fragment, so every one of them
		// is optional here: a comment that is not ours may carry anything the
		// schema allows and still be projected.
		struct thread_comment_author_json
		{
			std::string                  Login;
			std::optional<std::uint64_t> DatabaseId;
			std::optional<std::string>   Id;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"login", &thread_comment_author_json::Login,
					"databaseId", &thread_comment_author_json::DatabaseId,
					"id", &thread_comment_author_json::Id);
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
		// [[BPR-1368]]. The pending stop edit's answer. The author's
		// identities exist only on the `User` fragment.
		struct update_comment_review_json
		{
			std::string Id;
			std::string FullDatabaseId;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &update_comment_review_json::Id,
					"fullDatabaseId", &update_comment_review_json::FullDatabaseId);
			};
		};
		struct update_comment_node_json
		{
			std::string                Id;
			std::string                FullDatabaseId;
			std::string                Url;
			std::string                Body;
			std::string                State;
			bool                       ViewerDidAuthor = false;
			thread_comment_author_json Author;
			update_comment_review_json PullRequestReview;
			thread_pull_request_json   PullRequest;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &update_comment_node_json::Id,
					"fullDatabaseId", &update_comment_node_json::FullDatabaseId,
					"url", &update_comment_node_json::Url,
					"body", &update_comment_node_json::Body,
					"state", &update_comment_node_json::State,
					"viewerDidAuthor", &update_comment_node_json::ViewerDidAuthor,
					"author", &update_comment_node_json::Author,
					"pullRequestReview", &update_comment_node_json::PullRequestReview,
					"pullRequest", &update_comment_node_json::PullRequest);
			};
		};
		struct update_comment_payload_json
		{
			update_comment_node_json Comment;
			struct glaze
			{
				static constexpr auto value = glz::object("pullRequestReviewComment", &update_comment_payload_json::Comment);
			};
		};
		struct update_comment_data_json
		{
			update_comment_payload_json Update;
			struct glaze
			{
				static constexpr auto value = glz::object("updatePullRequestReviewComment", &update_comment_data_json::Update);
			};
		};
		struct update_comment_response_json
		{
			update_comment_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &update_comment_response_json::Data);
			};
		};
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

		struct commit_sha_json
		{
			std::string Sha;
			struct glaze
			{
				static constexpr auto value = glz::object("sha", &commit_sha_json::Sha);
			};
		};

		/// [[BPR-1560]]. GitHub's compare answer, of which only the echoed
		/// pair and the two commits are read.
		struct merge_base_response_json
		{
			std::string     Url;
			commit_sha_json BaseCommit;
			commit_sha_json MergeBaseCommit;
			/// [[BPR-1572]]. Unsigned: a negative count is malformed.
			std::optional<std::uint64_t> AheadBy;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"url", &merge_base_response_json::Url,
					"base_commit", &merge_base_response_json::BaseCommit,
					"merge_base_commit", &merge_base_response_json::MergeBaseCommit,
					"ahead_by", &merge_base_response_json::AheadBy);
			};
		};

		struct review_json_options : glz::opts
		{
			// Glaze discovers these overrides by their exact library-defined names.
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool escape_control_characters = true;
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool validate_skipped = true;
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool validate_trailing_whitespace = true;

			constexpr review_json_options()
			{
				error_on_unknown_keys = false;
				error_on_missing_keys = true;
				partial_read          = false;
			}
		};

		/// [[BPR-1538]]. The structural bounds for one page of the review list
		/// or the review comment collection: 100 objects of about 62 values
		/// each are 6,200 values, over the ordinary 4,096.
		constexpr auto list_preflight_limits = json_preflight_limits{ .Values = maximum_review_mutation_list_values };

		[[nodiscard]] auto publication_body(const application::review_mutation_request& aRequest) -> std::optional<std::string>
		{
			if (aRequest.Representation == core::review_publication_representation::native_line) {
				return aRequest.Proposal.Body;
			}
			if (aRequest.Representation != core::review_publication_representation::file_comment_with_permalink
				|| aRequest.Proposal.ReplyToThread || aRequest.Proposal.StartLine || !aRequest.Source || !aRequest.Source->Endpoint || !aRequest.LiveBinding
				|| aRequest.Source->Endpoint->path().generic_string() != aRequest.LiveBinding->Anchor.Path
				|| aRequest.Source->Comparison.Authority.Account != aRequest.ActingIdentity
				|| core::classify_review_body(aRequest.Proposal.Body) != core::review_body_compatibility::portable) {
				return std::nullopt;
			}
			return compile_review_permalink_body({ .Target     = aRequest.LiveBinding->Target,
												   .Endpoint   = aRequest.Source->Endpoint,
												   .Comparison = aRequest.Source->Comparison,
												   .LineCount  = aRequest.Source->LineCount },
												 aRequest.Proposal.Anchor, aRequest.Proposal.Body);
		}

		[[nodiscard]] auto mutation_body(const application::review_mutation_request& aRequest) -> std::string
		{
			const auto& binding = *aRequest.LiveBinding;
			const auto  commit  = core::canonical_hex(binding.Anchor.Commit);
			if (aRequest.Representation == core::review_publication_representation::file_comment_with_permalink) {
				const auto body = publication_body(aRequest);
				if (!body) {
					return { };
				}
				const auto object = file_comment_submission_json{ .Body = *body, .CommitId = std::string{ commit.view() }, .Path = binding.Anchor.Path };
				return glz::write<review_json_options{ }>(object).value_or(std::string{ });
			}
			const auto side   = binding.Anchor.Semantic.Side == core::diff_side::left ? std::string_view{ "LEFT" } : std::string_view{ "RIGHT" };
			auto       object = comment_submission_json{
				.Body     = aRequest.Proposal.Body,
				.CommitId = commit.view(),
				.Line     = static_cast<std::uint64_t>(binding.Anchor.Semantic.Line) + 1U,
				.Path     = binding.Anchor.Path,
				.Side     = side,
			};
			if (aRequest.Proposal.StartLine) {
				object.StartLine = static_cast<std::uint64_t>(*aRequest.Proposal.StartLine) + 1U;
				object.StartSide = side;
			}
			auto bytes = glz::write<review_json_options{ }>(object);
			if (!bytes) {
				return { };
			}
			// Preserve Qt compact JSON spelling in durable line-comment fingerprints.
			// Skip escaped backslashes so literal user text such as \u000B stays literal.
			for (std::size_t index = 0U; index + 1U < bytes->size(); ++index) {
				if ((*bytes)[index] != '\\') {
					continue;
				}
				++index;
				if ((*bytes)[index] == 'u' && index + 4U < bytes->size()) {
					for (auto digit = 0U; digit < 4U; ++digit) {
						auto& character = (*bytes)[++index];
						if (character >= 'A' && character <= 'F') {
							character = static_cast<char>(character + ('a' - 'A'));
						}
					}
				}
			}
			return std::move(*bytes);
		}

		// [[BPR-0657]]. The reply's GraphQL envelope: the reserved document
		// and the two variables, written by the same JSON rules as the REST
		// body so the durable fingerprint is byte-stable.
		[[nodiscard]] auto reply_body(const application::review_mutation_request& aRequest) -> std::string
		{
			const auto object = reply_query_json{
				.Query     = reply_document,
				.Variables = {
					.Thread = *aRequest.Proposal.ReplyToThread,
					.Body   = aRequest.Proposal.Body,
				},
			};
			auto bytes = glz::write<review_json_options{ }>(object);
			return bytes ? std::move(*bytes) : std::string{ };
		}

		[[nodiscard]] auto resolution_body(const application::review_thread_resolution_request& aRequest) -> std::string
		{
			const auto object = resolution_query_json{
				.Query     = aRequest.Resolve ? resolve_document : unresolve_document,
				.Variables = { .Thread = aRequest.Thread },
			};
			auto bytes = glz::write<review_json_options{ }>(object);
			return bytes ? std::move(*bytes) : std::string{ };
		}

		[[nodiscard]] auto submission_names(const core::review_disposition aDisposition)
			-> std::optional<review_submission_names>
		{
			switch (aDisposition) {
			case core::review_disposition::comment:
				return review_submission_names{ "COMMENT", "COMMENTED" };
			case core::review_disposition::approve:
				return review_submission_names{ "APPROVE", "APPROVED" };
			case core::review_disposition::request_changes:
				return review_submission_names{ "REQUEST_CHANGES", "CHANGES_REQUESTED" };
			}
			return std::nullopt;
		}

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

		[[nodiscard]] auto submitted_timestamp(const std::string_view aText) -> bool
		{
			if (aText.size() != 20U) {
				return false;
			}
			const auto value     = QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()));
			const auto timestamp = QDateTime::fromString(value, Qt::ISODate);
			return timestamp.isValid() && timestamp.toString(Qt::ISODate) == value
				&& aText.back() == 'Z';
		}

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

		void append_fingerprint_integer(std::string& aMaterial, const std::uint64_t aValue)
		{
			for (auto shift = 0U; shift < 64U; shift += 8U) {
				aMaterial.push_back(static_cast<char>((aValue >> shift) & 0xFFU));
			}
		}

		void append_fingerprint_text(
			std::string& aMaterial, const std::string_view aValue)
		{
			append_fingerprint_integer(aMaterial, aValue.size());
			aMaterial.append(aValue);
		}

		[[nodiscard]] auto canonical_write_fingerprint(
			const review_mutation_wire_request& aRequest)
			-> std::array<std::uint8_t, 32>
		{
			auto material = std::string{ };
			append_fingerprint_integer(
				material, static_cast<std::uint64_t>(aRequest.Server));
			append_fingerprint_integer(
				material, static_cast<std::uint64_t>(aRequest.Operation));
			append_fingerprint_integer(
				material, static_cast<std::uint64_t>(aRequest.Method));
			append_fingerprint_text(material, aRequest.Path);
			append_fingerprint_integer(material, aRequest.Headers.size());
			for (const auto& header : aRequest.Headers) {
				append_fingerprint_text(material, header.Name);
				append_fingerprint_text(material, header.Value);
			}
			append_fingerprint_text(material, aRequest.Body);
			return core::sha256(std::span{
				reinterpret_cast<const std::uint8_t*>(material.data()),
				material.size() });
		}

		void append_fingerprint_oid(
			std::string& aMaterial, const core::git_commit_oid& aOid)
		{
			append_fingerprint_integer(
				aMaterial, static_cast<std::uint64_t>(aOid.algorithm()));
			aMaterial.append(
				reinterpret_cast<const char*>(aOid.bytes().data()),
				aOid.bytes().size());
		}

		[[nodiscard]] auto canonical_authority_fingerprint(
			const application::review_mutation_live_binding& aBinding)
			-> std::array<std::uint8_t, 32>
		{
			const auto& target   = aBinding.Target;
			const auto& actor    = aBinding.ActingIdentity;
			auto        material = std::string{ };
			append_fingerprint_integer(material, target.Semantic.Host.value());
			append_fingerprint_integer(material, target.Semantic.Repository.value());
			append_fingerprint_integer(material, target.Semantic.PullRequest.value());
			append_fingerprint_oid(material, target.Semantic.ExpectedHead);
			append_fingerprint_text(material, target.Hostname.value());
			append_fingerprint_integer(
				material, static_cast<std::uint64_t>(target.ServerContract));
			append_fingerprint_text(material, target.Repository.Owner);
			append_fingerprint_text(material, target.Repository.Name);
			append_fingerprint_integer(material, target.Number.value());
			append_fingerprint_integer(material, target.RepositoryDatabaseId);
			append_fingerprint_text(material, target.RepositoryNodeId);
			append_fingerprint_integer(material, target.PullRequestDatabaseId);
			append_fingerprint_text(material, target.PullRequestNodeId);
			append_fingerprint_integer(material, target.BaseRepositoryDatabaseId);
			append_fingerprint_oid(material, target.BaseOid);
			// [[BPR-1560]]. Only a comparison base that differs from the
			// recorded base adds material, so every earlier fingerprint holds.
			if (application::comparison_base(target) != target.BaseOid) {
				append_fingerprint_oid(material, application::comparison_base(target));
			}
			append_fingerprint_text(material, target.BaseRef);
			append_fingerprint_text(material, target.HeadRef);
			append_fingerprint_integer(material, target.Open ? 1U : 0U);
			append_fingerprint_integer(material, actor.Account.value());
			append_fingerprint_integer(material, actor.DatabaseId);
			append_fingerprint_text(material, actor.NodeId);
			append_fingerprint_text(material, actor.Login);
			return core::sha256(std::span{
				reinterpret_cast<const std::uint8_t*>(material.data()),
				material.size() });
		}

		using diagnostic_json_object = std::map<std::string, glz::raw_json_view, std::less<>>;

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

		template <typename T>
		[[nodiscard]] auto document(const review_mutation_wire_response& aResponse) -> std::optional<T>
		{
			if (aResponse.Body.empty() || aResponse.Body.size() > maximum_review_mutation_response_bytes
				|| !json_preflight{ aResponse.Body }.run()) {
				return std::nullopt;
			}
			auto parsed = T{ };
			if (glz::read<review_json_options{ }>(parsed, aResponse.Body)) {
				return std::nullopt;
			}
			return parsed;
		}

		[[nodiscard]] auto valid_file_subject_metadata(const comment_response_json& aComment) -> bool
		{
			// GitHub file subjects may carry its synthetic first-line position.
			// This is receipt metadata; the permalink alone supplies our source anchor.
			return !aComment.StartLine && !aComment.StartSide && !aComment.InReplyToId
				&& (!aComment.Line || (aComment.Line == 1U && aComment.OriginalLine == 1U && aComment.Side == "RIGHT"));
		}

		[[nodiscard]] auto comment_observation(const comment_response_json&                aResponse,
											   const application::review_mutation_request& aExpected)
			-> review_mutation_parse_result<review_mutation_comment_observation>
		{
			const auto     commit            = core::parse_git_commit_oid(aResponse.CommitId);
			constexpr auto maximumGithubLine = static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1U;
			if (aResponse.Id == 0U || aResponse.User.Id == 0U || !commit
				|| !bounded_text(aResponse.NodeId, 256U)
				|| (aResponse.Line && (*aResponse.Line == 0U || *aResponse.Line > maximumGithubLine))) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			const auto& binding      = *aExpected.LiveBinding;
			const auto  expectedSide = binding.Anchor.Semantic.Side == core::diff_side::left ? "LEFT" : "RIGHT";
			const auto  fileSubject  = aResponse.SubjectType == "file";
			const auto  wantsFile    = aExpected.Representation == core::review_publication_representation::file_comment_with_permalink;
			const auto  expectedBody = publication_body(aExpected);
			if ((aResponse.SubjectType && *aResponse.SubjectType != "line" && !fileSubject)
				|| (fileSubject && !valid_file_subject_metadata(aResponse))
				|| (!fileSubject && !aResponse.Line)) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			// [[BPR-1212]]. A range is ours only with its exact first line on
			// the same side; a single line only without one, so a range ending
			// at the same line with the same text is never taken for it.
			const auto expectedStart = aExpected.Proposal.StartLine ? std::optional<std::uint64_t>{ static_cast<std::uint64_t>(*aExpected.Proposal.StartLine) + 1U } : std::nullopt;
			if (aResponse.Path != binding.Anchor.Path || *commit != binding.Anchor.Commit
				|| fileSubject != wantsFile || !expectedBody || aResponse.Body != *expectedBody
				|| (!fileSubject && (aResponse.Side != expectedSide || *aResponse.Line - 1U != binding.Anchor.Semantic.Line))
				|| (!fileSubject && (aResponse.StartLine != expectedStart || (expectedStart ? aResponse.StartSide != expectedSide : aResponse.StartSide.has_value())))
				|| aResponse.User.Id != binding.ActingIdentity.DatabaseId
				|| aResponse.User.NodeId != binding.ActingIdentity.NodeId
				|| aResponse.User.Login != binding.ActingIdentity.Login) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			// [[BPR-0816]]. Name the nested type: GCC 15 destroys a braced
			// aggregate member twice when a later member initializer throws.
			return review_mutation_comment_observation{
				.Receipt       = application::review_mutation_receipt{ .ServerCommentId = aResponse.NodeId, .Duplicate = false, .Representation = aExpected.Representation },
				.DatabaseId    = aResponse.Id,
				.NodeId        = aResponse.NodeId,
				.Anchor        = binding.Anchor,
				.Body          = aResponse.Body,
				.Author        = binding.ActingIdentity,
				.RemoteSubject = fileSubject ? application::review_remote_comment_subject::file : application::review_remote_comment_subject::line,
			};
		}

		// [[BPR-0657]]. A reply answered by GraphQL: the same identity checks a
		// REST comment gets, over the fields the mutation returns.
		[[nodiscard]] auto reply_observation(const reply_comment_json&                   aComment,
											 const application::review_mutation_request& aExpected)
			-> review_mutation_parse_result<review_mutation_comment_observation>
		{
			const auto     commit            = core::parse_git_commit_oid(aComment.Commit.Oid);
			constexpr auto maximumGithubLine = static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1U;
			if (aComment.Id.empty() || aComment.DatabaseId == 0U || !commit
				|| aComment.Line == 0U || aComment.Line > maximumGithubLine) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			const auto& binding      = *aExpected.LiveBinding;
			const auto  semanticLine = static_cast<std::uint32_t>(aComment.Line - 1U);
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
			const auto& author = aComment.Author;
			if (!databaseId.has_value() || *databaseId == 0U || !commit.has_value()
				|| !aComment.Line.has_value() || !author.has_value()
				|| !author->DatabaseId.has_value() || *author->DatabaseId == 0U
				|| !author->Id.has_value()) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			const auto& binding      = *aExpected.LiveBinding;
			const auto  expectedSide = binding.Anchor.Semantic.Side == core::diff_side::left ? "LEFT" : "RIGHT";
			const auto  semanticLine = static_cast<std::uint32_t>(*aComment.Line - 1U);
			if (aComment.Path != binding.Anchor.Path || *commit != binding.Anchor.Commit
				|| aDiffSide != expectedSide || semanticLine != binding.Anchor.Semantic.Line
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

		[[nodiscard]] auto response_headers_valid(
			const review_mutation_wire_response& aResponse) noexcept -> bool
		{
			return aResponse.Headers.size() <= 64U
				&& std::ranges::all_of(aResponse.Headers, [](const http_header& aHeader) {
					   return bounded_text(aHeader.Name, 256U)
						   && bounded_text(aHeader.Value, 8U * 1024U)
						   && aHeader.Name != "Location" && aHeader.Name != "location";
				   });
		}

		[[nodiscard]] auto has_header(
			const review_mutation_wire_response& aResponse,
			const std::string_view               aName) noexcept -> bool
		{
			return std::ranges::any_of(aResponse.Headers, [aName](const http_header& aHeader) {
				return aHeader.Name.size() == aName.size()
					&& std::ranges::equal(
						   aHeader.Name,
						   aName,
						   [](const unsigned char aLeft, const unsigned char aRight) {
							   return std::tolower(aLeft) == std::tolower(aRight);
						   });
			});
		}

		[[nodiscard]] auto same_attempt(
			const application::review_mutation_recovery_record& aLeft,
			const application::review_mutation_recovery_record& aRight) noexcept
			-> bool
		{
			return aLeft.IdempotencyKey == aRight.IdempotencyKey
				&& aLeft.RequestFingerprint == aRight.RequestFingerprint
				&& aLeft.AuthorityFingerprint == aRight.AuthorityFingerprint
				&& aLeft.Body == aRight.Body && aLeft.Anchor == aRight.Anchor;
		}

		[[nodiscard]] auto exact_comment_ids(
			const review_mutation_server_reconciliation& aObservation)
			-> std::vector<std::string>
		{
			auto result = std::vector<std::string>{ };
			result.reserve(aObservation.ExactReceipts.size());
			std::ranges::transform(
				aObservation.ExactReceipts,
				std::back_inserter(result),
				&application::review_mutation_receipt::ServerCommentId);
			return result;
		}

		[[nodiscard]] auto new_exact_receipt(
			const review_mutation_server_reconciliation& aObservation,
			const std::vector<std::string>&              aBaseline)
			-> std::optional<application::review_mutation_receipt>
		{
			auto result = std::optional<application::review_mutation_receipt>{ };
			for (const auto& receipt : aObservation.ExactReceipts) {
				if (std::ranges::binary_search(aBaseline, receipt.ServerCommentId)) {
					continue;
				}
				if (result.has_value()) {
					return std::nullopt;
				}
				result = receipt;
			}
			return result;
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

		/// [[BPR-0879]]. A reply is addressed by a thread identity exactly as a
		/// resolution is, so it is observed by exactly the same read. The read
		/// document names no state, so `Resolve` is carried unread.
		[[nodiscard]] auto thread_observation_subject(
			const application::review_mutation_request& aRequest)
			-> application::review_thread_resolution_request
		{
			return {
				.Target         = aRequest.Target,
				.ActingIdentity = aRequest.ActingIdentity,
				.Thread         = aRequest.Proposal.ReplyToThread.value_or(std::string{ }),
				.Resolve        = true,
				.LiveBinding    = aRequest.LiveBinding,
			};
		}

		[[nodiscard]] auto map_contract_failure(
			const review_mutation_contract_failure aFailure) noexcept
			-> application::review_mutation_failure
		{
			switch (aFailure) {
			case review_mutation_contract_failure::identity_mismatch:
			case review_mutation_contract_failure::invalid_request:
				return application::review_mutation_failure::undesignated_target;
			case review_mutation_contract_failure::target_unavailable:
				return application::review_mutation_failure::target_unavailable;
			case review_mutation_contract_failure::malformed_response:
				return application::review_mutation_failure::malformed_response;
			default:
				return application::review_mutation_failure::unavailable;
			}
		}
	} // namespace

	namespace {
		using staged_operation = application::staged_review_operation;
		struct staged_line_json
		{
			std::string                  Path, Body, Side;
			std::uint64_t                Line = 0;
			std::optional<std::uint64_t> StartLine;
			std::optional<std::string>   StartSide;
			struct glaze
			{
				static constexpr auto value = glz::object("path", &staged_line_json::Path, "body", &staged_line_json::Body, "line", &staged_line_json::Line, "side", &staged_line_json::Side, "start_line", &staged_line_json::StartLine, "start_side", &staged_line_json::StartSide);
			};
		};
		struct staged_batch_json
		{
			std::string                   Body, Commit;
			std::vector<staged_line_json> Comments;
			struct glaze
			{
				static constexpr auto value = glz::object("body", &staged_batch_json::Body, "commit_id", &staged_batch_json::Commit, "comments", &staged_batch_json::Comments);
			};
		};
		struct staged_body_json
		{
			std::string                Body;
			std::optional<std::string> Event;
			struct glaze
			{
				static constexpr auto value = glz::object("body", &staged_body_json::Body, "event", &staged_body_json::Event);
			};
		};
		struct staged_review_json
		{
			std::uint64_t              Id = 0;
			std::string                NodeId, WebUrl, PullRequestUrl, Body, Commit, State;
			review_author_json         User;
			std::optional<std::string> SubmittedAt;
			struct glaze
			{
				static constexpr auto value = glz::object("id", &staged_review_json::Id, "node_id", &staged_review_json::NodeId, "html_url", &staged_review_json::WebUrl, "pull_request_url", &staged_review_json::PullRequestUrl, "body", &staged_review_json::Body, "commit_id", &staged_review_json::Commit, "state", &staged_review_json::State, "user", &staged_review_json::User, "submitted_at", &staged_review_json::SubmittedAt);
			};
		};
		struct staged_comment_json
		{
			std::uint64_t                Id = 0, Review = 0;
			std::string                  NodeId, WebUrl, PullRequestUrl, Body, Path, Commit;
			review_author_json           User;
			std::optional<std::uint64_t> Line, StartLine, Reply;
			std::optional<std::string>   Side, StartSide, Subject;
			struct glaze
			{
				static constexpr auto value = glz::object("id", &staged_comment_json::Id, "node_id", &staged_comment_json::NodeId, "html_url", &staged_comment_json::WebUrl, "pull_request_url", &staged_comment_json::PullRequestUrl, "body", &staged_comment_json::Body, "path", &staged_comment_json::Path, "commit_id", &staged_comment_json::Commit, "user", &staged_comment_json::User, "pull_request_review_id", &staged_comment_json::Review, "line", &staged_comment_json::Line, "start_line", &staged_comment_json::StartLine, "side", &staged_comment_json::Side, "start_side", &staged_comment_json::StartSide, "subject_type", &staged_comment_json::Subject, "in_reply_to_id", &staged_comment_json::Reply);
			};
		};
		struct staged_issue_comment_json
		{
			std::uint64_t      Id = 0;
			std::string        NodeId, WebUrl, IssueUrl, Body;
			review_author_json User;
			struct glaze
			{
				static constexpr auto value = glz::object("id", &staged_issue_comment_json::Id, "node_id", &staged_issue_comment_json::NodeId, "html_url", &staged_issue_comment_json::WebUrl, "issue_url", &staged_issue_comment_json::IssueUrl, "body", &staged_issue_comment_json::Body, "user", &staged_issue_comment_json::User);
			};
		};
		[[nodiscard]] auto staged_api_origin(const application::review_mutation_target_binding& aTarget) -> std::string
		{
			return "https://" + (aTarget.ServerContract == application::github_server_contract::github_com_2022_11_28 ? std::string{ "api.github.com" } : std::string{ aTarget.Hostname.value() });
		}
		[[nodiscard]] auto staged_web_root(const application::review_mutation_target_binding& aTarget) -> std::string
		{
			return "https://" + std::string{ aTarget.Hostname.value() } + "/" + aTarget.Repository.Owner + "/" + aTarget.Repository.Name + "/pull/" + std::to_string(aTarget.Number.value());
		}
		[[nodiscard]] auto staged_actor(const review_author_json& aActor) -> std::optional<application::staged_review_actor>
		{
			if (!aActor.Id || !bounded_text(aActor.NodeId, 256) || !bounded_text(aActor.Login, 256)) {
				return std::nullopt;
			}
			return application::staged_review_actor{ aActor.Id, aActor.NodeId, aActor.Login };
		}
		[[nodiscard]] auto staged_list(staged_operation aOperation) -> bool
		{
			return aOperation == staged_operation::list_reviews || aOperation == staged_operation::list_comments || aOperation == staged_operation::list_issue_comments;
		}
		[[nodiscard]] auto staged_page_suffix(std::uint32_t aPage) -> std::string
		{
			return "?per_page=100&page=" + std::to_string(aPage);
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
		/// [[BPR-1367]]. Whether the `Link` header of page `aPage` of the list at
		/// `aWirePath` links a next page. Every link must name a page of that
		/// same list on the expected API origin, at most `aMaximum`; a list
		/// that is not paged (`aListed` false) may carry none.
		[[nodiscard]] auto paged_next(const review_mutation_wire_response& aResponse, const application::review_mutation_target_binding& aTarget, std::string_view aWirePath, std::uint32_t aPage, std::uint32_t aMaximum, bool aListed) -> std::optional<bool>
		{
			bool next = false, seen = false;
			for (const auto& header : aResponse.Headers) {
				if (header.Name.size() != 4 || !std::ranges::equal(header.Name, std::string_view{ "link" }, [](unsigned char aLeft, unsigned char aRight) { return std::tolower(aLeft) == std::tolower(aRight); })) {
					continue;
				}
				if (seen || !aListed) {
					return std::nullopt;
				}
				seen             = true;
				const auto links = parse_link_header(header.Value);
				if (!links) {
					return std::nullopt;
				}
				std::vector<std::string_view> relations;
				for (const auto& link : *links) {
					// Exactly `<url>; rel="relation"`, one relation per link.
					const auto names = link_relations(link);
					if (link.Parameters.size() != 1 || !link.Parameters.front().Quoted || !names || names->size() != 1) {
						return std::nullopt;
					}
					const auto url      = link.Target;
					const auto relation = names->front();
					if (std::ranges::find(relations, relation) != relations.end()) {
						return std::nullopt;
					}
					relations.push_back(relation);
					const auto prefix = staged_api_origin(aTarget) + std::string{ aWirePath.substr(0, aWirePath.find('?')) };
					// [[BPR-1538]]. GitHub names the next page by the
					// repository's number, not its name:
					// `https://api.github.com/repositories/1328277631/pulls/70/comments?per_page=100&page=2`
					// on MikeMarcin/BPR-Test#70, under `/api/v3` on GHES. Only
					// this repository's number names the same list.
					const auto byName   = std::string{ rest_prefix(aTarget.ServerContract) } + "/repos/" + percent_encode(aTarget.Repository.Owner) + "/" + percent_encode(aTarget.Repository.Name) + "/";
					const auto path     = std::string{ aWirePath.substr(0, aWirePath.find('?')) };
					const auto byNumber = path.starts_with(byName)
						? std::optional{ staged_api_origin(aTarget) + std::string{ rest_prefix(aTarget.ServerContract) } + "/repositories/" + std::to_string(aTarget.RepositoryDatabaseId) + "/" + path.substr(byName.size()) }
						: std::nullopt;
					// [[BPR-1540]]. The page a link names, read from it: a list
					// longer than the bound names its last page past it.
					const auto namedPage = [&url](const std::string& aPrefix) -> std::optional<std::uint32_t> {
						if (!url.starts_with(aPrefix)) {
							return std::nullopt;
						}
						const auto rest = url.substr(aPrefix.size());
						for (const auto& [lead, tail] : std::array<std::pair<std::string_view, std::string_view>, 2>{ { { "?per_page=100&page=", "" }, { "?page=", "&per_page=100" } } }) {
							if (!rest.starts_with(lead) || !rest.ends_with(tail) || rest.size() <= lead.size() + tail.size()) {
								continue;
							}
							const auto digits = rest.substr(lead.size(), rest.size() - lead.size() - tail.size());
							auto       number = std::uint32_t{ 0U };
							const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), number);
							if (parsed.ec == std::errc{ } && parsed.ptr == digits.data() + digits.size() && digits.front() != '0' && number <= 1'000'000U) {
								return number;
							}
						}
						return std::nullopt;
					};
					auto page = namedPage(prefix);
					if (!page && byNumber && aTarget.RepositoryDatabaseId != 0U) {
						page = namedPage(*byNumber);
					}
					if (page && relation != "last" && *page > aMaximum) {
						page.reset();
					}
					if (!page) {
						return std::nullopt;
					}
					if (relation == "next") {
						if (*page != aPage + 1) {
							return std::nullopt;
						}
						next = true;
					} else if (relation == "prev") {
						if (*page + 1 != aPage) {
							return std::nullopt;
						}
					} else if (relation == "first") {
						if (*page != 1) {
							return std::nullopt;
						}
					} else if (relation == "last") {
						if (*page < aPage) {
							return std::nullopt;
						}
					} else {
						return std::nullopt;
					}
				}
			}
			if (next && aPage >= aMaximum) {
				return std::nullopt;
			}
			return next;
		}
		[[nodiscard]] auto staged_next(const review_mutation_wire_response& aResponse, const application::staged_review_request& aExpected, const review_mutation_wire_request& aWire) -> std::optional<bool>
		{
			return paged_next(aResponse, aExpected.Binding.Target, aWire.Path, aExpected.Page, application::maximum_staged_review_pages, staged_list(aExpected.Operation));
		}
		[[nodiscard]] auto staged_review_entity(const staged_review_json& aReview, const application::staged_review_request& aExpected) -> std::optional<application::staged_review_entity>
		{
			const auto& target = aExpected.Binding.Target;
			const auto  actor  = staged_actor(aReview.User);
			const auto  commit = core::parse_git_commit_oid(aReview.Commit);
			if (!actor || !commit || !aReview.Id || !bounded_text(aReview.NodeId, 256) || !utf8_round_trip(aReview.Body)
				|| aReview.Body.size() > maximum_review_mutation_body_bytes
				|| aReview.PullRequestUrl != staged_api_origin(target) + pull_request_path(target, target.ServerContract)
				|| aReview.WebUrl != staged_web_root(target) + "#pullrequestreview-" + std::to_string(aReview.Id)) {
				return std::nullopt;
			}
			using state = application::staged_review_state;
			std::optional<state> status;
			if (aReview.State == "PENDING") {
				if (aReview.SubmittedAt) {
					return std::nullopt;
				}
				status = state::pending;
			} else if (aReview.SubmittedAt && submitted_timestamp(*aReview.SubmittedAt)) {
				if (aReview.State == "COMMENTED") {
					status = state::commented;
				}
				if (aReview.State == "APPROVED") {
					status = state::approved;
				}
				if (aReview.State == "CHANGES_REQUESTED") {
					status = state::changes_requested;
				}
				if (aReview.State == "DISMISSED") {
					status = state::dismissed;
				}
			}
			if (!status || (aExpected.Review.value() && aExpected.Review.value() != aReview.Id)) {
				return std::nullopt;
			}
			return application::staged_review_entity{ application::review_database_id{ aReview.Id }, aReview.NodeId, aReview.WebUrl, *actor, *commit, aReview.Body, *status };
		}
		/// `aLines` false reads GitHub's list of one review's comments, which
		/// has no `line`, `side`, `start_line`, `start_side` or `subject_type`
		/// ([[BPR-1367]], probed on MikeMarcin/BPR-Test#45): lines and sides
		/// it may carry are not read, and the comment has no source.
		[[nodiscard]] auto staged_comment_entity(const staged_comment_json& aComment, const application::staged_review_request& aExpected, const bool aLines = true) -> std::optional<application::staged_review_comment>
		{
			const auto& target = aExpected.Binding.Target;
			const auto  actor  = staged_actor(aComment.User);
			const auto  commit = core::parse_git_commit_oid(aComment.Commit);
			if (!actor || !commit || *commit != target.Semantic.ExpectedHead || !aComment.Id || !aComment.Review || !bounded_text(aComment.NodeId, 256)
				|| !utf8_round_trip(aComment.Body) || aComment.Body.size() > maximum_review_mutation_body_bytes
				|| aComment.PullRequestUrl != staged_api_origin(target) + pull_request_path(target, target.ServerContract)
				|| aComment.WebUrl != staged_web_root(target) + "#discussion_r" + std::to_string(aComment.Id)
				|| (aExpected.Review.value() && aExpected.Review.value() != aComment.Review)
				|| (aExpected.Comment && aExpected.Comment != aComment.Id)
				|| (aComment.Subject && *aComment.Subject != "line")) {
				return std::nullopt;
			}
			if (!aLines) {
				if (aComment.Path.empty() || !utf8_round_trip(aComment.Path)) {
					return std::nullopt;
				}
				return application::staged_review_comment{ aComment.Id, aComment.NodeId, aComment.WebUrl, *actor, application::review_database_id{ aComment.Review }, std::nullopt, aComment.Body, aComment.Reply.has_value() };
			}
			constexpr auto maximumLine = std::uint64_t{ std::numeric_limits<std::uint32_t>::max() } + 1;
			if (!aComment.Line || *aComment.Line == 0 || *aComment.Line > maximumLine
				|| (aComment.Side != "LEFT" && aComment.Side != "RIGHT")
				|| aComment.StartLine.has_value() != aComment.StartSide.has_value()
				|| (aComment.StartLine && (*aComment.StartLine == 0 || *aComment.StartLine >= *aComment.Line || aComment.StartSide != aComment.Side))) {
				return std::nullopt;
			}
			const auto source = core::tour_code_destination{ .Commit = aComment.Side == "LEFT" ? application::comparison_base(target) : *commit, .Path = aComment.Path, .Side = aComment.Side == "LEFT" ? core::diff_side::left : core::diff_side::right, .FirstLine = static_cast<std::uint32_t>(aComment.StartLine.value_or(*aComment.Line) - 1), .LastLine = static_cast<std::uint32_t>(*aComment.Line - 1) };
			if (!core::valid(source)) {
				return std::nullopt;
			}
			return application::staged_review_comment{ aComment.Id, aComment.NodeId, aComment.WebUrl, *actor, application::review_database_id{ aComment.Review }, source, aComment.Body, aComment.Reply.has_value() };
		}
	}

	auto compile_staged_review_request(const application::staged_review_request& aRequest) -> review_mutation_request_result
	{
		const auto& target = aRequest.Binding.Target;
		if (!application::valid(aRequest.Binding) || !valid_server(target.ServerContract) || !utf8_round_trip(aRequest.Body)
			|| aRequest.Body.size() > maximum_review_mutation_body_bytes || aRequest.Page == 0 || aRequest.Page > application::maximum_staged_review_pages
			|| (aRequest.Operation != staged_operation::create_pending && !aRequest.Lines.empty())
			|| (!staged_list(aRequest.Operation) && aRequest.Page != 1)
			|| (aRequest.Operation != staged_operation::update_pending_comment && !aRequest.CommentNode.empty())) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aRequest.Operation == staged_operation::update_pending_comment) {
			// [[BPR-1368]]. The one GraphQL operation of the catalog: the fixed
			// document, addressed by the comment's node ID, with the REST
			// review and comment identities its answer must name.
			if (!aRequest.Review.value() || !aRequest.Comment || !graphql_token(aRequest.CommentNode) || aRequest.Body.empty()) {
				return failed(review_mutation_contract_failure::invalid_request);
			}
			auto body = glz::write<review_json_options{ }>(update_comment_query_json{ .Query = update_review_comment_document, .Variables = { .Comment = aRequest.CommentNode, .Body = aRequest.Body } });
			if (!body || body->size() > maximum_review_mutation_body_bytes) {
				return failed(review_mutation_contract_failure::response_limit);
			}
			auto headers = common_headers();
			headers.push_back({ .Name = "Content-Type", .Value = "application/json" });
			return review_mutation_wire_request{ .Semantic = aRequest, .Server = target.ServerContract, .Operation = review_mutation_operation::staged_update_pending_comment, .Method = http_method::post, .Path = std::string{ graphql_path(target.ServerContract) }, .Headers = std::move(headers), .Body = std::move(*body) };
		}
		auto                      headers        = common_headers();
		auto                      path           = pull_request_path(target, target.ServerContract);
		const auto                repositoryPath = path.substr(0, path.rfind("/pulls/"));
		review_mutation_operation operation{ };
		std::string               body;
		bool                      wantsReview = false, wantsComment = false;
		switch (aRequest.Operation) {
		case staged_operation::list_reviews:
			operation = review_mutation_operation::staged_list_reviews;
			path += "/reviews" + staged_page_suffix(aRequest.Page);
			break;
		case staged_operation::read_review:
			operation   = review_mutation_operation::staged_read_review;
			wantsReview = true;
			break;
		case staged_operation::list_comments:
			operation   = review_mutation_operation::staged_list_comments;
			wantsReview = true;
			break;
		case staged_operation::create_pending: {
			operation = review_mutation_operation::staged_create_pending;
			if (aRequest.Lines.empty() || aRequest.Lines.size() > application::maximum_staged_review_comments) {
				return failed(review_mutation_contract_failure::invalid_request);
			}
			staged_batch_json batch{ .Body = aRequest.Body, .Commit = std::string{ core::canonical_hex(target.Semantic.ExpectedHead).view() } };
			for (const auto& line : aRequest.Lines) {
				if (!core::valid(line.Source) || line.Source.Blob || line.Body.empty() || !utf8_round_trip(line.Body) || line.Body.size() > maximum_review_mutation_body_bytes
					|| line.Source.Commit != (line.Source.Side == core::diff_side::right ? target.Semantic.ExpectedHead : application::comparison_base(target))) {
					return failed(review_mutation_contract_failure::invalid_request);
				}
				staged_line_json value{ .Path = line.Source.Path, .Body = line.Body, .Side = line.Source.Side == core::diff_side::right ? "RIGHT" : "LEFT", .Line = std::uint64_t{ line.Source.LastLine } + 1 };
				if (line.Source.FirstLine != line.Source.LastLine) {
					value.StartLine = std::uint64_t{ line.Source.FirstLine } + 1;
					value.StartSide = value.Side;
				}
				batch.Comments.push_back(std::move(value));
			}
			auto bytes = glz::write<review_json_options{ }>(batch);
			if (!bytes) {
				return failed(review_mutation_contract_failure::invalid_request);
			}
			body = std::move(*bytes);
			path += "/reviews";
			break;
		}
		case staged_operation::update_comment:
			operation    = review_mutation_operation::staged_update_comment;
			wantsComment = true;
			path         = repositoryPath + "/pulls/comments/" + std::to_string(aRequest.Comment);
			break;
		case staged_operation::update_summary:
			operation   = review_mutation_operation::staged_update_summary;
			wantsReview = true;
			break;
		case staged_operation::submit_comment:
			operation   = review_mutation_operation::staged_submit_comment;
			wantsReview = true;
			break;
		case staged_operation::discard_pending:
			operation   = review_mutation_operation::staged_discard_pending;
			wantsReview = true;
			break;
		case staged_operation::update_description: operation = review_mutation_operation::staged_update_description; break;
		case staged_operation::list_issue_comments:
			operation = review_mutation_operation::staged_list_issue_comments;
			path      = repositoryPath + "/issues/" + std::to_string(target.Number.value()) + "/comments" + staged_page_suffix(aRequest.Page);
			break;
		case staged_operation::create_issue_comment:
			operation = review_mutation_operation::staged_create_issue_comment;
			path      = repositoryPath + "/issues/" + std::to_string(target.Number.value()) + "/comments";
			break;
		case staged_operation::update_issue_comment:
			operation    = review_mutation_operation::staged_update_issue_comment;
			wantsComment = true;
			path         = repositoryPath + "/issues/comments/" + std::to_string(aRequest.Comment);
			break;
		default: return failed(review_mutation_contract_failure::invalid_request);
		}
		if (wantsReview != static_cast<bool>(aRequest.Review.value()) || wantsComment != (aRequest.Comment != 0)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (wantsReview) {
			path += "/reviews/" + std::to_string(aRequest.Review.value());
		}
		if (aRequest.Operation == staged_operation::list_comments) {
			path += "/comments" + staged_page_suffix(aRequest.Page);
		}
		if (aRequest.Operation == staged_operation::submit_comment) {
			path += "/events";
		}
		const auto exchange = review_mutation_exchange_for(operation);
		if (!exchange) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (exchange->Method == http_method::get || exchange->Method == http_method::delete_) {
			if (!aRequest.Body.empty()) {
				return failed(review_mutation_contract_failure::invalid_request);
			}
		} else {
			if (body.empty()) {
				if (aRequest.Body.empty()) {
					return failed(review_mutation_contract_failure::invalid_request);
				}
				const staged_body_json value{ aRequest.Body, aRequest.Operation == staged_operation::submit_comment ? std::optional<std::string>{ "COMMENT" } : std::nullopt };
				const auto             bytes = glz::write<review_json_options{ }>(value);
				if (!bytes) {
					return failed(review_mutation_contract_failure::invalid_request);
				}
				body = *bytes;
			}
			headers.push_back({ .Name = "Content-Type", .Value = "application/json" });
		}
		if (body.size() > maximum_review_mutation_body_bytes) {
			return failed(review_mutation_contract_failure::response_limit);
		}
		return review_mutation_wire_request{ .Semantic = aRequest, .Server = target.ServerContract, .Operation = operation, .Method = exchange->Method, .Path = std::move(path), .Headers = std::move(headers), .Body = std::move(body) };
	}
	auto parse_staged_review_response(const review_mutation_wire_response& aResponse, const application::staged_review_request& aExpected) -> review_mutation_parse_result<application::staged_review_observation>
	{
		const auto wire = compile_staged_review_request(aExpected);
		if (!wire) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		const auto exchange = review_mutation_exchange_for(wire->Operation);
		if (has_header(aResponse, "location") || (aResponse.Status >= 300 && aResponse.Status < 400)) {
			return failed(review_mutation_contract_failure::redirect);
		}
		if (aResponse.Status != exchange->SuccessfulStatus || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto next = staged_next(aResponse, aExpected, *wire);
		if (!next) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		application::staged_review_observation result{ .HasNext = *next };
		if (aExpected.Operation == staged_operation::update_description) {
			const auto target = parse_target_observation(aResponse, aExpected.Binding.Target);
			const auto body   = document<staged_body_json>(aResponse);
			if (!target || !body || !utf8_round_trip(body->Body) || body->Body.size() > maximum_review_mutation_body_bytes) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			result.Description = body->Body;
			return result;
		}
		if (aExpected.Operation == staged_operation::list_issue_comments || aExpected.Operation == staged_operation::create_issue_comment || aExpected.Operation == staged_operation::update_issue_comment) {
			std::optional<std::vector<staged_issue_comment_json>> objects;
			if (staged_list(aExpected.Operation)) {
				objects = document<std::vector<staged_issue_comment_json>>(aResponse);
			} else if (auto item = document<staged_issue_comment_json>(aResponse)) {
				objects = std::vector<staged_issue_comment_json>{ *item };
			}
			if (!objects || objects->size() > 100 || (*next && objects->empty())) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			const auto& target   = aExpected.Binding.Target;
			const auto  path     = pull_request_path(target, target.ServerContract);
			const auto  issueUrl = staged_api_origin(target) + path.substr(0, path.rfind("/pulls/")) + "/issues/" + std::to_string(target.Number.value());
			for (const auto& object : *objects) {
				const auto actor = staged_actor(object.User);
				if (!actor || !object.Id || !bounded_text(object.NodeId, 256) || !utf8_round_trip(object.Body) || object.Body.size() > maximum_review_mutation_body_bytes
					|| object.IssueUrl != issueUrl || object.WebUrl != staged_web_root(target) + "#issuecomment-" + std::to_string(object.Id)
					|| (aExpected.Comment && object.Id != aExpected.Comment)
					|| std::ranges::any_of(result.Comments, [&](const auto& aComment) { return aComment.Id == object.Id || aComment.NodeId == object.NodeId; })) {
					return failed(review_mutation_contract_failure::identity_mismatch);
				}
				result.Comments.push_back(application::staged_review_comment{ .Id = object.Id, .NodeId = object.NodeId, .WebUrl = object.WebUrl, .Actor = *actor, .Body = object.Body });
			}
			return result;
		}
		if (aExpected.Operation == staged_operation::update_pending_comment) {
			// [[BPR-1368]]. The edited comment is the one addressed, in the
			// review and pull request expected, still pending, mine, with the
			// body sent.
			const auto answer = document<update_comment_response_json>(aResponse);
			if (!answer) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			const auto& comment = answer->Data.Update.Comment;
			const auto& target  = aExpected.Binding.Target;
			const auto& who     = aExpected.Binding.ActingIdentity;
			if (comment.Id != aExpected.CommentNode || comment.FullDatabaseId != std::to_string(aExpected.Comment)
				|| comment.PullRequestReview.FullDatabaseId != std::to_string(aExpected.Review.value())
				|| comment.Url != staged_web_root(target) + "#discussion_r" + std::to_string(aExpected.Comment)
				|| comment.Body != aExpected.Body || comment.State != "PENDING" || !comment.ViewerDidAuthor
				|| comment.Author.Login != who.Login || comment.Author.DatabaseId != std::optional{ who.DatabaseId } || comment.Author.Id != std::optional{ who.NodeId }
				|| !binds_designated_pull_request(comment.PullRequest, target)) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			result.Comments.push_back(application::staged_review_comment{ .Id = aExpected.Comment, .NodeId = comment.Id, .WebUrl = comment.Url, .Actor = { who.DatabaseId, who.NodeId, who.Login }, .Review = aExpected.Review, .Body = comment.Body });
			return result;
		}
		const bool comments = aExpected.Operation == staged_operation::list_comments || aExpected.Operation == staged_operation::update_comment;
		if (comments) {
			std::optional<std::vector<staged_comment_json>> objects;
			if (staged_list(aExpected.Operation)) {
				objects = document<std::vector<staged_comment_json>>(aResponse);
			} else if (auto item = document<staged_comment_json>(aResponse)) {
				objects = std::vector<staged_comment_json>{ *item };
			}
			if (!objects || objects->size() > 100 || (*next && objects->empty())) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			for (const auto& object : *objects) {
				auto value = staged_comment_entity(object, aExpected, aExpected.Operation != staged_operation::list_comments);
				if (!value || std::ranges::any_of(result.Comments, [&](const auto& aComment) { return aComment.Id == value->Id || aComment.NodeId == value->NodeId; })) {
					return failed(review_mutation_contract_failure::identity_mismatch);
				}
				result.Comments.push_back(std::move(*value));
			}
		} else {
			std::optional<std::vector<staged_review_json>> objects;
			if (staged_list(aExpected.Operation)) {
				objects = document<std::vector<staged_review_json>>(aResponse);
			} else if (auto item = document<staged_review_json>(aResponse)) {
				objects = std::vector<staged_review_json>{ *item };
			}
			if (!objects || objects->size() > 100 || (*next && objects->empty())) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			for (const auto& object : *objects) {
				auto value = staged_review_entity(object, aExpected);
				if (!value || std::ranges::any_of(result.Reviews, [&](const auto& aReview) { return aReview.Id == value->Id || aReview.NodeId == value->NodeId; })) {
					return failed(review_mutation_contract_failure::identity_mismatch);
				}
				result.Reviews.push_back(std::move(*value));
			}
		}
		return result;
	}

	auto maximum_review_mutation_wire_response_bytes(const review_mutation_operation aOperation) noexcept -> std::size_t
	{
		return aOperation == review_mutation_operation::observe_reviews || aOperation == review_mutation_operation::observe_review_comments
			? maximum_review_mutation_list_response_bytes
			: maximum_review_mutation_response_bytes;
	}

	auto review_mutation_exchange_for(const review_mutation_operation aOperation) noexcept
		-> std::optional<review_mutation_exchange>
	{
		using operation = review_mutation_operation;
		switch (aOperation) {
		case operation::staged_list_reviews:
		case operation::staged_read_review:
		case operation::staged_list_comments:
		case operation::staged_list_issue_comments:
			return review_mutation_exchange{ };
		case operation::staged_create_pending:
		case operation::staged_submit_comment:
			return review_mutation_exchange{ .Method = http_method::post, .Witnessed = true };
		case operation::staged_create_issue_comment:
			return review_mutation_exchange{ .Method = http_method::post, .SuccessfulStatus = 201, .Witnessed = true };
		case operation::staged_update_summary:
			return review_mutation_exchange{ .Method = http_method::put, .Witnessed = true };
		case operation::staged_update_comment:
		case operation::staged_update_description:
		case operation::staged_update_issue_comment:
			return review_mutation_exchange{ .Method = http_method::patch, .Witnessed = true };
		case operation::staged_discard_pending:
			return review_mutation_exchange{ .Method = http_method::delete_, .Witnessed = true };
		case operation::staged_update_pending_comment:
			return review_mutation_exchange{ .Method = http_method::post, .Graphql = true, .Witnessed = true };
		case operation::observe_pull_request:
		case operation::observe_viewer:
		case operation::observe_review_comments:
		case operation::observe_reviews:
		case operation::observe_merge_base:
		case operation::observe_recorded_merge_base:
		case operation::observe_base_ancestry:
			return review_mutation_exchange{ };
		case operation::create_review_comment:
			return review_mutation_exchange{
				.Method           = http_method::post,
				.SuccessfulStatus = 201,
				.Witnessed        = true,
			};
		case operation::create_review:
			return review_mutation_exchange{
				.Method    = http_method::post,
				.Witnessed = true,
			};
		case operation::reply_review_thread:
			return review_mutation_exchange{
				.Method    = http_method::post,
				.Graphql   = true,
				.Witnessed = true,
			};
		// A resolution keeps no recovery record ([[BPR-0657]]): the server
		// treats it as idempotent, so repeating one after an ambiguous outcome
		// is safe and there is nothing a witness would make durable first.
		case operation::resolve_review_thread:
		case operation::unresolve_review_thread:
		case operation::observe_review_thread:
		case operation::observe_review_thread_comments:
			return review_mutation_exchange{
				.Method  = http_method::post,
				.Graphql = true,
			};
		}
		return std::nullopt;
	}

	auto compile_target_observation_request(
		const application::review_mutation_target_binding& aTarget,
		const application::github_server_contract          aServer)
		-> review_mutation_request_result
	{
		if (!application::valid(aTarget) || !valid_server(aServer)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		auto request = review_mutation_wire_request{
			.Semantic  = aTarget,
			.Server    = aServer,
			.Operation = review_mutation_operation::observe_pull_request,
			.Method    = http_method::get,
			.Path      = pull_request_path(aTarget, aServer),
			.Headers   = common_headers(),
			.Body      = { },
		};
		return request;
	}

	auto merge_base_ancestry_binding(
		const application::review_mutation_target_binding& aTarget,
		const core::git_commit_oid&                        aTip) -> application::review_mutation_target_binding
	{
		auto binding                  = aTarget;
		binding.Semantic.ExpectedHead = aTip;
		return binding;
	}

	auto compile_merge_base_observation_request(
		const application::review_mutation_target_binding& aTarget,
		const application::github_server_contract          aServer,
		const merge_base_observation_basis                 aBasis)
		-> review_mutation_request_result
	{
		if (!application::valid(aTarget) || !valid_server(aServer) || aTarget.BaseRef.empty() || aTarget.BaseRef.size() > 256U
			|| std::ranges::any_of(aTarget.BaseRef, [](const unsigned char aByte) { return aByte < 0x21U || aByte == 0x7fU; })
			|| (aBasis != merge_base_observation_basis::base_branch && aBasis != merge_base_observation_basis::recorded_base
				&& aBasis != merge_base_observation_basis::base_ancestry)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		// The ancestry binding's expected head is the base branch tip, so the
		// recorded base and the ancestry pairs compile alike: exact commits.
		const auto head = std::string{ core::canonical_hex(aTarget.Semantic.ExpectedHead).view() };
		const auto from = aBasis == merge_base_observation_basis::base_branch ? percent_encode(aTarget.BaseRef)
																			  : std::string{ core::canonical_hex(aTarget.BaseOid).view() };
		return review_mutation_wire_request{
			.Semantic  = aTarget,
			.Server    = aServer,
			.Operation = aBasis == merge_base_observation_basis::base_branch ? review_mutation_operation::observe_merge_base
				: aBasis == merge_base_observation_basis::recorded_base      ? review_mutation_operation::observe_recorded_merge_base
																			 : review_mutation_operation::observe_base_ancestry,
			.Method    = http_method::get,
			.Path      = std::string{ rest_prefix(aServer) } + "/repos/" + percent_encode(aTarget.Repository.Owner) + "/" + percent_encode(aTarget.Repository.Name)
				+ "/compare/" + from + "..." + head + "?page=2&per_page=1",
			.Headers = common_headers(),
			.Body    = { },
		};
	}

	auto parse_merge_base_observation(
		const review_mutation_wire_response&               aResponse,
		const application::review_mutation_target_binding& aExpected,
		const merge_base_observation_basis                 aBasis)
		-> review_mutation_parse_result<merge_base_observation>
	{
		if (!application::valid(aExpected)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse) || has_header(aResponse, "location")) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto response = document<merge_base_response_json>(aResponse);
		if (!response) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		const auto head      = std::string{ core::canonical_hex(aExpected.Semantic.ExpectedHead).view() };
		const auto base      = core::parse_git_commit_oid(response->BaseCommit.Sha);
		const auto mergeBase = core::parse_git_commit_oid(response->MergeBaseCommit.Sha);
		const auto format    = aExpected.Semantic.ExpectedHead.algorithm();
		if (!response->Url.ends_with("..." + head) || !base || !mergeBase || mergeBase->algorithm() != format || base->algorithm() != format
			|| (aBasis != merge_base_observation_basis::base_branch && *base != aExpected.BaseOid)) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return merge_base_observation{ .MergeBase = *mergeBase, .BaseCommit = *base, .AheadBy = response->AheadBy };
	}

	auto left_lines_numbered_in_comparison_base(
		const application::review_mutation_target_binding& aTarget,
		const merge_base_observation&                      aBranch,
		const std::optional<merge_base_observation>&       aRecorded,
		const std::optional<merge_base_observation>&       aAncestry,
		const std::optional<std::uint64_t>&                aReportedCommits) noexcept -> bool
	{
		const auto comparisonBase = application::comparison_base(aTarget);
		if (aBranch.MergeBase == comparisonBase) {
			return true;
		}
		return aRecorded && aAncestry && aReportedCommits && aRecorded->MergeBase == comparisonBase && aRecorded->AheadBy == aReportedCommits
			&& aAncestry->MergeBase == aTarget.BaseOid && aAncestry->BaseCommit == aTarget.BaseOid;
	}

	auto compile_viewer_observation_request(
		const application::review_mutation_acting_identity& aIdentity,
		const application::github_server_contract           aServer)
		-> review_mutation_request_result
	{
		if (!application::valid(aIdentity) || !valid_server(aServer)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		return review_mutation_wire_request{
			.Semantic  = aIdentity,
			.Server    = aServer,
			.Operation = review_mutation_operation::observe_viewer,
			.Method    = http_method::get,
			.Path      = std::string{ rest_prefix(aServer) } + "/user",
			.Headers   = common_headers(),
			.Body      = { },
		};
	}

	auto compile_review_comment_request(
		const application::review_mutation_request& aRequest,
		const application::github_server_contract   aServer)
		-> review_mutation_request_result
	{
		if (!valid_server(aServer) || !aRequest.LiveBinding.has_value()
			|| !application::valid(*aRequest.LiveBinding)
			|| aRequest.Proposal.Anchor != aRequest.LiveBinding->Anchor.Semantic
			|| (aRequest.Proposal.StartLine
				&& (aRequest.Proposal.ReplyToThread || !core::supported_comment_target(aRequest.Proposal.Anchor.Side)
					|| *aRequest.Proposal.StartLine >= aRequest.Proposal.Anchor.Line
					|| aRequest.Representation != core::review_publication_representation::native_line))
			|| aRequest.Target != aRequest.LiveBinding->Target.Semantic
			|| aRequest.ActingIdentity != aRequest.LiveBinding->ActingIdentity.Account
			|| aRequest.Proposal.Disposition != core::review_disposition::comment
			|| aRequest.Proposal.Body.empty()
			|| aRequest.Proposal.Body.size() > maximum_review_mutation_body_bytes
			|| !publication_body(aRequest)
			|| !utf8_round_trip(aRequest.Proposal.Body)
			|| !utf8_round_trip(aRequest.LiveBinding->Anchor.Path)
			|| aRequest.IdempotencyKey
				!= application::idempotency_key_for(aRequest.Proposal, aRequest.Target)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		auto headers = common_headers();
		headers.push_back({ .Name = "Content-Type", .Value = "application/json" });
		// [[BPR-0657]]. A reply names its thread and goes by GraphQL, the only
		// GitHub API that addresses a thread by the identity the read side
		// reported; a new root comment keeps the REST form.
		if (aRequest.Proposal.ReplyToThread.has_value()) {
			if (!graphql_token(*aRequest.Proposal.ReplyToThread)) {
				return failed(review_mutation_contract_failure::invalid_request);
			}
			auto body = reply_body(aRequest);
			if (body.empty() || body.size() > maximum_review_mutation_body_bytes) {
				return failed(review_mutation_contract_failure::invalid_request);
			}
			return review_mutation_wire_request{
				.Semantic  = aRequest,
				.Server    = aServer,
				.Operation = review_mutation_operation::reply_review_thread,
				.Method    = http_method::post,
				.Path      = std::string{ graphql_path(aServer) },
				.Headers   = std::move(headers),
				.Body      = std::move(body),
			};
		}
		auto body = mutation_body(aRequest);
		if (body.empty() || body.size() > maximum_review_mutation_body_bytes) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		return review_mutation_wire_request{
			.Semantic  = aRequest,
			.Server    = aServer,
			.Operation = review_mutation_operation::create_review_comment,
			.Method    = http_method::post,
			.Path      = pull_request_path(aRequest.LiveBinding->Target, aServer) + "/comments",
			.Headers   = std::move(headers),
			.Body      = std::move(body),
		};
	}

	auto compile_thread_resolution_request(
		const application::review_thread_resolution_request& aRequest,
		const application::github_server_contract            aServer)
		-> review_mutation_request_result
	{
		// A resolution binds the target and the actor; it has no source line.
		if (!valid_server(aServer) || !aRequest.LiveBinding.has_value()
			|| !application::valid(aRequest.LiveBinding->Target)
			|| !application::valid(aRequest.LiveBinding->ActingIdentity)
			|| aRequest.Target != aRequest.LiveBinding->Target.Semantic
			|| aRequest.ActingIdentity != aRequest.LiveBinding->ActingIdentity.Account
			|| !graphql_token(aRequest.Thread)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		auto headers = common_headers();
		headers.push_back({ .Name = "Content-Type", .Value = "application/json" });
		auto body = resolution_body(aRequest);
		if (body.empty() || body.size() > maximum_review_mutation_body_bytes) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		return review_mutation_wire_request{
			.Semantic  = aRequest,
			.Server    = aServer,
			.Operation = aRequest.Resolve ? review_mutation_operation::resolve_review_thread
										  : review_mutation_operation::unresolve_review_thread,
			.Method    = http_method::post,
			.Path      = std::string{ graphql_path(aServer) },
			.Headers   = std::move(headers),
			.Body      = std::move(body),
		};
	}

	auto compile_thread_target_observation_request(
		const application::review_thread_resolution_request& aRequest,
		const application::github_server_contract            aServer)
		-> review_mutation_request_result
	{
		// The mutation's own compiled request with a read document in its
		// place: identical authority, identical guards, no write.
		auto observation = compile_thread_resolution_request(aRequest, aServer);
		if (!observation.has_value()) {
			return observation;
		}
		auto body = glz::write<review_json_options{ }>(resolution_query_json{
			.Query     = observe_thread_document,
			.Variables = { .Thread = aRequest.Thread },
		});
		if (!body.has_value() || body->empty()
			|| body->size() > maximum_review_mutation_body_bytes) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		observation->Operation = review_mutation_operation::observe_review_thread;
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
							: compile_target_observation_request(aSemantic, aRequest.Server);
					} else if constexpr (std::same_as<semantic_type, application::review_mutation_acting_identity>) {
						canonical = compile_viewer_observation_request(aSemantic, aRequest.Server);
					} else if constexpr (std::same_as<semantic_type, application::staged_review_request>) {
						canonical = compile_staged_review_request(aSemantic);
					} else if constexpr (std::same_as<semantic_type, review_submission_request>) {
						canonical = aRequest.Operation == review_mutation_operation::observe_reviews
							? compile_review_submission_observation_request(aSemantic, aRequest.Server)
							: compile_review_submission_request(aSemantic, aRequest.Server);
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
			|| response->Number != aExpected.Number.value() || response->State != "open"
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

	namespace {
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
	}

	auto recorded_off_hunk_contract(const live_review_mutation_configuration& aConfiguration) -> std::optional<off_hunk_unsupported_contract>
	{
		if (!valid(aConfiguration) || aConfiguration.CredentialContext.Authority.BprProfile.value() != 2U
			|| !recorded_target_matches(aConfiguration.Target) || !recorded_actor_matches(aConfiguration.ActingIdentity)) {
			return std::nullopt;
		}
		return off_hunk_unsupported_contract{ .Resource = "PullRequestReviewComment", .Field = "pull_request_review_thread.line", .Code = "custom", .Message = "could not be resolved", .RecordedDesignatedScope = true };
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
		return application::valid(aConfiguration.CredentialContext)
			&& application::valid(aConfiguration.Target)
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
		if (Transport == nullptr || Lease == nullptr || !Lease->active()
			|| !valid(Configuration)) {
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
		if (auto* success = std::get_if<github_mutation_wire_success>(&completion)) {
			return std::move(success->Response);
		}
		if (auto* error = std::get_if<github_mutation_wire_error>(&completion)) {
			return std::unexpected(error->Failure);
		}
		return std::unexpected(application::review_mutation_failure::cancelled);
	}

	namespace {
		/// \brief [[BPR-1560]]. Whether the comment sits on a LEFT line.
		[[nodiscard]] auto publishes_left(const application::review_mutation_request& aRequest) noexcept -> bool
		{
			return !aRequest.Proposal.ReplyToThread && aRequest.Proposal.Anchor.Side == core::diff_side::left;
		}
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
			if (!valid(Configuration) || !aRequest.LiveBinding.has_value()
				|| aRequest.LiveBinding->Target != Configuration.Target
				|| aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity) {
				return std::unexpected(application::review_mutation_failure::undesignated_target);
			}
			if (const auto bound = observe_thread_target(aRequest, aStop);
				!bound.has_value()) {
				return std::unexpected(bound.error());
			}
			const auto wire = compile_thread_resolution_request(
				aRequest, Configuration.CredentialContext.Authority.ServerContract);
			if (!wire.has_value()) {
				return std::unexpected(map_contract_failure(wire.error()));
			}
			auto response = perform(*wire, aStop, { });
			if (!response.has_value()) {
				return std::unexpected(response.error());
			}
			const auto observed = parse_thread_resolution_response(*response, aRequest);
			if (!observed.has_value()) {
				return std::unexpected(map_contract_failure(observed.error()));
			}
			return observed->Receipt;
		} catch (const std::bad_alloc&) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
	}
	namespace {
		using publication_state = core::review_publication_state;
		using representation    = core::review_publication_representation;
		[[nodiscard]] auto publication_failure(const application::review_mutation_failure aFailure = application::review_mutation_failure::unavailable) -> application::review_mutation_submission
		{
			return { .Result = std::unexpected(aFailure) };
		}
		[[nodiscard]] auto publication_now() -> std::uint64_t
		{
			return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
		}
		[[nodiscard]] auto text_digest(const std::string_view aText) -> std::array<std::uint8_t, 32>
		{
			return core::sha256(std::span{ reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() });
		}
		[[nodiscard]] auto digest_hex(const std::array<std::uint8_t, 32>& aDigest) -> std::string
		{
			return QByteArray::fromRawData(reinterpret_cast<const char*>(aDigest.data()), static_cast<qsizetype>(aDigest.size())).toHex().toStdString();
		}
		[[nodiscard]] auto capability_key(const live_review_mutation_configuration& aConfig, const application::review_mutation_request& aRequest) -> application::review_publication_capability_key
		{
			const auto& authority = aConfig.CredentialContext.Authority;
			auto        anchor    = std::string{ "bpr-capability-anchor-v1" };
			append_fingerprint_text(anchor, digest_hex(canonical_authority_fingerprint(*aRequest.LiveBinding)));
			append_fingerprint_text(anchor, aRequest.LiveBinding->Anchor.Path);
			append_fingerprint_integer(anchor, aRequest.LiveBinding->Anchor.Semantic.Line);
			append_fingerprint_oid(anchor, aRequest.LiveBinding->Anchor.Commit);
			return { .Profile = authority.Profile, .ProfileRevision = authority.ProfileRevision, .Host = authority.Host, .Hostname = authority.Hostname, .ServerContract = authority.ServerContract, .Actor = aConfig.ActingIdentity, .RepositoryDatabaseId = aConfig.Target.RepositoryDatabaseId, .RepositoryNodeId = aConfig.Target.RepositoryNodeId, .ClassifierRevision = off_hunk_classifier_revision, .FreshnessGeneration = aConfig.CredentialContext.IdentityGeneration.value(), .AnchorDigest = text_digest(anchor) };
		}
		[[nodiscard]] auto publication_fingerprint(const application::review_mutation_request& aRequest, const live_review_mutation_configuration& aConfig) -> std::array<std::uint8_t, 32>
		{
			auto material = std::string{ "bpr-logical-publication-v1" };
			append_fingerprint_text(material, aRequest.IdempotencyKey);
			append_fingerprint_text(material, digest_hex(canonical_authority_fingerprint(*aRequest.LiveBinding)));
			append_fingerprint_text(material, digest_hex(canonical_write_fingerprint(*compile_review_comment_request(aRequest, aConfig.Target.ServerContract))));
			append_fingerprint_integer(material, aConfig.CredentialContext.Authority.Profile.value());
			append_fingerprint_integer(material, aConfig.CredentialContext.Authority.ProfileRevision.value());
			append_fingerprint_integer(material, aConfig.CredentialContext.Authority.BprProfile.value());
			return text_digest(material);
		}
		[[nodiscard]] auto capability_scope_digest(const application::review_mutation_request& aRequest, const live_review_mutation_configuration& aConfig) -> std::array<std::uint8_t, 32>
		{
			auto material = std::string{ "bpr-off-hunk-evidence-v1" };
			append_fingerprint_text(material, digest_hex(canonical_authority_fingerprint(*aRequest.LiveBinding)));
			append_fingerprint_integer(material, aConfig.CredentialContext.Authority.Profile.value());
			append_fingerprint_integer(material, aConfig.CredentialContext.Authority.ProfileRevision.value());
			append_fingerprint_integer(material, aConfig.CredentialContext.IdentityGeneration.value());
			append_fingerprint_integer(material, 20221128U);
			append_fingerprint_integer(material, off_hunk_classifier_revision);
			append_fingerprint_text(material, digest_hex(capability_key(aConfig, aRequest).AnchorDigest));
			return text_digest(material);
		}
		[[nodiscard]] auto prepared_child(const application::review_mutation_request& aRequest, const std::string& aParent, const review_mutation_server_reconciliation& aBaseline) -> application::review_mutation_recovery_record
		{
			const auto wire = compile_review_comment_request(aRequest, aRequest.LiveBinding->Target.ServerContract);
			return { .IdempotencyKey          = application::review_publication_child_key(aParent, aRequest.Representation),
					 .RequestFingerprint      = canonical_write_fingerprint(*wire),
					 .AuthorityFingerprint    = canonical_authority_fingerprint(*aRequest.LiveBinding),
					 .Body                    = *publication_body(aRequest),
					 .Anchor                  = aRequest.LiveBinding->Anchor,
					 .BaselineExactCommentIds = exact_comment_ids(aBaseline),
					 .Transmission            = application::review_mutation_recovery_transmission::prepared };
		}
		[[nodiscard]] auto file_child_state(const publication_state aState) -> bool
		{
			return aState == publication_state::fallback_prepared || aState == publication_state::fallback_possibly_transmitted || aState == publication_state::succeeded_fallback;
		}
		[[nodiscard]] auto selected_request(application::review_mutation_request aRequest, const application::review_publication_recovery_record& aRecord) -> application::review_mutation_request
		{
			aRequest.Representation = file_child_state(aRecord.State) ? representation::file_comment_with_permalink : representation::native_line;
			return aRequest;
		}
		[[nodiscard]] auto child_matches(const application::review_mutation_request& aRequest, const application::review_publication_recovery_record& aRecord) -> bool
		{
			const auto  wire  = compile_review_comment_request(aRequest, aRequest.LiveBinding->Target.ServerContract);
			const auto& child = file_child_state(aRecord.State) ? aRecord.Fallback : aRecord.Native;
			return wire && child && child->RequestFingerprint == canonical_write_fingerprint(*wire)
				&& child->AuthorityFingerprint == canonical_authority_fingerprint(*aRequest.LiveBinding)
				&& child->Body == publication_body(aRequest) && child->Anchor == aRequest.LiveBinding->Anchor;
		}
		[[nodiscard]] auto resolved_publication(application::review_publication_recovery_record aRecord, application::review_mutation_receipt aReceipt) -> application::review_publication_recovery_record
		{
			const bool file         = file_child_state(aRecord.State);
			auto&      child        = file ? aRecord.Fallback : aRecord.Native;
			aReceipt.Representation = file ? representation::file_comment_with_permalink : representation::native_line;
			child->State            = application::review_mutation_recovery_state::succeeded;
			child->Receipt          = std::move(aReceipt);
			aRecord.State           = file ? publication_state::succeeded_fallback : publication_state::succeeded_native;
			return aRecord;
		}
	}

	auto review_mutation_session::recover_publication(const application::review_mutation_request& aRequest, const application::review_publication_recovery_record& aRecord, const std::stop_token aStop) -> application::review_mutation_submission
	{
		const auto request = selected_request(aRequest, aRecord);
		if (!child_matches(request, aRecord)) {
			return publication_failure();
		}
		const auto& child = file_child_state(aRecord.State) ? aRecord.Fallback : aRecord.Native;
		if (child->Receipt) {
			auto receipt      = *child->Receipt;
			receipt.Duplicate = true;
			return { .Result = receipt };
		}
		if (child->Transmission != application::review_mutation_recovery_transmission::possibly_transmitted) {
			return publication_failure();
		}
		if (const auto exact = observe_exact(aStop); !exact) {
			return publication_failure(exact.error());
		}
		const auto observed = reconcile(request, aStop);
		if (!observed) {
			return publication_failure(observed.error());
		}
		auto result  = application::review_mutation_submission{ .Reconciliation = observed->Projection };
		auto receipt = new_exact_receipt(*observed, *child->BaselineExactCommentIds);
		if (!receipt) {
			return result;
		}
		receipt->Duplicate  = true;
		const auto resolved = resolved_publication(aRecord, *receipt);
		if (Recovery->advance_publication(aRecord, resolved)) {
			result.Result = *(file_child_state(resolved.State) ? resolved.Fallback : resolved.Native)->Receipt;
		}
		return result;
	}

	auto review_mutation_session::execute_publication_child(const application::review_mutation_request& aRequest, const application::review_publication_recovery_record& aRecord, const std::stop_token aStop) -> application::review_mutation_submission
	{
		const auto request = selected_request(aRequest, aRecord);
		if (!child_matches(request, aRecord)) {
			return publication_failure();
		}
		if (const auto exact = observe_exact(aStop, publishes_left(request)); !exact) {
			return publication_failure(exact.error());
		}
		if (aStop.stop_requested()) {
			return publication_failure(application::review_mutation_failure::cancelled);
		}
		const auto wire      = compile_review_comment_request(request, Configuration.Target.ServerContract);
		auto       result    = application::review_mutation_submission{ .MutationAttempted = true };
		auto       witnessed = std::make_shared<std::atomic_bool>(false);
		const auto response  = perform(*wire, aStop, [this, aRecord, witnessed] {
			if (witnessed->exchange(true, std::memory_order_acq_rel)) {
				return false;
			}
			auto       next                                    = aRecord;
			const bool file                                    = file_child_state(next.State);
			(file ? next.Fallback : next.Native)->Transmission = application::review_mutation_recovery_transmission::possibly_transmitted;
			next.State                                         = file ? publication_state::fallback_possibly_transmitted : publication_state::native_possibly_transmitted;
			return Recovery->advance_publication(aRecord, next).has_value();
		});
		const auto loaded    = Recovery->load_publication(aRecord.Key);
		if (!loaded || !*loaded) {
			return result;
		}
		const auto& current = **loaded;
		const bool  file    = file_child_state(current.State);
		const auto& child   = file ? current.Fallback : current.Native;
		if (!child || child->Transmission != application::review_mutation_recovery_transmission::possibly_transmitted) {
			return result;
		}
		const auto contract = UnsupportedContract ? UnsupportedContract : recorded_off_hunk_contract(Configuration);
		if (!file && response && contract && aRequest.Source->Placement != core::review_placement::legacy_line
			&& definitive_off_hunk_unsupported(*response, *wire, *contract)) {
			auto rejected          = current;
			rejected.State         = publication_state::native_definitively_unsupported;
			rejected.Authorization = { .Source         = application::review_fallback_authorization_source::native_rejection,
									   .ContractDigest = text_digest(response->Body),
									   .ScopeDigest    = capability_scope_digest(aRequest, Configuration) };
			if (!Recovery->advance_publication(current, rejected)) {
				return result;
			}
			const auto now = publication_now();
			static_cast<void>(PublicationCapabilities.record(capability_key(Configuration, aRequest), { .State = core::off_hunk_creation_capability::unsupported, .Source = application::review_publication_evidence_source::observed_definitive_unsupported, .ContractDigest = rejected.Authorization.ContractDigest, .ObservedAt = now, .ExpiresAt = now + application::review_publication_capability_cache::maximum_lifetime_seconds }, now));
			return result;
		}
		std::optional<application::review_mutation_receipt> receipt;
		if (response) {
			const auto parsed = parse_review_comment_response(*response, request);
			if (parsed) {
				receipt = parsed->Receipt;
			} else {
				result.Result = std::unexpected(map_contract_failure(parsed.error()));
			}
		} else {
			result.Result = std::unexpected(response.error());
		}
		// A witnessed write reconciles once even when the gesture was cancelled.
		const auto observed = reconcile(request, { });
		if (observed) {
			result.Reconciliation = observed->Projection;
		}
		if (receipt && std::ranges::binary_search(*child->BaselineExactCommentIds, receipt->ServerCommentId)) {
			receipt.reset();
		}
		if (!receipt && observed) {
			receipt = new_exact_receipt(*observed, *child->BaselineExactCommentIds);
		}
		if (!receipt) {
			return result;
		}
		const auto resolved = resolved_publication(current, *receipt);
		if (!Recovery->advance_publication(current, resolved)) {
			return result;
		}
		result.Result = *(file ? resolved.Fallback : resolved.Native)->Receipt;
		if (!file && aRequest.Source->Placement == core::review_placement::off_hunk) {
			const auto now = publication_now();
			static_cast<void>(PublicationCapabilities.record(capability_key(Configuration, aRequest), { .State = core::off_hunk_creation_capability::supported, .Source = application::review_publication_evidence_source::observed_native_success, .ContractDigest = child->RequestFingerprint, .ObservedAt = now, .ExpiresAt = now + application::review_publication_capability_cache::maximum_lifetime_seconds }, now));
		}
		return result;
	}

	auto review_mutation_session::submit_publication(const application::review_mutation_request& aRequest, const std::stop_token aStop) -> application::review_mutation_submission
	{
		const auto& source    = *aRequest.Source;
		const auto& authority = Configuration.CredentialContext.Authority;
		// [[BPR-1212]]. A new comment on the base side is GitHub's `side: LEFT`
		// on the head's path. It has no permalink form, so its source is held
		// to the same comparison, head and line count directly, and it never
		// falls back to a file comment.
		const auto onBase      = aRequest.Proposal.Anchor.Side == core::diff_side::left && !aRequest.Proposal.ReplyToThread;
		const auto sourceFound = onBase
			? aRequest.Representation == core::review_publication_representation::native_line && source.Endpoint
				&& aRequest.Proposal.Anchor.Comparison == core::comparison_digest(source.Comparison) && aRequest.Proposal.Anchor.Line < source.LineCount
				&& source.Endpoint->request().Diff.Comparison.Right.Commit == Configuration.Target.Semantic.ExpectedHead
			: compile_review_permalink_body({ .Target = Configuration.Target, .Endpoint = source.Endpoint, .Comparison = source.Comparison, .LineCount = source.LineCount }, aRequest.Proposal.Anchor, "Source validation").has_value();
		if (!source.Endpoint || source.Placement > core::review_placement::off_hunk
			|| source.Comparison.Authority.Profile != authority.Profile || source.Comparison.Authority.ProfileRevision.value() != authority.ProfileRevision.value()
			|| source.Comparison.Authority.Account != authority.Account || source.Endpoint->path().generic_string() != aRequest.LiveBinding->Anchor.Path
			|| !sourceFound) {
			return publication_failure(application::review_mutation_failure::undesignated_target);
		}
		const auto fingerprint = publication_fingerprint(aRequest, Configuration);
		const auto key         = digest_hex(fingerprint);
		auto       loaded      = Recovery->load_publication(key);
		if (!loaded) {
			return publication_failure();
		}
		auto current = *loaded;
		if (current && (current->Fingerprint != fingerprint || current->LegacyKey != aRequest.IdempotencyKey)) {
			return publication_failure();
		}
		auto result       = publication_failure();
		bool ownsPrepared = false;
		// At most one native child and one fallback child. Every resume reloads durable authority.
		for (unsigned step = 0U; step < 4U; ++step) {
			if (aStop.stop_requested()) {
				result.Result = std::unexpected(application::review_mutation_failure::cancelled);
				return result;
			}
			const auto evidence    = PublicationCapabilities.lookup(capability_key(Configuration, aRequest), publication_now());
			const auto observation = core::review_publication_observation{ .Placement         = source.Placement,
																		   .Capability        = evidence ? evidence->State : core::off_hunk_creation_capability::unknown,
																		   .Body              = aRequest.Proposal.StartLine || onBase ? core::review_body_compatibility::requires_line_subject : core::classify_review_body(aRequest.Proposal.Body),
																		   .State             = current ? current->State : publication_state::initial,
																		   .Eligible          = true,
																		   .CapabilityCurrent = evidence.has_value() };
			const auto plan        = core::plan_review_publication(observation);
			using action           = core::review_publication_action;
			if (plan.Action == action::refuse) {
				result.Result = std::unexpected(plan.Diagnostic == core::review_publication_diagnostic::incompatible_body ? application::review_mutation_failure::incompatible_body : application::review_mutation_failure::unsupported);
				return result;
			}
			if (plan.Action == action::already_succeeded || plan.Action == action::reconcile_native || plan.Action == action::reconcile_fallback) {
				return recover_publication(aRequest, *current, aStop);
			}
			if (!current) {
				if (const auto exact = observe_exact(aStop, publishes_left(aRequest)); !exact) {
					return publication_failure(exact.error());
				}
				auto next = application::review_publication_recovery_record{ .Key = key, .LegacyKey = aRequest.IdempotencyKey, .Fingerprint = fingerprint };
				if (plan.Action == action::admit_unsupported_evidence) {
					next.State         = publication_state::unsupported_evidence_admitted;
					next.Authorization = { .Source = application::review_fallback_authorization_source::scoped_evidence, .ContractDigest = evidence->ContractDigest, .ScopeDigest = capability_scope_digest(aRequest, Configuration), .Generation = capability_key(Configuration, aRequest).FreshnessGeneration, .ObservedAt = evidence->ObservedAt, .ExpiresAt = evidence->ExpiresAt };
				} else {
					const auto baseline = reconcile(aRequest, aStop);
					if (!baseline) {
						return publication_failure(baseline.error());
					}
					next.State  = publication_state::native_prepared;
					next.Native = prepared_child(aRequest, key, *baseline);
				}
				const auto localBody = publication_body(aRequest);
				if (aRequest.Proposal.Durable && (!Lineage || !localBody || !Lineage->prepare(aRequest, key, *localBody))) {
					return publication_failure();
				}
				const auto saved = Recovery->advance_publication(std::nullopt, next);
				if (!saved) {
					return publication_failure();
				}
				current      = *saved;
				ownsPrepared = next.State == publication_state::native_prepared;
				if (!ownsPrepared) {
					continue;
				}
			} else if (plan.Action == action::fallback_attempt && (current->State == publication_state::native_definitively_unsupported || current->State == publication_state::unsupported_evidence_admitted)) {
				if (const auto exact = observe_exact(aStop, publishes_left(aRequest)); !exact) {
					result.Result = std::unexpected(exact.error());
					return result;
				}
				auto request           = aRequest;
				request.Representation = representation::file_comment_with_permalink;
				if (!compile_review_comment_request(request, Configuration.Target.ServerContract)) {
					result.Result = std::unexpected(application::review_mutation_failure::incompatible_body);
					return result;
				}
				const auto baseline = reconcile(request, aStop);
				if (!baseline) {
					result.Result = std::unexpected(baseline.error());
					return result;
				}
				auto next            = *current;
				next.State           = publication_state::fallback_prepared;
				next.Fallback        = prepared_child(request, key, *baseline);
				const auto localBody = publication_body(request);
				if (request.Proposal.Durable && (!Lineage || !localBody || !Lineage->prepare(request, key, *localBody))) {
					return publication_failure();
				}
				const auto saved = Recovery->advance_publication(current, next);
				if (!saved) {
					return result;
				}
				current      = *saved;
				ownsPrepared = true;
			}
			if (!ownsPrepared) {
				return result;
			}
			const auto attempted       = execute_publication_child(aRequest, *current, aStop);
			const bool previousAttempt = result.MutationAttempted;
			result                     = attempted;
			result.MutationAttempted   = result.MutationAttempted || previousAttempt;
			loaded                     = Recovery->load_publication(key);
			if (!loaded || !*loaded) {
				return result;
			}
			current      = **loaded;
			ownsPrepared = false;
			if (current->State != publication_state::native_definitively_unsupported) {
				return result;
			}
		}
		return result;
	}

	auto review_mutation_session::finish_lineage(const application::review_mutation_request& aRequest, application::review_mutation_submission aResult, std::stop_token aStop) -> application::review_mutation_submission
	{
		if (!aRequest.Proposal.Durable) {
			return aResult;
		}
		aResult.Lineage = application::review_lineage_state::pending;
		if (!aResult.Result || !Lineage) {
			return aResult;
		}
		auto request           = aRequest;
		request.Representation = aResult.Result->Representation;
		const auto observed    = reconcile(request, aStop);
		if (!observed) {
			return aResult;
		}
		aResult.Reconciliation = observed->Projection;
		const auto exact       = std::ranges::any_of(observed->ExactReceipts, [&](const auto& aReceipt) { return aReceipt.ServerCommentId == aResult.Result->ServerCommentId; });
		if (!exact) {
			return aResult;
		}
		const auto logical   = digest_hex(publication_fingerprint(aRequest, Configuration));
		const auto localBody = publication_body(request);
		if (localBody && Lineage->complete(request, logical, *localBody, *aResult.Result)) {
			aResult.Lineage = application::review_lineage_state::recorded;
		}
		return aResult;
	}
	auto review_mutation_session::reconcile_publication(const application::review_mutation_request& aRequest, std::stop_token aStop) -> application::review_mutation_submission
	{
		try {
			std::scoped_lock                lock{ Mutex };
			github_mutation_operation_scope operation{ Transport, aStop };
			if (!operation.valid()) {
				return publication_failure(operation.failure());
			}
			if (!Recovery || !Lease || !Lease->active() || aRequest.Representation != core::review_publication_representation::native_line || !aRequest.Source || !aRequest.LiveBinding || !application::valid(*aRequest.LiveBinding) || aRequest.LiveBinding->Target != Configuration.Target || aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity || aRequest.Proposal.ReplyToThread || !compile_review_comment_request(aRequest, Configuration.Target.ServerContract)) {
				return publication_failure();
			}
			const auto fingerprint = publication_fingerprint(aRequest, Configuration);
			auto       loaded      = Recovery->load_publication(digest_hex(fingerprint));
			if (!loaded || !*loaded || (**loaded).Fingerprint != fingerprint || (**loaded).LegacyKey != aRequest.IdempotencyKey) {
				return publication_failure();
			}
			return finish_lineage(aRequest, recover_publication(aRequest, **loaded, aStop), aStop);
		} catch (...) {
			return publication_failure();
		}
	}
	void review_mutation_session::repair_pending_publications(const std::stop_token aStop)
	{
		try {
			std::scoped_lock                lock{ Mutex };
			github_mutation_operation_scope operation{ Transport, aStop };
			if (!operation.valid()) {
				return;
			}
			if (!Lineage || !Recovery || !Lease || !Lease->active() || !valid(Configuration) || aStop.stop_requested()) {
				return;
			}
			auto rows = Lineage->pending(Configuration.Target.Semantic, Configuration.ActingIdentity.Account);
			if (rows.empty() || !observe_exact(aStop)) {
				return;
			}
			auto wire = compile_review_comment_collection_request(Configuration.Target, Configuration.Target.ServerContract);
			if (!wire) {
				return;
			}
			auto response = perform(*wire, aStop);
			if (!response || response->Status != 200 || !response_headers_valid(*response) || has_header(*response, "Link")) {
				return;
			}
			auto comments = document<std::vector<comment_response_json>>(*response);
			if (!comments || comments->size() > 100U) {
				return;
			}
			for (const auto& row : rows) {
				const auto& scope = row.Target.Binding.Scope;
				if (scope.Host != Configuration.Target.Semantic.Host || scope.Repository != Configuration.Target.Semantic.Repository
					|| scope.PullRequest != Configuration.Target.Semantic.PullRequest || row.Actor != Configuration.ActingIdentity.Account
					|| row.ActorNode != Configuration.ActingIdentity.NodeId || row.ActorDatabaseId != Configuration.ActingIdentity.DatabaseId
					|| row.ActorLogin != Configuration.ActingIdentity.Login || row.ServerComment) {
					continue;
				}
				auto loaded = Recovery->load_publication(row.LogicalPublication);
				if (!loaded || !*loaded || (**loaded).Key != row.LogicalPublication || (**loaded).LegacyKey != row.IdempotencyKey) {
					continue;
				}
				const auto& journal = **loaded;
				const bool  file    = file_child_state(journal.State);
				const auto& child   = file ? journal.Fallback : journal.Native;
				if (!child || row.WireBodyDigest != core::private_digest(child->Body)
					|| (file ? row.Representation != "file_comment_with_permalink" : row.Representation != "native_line")
					|| (child->Transmission != application::review_mutation_recovery_transmission::possibly_transmitted && !child->Receipt)
					|| (!child->Receipt && !child->BaselineExactCommentIds)) {
					continue;
				}
				const auto                                          commit       = core::canonical_hex(row.AcceptedHead);
				const auto                                          expectedSide = row.Target.Line.Side == core::diff_side::left ? "LEFT" : "RIGHT";
				std::optional<application::review_mutation_receipt> observed;
				bool                                                ambiguous = false;
				for (const auto& comment : *comments) {
					const bool fileComment = comment.SubjectType == "file";
					const auto currentLine = comment.CommitId == commit.view() && comment.Line
						&& *comment.Line == static_cast<std::uint64_t>(row.Target.Line.Line) + 1U;
					const auto originalLine = comment.OriginalCommitId == commit.view()
						&& comment.OriginalLine && *comment.OriginalLine == static_cast<std::uint64_t>(row.Target.Line.Line) + 1U;
					if (comment.NodeId.empty() || comment.Id == 0 || comment.Path != row.Path
						|| comment.Body != child->Body || comment.User.Id != row.ActorDatabaseId
						|| comment.User.NodeId != row.ActorNode || comment.User.Login != row.ActorLogin || fileComment != file
						|| comment.InReplyToId
						|| (!file && (!(currentLine || originalLine) || comment.Side != expectedSide || comment.StartLine || comment.StartSide))
						|| (file && comment.CommitId != commit.view())
						|| (file && !valid_file_subject_metadata(comment))
						|| (child->BaselineExactCommentIds && std::ranges::binary_search(*child->BaselineExactCommentIds, comment.NodeId))) {
						continue;
					}
					if (observed) {
						ambiguous = true;
						break;
					}
					observed = application::review_mutation_receipt{ .ServerCommentId = comment.NodeId, .Duplicate = true, .Representation = file ? representation::file_comment_with_permalink : representation::native_line };
				}
				if (ambiguous || !observed || (child->Receipt && child->Receipt->ServerCommentId != observed->ServerCommentId)) {
					continue;
				}
				if (!child->Receipt) {
					auto resolved = resolved_publication(journal, *observed);
					if (!Recovery->advance_publication(journal, resolved)) {
						continue;
					}
				}
				static_cast<void>(Lineage->complete_observed(row, child->Body, *observed));
			}
		} catch (...) {
			// A later read can repair the same pending row; this path never writes remotely.
		}
	}

	auto review_mutation_session::submit_and_reconcile(
		const application::review_mutation_request& aRequest,
		const std::stop_token                       aStop)
		-> application::review_mutation_submission
	{
		const auto failure = [](const application::review_mutation_failure aFailure) {
			return application::review_mutation_submission{
				.Result = std::unexpected(aFailure),
			};
		};
		try {
			std::scoped_lock                lock{ Mutex };
			github_mutation_operation_scope operation{ Transport, aStop };
			if (!operation.valid()) {
				return publication_failure(operation.failure());
			}
			if (aStop.stop_requested()) {
				return failure(application::review_mutation_failure::cancelled);
			}
			if (Lease == nullptr || !Lease->active() || Recovery == nullptr) {
				return failure(application::review_mutation_failure::unavailable);
			}
			if (!valid(Configuration) || !aRequest.LiveBinding.has_value()
				|| !application::valid(*aRequest.LiveBinding)
				|| aRequest.Representation != core::review_publication_representation::native_line
				|| aRequest.LiveBinding->Target != Configuration.Target
				|| aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity) {
				return failure(application::review_mutation_failure::undesignated_target);
			}
			const auto wire = compile_review_comment_request(
				aRequest,
				Configuration.CredentialContext.Authority.ServerContract);
			if (!wire.has_value()) {
				return failure(map_contract_failure(wire.error()));
			}
			const auto fingerprint = canonical_write_fingerprint(*wire);
			const auto expected    = application::review_mutation_recovery_record{
				.IdempotencyKey       = aRequest.IdempotencyKey,
				.RequestFingerprint   = fingerprint,
				.AuthorityFingerprint = canonical_authority_fingerprint(
					*aRequest.LiveBinding),
				.Body   = aRequest.Proposal.Body,
				.Anchor = aRequest.LiveBinding->Anchor,
			};
			if (!application::valid(expected)) {
				return failure(application::review_mutation_failure::unavailable);
			}

			const auto finishKnown = [&](const application::review_mutation_recovery_record& aKnown)
				-> application::review_mutation_submission {
				if (!application::valid(aKnown) || !same_attempt(aKnown, expected)) {
					return failure(application::review_mutation_failure::unavailable);
				}
				if (aKnown.State == application::review_mutation_recovery_state::succeeded
					&& aKnown.Receipt.has_value()) {
					auto duplicate      = *aKnown.Receipt;
					duplicate.Duplicate = true;
					return { .Result = std::move(duplicate) };
				}
				if (aKnown.State != application::review_mutation_recovery_state::unresolved
					|| aKnown.Receipt.has_value()
					|| !aKnown.BaselineExactCommentIds.has_value()
					|| aKnown.Transmission
						!= application::review_mutation_recovery_transmission::possibly_transmitted) {
					return failure(application::review_mutation_failure::unavailable);
				}
				const auto current = observe_exact(aStop);
				if (!current.has_value()) {
					return failure(current.error());
				}
				auto observed = reconcile(aRequest, aStop);
				if (!observed.has_value()) {
					return failure(observed.error());
				}
				auto submission = application::review_mutation_submission{
					.Reconciliation = observed->Projection,
				};
				auto receipt = new_exact_receipt(
					*observed, *aKnown.BaselineExactCommentIds);
				if (!receipt.has_value()) {
					return submission;
				}
				receipt->Duplicate  = true;
				const auto resolved = Recovery->resolve(
					aRequest.IdempotencyKey, fingerprint, *receipt);
				if (!resolved.has_value()) {
					return submission;
				}
				submission.Result = std::move(*receipt);
				return submission;
			};

			if (aRequest.Proposal.Durable && (!Lineage || !aRequest.Source || aRequest.Proposal.ReplyToThread || aRequest.Proposal.Durable->DraftRevision == 0 || std::ranges::all_of(aRequest.Proposal.Durable->Incarnation, [](auto aByte) { return aByte == 0; }))) {
				return failure(application::review_mutation_failure::unavailable);
			}
			const auto loaded = Recovery->load(aRequest.IdempotencyKey);
			if (!loaded.has_value()) {
				return failure(application::review_mutation_failure::unavailable);
			}
			if (loaded->has_value()) {
				return finishKnown(**loaded);
			}
			if (aRequest.Source && !aRequest.Proposal.ReplyToThread) {
				return finish_lineage(aRequest, submit_publication(aRequest, aStop), aStop);
			}

			// [[BPR-0879]]. A reply names a thread, and a thread identity says
			// nothing about which pull request it belongs to. Ask before the
			// attempt is admitted, so a refusal leaves no prepared record
			// behind and no write has happened.
			if (aRequest.Proposal.ReplyToThread.has_value()) {
				const auto bound = observe_thread_target(
					thread_observation_subject(aRequest), aStop);
				if (!bound.has_value()) {
					return failure(bound.error());
				}
			}

			auto baseline = reconcile(aRequest, aStop);
			if (!baseline.has_value()) {
				return failure(baseline.error());
			}
			if (aStop.stop_requested()) {
				return failure(application::review_mutation_failure::cancelled);
			}
			auto admittedRecord                    = expected;
			admittedRecord.BaselineExactCommentIds = exact_comment_ids(*baseline);
			admittedRecord.Transmission
				= application::review_mutation_recovery_transmission::prepared;
			if (!application::valid(admittedRecord)) {
				return failure(application::review_mutation_failure::unavailable);
			}
			const auto admitted = Recovery->admit(admittedRecord);
			if (!admitted.has_value()) {
				return failure(application::review_mutation_failure::unavailable);
			}
			if (!admitted->Inserted) {
				return finishKnown(admitted->Record);
			}
			if (const auto current = observe_exact(aStop, publishes_left(aRequest)); !current.has_value()) {
				return failure(current.error());
			}
			if (aStop.stop_requested()) {
				return failure(application::review_mutation_failure::cancelled);
			}

			auto submission = application::review_mutation_submission{
				.MutationAttempted = true,
			};
			auto witnessed = std::make_shared<std::atomic_bool>(false);
			auto response  = perform(
				*wire,
				aStop,
				[this, witnessed, key = aRequest.IdempotencyKey, fingerprint] {
					if (witnessed->exchange(true, std::memory_order_acq_rel)) {
						return false;
					}
					const auto marked = Recovery->mark_possibly_transmitted(
						key, fingerprint);
					return marked.has_value()
						&& marked->Transmission
						== application::review_mutation_recovery_transmission::possibly_transmitted;
				});
			auto postReceipt = std::optional<application::review_mutation_receipt>{ };
			if (response.has_value()) {
				const auto parsed = parse_review_comment_response(*response, aRequest);
				if (parsed.has_value()) {
					postReceipt = parsed->Receipt;
				} else {
					submission.Result = std::unexpected(map_contract_failure(parsed.error()));
				}
			} else {
				submission.Result = std::unexpected(response.error());
			}
			const auto transmission = Recovery->load(aRequest.IdempotencyKey);
			if (!transmission.has_value() || !transmission->has_value()
				|| (**transmission).Transmission
					!= application::review_mutation_recovery_transmission::possibly_transmitted) {
				return submission;
			}

			// Once POST has been invoked its transmission is ambiguous even if the
			// caller cancels. The bounded transport therefore performs this one
			// credentialed GET with an independent token before returning.
			auto observed = reconcile(aRequest, { });
			if (observed.has_value()) {
				submission.Reconciliation = observed->Projection;
			}
			auto confirmed = postReceipt.has_value()
					&& !std::ranges::binary_search(
								 *admittedRecord.BaselineExactCommentIds,
								 postReceipt->ServerCommentId)
				? postReceipt
				: std::nullopt;
			if (!confirmed.has_value() && observed.has_value()) {
				confirmed = new_exact_receipt(
					*observed, *admittedRecord.BaselineExactCommentIds);
			}
			if (!confirmed.has_value()) {
				return submission;
			}
			confirmed->Duplicate = false;
			const auto resolved  = Recovery->resolve(
				aRequest.IdempotencyKey, fingerprint, *confirmed);
			if (!resolved.has_value()) {
				return submission;
			}
			submission.Result = std::move(*confirmed);
			return submission;
		} catch (...) {
			return failure(application::review_mutation_failure::unavailable);
		}
	}
} // namespace bpr::adapters::github::detail
