#include "github_read_contract.hpp"
#include "bpr/core/home_discovery_plan.hpp"
#include "bpr/core/json_preflight.hpp"
#include "bpr/core/markdown_image.hpp"
#include "github_graphql_operation_policy.hpp"
#include "github_http_helpers.hpp"

#include <QString>

#include <glaze/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <map>
#include <new>
#include <optional>
#include <string>
#include <utility>

namespace bpr::adapters::github::detail {
	namespace {
		constexpr std::string_view rest_version               = "2022-11-28";
		constexpr std::size_t      maximum_fixture_body_bytes = 4U * 1024U * 1024U;
		constexpr std::size_t      maximum_search_bytes       = 256U;
		constexpr qsizetype        maximum_cursor_bytes       = 1024;

		constexpr std::string_view review_threads_document = "query GetReviewThreads($owner:String!,$name:String!,"
															 "$number:Int!){repository(owner:$owner,name:$name){id "
															 "pullRequest(number:$number){id headRefOid files(first:100){nodes{"
															 "path viewerViewedState} pageInfo{hasNextPage endCursor}} "
															 "reviewThreads(first:100){nodes{"
															 "id isResolved isOutdated viewerCanResolve viewerCanUnresolve "
															 "subjectType path diffSide line originalLine "
															 "comments(first:100){nodes{id bodyText body url updatedAt fullDatabaseId commit{oid} originalCommit{oid} startLine originalStartLine pullRequestReview{id} path line originalLine outdated "
															 "author{__typename login ... on Node{id}} replyTo{id}} "
															 "pageInfo{hasNextPage endCursor}}} "
															 "pageInfo{hasNextPage "
															 "endCursor}}}}}";

		struct read_request_json_options : glz::opts
		{
			// NOLINTNEXTLINE(readability-identifier-naming) -- Glaze option name.
			bool escape_control_characters = true;
			// NOLINTNEXTLINE(readability-identifier-naming) -- Glaze option name.
			bool validate_skipped = true;
			// NOLINTNEXTLINE(readability-identifier-naming) -- Glaze option name.
			bool validate_trailing_whitespace = true;
			constexpr read_request_json_options()
			{
				null_terminated       = false;
				error_on_missing_keys = true;
			}
		};

		using json_preflight_result = core::json_preflight_result;

		/// [[BPR-1433]]. The shared core check with this contract's ceilings.
		class json_structural_preflight
		{
		public:
			explicit json_structural_preflight(const std::string_view aInput) noexcept
				: Input{ aInput }
			{
			}

			[[nodiscard]] auto run() const noexcept -> json_preflight_result
			{
				return core::json_preflight(Input, { .Depth = 32U, .ObjectMembers = 64U, .ArrayElements = 100U, .Values = 50'000U, .StringBytes = 256U * 1024U }).Result;
			}

		private:
			std::string_view Input;
		};

		[[nodiscard]] auto failure(const contract_failure_code aCode)
			-> std::unexpected<contract_failure>
		{
			return std::unexpected{ contract_failure{ .Code = aCode } };
		}

		[[nodiscard]] auto ascii_equal_fold(
			const std::string_view aLeft,
			const std::string_view aRight) noexcept -> bool
		{
			if (aLeft.size() != aRight.size()) {
				return false;
			}

			for (std::size_t index = 0; index < aLeft.size(); ++index) {
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

		[[nodiscard]] auto safe_path_component(
			const std::string_view aComponent) noexcept -> bool
		{
			if (
				aComponent.empty() || aComponent.size() > 100U || aComponent == "."
				|| aComponent == "..") {
				return false;
			}

			return std::ranges::all_of(aComponent, [](const unsigned char aCharacter) {
				return std::isalnum(aCharacter) != 0 || aCharacter == '-'
					|| aCharacter == '_' || aCharacter == '.';
			});
		}

		/// [[BPR-1680]]. The further repositories one personal search names:
		/// each `owner/name` of safe components, none repeated in any case nor
		/// the search's own, at most the core's repository bound in all, and
		/// the whole query inside the core's query bound.
		[[nodiscard]] auto safe_search_repositories(const std::string_view aOwner, const std::string_view aRepository, const std::string_view aTerms,
													const std::vector<std::string>& aRepositories) -> bool
		{
			if (aRepositories.empty()) {
				return true;
			}
			if (aRepositories.size() + 1U > core::maximum_discovery_search_repositories) {
				return false;
			}
			auto named = std::vector<std::string>{ std::string{ aOwner } + "/" + std::string{ aRepository } };
			auto bytes = std::string_view{ "is:pr is:open " }.size() + aTerms.size() + std::string_view{ " repo:" }.size() + named.front().size();
			for (const auto& repository : aRepositories) {
				const auto slash = repository.find('/');
				if (slash == std::string::npos || !safe_path_component(std::string_view{ repository }.substr(0U, slash))
					|| !safe_path_component(std::string_view{ repository }.substr(slash + 1U))
					|| std::ranges::any_of(named, [&repository](const std::string& aNamed) { return ascii_equal_fold(aNamed, repository); })) {
					return false;
				}
				named.push_back(repository);
				bytes += std::string_view{ " repo:" }.size() + repository.size();
			}
			return bytes <= core::maximum_discovery_search_query_bytes;
		}

		[[nodiscard]] auto hexadecimal_oid(const std::string_view aOid) noexcept -> bool
		{
			if (aOid.size() != 40U && aOid.size() != 64U) {
				return false;
			}

			return std::ranges::all_of(aOid, [](const unsigned char aCharacter) {
				return std::isxdigit(aCharacter) != 0;
			});
		}

		[[nodiscard]] auto safe_search_terms(
			const std::string_view aTerms) noexcept -> bool
		{
			return aTerms == "is:open" && aTerms.size() <= maximum_search_bytes;
		}

		/// \brief [[BPR-0988]]. Exactly one personal qualifier and one login.
		///
		/// \details `author:` and `review-requested:` are the qualifiers the
		/// GitHub.com and GHES 3.18 search contracts both document. The login
		/// is the viewer's as the server reported it: letters, digits, hyphen
		/// and underscore, bounded, and never a leading hyphen, so it cannot
		/// become another qualifier, an operator or a second term.
		[[nodiscard]] auto safe_personal_search_terms(
			const std::string_view aTerms) noexcept -> bool
		{
			auto login = aTerms;
			if (login.starts_with("author:")) {
				login.remove_prefix(std::string_view{ "author:" }.size());
			} else if (login.starts_with("review-requested:")) {
				login.remove_prefix(std::string_view{ "review-requested:" }.size());
			} else {
				return false;
			}
			return !login.empty() && login.size() <= 64U && login.front() != '-'
				&& std::ranges::all_of(login, [](const unsigned char aCharacter) {
					   return std::isalnum(aCharacter) != 0 || aCharacter == '-' || aCharacter == '_';
				   });
		}

		[[nodiscard]] auto path_prefix(
			const server_contract aServer,
			const bool            aGraphql) -> std::expected<std::string_view, contract_failure>
		{
			switch (aServer) {
			case server_contract::github_com:
				return std::string_view{ };
			case server_contract::ghes_3_18:
				return aGraphql ? std::string_view{ "/api" }
								: std::string_view{ "/api/v3" };
			}
			return failure(contract_failure_code::unsupported_server_contract);
		}

		[[nodiscard]] auto rest_headers() -> std::vector<http_header>
		{
			return {
				http_header{
					.Name  = "Accept",
					.Value = "application/vnd.github+json",
				},
				http_header{
					.Name  = "User-Agent",
					.Value = "BetterPullReview/0.1",
				},
				http_header{
					.Name  = "X-GitHub-Api-Version",
					.Value = std::string{ rest_version },
				},
			};
		}

		[[nodiscard]] auto graphql_headers() -> std::vector<http_header>
		{
			return {
				http_header{
					.Name  = "Accept",
					.Value = "application/json",
				},
				http_header{
					.Name  = "Content-Type",
					.Value = "application/json",
				},
				http_header{
					.Name  = "User-Agent",
					.Value = "BetterPullReview/0.1",
				},
			};
		}

		struct read_variables_json
		{
			std::optional<std::string_view> After;
			std::string_view                Name;
			std::uint32_t                   Number = 0U;
			std::string_view                Owner;
			std::optional<std::string_view> Thread;
			struct glaze
			{
				// This order preserves the canonical bytes of the former sorted DOM.
				static constexpr auto value = glz::object(
					"after", &read_variables_json::After,
					"name", &read_variables_json::Name,
					"number", &read_variables_json::Number,
					"owner", &read_variables_json::Owner,
					"thread", &read_variables_json::Thread);
			};
		};

		struct read_query_json
		{
			std::string_view    Query;
			read_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object("query", &read_query_json::Query, "variables", &read_query_json::Variables);
			};
		};

		struct read_query_envelope_json
		{
			std::string Query;
			// Validate the object before operation classification; decode variable
			// types afterwards to retain the existing failure ordering. Views never
			// outlive the synchronous validation call or the caller's body.
			std::map<std::string, glz::raw_json_view, std::less<>> Variables;
			struct glaze
			{
				static constexpr auto value = glz::object("query", &read_query_envelope_json::Query, "variables", &read_query_envelope_json::Variables);
			};
		};

		[[nodiscard]] auto read_query_body(
			const semantic_read_request&       aRequest,
			const std::string_view             aQuery,
			const github_graphql_page_request* aPage = nullptr) -> std::expected<std::string, contract_failure>
		{
			read_query_json document{
				.Query     = aQuery,
				.Variables = {
					.After  = aPage && aPage->Kind != github_graphql_page_kind::initial
						? std::optional<std::string_view>{ aPage->AfterCursor }
						: std::nullopt,
					.Name   = aRequest.Repository,
					.Number = aRequest.PullRequestNumber,
					.Owner  = aRequest.Owner,
					.Thread = aPage && aPage->Kind == github_graphql_page_kind::comments
						? std::optional<std::string_view>{ aPage->ThreadId }
						: std::nullopt,
				},
			};
			auto bytes = glz::write<read_request_json_options{ }>(document);
			if (!bytes) {
				return failure(contract_failure_code::unexpected_body);
			}
			return std::move(*bytes);
		}

		/// RFC-BPR-0084. GitHub's rendering of the accepted pull request's bodies.
		/// The repository and pull request scope the read to the accepted target;
		/// `nodes` answers each body's `bodyHTML`, whose image sources are
		/// GitHub's own served addresses.
		constexpr std::string_view body_images_document = "query ResolveBodyImages($ids:[ID!]!,$name:String!,$number:Int!,$owner:String!)"
														  "{repository(owner:$owner,name:$name){id pullRequest(number:$number){id}}"
														  " nodes(ids:$ids){__typename ... on PullRequest{id bodyHTML} ... on IssueComment{id bodyHTML}"
														  " ... on PullRequestReview{id bodyHTML} ... on PullRequestReviewComment{id bodyHTML}}}";

		struct body_images_variables_json
		{
			std::vector<std::string_view> Ids;
			std::string_view              Name;
			std::uint32_t                 Number = 0U;
			std::string_view              Owner;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"ids", &body_images_variables_json::Ids,
					"name", &body_images_variables_json::Name,
					"number", &body_images_variables_json::Number,
					"owner", &body_images_variables_json::Owner);
			};
		};

		struct body_images_query_json
		{
			std::string_view           Query;
			body_images_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object("query", &body_images_query_json::Query, "variables", &body_images_query_json::Variables);
			};
		};

