#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_MUTATION_CONTRACT_SUPPORT_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_MUTATION_CONTRACT_SUPPORT_HPP
#include "bpr/application/review_mutation_coordinator.hpp"
#include "bpr/core/comment_edit.hpp"
#include "bpr/core/reviewer_request.hpp"
#include "github_http_helpers.hpp"
#include "github_json_preflight.hpp"
#include "github_mutation_contract.hpp"
#include "review_comment_permalink.hpp"

#include <array>
#include <cstddef>
#include <expected>
#include <functional>
#include <glaze/json.hpp>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Wire shapes, bounds and parsing helpers the GitHub mutation contract's sources share.
namespace bpr::adapters::github::detail {
	constexpr std::string_view rest_version = "2022-11-28";

	[[nodiscard]] auto failed(const review_mutation_contract_failure aFailure)
		-> std::unexpected<review_mutation_contract_failure>;

	[[nodiscard]] auto bounded_text(
		const std::string_view aText,
		const std::size_t      aMaximum) noexcept -> bool;

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

	/// [[BPR-1775]]. The GraphQL type a kind's node must be.
	[[nodiscard]] constexpr auto comment_edit_typename(const core::comment_edit_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case core::comment_edit_kind::review_comment:       return "PullRequestReviewComment";
		case core::comment_edit_kind::pull_request_comment: return "IssueComment";
		case core::comment_edit_kind::review_body:          return "PullRequestReview";
		}
		return { };
	}

	[[nodiscard]] auto graphql_token(const std::string_view aValue) noexcept -> bool;

	[[nodiscard]] auto utf8_round_trip(const std::string_view aText) -> bool;

	[[nodiscard]] auto common_headers() -> std::vector<http_header>;

	[[nodiscard]] auto pull_request_path(
		const application::review_mutation_target_binding& aTarget,
		const application::github_server_contract          aServer)
		-> std::string;

	struct review_submission_names
	{
		std::string_view Event;
		std::string_view State;
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

	struct reaction_subject_variables_json
	{
		std::string_view Subject;
		struct glaze
		{
			static constexpr auto value = glz::object("subject", &reaction_subject_variables_json::Subject);
		};
	};

	struct reaction_subject_query_json
	{
		std::string_view                Query;
		reaction_subject_variables_json Variables;
		struct glaze
		{
			static constexpr auto value = glz::object("query", &reaction_subject_query_json::Query, "variables", &reaction_subject_query_json::Variables);
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
		const application::review_mutation_target_binding& aTarget) -> bool;

	struct reviewer_login_json
	{
		std::string Login;
		struct glaze
		{
			static constexpr auto value = glz::object("login", &reviewer_login_json::Login);
		};
	};

	struct reviewer_requested_json
	{
		std::string                Typename;
		std::optional<std::string> Id;
		std::optional<std::string> Login;
		struct glaze
		{
			static constexpr auto value = glz::object("__typename", &reviewer_requested_json::Typename, "id", &reviewer_requested_json::Id, "login", &reviewer_requested_json::Login);
		};
	};

	struct reviewer_request_node_json
	{
		std::optional<reviewer_requested_json> RequestedReviewer;
		struct glaze
		{
			static constexpr auto value = glz::object("requestedReviewer", &reviewer_request_node_json::RequestedReviewer);
		};
	};

	struct reviewer_request_nodes_json
	{
		std::vector<reviewer_request_node_json> Nodes;
		struct glaze
		{
			static constexpr auto value = glz::object("nodes", &reviewer_request_nodes_json::Nodes);
		};
	};

	struct reviewer_review_node_json
	{
		std::optional<reviewer_login_json> Author;
		struct glaze
		{
			static constexpr auto value = glz::object("author", &reviewer_review_node_json::Author);
		};
	};

	struct reviewer_review_nodes_json
	{
		std::vector<reviewer_review_node_json> Nodes;
		struct glaze
		{
			static constexpr auto value = glz::object("nodes", &reviewer_review_nodes_json::Nodes);
		};
	};

	/// \brief [[BPR-1804]]. A login as GitHub makes them: at most 39
	/// characters; a name is bounded text.
	[[nodiscard]] auto reviewer_login(const std::string_view aLogin) -> bool;

	/// \brief [[BPR-1804]]. The facts both answers share: the author, the
	/// open user requests and the submitted reviews, each list bounded and
	/// every login well formed. Team requests are not users and are left out.
	[[nodiscard]] auto reviewer_standing_facts(
		const std::optional<reviewer_login_json>& aAuthor,
		const reviewer_request_nodes_json&        aRequests,
		const reviewer_review_nodes_json&         aReviews)
		-> review_mutation_parse_result<core::reviewer_search_answer>;

	[[nodiscard]] auto reviewer_binds(
		const std::string& aId, const std::uint64_t aDatabaseId, const std::uint64_t aNumber,
		const thread_repository_json& aRepository, const application::review_reviewer_request& aExpected) -> bool;

	// [[BPR-1775]]. A comment as an edit's observation or answer names it.
	struct comment_subject_json
	{
		std::string              Typename;
		std::string              Id;
		std::string              Body;
		bool                     ViewerCanUpdate = false;
		bool                     ViewerDidAuthor = false;
		std::string              UpdatedAt;
		thread_pull_request_json PullRequest;
		struct glaze
		{
			static constexpr auto value = glz::object(
				"__typename", &comment_subject_json::Typename,
				"id", &comment_subject_json::Id,
				"body", &comment_subject_json::Body,
				"viewerCanUpdate", &comment_subject_json::ViewerCanUpdate,
				"viewerDidAuthor", &comment_subject_json::ViewerDidAuthor,
				"updatedAt", &comment_subject_json::UpdatedAt,
				"pullRequest", &comment_subject_json::PullRequest);
		};
	};

	/// [[BPR-1775]]. The comment, held to what was asked: its node, the
	/// GraphQL type of its kind, and the designated pull request by every
	/// identity the target binds.
	[[nodiscard]] auto comment_observation(
		const comment_subject_json&                     aComment,
		const application::review_comment_edit_request& aExpected)
		-> review_mutation_parse_result<review_description_observation>;

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

	/// \brief [[BPR-1706]]. The source a file comment's permalink names: the
	/// right document, or for a commit-filtered diff's left line the core
	/// placed as a file comment, the left one.
	[[nodiscard]] auto permalink_source(const application::review_mutation_source_binding& aSource, const application::review_mutation_target_binding& aTarget,
										const core::diff_side aSide) -> review_permalink_source;

	[[nodiscard]] auto publication_body(const application::review_mutation_request& aRequest) -> std::optional<std::string>;

	[[nodiscard]] auto submission_names(const core::review_disposition aDisposition)
		-> std::optional<review_submission_names>;

	[[nodiscard]] auto submitted_timestamp(const std::string_view aText) -> bool;

	void append_fingerprint_integer(std::string& aMaterial, const std::uint64_t aValue);

	void append_fingerprint_text(
		std::string& aMaterial, const std::string_view aValue);

	[[nodiscard]] auto canonical_write_fingerprint(
		const review_mutation_wire_request& aRequest)
		-> std::array<std::uint8_t, 32>;

	void append_fingerprint_oid(
		std::string& aMaterial, const core::git_commit_oid& aOid);

	[[nodiscard]] auto canonical_authority_fingerprint(
		const application::review_mutation_live_binding& aBinding)
		-> std::array<std::uint8_t, 32>;

	using diagnostic_json_object = std::map<std::string, glz::raw_json_view, std::less<>>;

	template <typename T>
	[[nodiscard]] auto document(const review_mutation_wire_response& aResponse, const std::size_t aLimit = maximum_review_mutation_response_bytes, const json_preflight_limits aPreflight = { }) -> std::optional<T>
	{
		if (aResponse.Body.empty() || aResponse.Body.size() > aLimit
			|| !json_preflight{ aResponse.Body, aPreflight }.run()) {
			return std::nullopt;
		}
		auto parsed = T{ };
		if (glz::read<review_json_options{ }>(parsed, aResponse.Body)) {
			return std::nullopt;
		}
		return parsed;
	}

	[[nodiscard]] auto valid_file_subject_metadata(const comment_response_json& aComment) -> bool;

	[[nodiscard]] auto comment_observation(const comment_response_json&                aResponse,
										   const application::review_mutation_request& aExpected)
		-> review_mutation_parse_result<review_mutation_comment_observation>;

	[[nodiscard]] auto response_headers_valid(
		const review_mutation_wire_response& aResponse) noexcept -> bool;

	[[nodiscard]] auto has_header(
		const review_mutation_wire_response& aResponse,
		const std::string_view               aName) noexcept -> bool;

	[[nodiscard]] auto map_contract_failure(
		const review_mutation_contract_failure aFailure) noexcept
		-> application::review_mutation_failure;

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

	[[nodiscard]] auto staged_api_origin(const application::review_mutation_target_binding& aTarget) -> std::string;

	[[nodiscard]] auto staged_web_root(const application::review_mutation_target_binding& aTarget) -> std::string;

	[[nodiscard]] auto staged_actor(const review_author_json& aActor) -> std::optional<application::staged_review_actor>;

	[[nodiscard]] auto staged_page_suffix(std::uint32_t aPage) -> std::string;

	/// [[BPR-1367]]. Whether the `Link` header of page `aPage` of the list at
	/// `aWirePath` links a next page. Every link must name a page of that
	/// same list on the expected API origin, at most `aMaximum`; a list
	/// that is not paged (`aListed` false) may carry none.
	[[nodiscard]] auto paged_next(const review_mutation_wire_response& aResponse, const application::review_mutation_target_binding& aTarget, std::string_view aWirePath, std::uint32_t aPage, std::uint32_t aMaximum, bool aListed) -> std::optional<bool>;

	/// `aLines` false reads GitHub's list of one review's comments, which
	/// has no `line`, `side`, `start_line`, `start_side` or `subject_type`
	/// ([[BPR-1367]], probed on MikeMarcin/BPR-Test#45): lines and sides
	/// it may carry are not read, and the comment has no source.
	[[nodiscard]] auto staged_comment_entity(const staged_comment_json& aComment, const application::staged_review_request& aExpected, const bool aLines = true) -> std::optional<application::staged_review_comment>;

	using publication_state = core::review_publication_state;

	using representation = core::review_publication_representation;

	[[nodiscard]] auto text_digest(const std::string_view aText) -> std::array<std::uint8_t, 32>;

	[[nodiscard]] auto digest_hex(const std::array<std::uint8_t, 32>& aDigest) -> std::string;

	[[nodiscard]] auto publication_fingerprint(const application::review_mutation_request& aRequest, const live_review_mutation_configuration& aConfig) -> std::array<std::uint8_t, 32>;

	[[nodiscard]] auto utc_now_ms() -> std::int64_t;
}
#endif
