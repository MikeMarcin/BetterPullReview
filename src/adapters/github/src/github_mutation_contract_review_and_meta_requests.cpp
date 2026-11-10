#include "bpr/application/review_mutation_coordinator.hpp"
#include "github_http_helpers.hpp"
#include "github_mutation_contract.hpp"
#include "github_mutation_contract_support.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <glaze/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail {
	namespace {
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

		/// [[BPR-1741]]. A reaction subject: a review-thread comment, a
		/// pull request comment or a review, each named with the pull request
		/// it belongs to (as a thread is, [[BPR-0879]]) and its reactions as
		/// `ListReactions` selects them.
		constexpr auto observe_reaction_subject_document = std::string_view{
			"query ObserveReactionSubject($subject:ID!){node(id:$subject){"
			"__typename id ... on Reactable{viewerCanReact reactionGroups{content viewerHasReacted reactors(first:5){totalCount "
			"nodes{__typename ... on User{login} ... on Bot{login} ... on Organization{login} ... on Mannequin{login}}}}} "
			"... on PullRequestReviewComment{pullRequest{id databaseId number repository{id databaseId nameWithOwner}}} "
			"... on IssueComment{pullRequest{id databaseId number repository{id databaseId nameWithOwner}}} "
			"... on PullRequestReview{pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}"
			"}}"
		};

		constexpr auto add_reaction_document = std::string_view{
			"mutation AddReaction($subject:ID!,$content:ReactionContent!){addReaction(input:{subjectId:$subject,content:$content}){"
			"reaction{content} subject{"
			"__typename id ... on Reactable{viewerCanReact reactionGroups{content viewerHasReacted reactors(first:5){totalCount "
			"nodes{__typename ... on User{login} ... on Bot{login} ... on Organization{login} ... on Mannequin{login}}}}} "
			"... on PullRequestReviewComment{pullRequest{id databaseId number repository{id databaseId nameWithOwner}}} "
			"... on IssueComment{pullRequest{id databaseId number repository{id databaseId nameWithOwner}}} "
			"... on PullRequestReview{pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}"
			"}}}"
		};

		constexpr auto remove_reaction_document = std::string_view{
			"mutation RemoveReaction($subject:ID!,$content:ReactionContent!){removeReaction(input:{subjectId:$subject,content:$content}){"
			"reaction{content} subject{"
			"__typename id ... on Reactable{viewerCanReact reactionGroups{content viewerHasReacted reactors(first:5){totalCount "
			"nodes{__typename ... on User{login} ... on Bot{login} ... on Organization{login} ... on Mannequin{login}}}}} "
			"... on PullRequestReviewComment{pullRequest{id databaseId number repository{id databaseId nameWithOwner}}} "
			"... on IssueComment{pullRequest{id databaseId number repository{id databaseId nameWithOwner}}} "
			"... on PullRequestReview{pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}"
			"}}}"
		};

		/// [[BPR-1774]]. The pull request a description write reaches, named by
		/// its node as the accepted read gave it, with the identities that bind
		/// it to the designated pull request, its body, and GitHub's word on
		/// whether this viewer may update it.
		constexpr auto observe_description_document = std::string_view{
			"query ObserveDescription($pullRequest:ID!){node(id:$pullRequest){... on PullRequest{"
			"id databaseId number body viewerCanUpdate updatedAt repository{id databaseId nameWithOwner}}}}"
		};

		constexpr auto update_description_document = std::string_view{
			"mutation UpdateDescription($pullRequest:ID!,$body:String!){updatePullRequest(input:{pullRequestId:$pullRequest,body:$body}){"
			"pullRequest{id databaseId number body viewerCanUpdate updatedAt repository{id databaseId nameWithOwner}}}}"
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

		// [[BPR-1741]]. A reaction's envelopes, outbound.
		struct reaction_variables_json
		{
			std::string_view Subject;
			std::string_view Content;
			struct glaze
			{
				static constexpr auto value = glz::object("subject", &reaction_variables_json::Subject, "content", &reaction_variables_json::Content);
			};
		};

		struct reaction_query_json
		{
			std::string_view        Query;
			reaction_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object("query", &reaction_query_json::Query, "variables", &reaction_query_json::Variables);
			};
		};

		// [[BPR-1774]]. A description's envelopes, outbound.
		struct description_variables_json
		{
			std::string_view PullRequest;
			std::string_view Body;
			struct glaze
			{
				static constexpr auto value = glz::object("pullRequest", &description_variables_json::PullRequest, "body", &description_variables_json::Body);
			};
		};

		struct description_query_json
		{
			std::string_view           Query;
			description_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object("query", &description_query_json::Query, "variables", &description_query_json::Variables);
			};
		};

		struct description_observation_variables_json
		{
			std::string_view PullRequest;
			struct glaze
			{
				static constexpr auto value = glz::object("pullRequest", &description_observation_variables_json::PullRequest);
			};
		};

		struct description_observation_query_json
		{
			std::string_view                       Query;
			description_observation_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object("query", &description_observation_query_json::Query, "variables", &description_observation_query_json::Variables);
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
				// RFC-BPR-0089 M4. A filtered diff's line the host placed.
				.Line = static_cast<std::uint64_t>(application::remote_line(binding.Anchor)) + 1U,
				.Path = binding.Anchor.Path,
				.Side = side,
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

		[[nodiscard]] auto staged_list(staged_operation aOperation) -> bool
		{
			return aOperation == staged_operation::list_reviews || aOperation == staged_operation::list_comments || aOperation == staged_operation::list_issue_comments;
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

		/// \brief [[BPR-1736]]. Only the one member read from `GET /meta`.
		struct server_meta_json
		{
			std::optional<std::string> InstalledVersion;
			struct glaze
			{
				static constexpr auto value = glz::object("installed_version", &server_meta_json::InstalledVersion);
			};
		};
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
		const auto exchange  = review_mutation_exchange_for(wire->Operation);
		const auto limit     = maximum_review_mutation_wire_response_bytes(wire->Operation);
		const auto preflight = staged_list(aExpected.Operation) ? json_preflight_limits{ .Values = maximum_review_mutation_list_values } : json_preflight_limits{ };
		if (aResponse.Body.size() > limit) {
			return failed(review_mutation_contract_failure::response_limit);
		}
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
			const auto body   = document<staged_body_json>(aResponse, limit, preflight);
			if (!target || !body || !utf8_round_trip(body->Body) || body->Body.size() > maximum_review_mutation_body_bytes) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			result.Description = body->Body;
			return result;
		}
		if (aExpected.Operation == staged_operation::list_issue_comments || aExpected.Operation == staged_operation::create_issue_comment || aExpected.Operation == staged_operation::update_issue_comment) {
			std::optional<std::vector<staged_issue_comment_json>> objects;
			if (staged_list(aExpected.Operation)) {
				objects = document<std::vector<staged_issue_comment_json>>(aResponse, limit, preflight);
			} else if (auto item = document<staged_issue_comment_json>(aResponse, limit, preflight)) {
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
			const auto answer = document<update_comment_response_json>(aResponse, limit, preflight);
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
				objects = document<std::vector<staged_comment_json>>(aResponse, limit, preflight);
			} else if (auto item = document<staged_comment_json>(aResponse, limit, preflight)) {
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
				objects = document<std::vector<staged_review_json>>(aResponse, limit, preflight);
			} else if (auto item = document<staged_review_json>(aResponse, limit, preflight)) {
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
		// [[BPR-1775]]. A comment's body has the description's limit.
		if (aOperation == review_mutation_operation::observe_pull_request_description
			|| aOperation == review_mutation_operation::update_pull_request_description
			|| aOperation == review_mutation_operation::observe_comment_for_edit
			|| aOperation == review_mutation_operation::edit_review_comment
			|| aOperation == review_mutation_operation::edit_issue_comment
			|| aOperation == review_mutation_operation::edit_review_body) {
			return maximum_description_response_bytes;
		}
		return aOperation == review_mutation_operation::observe_reviews || aOperation == review_mutation_operation::observe_review_comments
				|| aOperation == review_mutation_operation::staged_list_reviews || aOperation == review_mutation_operation::staged_list_comments || aOperation == review_mutation_operation::staged_list_issue_comments
			? maximum_review_mutation_list_response_bytes
			: maximum_review_mutation_response_bytes;
	}

	auto maximum_review_mutation_wire_request_bytes(const review_mutation_operation aOperation) noexcept -> std::size_t
	{
		return aOperation == review_mutation_operation::update_pull_request_description
				|| aOperation == review_mutation_operation::edit_review_comment
				|| aOperation == review_mutation_operation::edit_issue_comment
				|| aOperation == review_mutation_operation::edit_review_body
			? maximum_description_request_bytes
			: maximum_review_mutation_body_bytes;
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
		case operation::observe_server_meta:
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
		// [[BPR-1741]]. A reaction keeps no recovery record either: it
		// is idempotent by state, so an ambiguous one is confirmed by reading
		// the state back, never by sending it again.
		case operation::observe_reaction_subject:
		case operation::add_reaction:
		case operation::remove_reaction:
		// [[BPR-1774]]. A description keeps no recovery record: an ambiguous
		// write is confirmed by reading the body back, never by sending it
		// again.
		case operation::observe_pull_request_description:
		case operation::update_pull_request_description:
		// [[BPR-1775]]. Nor does a comment edit: the body is the record.
		case operation::observe_comment_for_edit:
		case operation::edit_review_comment:
		case operation::edit_issue_comment:
		case operation::edit_review_body:
		// [[BPR-1804]]. Nor does a review request: GitHub's list of open
		// requests is the record, read back after an ambiguous write.
		case operation::observe_reviewers:
		case operation::request_reviews:
		// [[BPR-1852]]. Nor does a draft-state change: the state is the
		// record, read back after an ambiguous write.
		case operation::observe_pull_request_draft_state:
		case operation::mark_pull_request_ready_for_review:
		case operation::convert_pull_request_to_draft:
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
		// [[BPR-1826]]. A read of the pull request's identity and state; a
		// closed or merged binding reads it before resolving a thread.
		if (!application::valid_identity(aTarget) || !valid_server(aServer)) {
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

	auto compile_server_meta_request(const application::review_mutation_target_binding& aTarget, const application::github_server_contract aServer)
		-> review_mutation_request_result
	{
		if (!application::valid(aTarget) || aServer != application::github_server_contract::ghes_3_18_2022_11_28 || aTarget.ServerContract != aServer) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		return review_mutation_wire_request{
			.Semantic  = aTarget,
			.Server    = aServer,
			.Operation = review_mutation_operation::observe_server_meta,
			.Method    = http_method::get,
			.Path      = std::string{ rest_prefix(aServer) } + "/meta",
			.Headers   = common_headers(),
			.Body      = { },
		};
	}

	auto parse_server_meta(const review_mutation_wire_response& aResponse) -> std::optional<std::string>
	{
		if (aResponse.Status != 200 || !response_headers_valid(aResponse) || has_header(aResponse, "location")) {
			return std::nullopt;
		}
		const auto meta = document<server_meta_json>(aResponse);
		if (!meta || !meta->InstalledVersion || !core::valid_github_server_version(core::github_server_product::github_enterprise_server, *meta->InstalledVersion)) {
			return std::nullopt;
		}
		return meta->InstalledVersion;
	}

	auto enterprise_version_header(const review_mutation_wire_response& aResponse) -> std::optional<std::string>
	{
		auto found = std::optional<std::string>{ };
		for (const auto& header : aResponse.Headers) {
			auto name = header.Name;
			std::ranges::transform(name, name.begin(), [](const unsigned char aByte) { return static_cast<char>(std::tolower(aByte)); });
			if (name != "x-github-enterprise-version") {
				continue;
			}
			if (found) {
				return std::nullopt;
			}
			found = header.Value;
		}
		if (!found || !core::valid_github_server_version(core::github_server_product::github_enterprise_server, *found)) {
			return std::nullopt;
		}
		return found;
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
			// [[BPR-1745]]. Only a reply, in GitHub's own form, may be
			// bound to its thread rather than a line.
			|| (application::thread_bound_binding(aRequest.LiveBinding->Anchor)
				&& (!aRequest.Proposal.ReplyToThread || aRequest.Source || aRequest.Representation != core::review_publication_representation::native_line))
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
		// [[BPR-1826]]. Its pull request may be closed or merged.
		if (!valid_server(aServer) || !aRequest.LiveBinding.has_value()
			|| !application::valid_for(aRequest.LiveBinding->Target, core::pull_request_change::thread_resolution)
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

	auto compile_reaction_request(
		const application::review_reaction_request& aRequest,
		const application::github_server_contract   aServer)
		-> review_mutation_request_result
	{
		// A reaction binds the target and the actor, as a resolution does.
		if (!valid_server(aServer) || !aRequest.LiveBinding.has_value()
			|| !application::valid(aRequest.LiveBinding->Target)
			|| !application::valid(aRequest.LiveBinding->ActingIdentity)
			|| aRequest.LiveBinding->Target.ServerContract != aServer
			|| aRequest.Target != aRequest.LiveBinding->Target.Semantic
			|| aRequest.ActingIdentity != aRequest.LiveBinding->ActingIdentity.Account
			|| !graphql_token(aRequest.Subject) || core::reaction_graphql_name(aRequest.Content).empty()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		auto headers = common_headers();
		headers.push_back({ .Name = "Content-Type", .Value = "application/json" });
		auto body = glz::write<review_json_options{ }>(reaction_query_json{
			.Query     = aRequest.Add ? add_reaction_document : remove_reaction_document,
			.Variables = { .Subject = aRequest.Subject, .Content = core::reaction_graphql_name(aRequest.Content) },
		});
		if (!body.has_value() || body->empty() || body->size() > maximum_review_mutation_body_bytes) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		return review_mutation_wire_request{
			.Semantic  = aRequest,
			.Server    = aServer,
			.Operation = aRequest.Add ? review_mutation_operation::add_reaction : review_mutation_operation::remove_reaction,
			.Method    = http_method::post,
			.Path      = std::string{ graphql_path(aServer) },
			.Headers   = std::move(headers),
			.Body      = std::move(*body),
		};
	}

	auto compile_reaction_subject_observation_request(
		const application::review_reaction_request& aRequest,
		const application::github_server_contract   aServer)
		-> review_mutation_request_result
	{
		auto observation = compile_reaction_request(aRequest, aServer);
		if (!observation.has_value()) {
			return observation;
		}
		auto body = glz::write<review_json_options{ }>(reaction_subject_query_json{
			.Query     = observe_reaction_subject_document,
			.Variables = { .Subject = aRequest.Subject },
		});
		if (!body.has_value() || body->empty() || body->size() > maximum_review_mutation_body_bytes) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		observation->Operation = review_mutation_operation::observe_reaction_subject;
		observation->Body      = std::move(*body);
		return observation;
	}

	auto compile_description_request(
		const application::review_description_request& aRequest,
		const application::github_server_contract      aServer)
		-> review_mutation_request_result
	{
		// [[BPR-1774]]. A description binds the target and the actor, as a
		// resolution does, and names the pull request by the node the
		// accepted read gave it.
		if (!valid_server(aServer) || !aRequest.LiveBinding.has_value()
			|| !application::valid(aRequest.LiveBinding->Target)
			|| !application::valid(aRequest.LiveBinding->ActingIdentity)
			|| aRequest.LiveBinding->Target.ServerContract != aServer
			|| aRequest.Target != aRequest.LiveBinding->Target.Semantic
			|| aRequest.ActingIdentity != aRequest.LiveBinding->ActingIdentity.Account
			|| !graphql_token(aRequest.LiveBinding->Target.PullRequestNodeId)
			|| !utf8_round_trip(aRequest.Body) || !utf8_round_trip(aRequest.Original)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		auto headers = common_headers();
		headers.push_back({ .Name = "Content-Type", .Value = "application/json" });
		auto body = glz::write<review_json_options{ }>(description_query_json{
			.Query     = update_description_document,
			.Variables = { .PullRequest = aRequest.LiveBinding->Target.PullRequestNodeId, .Body = aRequest.Body },
		});
		if (!body.has_value() || body->empty() || body->size() > maximum_description_request_bytes) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		return review_mutation_wire_request{
			.Semantic  = aRequest,
			.Server    = aServer,
			.Operation = review_mutation_operation::update_pull_request_description,
			.Method    = http_method::post,
			.Path      = std::string{ graphql_path(aServer) },
			.Headers   = std::move(headers),
			.Body      = std::move(*body),
		};
	}

	auto compile_description_observation_request(
		const application::review_description_request& aRequest,
		const application::github_server_contract      aServer)
		-> review_mutation_request_result
	{
		// The mutation's own guards with a read document in its place. A text
		// too long to send is still observed, so the read names the refusal.
		auto bounded = aRequest;
		bounded.Body.clear();
		auto observation = compile_description_request(bounded, aServer);
		if (!observation.has_value()) {
			return observation;
		}
		auto body = glz::write<review_json_options{ }>(description_observation_query_json{
			.Query     = observe_description_document,
			.Variables = { .PullRequest = aRequest.LiveBinding->Target.PullRequestNodeId },
		});
		if (!body.has_value() || body->empty() || body->size() > maximum_review_mutation_body_bytes) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		observation->Semantic  = aRequest;
		observation->Operation = review_mutation_operation::observe_pull_request_description;
		observation->Body      = std::move(*body);
		return observation;
	}
}