		/// [[BPR-1741]]. The reactions of the accepted pull request's
		/// comments and reviews: the viewer's permission, and each group's
		/// content, whether the viewer reacted, how many did and the first
		/// five who did. Bound to the target like `ResolveBodyImages`; its
		/// variables are the same.
		constexpr std::string_view reactions_document = "query ListReactions($ids:[ID!]!,$name:String!,$number:Int!,$owner:String!)"
														"{repository(owner:$owner,name:$name){id pullRequest(number:$number){id}}"
														" nodes(ids:$ids){__typename ... on PullRequestReviewComment{id viewerCanReact reactionGroups{...Reactions}}"
														" ... on IssueComment{id viewerCanReact reactionGroups{...Reactions}}"
														" ... on PullRequestReview{id viewerCanReact reactionGroups{...Reactions}}}}"
														" fragment Reactions on ReactionGroup{content viewerHasReacted reactors(first:5){totalCount"
														" nodes{__typename ... on User{login} ... on Bot{login} ... on Organization{login} ... on Mannequin{login}}}}";

		[[nodiscard]] auto body_images_query_body(const semantic_read_request& aRequest, const std::string_view aDocument) -> std::expected<std::string, contract_failure>
		{
			auto document             = body_images_query_json{ };
			document.Query            = aDocument;
			document.Variables.Ids    = { aRequest.Nodes.begin(), aRequest.Nodes.end() };
			document.Variables.Name   = aRequest.Repository;
			document.Variables.Number = aRequest.PullRequestNumber;
			document.Variables.Owner  = aRequest.Owner;
			auto bytes                = glz::write<read_request_json_options{ }>(document);
			if (!bytes) {
				return failure(contract_failure_code::unexpected_body);
			}
			return std::move(*bytes);
		}

		/// GraphQL node identities: 1 to `aMaximum` distinct opaque ASCII tokens.
		[[nodiscard]] auto valid_body_nodes(const std::vector<std::string>& aNodes, const std::size_t aMaximum) -> bool
		{
			if (aNodes.empty() || aNodes.size() > aMaximum) {
				return false;
			}
			for (std::size_t index = 0; index < aNodes.size(); ++index) {
				const auto& node = aNodes[index];
				if (node.empty() || node.size() > 128U
					|| !std::ranges::all_of(node, [](const char aChar) { return (aChar >= 'a' && aChar <= 'z') || (aChar >= 'A' && aChar <= 'Z') || (aChar >= '0' && aChar <= '9') || aChar == '_' || aChar == '-' || aChar == '='; })
					|| std::find(aNodes.begin(), aNodes.begin() + static_cast<std::ptrdiff_t>(index), node) != aNodes.begin() + static_cast<std::ptrdiff_t>(index)) {
					return false;
				}
			}
			return true;
		}

		/// [[BPR-1589]]. Whether `login` is in one team, as GitHub answers it to
		/// this account: the organization's teams that `login` is in, searched
		/// for the slug (`query:` matches names and slugs, so the reader looks
		/// for the exact slug among them). Asking a team for its members by
		/// login instead times out on a large team (HTTP 502 and 504, live,
		/// 2026-10-18). Without `read:org` GitHub answers with an
		/// `INSUFFICIENT_SCOPES` error; an organization the account cannot see
		/// is `null`.
		constexpr std::string_view team_membership_document = "query GetTeamMembership($login:String!,$org:String!,$slug:String!)"
															  "{organization(login:$org){id teams(first:25,query:$slug,userLogins:[$login]){nodes{slug}}}}";

		struct team_membership_variables_json
		{
			std::string_view Login;
			std::string_view Org;
			std::string_view Slug;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"login", &team_membership_variables_json::Login,
					"org", &team_membership_variables_json::Org,
					"slug", &team_membership_variables_json::Slug);
			};
		};

		struct team_membership_query_json
		{
			std::string_view               Query;
			team_membership_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object("query", &team_membership_query_json::Query, "variables", &team_membership_query_json::Variables);
			};
		};

		[[nodiscard]] auto team_membership_query_body(const semantic_read_request& aRequest) -> std::expected<std::string, contract_failure>
		{
			auto document            = team_membership_query_json{ };
			document.Query           = team_membership_document;
			document.Variables.Login = aRequest.Login;
			document.Variables.Org   = aRequest.Organization;
			document.Variables.Slug  = aRequest.Team;
			auto bytes               = glz::write<read_request_json_options{ }>(document);
			if (!bytes) {
				return failure(contract_failure_code::unexpected_body);
			}
			return std::move(*bytes);
		}

		/// [[BPR-1680]]. Home's metadata, submitted reviews and waiting summary
		/// of one pull request, as one alias of `ListHomePullRequests` asks for
		/// them: the first page (`core::discovery_batch_page`) of its reviews,
		/// review requests and last commit's check contexts, each with its total
		/// and whether more follow, so a longer one is read the REST way.
		constexpr std::string_view home_pull_request_fragment = "fragment HomePullRequest on PullRequest{id number title isDraft state createdAt additions deletions headRefOid baseRefOid baseRefName"
																" author{__typename login ... on Node{id}} comments{totalCount}"
																" reviews(first:50){totalCount pageInfo{hasNextPage} nodes{databaseId state submittedAt author{__typename login ... on Node{id}} commit{oid} comments{totalCount}}}"
																" reviewRequests(first:50){totalCount pageInfo{hasNextPage} nodes{id asCodeOwner requestedReviewer{__typename ... on User{id login} ... on Team{id name slug organization{login}}}}}"
																" mergeStateStatus reviewDecision autoMergeRequest{enabledAt}"
																" commits(last:1){nodes{commit{oid statusCheckRollup{contexts(first:50){totalCount pageInfo{hasNextPage}"
																" nodes{__typename ... on CheckRun{name status conclusion} ... on StatusContext{context state}}}}}}}}";
		static_assert(core::discovery_batch_page == 50U, "the fragment's first pages are the core's batch page");

		struct home_variables_json
		{
			std::string_view Name;
			std::string_view Owner;
			struct glaze
			{
				static constexpr auto value = glz::object("name", &home_variables_json::Name, "owner", &home_variables_json::Owner);
			};
		};

		struct home_query_json
		{
			std::string_view    Query;
			home_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object("query", &home_query_json::Query, "variables", &home_query_json::Variables);
			};
		};

		/// [[BPR-1680]]. One alias per pull request, `p0` upward in ascending
		/// number order; the numbers are literals, the repository variables.
		[[nodiscard]] auto home_pull_requests_body(const semantic_read_request& aRequest) -> std::expected<std::string, contract_failure>
		{
			auto query = std::string{ "query ListHomePullRequests($owner:String!,$name:String!){repository(owner:$owner,name:$name){id nameWithOwner" };
			for (std::size_t index = 0U; index < aRequest.PullRequestNumbers.size(); ++index) {
				query += " p" + std::to_string(index) + ":pullRequest(number:" + std::to_string(aRequest.PullRequestNumbers[index]) + "){...HomePullRequest}";
			}
			query += "}} ";
			query += home_pull_request_fragment;
			auto document            = home_query_json{ };
			document.Query           = query;
			document.Variables.Name  = aRequest.Repository;
			document.Variables.Owner = aRequest.Owner;
			auto bytes               = glz::write<read_request_json_options{ }>(document);
			if (!bytes) {
				return failure(contract_failure_code::unexpected_body);
			}
			return std::move(*bytes);
		}

		/// [[BPR-1680]]. The pull requests one Home query reads: 1 to the core's
		/// batch bound, ascending, each a valid number.
		[[nodiscard]] auto valid_home_numbers(const std::vector<std::uint32_t>& aNumbers) noexcept -> bool
		{
			return !aNumbers.empty() && aNumbers.size() <= core::maximum_discovery_batch_pull_requests
				&& std::ranges::all_of(aNumbers, [](const std::uint32_t aNumber) { return aNumber != 0U && aNumber <= core::maximum_pull_request_number; })
				&& std::ranges::adjacent_find(aNumbers, std::greater_equal<>{ }) == aNumbers.end();
		}

		/// [[BPR-1589]]. A GitHub login or organization: 1 to 39 letters,
		/// digits and hyphens; a team slug: 1 to 100 letters, digits, `-`,
		/// `_` and `.`.
		[[nodiscard]] auto github_name(const std::string_view aName, const std::size_t aMaximum, const bool aSlug) noexcept -> bool
		{
			return !aName.empty() && aName.size() <= aMaximum
				&& std::ranges::all_of(aName, [aSlug](const char aChar) {
					   return (aChar >= 'a' && aChar <= 'z') || (aChar >= 'A' && aChar <= 'Z') || (aChar >= '0' && aChar <= '9') || aChar == '-'
						   || (aSlug && (aChar == '_' || aChar == '.'));
				   });
		}

		[[nodiscard]] auto bounded_graphql_token(
			const std::string_view aValue,
			const std::size_t      aMaximum) noexcept -> bool
		{
			return !aValue.empty() && aValue.size() <= aMaximum
				&& std::ranges::all_of(aValue, [](const char aCharacter) {
					   const auto byte = static_cast<unsigned char>(aCharacter);
					   return byte >= 0x21U && byte <= 0x7eU;
				   });
		}

		[[nodiscard]] auto valid_graphql_page(
			const github_graphql_page_request& aPage,
			const std::uint32_t                aSequence) noexcept -> bool
		{
			const auto& shape = aPage.Shape;
			if (
				(shape.ResolvePermission || shape.UnresolvePermission)
				&& !shape.ReviewThreads) {
				return false;
			}

			switch (aPage.Kind) {
			case github_graphql_page_kind::initial:
				return aSequence == 1U
					&& (shape.ReviewThreads || shape.ViewedFileState)
					&& aPage.AfterCursor.empty() && aPage.ThreadId.empty();
			case github_graphql_page_kind::files:
				return aSequence > 1U && shape.ViewedFileState
					&& aPage.ThreadId.empty()
					&& bounded_graphql_token(aPage.AfterCursor, 1'024U);
			case github_graphql_page_kind::threads:
				return aSequence > 1U && shape.ReviewThreads
					&& aPage.ThreadId.empty()
					&& bounded_graphql_token(aPage.AfterCursor, 1'024U);
			case github_graphql_page_kind::comments:
				return aSequence > 1U && shape.ReviewThreads
					&& bounded_graphql_token(aPage.AfterCursor, 1'024U)
					&& bounded_graphql_token(aPage.ThreadId, 256U);
			}
			return false;
		}

		[[nodiscard]] auto page_info_selection() -> std::string
		{
			return "pageInfo{hasNextPage endCursor}";
		}

		[[nodiscard]] auto file_connection_selection(
			const std::string_view                aArguments,
			const application::github_read_shape& aShape) -> std::string
		{
			std::string result{ "files(" };
			result += aArguments;
			result += "){totalCount nodes{path";
			if (aShape.ViewedFileState) {
				result += " viewerViewedState";
			}
			result += "} ";
			result += page_info_selection();
			result += '}';
			return result;
		}

		[[nodiscard]] auto comment_connection_selection(
			const std::string_view aArguments) -> std::string
		{
			std::string result{ "comments(" };
			result += aArguments;
			result += "){totalCount nodes{id bodyText body url updatedAt fullDatabaseId commit{oid} originalCommit{oid} startLine originalStartLine pullRequestReview{id} path line originalLine "
					  "outdated author{__typename login ... on Node{id}} replyTo{id}} ";
			result += page_info_selection();
			result += '}';
			return result;
		}

		[[nodiscard]] auto thread_connection_selection(
			const std::string_view                aArguments,
			const application::github_read_shape& aShape) -> std::string
		{
			std::string result{ "reviewThreads(" };
			result += aArguments;
			result += "){totalCount nodes{id isResolved isOutdated";
			if (aShape.ResolvePermission) {
				result += " viewerCanResolve";
			}
			if (aShape.UnresolvePermission) {
				result += " viewerCanUnresolve";
			}
			result += " subjectType path diffSide line originalLine ";
			result += comment_connection_selection("first:100");
			result += "} ";
			result += page_info_selection();
			result += '}';
			return result;
		}

		[[nodiscard]] auto paged_review_body(
			const semantic_read_request&       aRequest,
			const github_graphql_page_request& aPage) -> std::expected<std::string, contract_failure>
		{
			std::string query;
			switch (aPage.Kind) {
			case github_graphql_page_kind::initial:
				query = "query GetReviewData($owner:String!,$name:String!,"
						"$number:Int!){repository(owner:$owner,name:$name){id "
						"pullRequest(number:$number){id headRefOid";
				if (aPage.Shape.ViewedFileState) {
					query += ' ';
					query += file_connection_selection("first:100", aPage.Shape);
				}
				if (aPage.Shape.ReviewThreads) {
					query += ' ';
					query += thread_connection_selection("first:100", aPage.Shape);
				}
				query += "}}}";
				break;
			case github_graphql_page_kind::files:
				query = "query GetReviewFilesPage($owner:String!,$name:String!,"
						"$number:Int!,$after:String!){repository(owner:$owner,"
						"name:$name){id pullRequest(number:$number){id headRefOid ";
				query += file_connection_selection(
					"first:100,after:$after",
					aPage.Shape);
				query += "}}}";
				break;
			case github_graphql_page_kind::threads:
				query = "query GetReviewThreadsPage($owner:String!,$name:String!,"
						"$number:Int!,$after:String!){repository(owner:$owner,"
						"name:$name){id pullRequest(number:$number){id headRefOid ";
				query += thread_connection_selection(
					"first:100,after:$after",
					aPage.Shape);
				query += "}}}";
				break;
			case github_graphql_page_kind::comments:
				query = "query GetReviewCommentsPage($owner:String!,$name:String!,"
						"$number:Int!,$thread:ID!,$after:String!){"
						"repository(owner:$owner,name:$name){id "
						"pullRequest(number:$number){id headRefOid}} "
						"node(id:$thread){... on PullRequestReviewThread{id ";
				query += comment_connection_selection(
					"first:100,after:$after");
				query += "}}}";
				break;
			}

			return read_query_body(aRequest, query, &aPage);
		}

		[[nodiscard]] auto repository_path(
			const std::string_view       aPrefix,
			const semantic_read_request& aRequest) -> std::string
		{
			return std::string{ aPrefix } + "/repos/" + aRequest.Owner + "/"
				+ aRequest.Repository;
		}

		[[nodiscard]] auto pull_request_path(
			const std::string_view       aPrefix,
			const semantic_read_request& aRequest) -> std::string
		{
			return repository_path(aPrefix, aRequest) + "/pulls/"
				+ std::to_string(aRequest.PullRequestNumber);
		}

		[[nodiscard]] auto needs_repository(const read_operation aOperation) noexcept
			-> bool
		{
			return aOperation != read_operation::get_viewer_identity;
		}

		[[nodiscard]] auto needs_pull_request_number(
			const read_operation aOperation) noexcept -> bool
		{
			switch (aOperation) {
			case read_operation::get_pull_request:
			case read_operation::list_pull_request_files:
			case read_operation::list_pull_request_commits:
			case read_operation::list_pull_request_timeline:
			case read_operation::list_reviews:
			case read_operation::list_review_threads:
			case read_operation::list_issue_comments:
			case read_operation::list_review_comments:
			case read_operation::list_requested_reviewers:
			case read_operation::list_review_requests:
			case read_operation::get_merge_assessment:
			case read_operation::resolve_body_images:
			case read_operation::list_reactions:
				return true;
			// [[BPR-1589]]. Bound to the pull request's repository for its
			// correlation; the query names only the team and the login.
			case read_operation::get_team_membership:
			// [[BPR-1680]]. Its pull requests are `PullRequestNumbers`.
			case read_operation::list_home_pull_requests:
			case read_operation::get_viewer_identity:
			case read_operation::get_repository:
			case read_operation::list_commit_statuses:
			case read_operation::list_checks:
			case read_operation::search_pull_requests:
			case read_operation::search_personal_pull_requests:
			case read_operation::list_branch_rules:
			case read_operation::get_branch_protection:
			case read_operation::get_merge_base:
				return false;
			}
			return false;
		}

		[[nodiscard]] auto maximum_page_for(
			const read_operation aOperation) noexcept -> std::uint32_t
		{
			switch (aOperation) {
			case read_operation::list_pull_request_timeline:
				return std::numeric_limits<std::uint32_t>::max();
			case read_operation::list_pull_request_files:
				return 30U;
			case read_operation::list_pull_request_commits:
				return 3U;
			case read_operation::list_reviews:
			case read_operation::list_issue_comments:
			case read_operation::list_review_comments:
			case read_operation::list_commit_statuses:
			case read_operation::list_checks:
				return 100U;
			case read_operation::search_pull_requests:
				return 10U;
			case read_operation::get_viewer_identity:
			case read_operation::get_repository:
			case read_operation::get_pull_request:
			case read_operation::get_merge_assessment:
			case read_operation::resolve_body_images:
			case read_operation::get_branch_protection:
			case read_operation::get_merge_base:
			case read_operation::get_team_membership:
			case read_operation::list_home_pull_requests:
			case read_operation::list_reactions:
				return 1U;
			case read_operation::list_review_requests:
			case read_operation::list_branch_rules:
				return 10U;
			case read_operation::list_review_threads:
				return 1'024U;
			// [[BPR-0988]]. Discovery reads one bounded page: a longer answer is
			// partial, never exhausted.
			case read_operation::search_personal_pull_requests:
			case read_operation::list_requested_reviewers:
				return 1U;
			}
			return 0U;
		}

		[[nodiscard]] auto known_read_operation(
			const read_operation aOperation) noexcept -> bool
		{
			return std::ranges::find(all_read_operations, aOperation)
				!= all_read_operations.end();
		}

		[[nodiscard]] auto unsafe_header_name(const std::string_view aName) noexcept
			-> bool
		{
			constexpr std::array<std::string_view, 6> unsafeNames{
				"Authorization",
				"Cookie",
				"Proxy-Authorization",
				"X-GitHub-OTP",
				"X-OAuth-Scopes",
				"X-Accepted-OAuth-Scopes",
			};
			return std::ranges::any_of(unsafeNames, [aName](const auto aCandidate) {
				return ascii_equal_fold(aName, aCandidate);
			});
		}

		[[nodiscard]] auto validate_graphql_body(
			const std::string_view aBody,
			const bool             aPaged,
			const bool             aNodes = false,
			const bool             aTeam  = false,
			const bool             aHome  = false) -> std::expected<void, contract_failure>
		{
			if (aBody.size() > maximum_fixture_body_bytes) {
				return failure(contract_failure_code::response_limit);
			}
			const auto preflight = json_structural_preflight{ aBody }.run();
			if (preflight == json_preflight_result::structural_limit) {
				return failure(contract_failure_code::response_limit);
			}
			if (preflight != json_preflight_result::valid) {
				return failure(contract_failure_code::unexpected_body);
			}

			read_query_envelope_json document;
			if (glz::read<read_request_json_options{ }>(document, aBody)) {
				return failure(contract_failure_code::unexpected_body);
			}

			const auto policy = inspect_graphql_operation_policy(document.Query);
			if (policy == graphql_operation_policy_result::write_operation) {
				return failure(contract_failure_code::mutation_operation);
			}
			if (policy == graphql_operation_policy_result::response_limit) {
				return failure(contract_failure_code::response_limit);
			}
			if (policy != graphql_operation_policy_result::query_only) {
				return failure(contract_failure_code::unexpected_body);
			}

			const auto& variables = document.Variables;
			// [[BPR-1680]]. Home's batched read names its repository and
			// nothing else; its pull requests are literals of the document.
			if (aHome) {
				std::string owner;
				std::string name;
				if (aPaged || variables.size() != 2U || !variables.contains("owner") || !variables.contains("name")
					|| glz::read<read_request_json_options{ }>(owner, variables.at("owner").str)
					|| glz::read<read_request_json_options{ }>(name, variables.at("name").str)) {
					return failure(contract_failure_code::unexpected_body);
				}
				return { };
			}
			// [[BPR-1589]]. The team-membership read names a team and a login,
			// and nothing else.
			if (aTeam) {
				if (aPaged || variables.size() != 3U || !variables.contains("login") || !variables.contains("org") || !variables.contains("slug")) {
					return failure(contract_failure_code::unexpected_body);
				}
				return { };
			}
			if (!variables.contains("owner") || !variables.contains("name")
				|| !variables.contains("number") || (aNodes != variables.contains("ids"))
				|| (!aPaged && variables.size() != (aNodes ? 4U : 3U))) {
				return failure(contract_failure_code::unexpected_body);
			}
			std::string   owner;
			std::string   name;
			std::uint32_t number = 0U;
			if (glz::read<read_request_json_options{ }>(owner, variables.at("owner").str)
				|| glz::read<read_request_json_options{ }>(name, variables.at("name").str)
				|| glz::read<read_request_json_options{ }>(number, variables.at("number").str)) {
				return failure(contract_failure_code::unexpected_body);
			}
			return { };
		}

		using fixture_json_value  = glz::raw_json_view;
		using fixture_json_object = std::map<std::string, fixture_json_value, std::less<>>;
		using fixture_json_array  = std::vector<fixture_json_value>;

		// These views borrow the caller's complete response for this synchronous
		// validation only. Glaze owns all parsing; field inventories and semantic
		// checks below retain the distinction between absence and explicit null.
		[[nodiscard]] auto json_tag(const fixture_json_value& aValue) noexcept -> char
		{
			const auto begin = aValue.str.find_first_not_of(" \t\r\n");
			return begin == std::string_view::npos ? '\0' : aValue.str[begin];
		}
		[[nodiscard]] auto json_is_object(const fixture_json_value& aValue) noexcept -> bool { return json_tag(aValue) == '{'; }
		[[nodiscard]] auto json_is_array(const fixture_json_value& aValue) noexcept -> bool { return json_tag(aValue) == '['; }
		[[nodiscard]] auto json_is_string(const fixture_json_value& aValue) noexcept -> bool { return json_tag(aValue) == '"'; }
		[[nodiscard]] auto json_is_null(const fixture_json_value& aValue) noexcept -> bool { return json_tag(aValue) == 'n'; }
		[[nodiscard]] auto json_is_undefined(const fixture_json_value& aValue) noexcept -> bool { return aValue.str.empty(); }
		[[nodiscard]] auto json_is_bool(const fixture_json_value& aValue) noexcept -> bool { return json_tag(aValue) == 't' || json_tag(aValue) == 'f'; }
		[[nodiscard]] auto json_is_number(const fixture_json_value& aValue) noexcept -> bool
		{
			const auto tag = json_tag(aValue);
			return tag == '-' || (tag >= '0' && tag <= '9');
		}
		[[nodiscard]] auto json_object(const fixture_json_value& aValue) -> fixture_json_object
		{
			fixture_json_object result;
			if (!json_is_object(aValue) || glz::read<read_request_json_options{ }>(result, aValue.str)) {
				return { };
			}
			return result;
		}
		[[nodiscard]] auto json_array(const fixture_json_value& aValue) -> fixture_json_array
		{
			fixture_json_array result;
			if (!json_is_array(aValue) || glz::read<read_request_json_options{ }>(result, aValue.str)) {
				return { };
			}
			return result;
		}
		[[nodiscard]] auto json_member(const fixture_json_object& aObject, const std::string_view aKey) -> fixture_json_value
		{
			const auto found = aObject.find(aKey);
			return found == aObject.end() ? fixture_json_value{ } : found->second;
		}
		[[nodiscard]] auto json_text(const fixture_json_value& aValue) -> QString
		{
			std::string result;
			return json_is_string(aValue) && !glz::read<read_request_json_options{ }>(result, aValue.str)
				? QString::fromStdString(result)
				: QString{ };
		}
		[[nodiscard]] auto json_bool(const fixture_json_value& aValue) -> bool
		{
			bool result = false;
			return json_is_bool(aValue) && !glz::read<read_request_json_options{ }>(result, aValue.str) && result;
		}
		[[nodiscard]] auto json_uint(const fixture_json_value& aValue) noexcept -> std::uint64_t
		{
			std::uint64_t result = 0U;
			return !glz::read<read_request_json_options{ }>(result, aValue.str) ? result : 0U;
		}
		// Used only by the pre-existing incomplete-page classification, never IDs.
		[[nodiscard]] auto json_real(const fixture_json_value& aValue) noexcept -> double
		{
			double result = 0.0;
			return !glz::read<read_request_json_options{ }>(result, aValue.str) ? result : 0.0;
		}

		[[nodiscard]] auto sorted_keys(const fixture_json_object& aObject)
			-> std::vector<std::string>
		{
			std::vector<std::string> keys;
			keys.reserve(static_cast<std::size_t>(aObject.size()));
			for (auto iterator = aObject.cbegin(); iterator != aObject.cend();
				 ++iterator) {
				keys.emplace_back(iterator->first);
			}
			std::ranges::sort(keys);
			return keys;
		}

		[[nodiscard]] auto has_exact_keys(
			const fixture_json_object&                    aObject,
			const std::initializer_list<std::string_view> aExpected) -> bool
		{
			std::vector<std::string> keys;
			keys.reserve(aExpected.size());
			for (const auto key : aExpected) {
				keys.emplace_back(key);
			}
			std::ranges::sort(keys);
			return sorted_keys(aObject) == keys;
		}

		[[nodiscard]] auto has_required_fixture_keys(
			const fixture_json_object&                    aObject,
			const std::initializer_list<std::string_view> aExpected) -> bool
		{
			return std::ranges::all_of(
				aExpected,
				[&aObject](const std::string_view aKey) {
					return aObject.contains(aKey);
				});
		}

		constexpr std::size_t maximum_fixture_page_records = 100U;

		[[nodiscard]] auto exact_integer(const fixture_json_value& aValue, const bool aPositive = false) noexcept -> bool
		{
			std::uint64_t value = 0U;
			return !glz::read<read_request_json_options{ }>(value, aValue.str) && (!aPositive || value != 0U);
		}

		[[nodiscard]] auto nonempty_string(const fixture_json_value& aValue) -> bool
		{
			return json_is_string(aValue) && !json_text(aValue).isEmpty();
		}

		[[nodiscard]] auto valid_line(const fixture_json_value& aValue) noexcept -> bool
		{
			return exact_integer(aValue, true)
				&& json_uint(aValue)
				<= static_cast<std::uint64_t>(
					   std::numeric_limits<std::int32_t>::max());
		}

		[[nodiscard]] auto valid_pull_request_number(
			const fixture_json_value& aValue) noexcept -> bool
		{
			return exact_integer(aValue, true)
				&& json_uint(aValue)
				<= static_cast<std::uint64_t>(
					   core::maximum_pull_request_number);
		}

		[[nodiscard]] auto valid_json_oid(const fixture_json_value& aValue) -> bool
		{
			if (!json_is_string(aValue)) {
				return false;
			}
			const auto text = json_text(aValue);
			if (text.size() != 40 && text.size() != 64) {
				return false;
			}
			return std::ranges::all_of(text, [](const QChar aCharacter) {
				return (
						   aCharacter >= u'0' && aCharacter <= u'9')
					|| (aCharacter >= u'a' && aCharacter <= u'f')
					|| (aCharacter >= u'A' && aCharacter <= u'F');
			});
		}

		[[nodiscard]] auto one_of(
			const fixture_json_value&                     aValue,
			const std::initializer_list<std::string_view> aAllowed) -> bool
		{
			if (!json_is_string(aValue)) {
				return false;
			}
			const auto text = json_text(aValue);
			return std::ranges::any_of(aAllowed, [&text](const auto aCandidate) {
				return text == QString::fromUtf8(aCandidate.data(), static_cast<qsizetype>(aCandidate.size()));
			});
		}

		[[nodiscard]] auto exact_rest_actor(const fixture_json_value& aValue) -> bool
		{
			if (
				!json_is_object(aValue)
				|| !has_exact_keys(
					json_object(aValue),
					{ "login", "node_id" })) {
				return false;
			}
			const auto object = json_object(aValue);
			return nonempty_string(json_member(object, "login"))
				&& nonempty_string(json_member(object, "node_id"));
		}

		[[nodiscard]] auto optional_rest_actor(const fixture_json_value& aValue) -> bool
		{
			return json_is_null(aValue) || exact_rest_actor(aValue);
		}

		[[nodiscard]] auto exact_graphql_author(const fixture_json_value& aValue) -> bool
		{
			return json_is_object(aValue)
				&& has_exact_keys(json_object(aValue), { "id", "login" })
				&& nonempty_string(json_member(json_object(aValue), "id"))
				&& nonempty_string(json_member(json_object(aValue), "login"));
		}

		[[nodiscard]] auto exact_page_info(
			const fixture_json_value& aValue,
			const std::size_t         aNodeCount) -> bool
		{
			if (
				!json_is_object(aValue)
				|| !has_exact_keys(
					json_object(aValue),
					{ "endCursor", "hasNextPage" })) {
				return false;
			}
			const auto object = json_object(aValue);
			if (
				!json_is_bool(json_member(object, "hasNextPage"))
				|| json_bool(json_member(object, "hasNextPage"))) {
				return false;
			}
			const auto cursor = json_member(object, "endCursor");
			if (aNodeCount == 0) {
				return json_is_null(cursor);
			}
			return nonempty_string(cursor)
				&& json_text(cursor).toUtf8().size() <= maximum_cursor_bytes;
		}

		[[nodiscard]] auto bounded_object_array(
			const fixture_json_value&                     aValue,
			const std::initializer_list<std::string_view> aKeys) -> bool
		{
			if (!json_is_array(aValue)) {
				return false;
			}
			const auto array = json_array(aValue);
			return array.size() <= maximum_fixture_page_records
				&& std::ranges::all_of(array, [aKeys](const fixture_json_value& aItem) {
					   return json_is_object(aItem)
						   && has_exact_keys(json_object(aItem), aKeys);
				   });
		}

		[[nodiscard]] auto validate_viewer_fixture(const fixture_json_value& aDocument)
			-> bool
		{
			if (
				!json_is_object(aDocument)
				|| !has_exact_keys(
					json_object(aDocument),
					{ "id", "login", "name", "node_id" })) {
				return false;
			}
			const auto object = json_object(aDocument);
			return exact_integer(json_member(object, "id"), true)
				&& nonempty_string(json_member(object, "login"))
				&& (json_is_null(json_member(object, "name"))
					|| json_is_string(json_member(object, "name")))
				&& nonempty_string(json_member(object, "node_id"));
		}

		[[nodiscard]] auto validate_repository_fixture(const fixture_json_value& aDocument)
			-> bool
		{
			if (
				!json_is_object(aDocument)
				|| !has_exact_keys(
					json_object(aDocument),
					{
						"default_branch",
						"full_name",
						"id",
						"name",
						"node_id",
						"owner",
						"private",
					})) {
				return false;
			}
			const auto object = json_object(aDocument);
			return exact_integer(json_member(object, "id"), true)
				&& nonempty_string(json_member(object, "name"))
				&& nonempty_string(json_member(object, "full_name"))
				&& nonempty_string(json_member(object, "node_id"))
				&& json_is_object(json_member(object, "owner"))
				&& has_exact_keys(
					   json_object(json_member(object, "owner")),
					   { "login" })
				&& nonempty_string(
					   json_member(json_object(json_member(object, "owner")), "login"))
				&& json_is_bool(json_member(object, "private"))
				&& nonempty_string(json_member(object, "default_branch"));
		}

		[[nodiscard]] auto exact_pull_ref(
			const fixture_json_value& aValue,
			const bool                aRepositoryRequired) -> bool
		{
			if (
				!json_is_object(aValue)
				|| !has_exact_keys(json_object(aValue), { "ref", "repo", "sha" })) {
				return false;
			}
			const auto object     = json_object(aValue);
			const auto repository = json_member(object, "repo");
			if (
				!nonempty_string(json_member(object, "ref"))
				|| !valid_json_oid(json_member(object, "sha"))) {
				return false;
			}
			if (json_is_null(repository)) {
				return !aRepositoryRequired;
			}
			return json_is_object(repository)
				&& has_exact_keys(
					   json_object(repository),
					   { "full_name", "id" })
				&& exact_integer(json_member(json_object(repository), "id"), true)
				&& nonempty_string(
					   json_member(json_object(repository), "full_name"));
		}

		[[nodiscard]] auto validate_pull_request_fixture(
			const fixture_json_value& aDocument) -> bool
		{
			if (
				!json_is_object(aDocument)
				|| !has_exact_keys(
					json_object(aDocument),
					{
						"base",
						"body",
						"changed_files",
						"commits",
						"draft",
						"head",
						"id",
						"merged",
						"node_id",
						"number",
						"state",
						"title",
						"user",
					})) {
				return false;
			}
			const auto object = json_object(aDocument);
			const auto merged = json_bool(json_member(object, "merged"));
			return exact_integer(json_member(object, "id"), true)
				&& valid_pull_request_number(json_member(object, "number"))
				&& exact_integer(json_member(object, "changed_files"))
				&& exact_integer(json_member(object, "commits"), true)
				&& nonempty_string(json_member(object, "node_id"))
				&& one_of(json_member(object, "state"), { "open", "closed" })
				&& nonempty_string(json_member(object, "title"))
				&& (json_is_null(json_member(object, "body"))
					|| json_is_string(json_member(object, "body")))
				&& json_is_bool(json_member(object, "draft"))
				&& optional_rest_actor(json_member(object, "user"))
				&& exact_pull_ref(json_member(object, "base"), true)
				&& exact_pull_ref(json_member(object, "head"), false)
				&& json_is_bool(json_member(object, "merged"))
				&& (!merged || json_text(json_member(object, "state")) == "closed");
		}

		// [[BPR-0892]]. GitHub always sends a file's `sha` and `changes`, and
		// its `patch` unless the diff is binary or too large. A recorded page
		// keeps what GitHub sent, so a course reads the shape the product reads.
		// Each of the three is optional and checked. Nothing else is admitted.
		[[nodiscard]] auto has_file_fixture_keys(const fixture_json_object& aObject) -> bool
		{
			// Static: MSVC can materialize a constexpr local afresh for each use
			// inside the lambda, so `find` and `end()` would name two arrays and
			// its checked iterators abort on the comparison.
			static constexpr auto allowed = std::array<std::string_view, 8U>{
				"additions",
				"changes",
				"deletions",
				"filename",
				"patch",
				"previous_filename",
				"sha",
				"status",
			};
			return has_required_fixture_keys(aObject, { "additions", "deletions", "filename", "status" })
				&& std::ranges::all_of(sorted_keys(aObject), [](const std::string& aKey) {
					   return std::ranges::find(allowed, aKey) != allowed.end();
				   });
		}

		[[nodiscard]] auto valid_file_fixture_extras(const fixture_json_object& aObject) -> bool
		{
			if (aObject.contains("changes")) {
				const auto& changes = json_member(aObject, "changes");
				if (!exact_integer(changes)) {
					return false;
				}
				const auto additions = json_uint(json_member(aObject, "additions"));
				const auto total     = json_uint(changes);
				if (total < additions || total - additions != json_uint(json_member(aObject, "deletions"))) {
					return false;
				}
			}
			return (!aObject.contains("patch") || json_is_string(json_member(aObject, "patch")))
				&& (!aObject.contains("sha") || valid_json_oid(json_member(aObject, "sha")));
		}

		[[nodiscard]] auto validate_files_fixture(const fixture_json_value& aDocument)
			-> bool
		{
			if (
				!json_is_array(aDocument)
				|| json_array(aDocument).size() > maximum_fixture_page_records) {
				return false;
			}
			return std::ranges::all_of(json_array(aDocument), [](const fixture_json_value& aValue) {
				if (!json_is_object(aValue) || !has_file_fixture_keys(json_object(aValue))) {
					return false;
				}
				const auto object = json_object(aValue);
				if (
					!nonempty_string(json_member(object, "filename"))
					|| !one_of(
						json_member(object, "status"),
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
					|| !valid_file_fixture_extras(object)) {
					return false;
				}
				const auto hasPrevious = object.contains("previous_filename");
				const auto status      = json_text(json_member(object, "status"));
				if (
					hasPrevious
					&& ((
							status != "renamed"
							&& status != "copied")
						|| !nonempty_string(
							json_member(object, "previous_filename")))) {
					return false;
				}
				return true;
			});
		}

		[[nodiscard]] auto validate_commits_fixture(const fixture_json_value& aDocument)
			-> bool
		{
			if (
				!json_is_array(aDocument)
				|| json_array(aDocument).size() > maximum_fixture_page_records) {
				return false;
			}
			return std::ranges::all_of(json_array(aDocument), [](const fixture_json_value& aValue) {
				// RFC-BPR-0089. A recorded commit may keep GitHub's `parents`,
				// each exactly `{ "sha": <object id> }`; nothing else is admitted.
				if (
					!json_is_object(aValue)
					|| (!has_exact_keys(json_object(aValue), { "author", "commit", "node_id", "sha" })
						&& !has_exact_keys(json_object(aValue), { "author", "commit", "node_id", "parents", "sha" }))) {
					return false;
				}
				const auto object  = json_object(aValue);
				const auto commit  = json_member(object, "commit");
				const auto parents = json_member(object, "parents");
				if (!json_is_undefined(parents)
					&& (!json_is_array(parents) || json_array(parents).size() > application::maximum_github_commit_parents
						|| !std::ranges::all_of(json_array(parents), [](const fixture_json_value& aParent) {
							   return json_is_object(aParent) && has_exact_keys(json_object(aParent), { "sha" })
								   && valid_json_oid(json_member(json_object(aParent), "sha"));
						   }))) {
					return false;
				}
				return valid_json_oid(json_member(object, "sha"))
					&& nonempty_string(json_member(object, "node_id"))
					&& optional_rest_actor(json_member(object, "author"))
					&& json_is_object(commit)
					&& has_exact_keys(json_object(commit), { "message" })
					&& json_is_string(json_member(json_object(commit), "message"));
			});
		}

		[[nodiscard]] auto validate_reviews_fixture(const fixture_json_value& aDocument)
			-> bool
		{
			if (
				!json_is_array(aDocument)
				|| json_array(aDocument).size() > maximum_fixture_page_records) {
				return false;
			}
			return std::ranges::all_of(json_array(aDocument), [](const fixture_json_value& aValue) {
				if (
					!json_is_object(aValue)
					|| !has_exact_keys(
						json_object(aValue),
						{
							"body",
							"commit_id",
							"id",
							"node_id",
							"state",
							"user",
						})) {
					return false;
				}
				const auto object = json_object(aValue);
				return exact_integer(json_member(object, "id"), true)
					&& nonempty_string(json_member(object, "node_id"))
					&& optional_rest_actor(json_member(object, "user"))
					&& json_is_string(json_member(object, "body"))
					&& one_of(
						   json_member(object, "state"),
						   {
							   "PENDING",
							   "COMMENTED",
							   "APPROVED",
							   "CHANGES_REQUESTED",
							   "DISMISSED",
						   })
					&& (json_is_null(json_member(object, "commit_id"))
						|| valid_json_oid(json_member(object, "commit_id")));
			});
		}

		[[nodiscard]] auto exact_graphql_comment(const fixture_json_value& aValue) -> bool
		{
			if (!json_is_object(aValue)) {
				return false;
			}
			// [[BPR-1564]]. A comment on several lines names its first, as
			// GitHub's `startLine` and `originalStartLine` do; one line names
			// neither. Nothing else beyond the required keys is accepted.
			auto keys = sorted_keys(json_object(aValue));
			for (const auto optional : { std::string_view{ "originalStartLine" }, std::string_view{ "startLine" } }) {
				if (const auto found = std::ranges::find(keys, optional); found != keys.end()) {
					const auto value = json_member(json_object(aValue), optional);
					if (!json_is_null(value) && !valid_line(value)) {
						return false;
					}
					keys.erase(found);
				}
			}
			// RFC-BPR-0089 M4. The commit a comment is on and the one it was
			// written at, each GitHub's `{oid}`, as the live query asks.
			for (const auto optional : { std::string_view{ "commit" }, std::string_view{ "originalCommit" } }) {
				if (const auto found = std::ranges::find(keys, optional); found != keys.end()) {
					const auto value = json_member(json_object(aValue), optional);
					if (!json_is_null(value)
						&& (!json_is_object(value) || !has_exact_keys(json_object(value), { "oid" }) || !nonempty_string(json_member(json_object(value), "oid")))) {
						return false;
					}
					keys.erase(found);
				}
			}
			if (keys != std::vector<std::string>{ "author", "bodyText", "id", "line", "originalLine", "outdated", "path", "replyTo" }) {
				return false;
			}
			const auto object       = json_object(aValue);
			const auto replyTo      = json_member(object, "replyTo");
			const auto line         = json_member(object, "line");
			const auto originalLine = json_member(object, "originalLine");
			return nonempty_string(json_member(object, "id"))
				&& json_is_string(json_member(object, "bodyText"))
				&& nonempty_string(json_member(object, "path"))
				&& (json_is_null(line) || valid_line(line))
				&& (json_is_null(originalLine)
					|| valid_line(originalLine))
				&& json_is_bool(json_member(object, "outdated"))
				&& (json_is_null(json_member(object, "author"))
					|| exact_graphql_author(json_member(object, "author")))
				&& (json_is_null(replyTo)
					|| (json_is_object(replyTo)
						&& has_exact_keys(json_object(replyTo), { "id" })
						&& nonempty_string(json_member(json_object(replyTo), "id"))));
		}

		[[nodiscard]] auto validate_review_threads_fixture(
			const fixture_json_value& aDocument) -> bool
		{
			if (
				!json_is_object(aDocument)
				|| !has_exact_keys(json_object(aDocument), { "data" })) {
				return false;
			}
			const auto data = json_member(json_object(aDocument), "data");
			if (
				!json_is_object(data)
				|| !has_exact_keys(json_object(data), { "repository" })) {
				return false;
			}
			const auto repository = json_member(json_object(data), "repository");
			if (
				!json_is_object(repository)
				|| !has_exact_keys(
					json_object(repository),
					{ "id", "pullRequest" })
				|| !nonempty_string(json_member(json_object(repository), "id"))) {
				return false;
			}
			const auto pullRequest = json_member(json_object(repository), "pullRequest");
			if (
				!json_is_object(pullRequest)
				|| !has_exact_keys(
					json_object(pullRequest),
					{ "files", "headRefOid", "id", "reviewThreads" })
				|| !nonempty_string(json_member(json_object(pullRequest), "headRefOid"))
				|| !nonempty_string(json_member(json_object(pullRequest), "id"))) {
				return false;
			}
			const auto files = json_member(json_object(pullRequest), "files");
			if (
				!json_is_object(files)
				|| !has_exact_keys(
					json_object(files), { "nodes", "pageInfo", "totalCount" })
				|| !exact_integer(json_member(json_object(files), "totalCount"))
				|| !bounded_object_array(
					json_member(json_object(files), "nodes"),
					{ "path", "viewerViewedState" })) {
				return false;
			}
			const auto fileNodes = json_array(json_member(json_object(files), "nodes"));
			if (
				!exact_page_info(
					json_member(json_object(files), "pageInfo"),
					fileNodes.size())
				|| json_uint(json_member(json_object(files), "totalCount"))
					!= static_cast<std::uint64_t>(fileNodes.size())
				|| !std::ranges::all_of(
					fileNodes,
					[](const fixture_json_value& aValue) {
						const auto object = json_object(aValue);
						return nonempty_string(json_member(object, "path"))
							&& one_of(
								   json_member(object, "viewerViewedState"),
								   { "DISMISSED", "UNVIEWED", "VIEWED" });
					})) {
				return false;
			}
			const auto threads = json_member(json_object(pullRequest), "reviewThreads");
			if (
				!json_is_object(threads)
				|| !has_exact_keys(
					json_object(threads),
					{ "nodes", "pageInfo", "totalCount" })
				|| !exact_integer(json_member(json_object(threads), "totalCount"))
				|| !bounded_object_array(
					json_member(json_object(threads), "nodes"),
					{
						"comments",
						"diffSide",
						"id",
						"isOutdated",
						"isResolved",
						"line",
						"originalLine",
						"path",
						"subjectType",
						"viewerCanResolve",
						"viewerCanUnresolve",
					})) {
				return false;
			}
			const auto threadNodes = json_array(json_member(json_object(threads), "nodes"));
			if (
				!exact_page_info(
					json_member(json_object(threads), "pageInfo"),
					threadNodes.size())
				|| json_uint(json_member(json_object(threads), "totalCount"))
					!= static_cast<std::uint64_t>(threadNodes.size())) {
				return false;
			}

			for (const auto& threadValue : threadNodes) {
				const auto thread       = json_object(threadValue);
				const auto comments     = json_member(thread, "comments");
				const auto diffSide     = json_member(thread, "diffSide");
				const auto line         = json_member(thread, "line");
				const auto originalLine = json_member(thread, "originalLine");
				const auto commentNodes = json_array(json_member(json_object(comments), "nodes"));
				const auto fileSubject  = json_text(json_member(thread, "subjectType")) == "FILE";
				const auto lineSubject  = json_text(json_member(thread, "subjectType")) == "LINE";
				if (
					!nonempty_string(json_member(thread, "id"))
					|| !json_is_bool(json_member(thread, "isResolved"))
					|| !json_is_bool(json_member(thread, "isOutdated"))
					|| !json_is_bool(json_member(thread, "viewerCanResolve"))
					|| !json_is_bool(json_member(thread, "viewerCanUnresolve"))
					|| !one_of(diffSide, { "LEFT", "RIGHT" })
					|| (!fileSubject && !lineSubject)
					|| !nonempty_string(json_member(thread, "path"))
					|| !(
						json_is_null(line) || valid_line(line))
					|| !(
						json_is_null(originalLine)
						|| valid_line(originalLine))
					|| (fileSubject
						&& !(json_is_null(line) && json_is_null(originalLine))
						&& !(json_uint(line) == 1U && json_uint(originalLine) == 1U
							 && json_text(diffSide) == "RIGHT"))
					|| (lineSubject
						&& !json_bool(json_member(thread, "isOutdated"))
						&& json_is_null(line) && !json_is_null(originalLine))
					|| !json_is_object(comments)
					|| !has_exact_keys(
						json_object(comments),
						{ "nodes", "pageInfo", "totalCount" })
					|| !exact_integer(json_member(json_object(comments), "totalCount"))
					|| !json_is_array(json_member(json_object(comments), "nodes"))
					|| commentNodes.size()
						> maximum_fixture_page_records
					|| !exact_page_info(
						json_member(json_object(comments), "pageInfo"),
						commentNodes.size())
					|| json_uint(json_member(json_object(comments), "totalCount"))
						!= static_cast<std::uint64_t>(commentNodes.size())
					|| !std::ranges::all_of(
						commentNodes,
						exact_graphql_comment)) {
					return false;
				}
				for (const auto& commentValue : commentNodes) {
					const auto comment             = json_object(commentValue);
					const auto commentLine         = json_member(comment, "line");
					const auto commentOriginalLine = json_member(comment, "originalLine");
					if (
						(
							fileSubject
							&& (!json_is_null(commentLine)
								|| !json_is_null(commentOriginalLine)))
						|| json_text(json_member(comment, "path")) != json_text(json_member(thread, "path"))) {
						return false;
					}
				}
			}
			return true;
		}

		[[nodiscard]] auto validate_issue_comments_fixture(
			const fixture_json_value& aDocument) -> bool
		{
			if (
				!json_is_array(aDocument)
				|| json_array(aDocument).size() > maximum_fixture_page_records) {
				return false;
			}
			return std::ranges::all_of(json_array(aDocument), [](const fixture_json_value& aValue) {
				if (
					!json_is_object(aValue)
					|| !has_exact_keys(
						json_object(aValue),
						{
							"body",
							"node_id",
							"user",
						})) {
					return false;
				}
				const auto object = json_object(aValue);
				return nonempty_string(json_member(object, "node_id"))
					&& optional_rest_actor(json_member(object, "user"))
					&& json_is_string(json_member(object, "body"));
			});
		}

		/// Validates a REST inline review-comment listing.
		///
		/// Unlike the sanitized recorded fixtures above this one accepts the
		/// live payload's full key set. Its fixtures are byte-exact captures
		/// of the real service promoted under their own digests, so demanding
		/// an exact key list would mean editing the capture, which is the one
		/// thing a promoted capture may not survive. The required keys and
		/// their types are still exact, and `in_reply_to_id` is required to be
		/// an exact positive integer when present, because reply linkage is
		/// the only structure this response carries.
		[[nodiscard]] auto validate_review_comments_fixture(
			const fixture_json_value& aDocument) -> bool
		{
			if (
				!json_is_array(aDocument)
				|| json_array(aDocument).size() > maximum_fixture_page_records) {
				return false;
			}
			return std::ranges::all_of(json_array(aDocument), [](const fixture_json_value& aValue) {
				if (
					!json_is_object(aValue)
					|| !has_required_fixture_keys(
						json_object(aValue),
						{
							"body",
							"id",
							"path",
							"user",
						})) {
					return false;
				}
				const auto object = json_object(aValue);
				const auto reply  = json_member(object, "in_reply_to_id");
				if (
					!json_is_undefined(reply) && !json_is_null(reply)
					&& !exact_integer(reply, true)) {
					return false;
				}
				const auto user = json_member(object, "user");
				if (
					!json_is_null(user)
					&& (!json_is_object(user)
						|| !has_required_fixture_keys(json_object(user),
													  { "login", "node_id" })
						|| !nonempty_string(json_member(json_object(user), "login"))
						|| !nonempty_string(json_member(json_object(user), "node_id")))) {
					return false;
				}
				return exact_integer(json_member(object, "id"), true)
					&& nonempty_string(json_member(object, "path"))
					&& json_is_string(json_member(object, "body"));
			});
		}

		[[nodiscard]] auto validate_checks_fixture(const fixture_json_value& aDocument)
			-> bool
		{
			if (
				!json_is_object(aDocument)
				|| !has_exact_keys(
					json_object(aDocument),
					{ "check_runs", "total_count" })
				|| !exact_integer(json_member(json_object(aDocument), "total_count"))
				|| !bounded_object_array(
					json_member(json_object(aDocument), "check_runs"),
					{
						"conclusion",
						"head_sha",
						"name",
						"node_id",
						"status",
					})) {
				return false;
			}
			const auto runs = json_array(json_member(json_object(aDocument), "check_runs"));
			if (
				json_uint(json_member(json_object(aDocument), "total_count"))
				!= static_cast<std::uint64_t>(runs.size())) {
				return false;
			}
			return std::ranges::all_of(runs, [](const fixture_json_value& aValue) {
				const auto object    = json_object(aValue);
				const auto completed = json_text(json_member(object, "status")) == "completed";
				return nonempty_string(json_member(object, "node_id"))
					&& nonempty_string(json_member(object, "name"))
					&& valid_json_oid(json_member(object, "head_sha"))
					&& one_of(
						   json_member(object, "status"),
						   {
							   "queued",
							   "in_progress",
							   "completed",
							   "waiting",
							   "requested",
							   "pending",
						   })
					&& (json_is_null(json_member(object, "conclusion"))
						|| one_of(
							json_member(object, "conclusion"),
							{
								"success",
								"failure",
								"neutral",
								"cancelled",
								"skipped",
								"timed_out",
								"action_required",
								"stale",
							}))
					&& (completed
							? !json_is_null(json_member(object, "conclusion"))
							: json_is_null(json_member(object, "conclusion")));
			});
		}

		[[nodiscard]] auto exact_search_item(const fixture_json_value& aValue) -> bool
		{
			if (
				!json_is_object(aValue)
				|| !has_exact_keys(
					json_object(aValue),
					{
						"draft",
						"html_url",
						"node_id",
						"number",
						"pull_request",
						"repository_url",
						"state",
						"title",
					})) {
				return false;
			}
			const auto object      = json_object(aValue);
			const auto pullRequest = json_member(object, "pull_request");
			return nonempty_string(json_member(object, "node_id"))
				&& valid_pull_request_number(json_member(object, "number"))
				&& one_of(json_member(object, "state"), { "open" })
				&& nonempty_string(json_member(object, "title"))
				&& json_is_bool(json_member(object, "draft"))
				&& nonempty_string(json_member(object, "repository_url"))
				&& nonempty_string(json_member(object, "html_url"))
				&& json_is_object(pullRequest)
				&& has_exact_keys(json_object(pullRequest), { "url" })
				&& nonempty_string(json_member(json_object(pullRequest), "url"));
		}

		[[nodiscard]] auto validate_search_fixture(const fixture_json_value& aDocument)
			-> bool
		{
			if (
				!json_is_object(aDocument)
				|| !has_exact_keys(
					json_object(aDocument),
					{ "incomplete_results", "items", "total_count" })
				|| !json_is_bool(json_member(json_object(aDocument), "incomplete_results"))
				|| json_bool(json_member(json_object(aDocument), "incomplete_results"))
				|| !exact_integer(json_member(json_object(aDocument), "total_count"))
				|| !json_is_array(json_member(json_object(aDocument), "items"))
				|| json_array(json_member(json_object(aDocument), "items")).size()
					> maximum_fixture_page_records) {
				return false;
			}
			const auto items = json_array(json_member(json_object(aDocument), "items"));
			return json_uint(json_member(json_object(aDocument), "total_count"))
				== static_cast<std::uint64_t>(items.size())
				&& std::ranges::all_of(items, exact_search_item);
		}

		[[nodiscard]] auto response_is_incomplete(
			const read_operation      aOperation,
			const fixture_json_value& aDocument) -> bool
		{
			if (aOperation == read_operation::list_checks && json_is_object(aDocument)) {
				const auto object = json_object(aDocument);
				return json_is_number(json_member(object, "total_count"))
					&& json_is_array(json_member(object, "check_runs"))
					&& json_real(json_member(object, "total_count"))
					> static_cast<double>(
						   json_array(json_member(object, "check_runs")).size());
			}
			if (
				aOperation == read_operation::search_pull_requests
				&& json_is_object(aDocument)) {
				const auto object = json_object(aDocument);
				return (
						   json_is_bool(json_member(object, "incomplete_results"))
						   && json_bool(json_member(object, "incomplete_results")))
					|| (json_is_number(json_member(object, "total_count"))
						&& json_is_array(json_member(object, "items"))
						&& json_real(json_member(object, "total_count"))
							> static_cast<double>(
								json_array(json_member(object, "items")).size()));
			}
			if (
				aOperation != read_operation::list_review_threads
				|| !json_is_object(aDocument)) {
				return false;
			}

			const auto pullRequest = json_object(json_member(json_object(json_member(json_object(json_member(json_object(aDocument), "data")), "repository")), "pullRequest"));
			const auto files       = json_object(json_member(pullRequest, "files"));
			if (json_bool(json_member(json_object(json_member(files, "pageInfo")), "hasNextPage"))) {
				return true;
			}

			const auto threads = json_object(json_member(pullRequest, "reviewThreads"));
			if (json_bool(json_member(json_object(json_member(threads, "pageInfo")), "hasNextPage"))) {
				return true;
			}
			return std::ranges::any_of(
				json_array(json_member(threads, "nodes")),
				[](const fixture_json_value& aValue) {
					return json_bool(json_member(json_object(json_member(json_object(json_member(json_object(aValue), "comments")), "pageInfo")), "hasNextPage"));
				});
		}

		[[nodiscard]] auto response_shape_is_valid(
			const read_operation      aOperation,
			const fixture_json_value& aDocument) -> bool
		{
			switch (aOperation) {
			// Timeline normalization belongs to the bounded production pager.
			case read_operation::list_pull_request_timeline:
			// [[BPR-1560]]. Read and bound by its own production reader.
			case read_operation::get_merge_base:
			// [[BPR-1589]]. Read and bound by its own production reader.
			case read_operation::get_team_membership:
			// [[BPR-1680]]. Read and bound by its own production reader.
			case read_operation::list_home_pull_requests:
				return false;
			case read_operation::get_viewer_identity:
				return validate_viewer_fixture(aDocument);
			case read_operation::get_repository:
				return validate_repository_fixture(aDocument);
			case read_operation::get_pull_request:
				return validate_pull_request_fixture(aDocument);
			case read_operation::list_pull_request_files:
				return validate_files_fixture(aDocument);
			case read_operation::list_pull_request_commits:
				return validate_commits_fixture(aDocument);
			case read_operation::list_reviews:
				return validate_reviews_fixture(aDocument);
			case read_operation::list_review_threads:
				return validate_review_threads_fixture(aDocument);
			case read_operation::list_issue_comments:
				return validate_issue_comments_fixture(aDocument);
			case read_operation::list_commit_statuses:
				return false; // Optional production-only collection.
			case read_operation::list_checks:
				return validate_checks_fixture(aDocument);
			case read_operation::search_pull_requests:
				return validate_search_fixture(aDocument);
			case read_operation::list_review_comments:
				return validate_review_comments_fixture(aDocument);
			case read_operation::search_personal_pull_requests:
			case read_operation::list_review_requests:
			case read_operation::get_merge_assessment:
			case read_operation::resolve_body_images:
			case read_operation::list_branch_rules:
			case read_operation::get_branch_protection:
			case read_operation::list_requested_reviewers:
			case read_operation::list_reactions:
				return false; // Discovery normalization belongs to its bounded production reader.
			}
			return false;
		}
	}

	auto operation_name(const read_operation aOperation) noexcept
		-> std::string_view
	{
		switch (aOperation) {
		case read_operation::list_pull_request_timeline:
			return "ListPullRequestTimeline";
		case read_operation::get_viewer_identity:
			return "GetViewerIdentity";
		case read_operation::get_repository:
			return "GetRepository";
		case read_operation::get_pull_request:
			return "GetPullRequest";
		case read_operation::list_pull_request_files:
			return "ListPullRequestFiles";
		case read_operation::list_pull_request_commits:
			return "ListPullRequestCommits";
		case read_operation::list_reviews:
			return "ListReviews";
		case read_operation::list_review_threads:
			return "ListReviewThreads";
		case read_operation::list_issue_comments:
			return "ListIssueComments";
		case read_operation::list_commit_statuses:
			return "ListCommitStatuses";
		case read_operation::list_checks:
			return "ListChecks";
		case read_operation::search_pull_requests:
			return "SearchPullRequests";
		case read_operation::list_review_comments:
			return "ListReviewComments";
		case read_operation::search_personal_pull_requests:
			return "SearchPersonalPullRequests";
		case read_operation::list_requested_reviewers:
			return "ListRequestedReviewers";
		case read_operation::list_review_requests:    return "ListReviewRequests";
		case read_operation::get_merge_assessment:    return "GetMergeAssessment";
		case read_operation::list_branch_rules:       return "ListBranchRules";
		case read_operation::get_branch_protection:   return "GetBranchProtection";
		case read_operation::get_merge_base:          return "GetMergeBase";
		case read_operation::get_team_membership:     return "GetTeamMembership";
		case read_operation::list_home_pull_requests: return "ListHomePullRequests";
		case read_operation::resolve_body_images:     return "ResolveBodyImages";
		case read_operation::list_reactions:          return "ListReactions";
		}
		return { };
	}

	auto read_operation_from_name(const std::string_view aName) noexcept
		-> operation_result
	{
		for (const auto operation : all_read_operations) {
			if (operation_name(operation) == aName) {
				return operation;
			}
		}

		constexpr std::array<std::string_view, 11> mutations{
			"MarkFileViewed",
			"MarkFileUnviewed",
			"CreateReview",
			"SubmitReview",
			"CreateThread",
			"ReplyToThread",
			"ResolveThread",
			"UnresolveThread",
			// [[BPR-1368]]. The pending tour stop edit.
			"UpdateReviewComment",
			// [[BPR-1741]]. A reaction added or removed.
			"AddReaction",
			"RemoveReaction",
		};
		if (std::ranges::find(mutations, aName) != mutations.end()) {
			return failure(contract_failure_code::mutation_operation);
		}
		return failure(contract_failure_code::unsupported_operation);
	}

	auto compile_read_request_impl(
		const semantic_read_request& aRequest,
		const bool                   aValidateResult)
		-> request_result
	{
		if (!known_read_operation(aRequest.Operation)) {
			return failure(contract_failure_code::unsupported_operation);
		}
		const auto repositoryRequired = needs_repository(aRequest.Operation);
		if (repositoryRequired) {
			if (
				!safe_path_component(aRequest.Owner)
				|| !safe_path_component(aRequest.Repository)) {
				return failure(contract_failure_code::invalid_repository);
			}
		} else if (!aRequest.Owner.empty() || !aRequest.Repository.empty()) {
			return failure(contract_failure_code::invalid_repository);
		}
		const auto pullRequestNumberRequired = needs_pull_request_number(aRequest.Operation);
		if (
			(
				pullRequestNumberRequired
				&& (aRequest.PullRequestNumber == 0U
					|| aRequest.PullRequestNumber
						> core::maximum_pull_request_number))
			|| (!pullRequestNumberRequired
				&& aRequest.PullRequestNumber != 0U)) {
			return failure(
				contract_failure_code::invalid_pull_request_number);
		}
		const auto maximumOperationPage = maximum_page_for(aRequest.Operation);
		if (aRequest.Page == 0U || aRequest.Page > maximumOperationPage) {
			return failure(contract_failure_code::invalid_page);
		}
		const auto mergeBase       = aRequest.Operation == read_operation::get_merge_base;
		const auto headOidRequired = (aRequest.Operation == read_operation::list_checks || aRequest.Operation == read_operation::list_commit_statuses || mergeBase);
		if (
			(headOidRequired && !hexadecimal_oid(aRequest.HeadOid))
			|| (!headOidRequired && !aRequest.HeadOid.empty())
			|| (mergeBase && aRequest.BranchRef.empty() && (!hexadecimal_oid(aRequest.BaseOid) || aRequest.BaseOid.size() != aRequest.HeadOid.size()))
			|| ((!mergeBase || !aRequest.BranchRef.empty()) && !aRequest.BaseOid.empty())) {
			return failure(contract_failure_code::invalid_commit_oid);
		}
		const auto searchTermsRequired = aRequest.Operation == read_operation::search_pull_requests;
		const auto personalTerms       = aRequest.Operation == read_operation::search_personal_pull_requests;
		if (
			(
				searchTermsRequired
				&& !safe_search_terms(aRequest.SearchTerms))
			|| (personalTerms && !safe_personal_search_terms(aRequest.SearchTerms))
			|| (personalTerms && !safe_search_repositories(aRequest.Owner, aRequest.Repository, aRequest.SearchTerms, aRequest.SearchRepositories))
			|| (!personalTerms && !aRequest.SearchRepositories.empty())
			|| (!searchTermsRequired && !personalTerms
				&& !aRequest.SearchTerms.empty())) {
			return failure(contract_failure_code::invalid_search_terms);
		}

		const auto graphql = graphql_read(aRequest.Operation);
		if (
			(!graphql && aRequest.GraphqlPage.has_value())
			|| (aRequest.GraphqlPage.has_value()
				&& !valid_graphql_page(
					*aRequest.GraphqlPage,
					aRequest.Page))
			|| (aRequest.Operation == read_operation::list_review_threads && !aRequest.GraphqlPage.has_value()
				&& aRequest.Page != 1U)) {
			return failure(contract_failure_code::unexpected_body);
		}
		// [[BPR-1560]]. The merge-base read names its base either by the base
		// branch (an open pull request: its current tip) or by a commit.
		const auto branch = aRequest.Operation == read_operation::list_branch_rules || aRequest.Operation == read_operation::get_branch_protection
			|| (aRequest.Operation == read_operation::get_merge_base && !aRequest.BranchRef.empty());
		if ((branch && (aRequest.BranchRef.empty() || aRequest.BranchRef.size() > 256U || std::ranges::any_of(aRequest.BranchRef, [](unsigned char aByte) { return aByte < 0x21U || aByte == 0x7fU; }))) || (!branch && !aRequest.BranchRef.empty())) {
			return failure(contract_failure_code::invalid_repository);
		}
		if ((aRequest.Operation != read_operation::list_review_requests && !aRequest.ReviewCursor.empty()) || (aRequest.Operation == read_operation::list_review_requests && ((aRequest.Page == 1U) != aRequest.ReviewCursor.empty() || (!aRequest.ReviewCursor.empty() && !bounded_graphql_token(aRequest.ReviewCursor, 1024U)))) || (aRequest.Operation != read_operation::list_review_threads && aRequest.GraphqlPage)) {
			return failure(contract_failure_code::unexpected_body);
		}
		const auto home = aRequest.Operation == read_operation::list_home_pull_requests;
		if ((home && !valid_home_numbers(aRequest.PullRequestNumbers)) || (!home && !aRequest.PullRequestNumbers.empty())) {
			return failure(contract_failure_code::invalid_pull_request_number);
		}
		const auto images    = aRequest.Operation == read_operation::resolve_body_images;
		const auto reactions = aRequest.Operation == read_operation::list_reactions;
		if ((images && !valid_body_nodes(aRequest.Nodes, core::markdown_image_bodies_per_resolution))
			|| (reactions && !valid_body_nodes(aRequest.Nodes, maximum_reaction_subjects_per_read))
			|| (!images && !reactions && !aRequest.Nodes.empty())) {
			return failure(contract_failure_code::unexpected_body);
		}
		const auto team = aRequest.Operation == read_operation::get_team_membership;
		if ((team && (!github_name(aRequest.Organization, 39U, false) || !github_name(aRequest.Team, 100U, true) || !github_name(aRequest.Login, 39U, false)))
			|| (!team && (!aRequest.Organization.empty() || !aRequest.Team.empty() || !aRequest.Login.empty()))) {
			return failure(contract_failure_code::unexpected_body);
		}
		const auto prefix = path_prefix(aRequest.Server, graphql);
		if (!prefix.has_value()) {
			return std::unexpected{ prefix.error() };
		}

		wire_request result{
			.Semantic  = aRequest,
			.Server    = aRequest.Server,
			.Operation = aRequest.Operation,
			.Method    = graphql ? http_method::post : http_method::get,
			.Path      = { },
			.Query     = { },
			.Headers   = graphql ? graphql_headers() : rest_headers(),
			.Body      = { },
		};

		switch (aRequest.Operation) {
		case read_operation::get_viewer_identity:
			result.Path = std::string{ *prefix } + "/user";
			break;
		case read_operation::get_repository:
			result.Path = repository_path(*prefix, aRequest);
			break;
		case read_operation::get_pull_request:
			result.Path = pull_request_path(*prefix, aRequest);
			break;
		case read_operation::list_pull_request_files:
			result.Path  = pull_request_path(*prefix, aRequest) + "/files";
			result.Query = "page=" + std::to_string(aRequest.Page)
				+ "&per_page=100";
			break;
		case read_operation::list_pull_request_commits:
			result.Path  = pull_request_path(*prefix, aRequest) + "/commits";
			result.Query = "page=" + std::to_string(aRequest.Page)
				+ "&per_page=100";
			break;
		case read_operation::list_pull_request_timeline:
			result.Path  = repository_path(*prefix, aRequest) + "/issues/" + std::to_string(aRequest.PullRequestNumber) + "/timeline";
			result.Query = "page=" + std::to_string(aRequest.Page) + "&per_page=100";
			break;
		case read_operation::list_reviews:
			result.Path  = pull_request_path(*prefix, aRequest) + "/reviews";
			result.Query = "page=" + std::to_string(aRequest.Page)
				+ "&per_page=100";
			break;
		case read_operation::list_review_threads: {
			result.Path = std::string{ *prefix } + "/graphql";
			auto body   = aRequest.GraphqlPage.has_value()
				? paged_review_body(aRequest, *aRequest.GraphqlPage)
				: read_query_body(aRequest, review_threads_document);
			if (!body) {
				return std::unexpected{ body.error() };
			}
			result.Body = std::move(*body);
			break;
		}
		case read_operation::get_merge_assessment:
		case read_operation::list_review_requests: {
			const auto assessment = aRequest.Operation == read_operation::get_merge_assessment;
			auto       query      = std::string{ "query " } + (assessment ? "GetMergeAssessment" : "ListReviewRequests") + "($owner:String!,$name:String!,$number:Int!";
			if (!aRequest.ReviewCursor.empty()) {
				query += ",$after:String!";
			}
			query += "){repository(owner:$owner,name:$name){id pullRequest(number:$number){id headRefOid baseRefOid baseRefName ";
			query += assessment ? "mergeStateStatus reviewDecision autoMergeRequest{enabledAt}" : "reviewRequests(first:100";
			if (!assessment) {
				if (!aRequest.ReviewCursor.empty()) {
					query += ",after:$after";
				}
				query += "){totalCount nodes{id asCodeOwner requestedReviewer{__typename ... on User{id login} ... on Team{id name slug organization{login}}}} pageInfo{hasNextPage endCursor}}";
			}
			query += "}}}";
			auto page        = github_graphql_page_request{ };
			page.Kind        = github_graphql_page_kind::threads;
			page.AfterCursor = aRequest.ReviewCursor;
			auto body        = read_query_body(aRequest, query, aRequest.ReviewCursor.empty() ? nullptr : &page);
			if (!body) {
				return std::unexpected{ body.error() };
			}
			result.Path = std::string{ *prefix } + "/graphql";
			result.Body = std::move(*body);
			break;
		}
		case read_operation::list_home_pull_requests: {
			auto body = home_pull_requests_body(aRequest);
			if (!body) {
				return std::unexpected{ body.error() };
			}
			result.Path = std::string{ *prefix } + "/graphql";
			result.Body = std::move(*body);
			break;
		}
		case read_operation::get_team_membership: {
			auto body = team_membership_query_body(aRequest);
			if (!body) {
				return std::unexpected{ body.error() };
			}
			result.Path = std::string{ *prefix } + "/graphql";
			result.Body = std::move(*body);
			break;
		}
		case read_operation::list_reactions: {
			auto body = body_images_query_body(aRequest, reactions_document);
			if (!body) {
				return std::unexpected{ body.error() };
			}
			result.Path = std::string{ *prefix } + "/graphql";
			result.Body = std::move(*body);
			break;
		}
		case read_operation::resolve_body_images: {
			auto body = body_images_query_body(aRequest, body_images_document);
			if (!body) {
				return std::unexpected{ body.error() };
			}
			result.Path = std::string{ *prefix } + "/graphql";
			result.Body = std::move(*body);
			break;
		}
		case read_operation::list_branch_rules:
			result.Path  = repository_path(*prefix, aRequest) + "/rules/branches/" + percent_encode(aRequest.BranchRef);
			result.Query = "page=" + std::to_string(aRequest.Page) + "&per_page=100";
			break;
		case read_operation::get_branch_protection:
			result.Path = repository_path(*prefix, aRequest) + "/branches/" + percent_encode(aRequest.BranchRef) + "/protection";
			break;
		case read_operation::list_issue_comments:
			result.Path = repository_path(*prefix, aRequest) + "/issues/"
				+ std::to_string(aRequest.PullRequestNumber) + "/comments";
			result.Query = "page=" + std::to_string(aRequest.Page)
				+ "&per_page=100";
			break;
		case read_operation::list_review_comments:
			result.Path  = pull_request_path(*prefix, aRequest) + "/comments";
			result.Query = "page=" + std::to_string(aRequest.Page)
				+ "&per_page=100";
			break;
		case read_operation::list_commit_statuses:
			result.Path  = repository_path(*prefix, aRequest) + "/commits/" + aRequest.HeadOid + "/statuses";
			result.Query = "page=" + std::to_string(aRequest.Page) + "&per_page=100";
			break;
		case read_operation::list_checks:
			result.Path = repository_path(*prefix, aRequest) + "/commits/"
				+ aRequest.HeadOid
				+ "/check-runs";
			result.Query = "page=" + std::to_string(aRequest.Page)
				+ "&per_page=100";
			break;
		case read_operation::search_pull_requests: {
			result.Path           = std::string{ *prefix } + "/search/issues";
			const auto expression = "is:pr " + aRequest.SearchTerms + " repo:"
				+ aRequest.Owner + "/" + aRequest.Repository;
			result.Query = "page=" + std::to_string(aRequest.Page)
				+ "&per_page=100&q=" + percent_encode(expression);
			break;
		}
		case read_operation::search_personal_pull_requests: {
			result.Path     = std::string{ *prefix } + "/search/issues";
			auto expression = "is:pr is:open " + aRequest.SearchTerms + " repo:"
				+ aRequest.Owner + "/" + aRequest.Repository;
			// [[BPR-1680]]. One search for several saved repositories.
			for (const auto& repository : aRequest.SearchRepositories) {
				expression += " repo:" + repository;
			}
			result.Query = "page=" + std::to_string(aRequest.Page)
				+ "&per_page=100&q=" + percent_encode(expression);
			break;
		}
		case read_operation::list_requested_reviewers:
			result.Path = pull_request_path(*prefix, aRequest) + "/requested_reviewers";
			break;
		case read_operation::get_merge_base:
			// [[BPR-1560]]. Page 2 of one commit per page carries the merge
			// base and base commit but no file list, which only page 1 has, so
			// the answer stays small however large the pull request is.
			result.Path  = repository_path(*prefix, aRequest) + "/compare/" + (aRequest.BranchRef.empty() ? aRequest.BaseOid : percent_encode(aRequest.BranchRef)) + "..." + aRequest.HeadOid;
			result.Query = "page=2&per_page=1";
			break;
		}

		if (aValidateResult) {
			if (const auto validation = validate_outbound_read_request(result);
				!validation.has_value()) {
				return std::unexpected{ validation.error() };
			}
		}
		return result;
	}

	auto compile_read_request(const semantic_read_request& aRequest)
		-> request_result
	{
		return compile_read_request_impl(aRequest, true);
	}

	auto validate_outbound_read_request(const wire_request& aRequest)
		-> std::expected<void, contract_failure>
	{
		if (!known_read_operation(aRequest.Operation)) {
			return failure(contract_failure_code::unsupported_operation);
		}
		if (
			std::ranges::any_of(
				aRequest.Headers,
				[](const http_header& aHeader) {
					return unsafe_header_name(aHeader.Name);
				})) {
			return failure(contract_failure_code::unsafe_header);
		}

		const auto graphql = graphql_read(aRequest.Operation);
		if (graphql) {
			if (aRequest.Method != http_method::post) {
				return failure(contract_failure_code::unexpected_method);
			}
			if (const auto validation = validate_graphql_body(
					aRequest.Body,
					aRequest.Semantic.GraphqlPage.has_value() || !aRequest.Semantic.ReviewCursor.empty(),
					aRequest.Operation == read_operation::resolve_body_images || aRequest.Operation == read_operation::list_reactions,
					aRequest.Operation == read_operation::get_team_membership,
					aRequest.Operation == read_operation::list_home_pull_requests);
				!validation.has_value()) {
				return validation;
			}
		} else if (aRequest.Method != http_method::get) {
			return failure(contract_failure_code::mutation_operation);
		} else if (!aRequest.Body.empty()) {
			return failure(contract_failure_code::unexpected_body);
		}

		const auto canonical = compile_read_request_impl(aRequest.Semantic, false);
		if (!canonical.has_value()) {
			return std::unexpected{ canonical.error() };
		}
		if (
			aRequest.Server != canonical->Server
			|| aRequest.Semantic.Server != aRequest.Server) {
			return failure(contract_failure_code::unexpected_path);
		}
		if (
			aRequest.Operation != canonical->Operation
			|| aRequest.Semantic.Operation != aRequest.Operation) {
			return failure(contract_failure_code::unexpected_operation);
		}
		if (aRequest.Method != canonical->Method) {
			return failure(contract_failure_code::unexpected_method);
		}
		if (aRequest.Path != canonical->Path) {
			return failure(contract_failure_code::unexpected_path);
		}
		if (aRequest.Query != canonical->Query) {
			return failure(contract_failure_code::unexpected_query);
		}
		if (aRequest.Headers != canonical->Headers) {
			return failure(contract_failure_code::unexpected_headers);
		}
		if (aRequest.Body != canonical->Body) {
			return failure(contract_failure_code::unexpected_body);
		}
		return { };
	}

	auto validate_strict_fixture_response(
		const read_operation aOperation,
		const wire_response& aResponse) -> std::expected<void, contract_failure>
	{
		if (!known_read_operation(aOperation)) {
			return failure(contract_failure_code::unsupported_operation);
		}
		switch (aResponse.Completeness) {
		case page_completeness::complete:
			break;
		case page_completeness::has_next_page:
			return failure(contract_failure_code::response_limit);
		case page_completeness::unknown:
		default:
			return failure(
				contract_failure_code::unexpected_response_shape);
		}
		if (aResponse.Status != 200) {
			return failure(contract_failure_code::invalid_response_status);
		}
		if (aResponse.Body.size() > maximum_fixture_body_bytes) {
			return failure(contract_failure_code::response_limit);
		}

		try {
			const std::vector<http_header> expectedHeaders{
				http_header{
					.Name  = "Content-Type",
					.Value = "application/json",
				},
			};
			if (aResponse.Headers != expectedHeaders) {
				return failure(
					contract_failure_code::invalid_response_headers);
			}

			const auto preflight = json_structural_preflight{ aResponse.Body }.run();
			if (preflight == json_preflight_result::structural_limit) {
				return failure(contract_failure_code::response_limit);
			}
			if (preflight == json_preflight_result::duplicate_key) {
				return failure(
					contract_failure_code::unexpected_response_fields);
			}
			if (preflight != json_preflight_result::valid) {
				return failure(contract_failure_code::malformed_response);
			}

			glz::skip validated;
			if (glz::read<read_request_json_options{ }>(validated, aResponse.Body)) {
				return failure(contract_failure_code::malformed_response);
			}
			const fixture_json_value document{ std::string_view{ aResponse.Body } };
			if (!json_is_object(document) && !json_is_array(document)) {
				return failure(contract_failure_code::malformed_response);
			}
			if (response_is_incomplete(aOperation, document)) {
				return failure(contract_failure_code::response_limit);
			}
			if (!response_shape_is_valid(aOperation, document)) {
				return failure(
					contract_failure_code::unexpected_response_fields);
			}
			return { };
		} catch (const std::bad_alloc&) {
			return failure(contract_failure_code::response_limit);
		}
	}

	strict_recorded_transport::strict_recorded_transport(
		std::vector<recorded_exchange> aExchanges)
		: Exchanges{ std::move(aExchanges) }
	{
	}

	void strict_recorded_transport::rewind() noexcept
	{
		NextExchange = 0U;
	}

	void strict_recorded_transport::skip_until(const read_operation aOperation) noexcept
	{
		while (NextExchange < Exchanges.size()
			   && Exchanges[NextExchange].Request.Operation != aOperation) {
			++NextExchange;
		}
	}

	auto strict_recorded_transport::perform(const wire_request& aRequest)
		-> response_result
	{
		if (const auto validation = validate_outbound_read_request(aRequest);
			!validation.has_value()) {
			return std::unexpected{ validation.error() };
		}
		if (NextExchange >= Exchanges.size()) {
			return failure(contract_failure_code::recording_exhausted);
		}

		const auto& expected = Exchanges[NextExchange];
		if (aRequest.Operation != expected.Request.Operation) {
			return failure(contract_failure_code::unexpected_operation);
		}
		if (aRequest.Method != expected.Request.Method) {
			return failure(contract_failure_code::unexpected_method);
		}
		if (aRequest.Path != expected.Request.Path) {
			return failure(contract_failure_code::unexpected_path);
		}
		if (aRequest.Query != expected.Request.Query) {
			return failure(contract_failure_code::unexpected_query);
		}
		if (aRequest.Headers != expected.Request.Headers) {
			return failure(contract_failure_code::unexpected_headers);
		}
		if (aRequest.Body != expected.Request.Body) {
			return failure(contract_failure_code::unexpected_body);
		}
		if (aRequest.Server != expected.Request.Server) {
			return failure(contract_failure_code::unexpected_path);
		}

		if (const auto validation = validate_strict_fixture_response(
				aRequest.Operation,
				expected.Response);
			!validation.has_value()) {
			return std::unexpected{ validation.error() };
		}
		++NextExchange;
		return expected.Response;
	}

	auto strict_recorded_transport::consumed_all() const noexcept -> bool
	{
		return NextExchange == Exchanges.size();
	}
}
