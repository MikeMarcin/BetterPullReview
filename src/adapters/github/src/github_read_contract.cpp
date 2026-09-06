#include "github_read_contract.hpp"
#include "github_graphql_operation_policy.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
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
															 "comments(first:100){nodes{id bodyText path line originalLine outdated "
															 "author{login ... on Node{id}} replyTo{id}} "
															 "pageInfo{hasNextPage endCursor}}} "
															 "pageInfo{hasNextPage "
															 "endCursor}}}}}";

		enum class json_preflight_result : std::uint8_t {
			valid,
			malformed,
			duplicate_key,
			structural_limit,
		};

		class json_structural_preflight
		{
		public:
			explicit json_structural_preflight(const std::string_view aInput)
				: Input{ aInput }
			{
			}

			[[nodiscard]] auto run() noexcept -> json_preflight_result
			{
				whitespace();
				const auto result = value(0U);
				if (result != json_preflight_result::valid) {
					return result;
				}
				whitespace();
				return Position == Input.size()
					? json_preflight_result::valid
					: json_preflight_result::malformed;
			}

		private:
			static constexpr std::size_t maximum_depth          = 32U;
			static constexpr std::size_t maximum_object_members = 64U;
			static constexpr std::size_t maximum_array_elements = 100U;
			static constexpr std::size_t maximum_total_values   = 50'000U;
			static constexpr std::size_t maximum_string_bytes   = 256U * 1024U;

			[[nodiscard]] auto value(const std::size_t aDepth) noexcept
				-> json_preflight_result
			{
				if (Values == maximum_total_values) {
					return json_preflight_result::structural_limit;
				}
				++Values;
				whitespace();
				if (Position >= Input.size()) {
					return json_preflight_result::malformed;
				}
				switch (Input[Position]) {
				case '{':
					return object(aDepth);
				case '[':
					return array(aDepth);
				case '"':
					return string(nullptr, false)
						? json_preflight_result::valid
						: json_preflight_result::malformed;
				case 't':
					return literal("true");
				case 'f':
					return literal("false");
				case 'n':
					return literal("null");
				default:
					return number();
				}
			}

			[[nodiscard]] auto object(const std::size_t aDepth) noexcept
				-> json_preflight_result
			{
				if (aDepth >= maximum_depth) {
					return json_preflight_result::structural_limit;
				}
				++Position;
				whitespace();
				if (consume('}')) {
					return json_preflight_result::valid;
				}

				std::array<std::string_view, maximum_object_members> keys;
				std::size_t                                          count = 0U;
				while (true) {
					if (count == keys.size()) {
						return json_preflight_result::structural_limit;
					}
					std::string_view key;
					if (!string(&key, true)) {
						return json_preflight_result::malformed;
					}
					if (
						std::ranges::find(
							keys.begin(),
							keys.begin() + static_cast<std::ptrdiff_t>(count),
							key)
						!= keys.begin() + static_cast<std::ptrdiff_t>(count)) {
						return json_preflight_result::duplicate_key;
					}
					keys[count++] = key;
					whitespace();
					if (!consume(':')) {
						return json_preflight_result::malformed;
					}
					const auto child = value(aDepth + 1U);
					if (child != json_preflight_result::valid) {
						return child;
					}
					whitespace();
					if (consume('}')) {
						return json_preflight_result::valid;
					}
					if (!consume(',')) {
						return json_preflight_result::malformed;
					}
					whitespace();
				}
			}

			[[nodiscard]] auto array(const std::size_t aDepth) noexcept
				-> json_preflight_result
			{
				if (aDepth >= maximum_depth) {
					return json_preflight_result::structural_limit;
				}
				++Position;
				whitespace();
				if (consume(']')) {
					return json_preflight_result::valid;
				}
				std::size_t count = 0U;
				while (true) {
					if (count == maximum_array_elements) {
						return json_preflight_result::structural_limit;
					}
					++count;
					const auto child = value(aDepth + 1U);
					if (child != json_preflight_result::valid) {
						return child;
					}
					whitespace();
					if (consume(']')) {
						return json_preflight_result::valid;
					}
					if (!consume(',')) {
						return json_preflight_result::malformed;
					}
					whitespace();
				}
			}

			[[nodiscard]] auto string(
				std::string_view* aOutput,
				const bool        aObjectKey) noexcept -> bool
			{
				if (!consume('"')) {
					return false;
				}
				const auto start = Position;
				while (Position < Input.size()) {
					if (Position - start > maximum_string_bytes) {
						return false;
					}
					const auto character = static_cast<unsigned char>(Input[Position++]);
					if (character == '"') {
						if (aOutput != nullptr) {
							*aOutput = Input.substr(
								start,
								Position - start - 1U);
						}
						return true;
					}
					if (character < 0x20U) {
						return false;
					}
					if (character != '\\') {
						continue;
					}
					if (aObjectKey || Position >= Input.size()) {
						return false;
					}
					const auto escaped = Input[Position++];
					if (
						escaped == '"' || escaped == '\\' || escaped == '/'
						|| escaped == 'b' || escaped == 'f' || escaped == 'n'
						|| escaped == 'r' || escaped == 't') {
						continue;
					}
					if (escaped != 'u' || Input.size() - Position < 4U) {
						return false;
					}
					for (std::size_t index = 0U; index < 4U; ++index) {
						const auto digit = static_cast<unsigned char>(Input[Position++]);
						if (std::isxdigit(digit) == 0) {
							return false;
						}
					}
				}
				return false;
			}

			[[nodiscard]] auto number() noexcept -> json_preflight_result
			{
				const auto start = Position;
				(void)consume('-');
				if (consume('0')) {
					if (
						Position < Input.size()
						&& Input[Position] >= '0' && Input[Position] <= '9') {
						return json_preflight_result::malformed;
					}
				} else {
					if (
						Position >= Input.size() || Input[Position] < '1'
						|| Input[Position] > '9') {
						return json_preflight_result::malformed;
					}
					while (
						Position < Input.size() && Input[Position] >= '0'
						&& Input[Position] <= '9') {
						++Position;
					}
				}
				if (consume('.')) {
					const auto digits = Position;
					while (
						Position < Input.size() && Input[Position] >= '0'
						&& Input[Position] <= '9') {
						++Position;
					}
					if (Position == digits) {
						return json_preflight_result::malformed;
					}
				}
				if (
					Position < Input.size()
					&& (Input[Position] == 'e' || Input[Position] == 'E')) {
					++Position;
					if (
						Position < Input.size()
						&& (Input[Position] == '+' || Input[Position] == '-')) {
						++Position;
					}
					const auto digits = Position;
					while (
						Position < Input.size() && Input[Position] >= '0'
						&& Input[Position] <= '9') {
						++Position;
					}
					if (Position == digits) {
						return json_preflight_result::malformed;
					}
				}
				return Position != start ? json_preflight_result::valid
										 : json_preflight_result::malformed;
			}

			[[nodiscard]] auto literal(const std::string_view aExpected) noexcept
				-> json_preflight_result
			{
				if (Input.substr(Position, aExpected.size()) != aExpected) {
					return json_preflight_result::malformed;
				}
				Position += aExpected.size();
				return json_preflight_result::valid;
			}

			auto whitespace() noexcept -> void
			{
				while (
					Position < Input.size()
					&& (Input[Position] == ' ' || Input[Position] == '\t'
						|| Input[Position] == '\r' || Input[Position] == '\n')) {
					++Position;
				}
			}

			[[nodiscard]] auto consume(const char aExpected) noexcept -> bool
			{
				if (Position >= Input.size() || Input[Position] != aExpected) {
					return false;
				}
				++Position;
				return true;
			}

			std::string_view Input;
			std::size_t      Position = 0U;
			std::size_t      Values   = 0U;
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

		[[nodiscard]] auto percent_encode(const std::string_view aValue) -> std::string
		{
			constexpr std::string_view hexadecimal = "0123456789ABCDEF";
			std::string                encoded;
			encoded.reserve(aValue.size());

			for (const auto rawCharacter : aValue) {
				const auto character  = static_cast<unsigned char>(rawCharacter);
				const auto unreserved = std::isalnum(character) != 0 || character == '-'
					|| character == '_' || character == '.' || character == '~';
				if (unreserved) {
					encoded.push_back(static_cast<char>(character));
					continue;
				}

				encoded.push_back('%');
				encoded.push_back(hexadecimal[(static_cast<unsigned>(character) >> 4U) & 0x0FU]);
				encoded.push_back(hexadecimal[character & 0x0FU]);
			}
			return encoded;
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

		struct read_request_json_options : glz::opts
		{
			bool escape_control_characters    = true;
			bool validate_skipped             = true;
			bool validate_trailing_whitespace = true;
			constexpr read_request_json_options()
			{
				null_terminated       = false;
				error_on_missing_keys = true;
			}
		};

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
			result += "){totalCount nodes{id bodyText path line originalLine "
					  "outdated author{login ... on Node{id}} replyTo{id}} ";
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
			case read_operation::list_reviews:
			case read_operation::list_review_threads:
			case read_operation::list_issue_comments:
			case read_operation::list_review_comments:
				return true;
			case read_operation::get_viewer_identity:
			case read_operation::get_repository:
			case read_operation::list_checks:
			case read_operation::search_pull_requests:
				return false;
			}
			return false;
		}

		[[nodiscard]] auto maximum_page_for(
			const read_operation aOperation) noexcept -> std::uint32_t
		{
			switch (aOperation) {
			case read_operation::list_pull_request_files:
				return 30U;
			case read_operation::list_pull_request_commits:
				return 3U;
			case read_operation::list_reviews:
			case read_operation::list_issue_comments:
			case read_operation::list_review_comments:
			case read_operation::list_checks:
				return 100U;
			case read_operation::search_pull_requests:
				return 10U;
			case read_operation::get_viewer_identity:
			case read_operation::get_repository:
			case read_operation::get_pull_request:
				return 1U;
			case read_operation::list_review_threads:
				return 1'024U;
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
			const bool             aPaged) -> std::expected<void, contract_failure>
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
			if (!variables.contains("owner") || !variables.contains("name")
				|| !variables.contains("number") || (!aPaged && variables.size() != 3U)) {
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

		[[nodiscard]] auto sorted_keys(const QJsonObject& aObject)
			-> std::vector<std::string>
		{
			std::vector<std::string> keys;
			keys.reserve(static_cast<std::size_t>(aObject.size()));
			for (auto iterator = aObject.constBegin(); iterator != aObject.constEnd();
				 ++iterator) {
				keys.emplace_back(iterator.key().toStdString());
			}
			std::ranges::sort(keys);
			return keys;
		}

		[[nodiscard]] auto has_exact_keys(
			const QJsonObject&                            aObject,
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
			const QJsonObject&                            aObject,
			const std::initializer_list<std::string_view> aExpected) -> bool
		{
			return std::ranges::all_of(
				aExpected,
				[&aObject](const std::string_view aKey) {
					return aObject.contains(
						QString::fromUtf8(aKey.data(),
										  static_cast<qsizetype>(aKey.size())));
				});
		}

		constexpr qsizetype maximum_fixture_page_records = 100;
		constexpr double    maximum_exact_json_integer   = 9'007'199'254'740'991.0;

		[[nodiscard]] auto exact_integer(
			const QJsonValue& aValue,
			const bool        aPositive = false) noexcept -> bool
		{
			if (!aValue.isDouble()) {
				return false;
			}
			const auto number = aValue.toDouble();
			return std::isfinite(number) && std::trunc(number) == number
				&& number >= (aPositive ? 1.0 : 0.0)
				&& number <= maximum_exact_json_integer;
		}

		[[nodiscard]] auto nonempty_string(const QJsonValue& aValue) -> bool
		{
			return aValue.isString() && !aValue.toString().isEmpty();
		}

		[[nodiscard]] auto valid_line(const QJsonValue& aValue) noexcept -> bool
		{
			return exact_integer(aValue, true)
				&& aValue.toDouble()
				<= static_cast<double>(
					   std::numeric_limits<std::int32_t>::max());
		}

		[[nodiscard]] auto valid_pull_request_number(
			const QJsonValue& aValue) noexcept -> bool
		{
			return exact_integer(aValue, true)
				&& aValue.toDouble()
				<= static_cast<double>(
					   core::maximum_pull_request_number);
		}

		[[nodiscard]] auto valid_json_oid(const QJsonValue& aValue) -> bool
		{
			if (!aValue.isString()) {
				return false;
			}
			const auto text = aValue.toString();
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
			const QJsonValue&                             aValue,
			const std::initializer_list<std::string_view> aAllowed) -> bool
		{
			if (!aValue.isString()) {
				return false;
			}
			const auto text = aValue.toString();
			return std::ranges::any_of(aAllowed, [&text](const auto aCandidate) {
				return text == QString::fromUtf8(aCandidate.data(), static_cast<qsizetype>(aCandidate.size()));
			});
		}

		[[nodiscard]] auto exact_rest_actor(const QJsonValue& aValue) -> bool
		{
			if (
				!aValue.isObject()
				|| !has_exact_keys(
					aValue.toObject(),
					{ "login", "node_id" })) {
				return false;
			}
			const auto object = aValue.toObject();
			return nonempty_string(object.value("login"))
				&& nonempty_string(object.value("node_id"));
		}

		[[nodiscard]] auto optional_rest_actor(const QJsonValue& aValue) -> bool
		{
			return aValue.isNull() || exact_rest_actor(aValue);
		}

		[[nodiscard]] auto exact_graphql_author(const QJsonValue& aValue) -> bool
		{
			return aValue.isObject()
				&& has_exact_keys(aValue.toObject(), { "id", "login" })
				&& nonempty_string(aValue.toObject().value("id"))
				&& nonempty_string(aValue.toObject().value("login"));
		}

		[[nodiscard]] auto exact_page_info(
			const QJsonValue& aValue,
			const qsizetype   aNodeCount) -> bool
		{
			if (
				!aValue.isObject()
				|| !has_exact_keys(
					aValue.toObject(),
					{ "endCursor", "hasNextPage" })) {
				return false;
			}
			const auto object = aValue.toObject();
			if (
				!object.value("hasNextPage").isBool()
				|| object.value("hasNextPage").toBool()) {
				return false;
			}
			const auto cursor = object.value("endCursor");
			if (aNodeCount == 0) {
				return cursor.isNull();
			}
			return nonempty_string(cursor)
				&& cursor.toString().toUtf8().size() <= maximum_cursor_bytes;
		}

		[[nodiscard]] auto bounded_object_array(
			const QJsonValue&                             aValue,
			const std::initializer_list<std::string_view> aKeys) -> bool
		{
			if (!aValue.isArray()) {
				return false;
			}
			const auto array = aValue.toArray();
			return array.size() <= maximum_fixture_page_records
				&& std::ranges::all_of(array, [aKeys](const QJsonValue& aItem) {
					   return aItem.isObject()
						   && has_exact_keys(aItem.toObject(), aKeys);
				   });
		}

		[[nodiscard]] auto validate_viewer_fixture(const QJsonDocument& aDocument)
			-> bool
		{
			if (
				!aDocument.isObject()
				|| !has_exact_keys(
					aDocument.object(),
					{ "id", "login", "name", "node_id" })) {
				return false;
			}
			const auto object = aDocument.object();
			return exact_integer(object.value("id"), true)
				&& nonempty_string(object.value("login"))
				&& (object.value("name").isNull()
					|| object.value("name").isString())
				&& nonempty_string(object.value("node_id"));
		}

		[[nodiscard]] auto validate_repository_fixture(const QJsonDocument& aDocument)
			-> bool
		{
			if (
				!aDocument.isObject()
				|| !has_exact_keys(
					aDocument.object(),
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
			const auto object = aDocument.object();
			return exact_integer(object.value("id"), true)
				&& nonempty_string(object.value("name"))
				&& nonempty_string(object.value("full_name"))
				&& nonempty_string(object.value("node_id"))
				&& object.value("owner").isObject()
				&& has_exact_keys(
					   object.value("owner").toObject(),
					   { "login" })
				&& nonempty_string(
					   object.value("owner").toObject().value("login"))
				&& object.value("private").isBool()
				&& nonempty_string(object.value("default_branch"));
		}

		[[nodiscard]] auto exact_pull_ref(
			const QJsonValue& aValue,
			const bool        aRepositoryRequired) -> bool
		{
			if (
				!aValue.isObject()
				|| !has_exact_keys(aValue.toObject(), { "ref", "repo", "sha" })) {
				return false;
			}
			const auto object     = aValue.toObject();
			const auto repository = object.value("repo");
			if (
				!nonempty_string(object.value("ref"))
				|| !valid_json_oid(object.value("sha"))) {
				return false;
			}
			if (repository.isNull()) {
				return !aRepositoryRequired;
			}
			return repository.isObject()
				&& has_exact_keys(
					   repository.toObject(),
					   { "full_name", "id" })
				&& exact_integer(repository.toObject().value("id"), true)
				&& nonempty_string(
					   repository.toObject().value("full_name"));
		}

		[[nodiscard]] auto validate_pull_request_fixture(
			const QJsonDocument& aDocument) -> bool
		{
			if (
				!aDocument.isObject()
				|| !has_exact_keys(
					aDocument.object(),
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
			const auto object = aDocument.object();
			const auto merged = object.value("merged").toBool();
			return exact_integer(object.value("id"), true)
				&& valid_pull_request_number(object.value("number"))
				&& exact_integer(object.value("changed_files"))
				&& exact_integer(object.value("commits"), true)
				&& nonempty_string(object.value("node_id"))
				&& one_of(object.value("state"), { "open", "closed" })
				&& nonempty_string(object.value("title"))
				&& (object.value("body").isNull()
					|| object.value("body").isString())
				&& object.value("draft").isBool()
				&& optional_rest_actor(object.value("user"))
				&& exact_pull_ref(object.value("base"), true)
				&& exact_pull_ref(object.value("head"), false)
				&& object.value("merged").isBool()
				&& (!merged || object.value("state").toString() == "closed");
		}

		[[nodiscard]] auto validate_files_fixture(const QJsonDocument& aDocument)
			-> bool
		{
			if (
				!aDocument.isArray()
				|| aDocument.array().size() > maximum_fixture_page_records) {
				return false;
			}
			return std::ranges::all_of(aDocument.array(), [](const QJsonValue& aValue) {
				if (
					!aValue.isObject()
					|| !(
						has_exact_keys(
							aValue.toObject(),
							{
								"additions",
								"deletions",
								"filename",
								"status",
							})
						|| has_exact_keys(
							aValue.toObject(),
							{
								"additions",
								"deletions",
								"filename",
								"previous_filename",
								"status",
							}))) {
					return false;
				}
				const auto object = aValue.toObject();
				if (
					!nonempty_string(object.value("filename"))
					|| !one_of(
						object.value("status"),
						{
							"added",
							"modified",
							"removed",
							"renamed",
							"copied",
							"changed",
							"unchanged",
						})
					|| !exact_integer(object.value("additions"))
					|| !exact_integer(object.value("deletions"))) {
					return false;
				}
				const auto hasPrevious = object.contains("previous_filename");
				const auto status      = object.value("status").toString();
				if (
					hasPrevious
					&& ((
							status != "renamed"
							&& status != "copied")
						|| !nonempty_string(
							object.value("previous_filename")))) {
					return false;
				}
				return true;
			});
		}

		[[nodiscard]] auto validate_commits_fixture(const QJsonDocument& aDocument)
			-> bool
		{
			if (
				!aDocument.isArray()
				|| aDocument.array().size() > maximum_fixture_page_records) {
				return false;
			}
			return std::ranges::all_of(aDocument.array(), [](const QJsonValue& aValue) {
				if (
					!aValue.isObject()
					|| !has_exact_keys(
						aValue.toObject(),
						{ "author", "commit", "node_id", "sha" })) {
					return false;
				}
				const auto object = aValue.toObject();
				const auto commit = object.value("commit");
				return valid_json_oid(object.value("sha"))
					&& nonempty_string(object.value("node_id"))
					&& optional_rest_actor(object.value("author"))
					&& commit.isObject()
					&& has_exact_keys(commit.toObject(), { "message" })
					&& commit.toObject().value("message").isString();
			});
		}

		[[nodiscard]] auto validate_reviews_fixture(const QJsonDocument& aDocument)
			-> bool
		{
			if (
				!aDocument.isArray()
				|| aDocument.array().size() > maximum_fixture_page_records) {
				return false;
			}
			return std::ranges::all_of(aDocument.array(), [](const QJsonValue& aValue) {
				if (
					!aValue.isObject()
					|| !has_exact_keys(
						aValue.toObject(),
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
				const auto object = aValue.toObject();
				return exact_integer(object.value("id"), true)
					&& nonempty_string(object.value("node_id"))
					&& optional_rest_actor(object.value("user"))
					&& object.value("body").isString()
					&& one_of(
						   object.value("state"),
						   {
							   "PENDING",
							   "COMMENTED",
							   "APPROVED",
							   "CHANGES_REQUESTED",
							   "DISMISSED",
						   })
					&& (object.value("commit_id").isNull()
						|| valid_json_oid(object.value("commit_id")));
			});
		}

		[[nodiscard]] auto exact_graphql_comment(const QJsonValue& aValue) -> bool
		{
			if (
				!aValue.isObject()
				|| !has_exact_keys(
					aValue.toObject(),
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
				return false;
			}
			const auto object       = aValue.toObject();
			const auto replyTo      = object.value("replyTo");
			const auto line         = object.value("line");
			const auto originalLine = object.value("originalLine");
			return nonempty_string(object.value("id"))
				&& object.value("bodyText").isString()
				&& nonempty_string(object.value("path"))
				&& (line.isNull() || valid_line(line))
				&& (originalLine.isNull()
					|| valid_line(originalLine))
				&& object.value("outdated").isBool()
				&& (object.value("author").isNull()
					|| exact_graphql_author(object.value("author")))
				&& (replyTo.isNull()
					|| (replyTo.isObject()
						&& has_exact_keys(replyTo.toObject(), { "id" })
						&& nonempty_string(replyTo.toObject().value("id"))));
		}

		[[nodiscard]] auto validate_review_threads_fixture(
			const QJsonDocument& aDocument) -> bool
		{
			if (
				!aDocument.isObject()
				|| !has_exact_keys(aDocument.object(), { "data" })) {
				return false;
			}
			const auto data = aDocument.object().value("data");
			if (
				!data.isObject()
				|| !has_exact_keys(data.toObject(), { "repository" })) {
				return false;
			}
			const auto repository = data.toObject().value("repository");
			if (
				!repository.isObject()
				|| !has_exact_keys(
					repository.toObject(),
					{ "id", "pullRequest" })
				|| !nonempty_string(repository.toObject().value("id"))) {
				return false;
			}
			const auto pullRequest = repository.toObject().value("pullRequest");
			if (
				!pullRequest.isObject()
				|| !has_exact_keys(
					pullRequest.toObject(),
					{ "files", "headRefOid", "id", "reviewThreads" })
				|| !nonempty_string(pullRequest.toObject().value("headRefOid"))
				|| !nonempty_string(pullRequest.toObject().value("id"))) {
				return false;
			}
			const auto files = pullRequest.toObject().value("files");
			if (
				!files.isObject()
				|| !has_exact_keys(
					files.toObject(), { "nodes", "pageInfo", "totalCount" })
				|| !exact_integer(files.toObject().value("totalCount"))
				|| !bounded_object_array(
					files.toObject().value("nodes"),
					{ "path", "viewerViewedState" })) {
				return false;
			}
			const auto fileNodes = files.toObject().value("nodes").toArray();
			if (
				!exact_page_info(
					files.toObject().value("pageInfo"),
					fileNodes.size())
				|| files.toObject().value("totalCount").toDouble()
					!= static_cast<double>(fileNodes.size())
				|| !std::ranges::all_of(
					fileNodes,
					[](const QJsonValue& aValue) {
						const auto object = aValue.toObject();
						return nonempty_string(object.value("path"))
							&& one_of(
								   object.value("viewerViewedState"),
								   { "DISMISSED", "UNVIEWED", "VIEWED" });
					})) {
				return false;
			}
			const auto threads = pullRequest.toObject().value("reviewThreads");
			if (
				!threads.isObject()
				|| !has_exact_keys(
					threads.toObject(),
					{ "nodes", "pageInfo", "totalCount" })
				|| !exact_integer(threads.toObject().value("totalCount"))
				|| !bounded_object_array(
					threads.toObject().value("nodes"),
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
			const auto threadNodes = threads.toObject().value("nodes").toArray();
			if (
				!exact_page_info(
					threads.toObject().value("pageInfo"),
					threadNodes.size())
				|| threads.toObject().value("totalCount").toDouble()
					!= static_cast<double>(threadNodes.size())) {
				return false;
			}

			for (const auto& threadValue : threadNodes) {
				const auto thread       = threadValue.toObject();
				const auto comments     = thread.value("comments");
				const auto diffSide     = thread.value("diffSide");
				const auto line         = thread.value("line");
				const auto originalLine = thread.value("originalLine");
				const auto commentNodes = comments.toObject().value("nodes").toArray();
				const auto fileSubject  = thread.value("subjectType").toString() == "FILE";
				const auto lineSubject  = thread.value("subjectType").toString() == "LINE";
				if (
					!nonempty_string(thread.value("id"))
					|| !thread.value("isResolved").isBool()
					|| !thread.value("isOutdated").isBool()
					|| !thread.value("viewerCanResolve").isBool()
					|| !thread.value("viewerCanUnresolve").isBool()
					|| !one_of(diffSide, { "LEFT", "RIGHT" })
					|| (!fileSubject && !lineSubject)
					|| !nonempty_string(thread.value("path"))
					|| !(
						line.isNull() || valid_line(line))
					|| !(
						originalLine.isNull()
						|| valid_line(originalLine))
					|| (fileSubject
						&& (!line.isNull() || !originalLine.isNull()))
					|| (lineSubject
						&& ((!thread.value("isOutdated").toBool()
							 && line.isNull())
							|| (thread.value("isOutdated").toBool()
								&& line.isNull() && originalLine.isNull())))
					|| !comments.isObject()
					|| !has_exact_keys(
						comments.toObject(),
						{ "nodes", "pageInfo", "totalCount" })
					|| !exact_integer(comments.toObject().value("totalCount"))
					|| !comments.toObject().value("nodes").isArray()
					|| commentNodes.size()
						> maximum_fixture_page_records
					|| !exact_page_info(
						comments.toObject().value("pageInfo"),
						commentNodes.size())
					|| comments.toObject().value("totalCount").toDouble()
						!= static_cast<double>(commentNodes.size())
					|| !std::ranges::all_of(
						commentNodes,
						exact_graphql_comment)) {
					return false;
				}
				for (const auto& commentValue : commentNodes) {
					const auto comment             = commentValue.toObject();
					const auto commentLine         = comment.value("line");
					const auto commentOriginalLine = comment.value("originalLine");
					if (
						(
							fileSubject
							&& (!commentLine.isNull()
								|| !commentOriginalLine.isNull()))
						|| comment.value("path") != thread.value("path")) {
						return false;
					}
				}
			}
			return true;
		}

		[[nodiscard]] auto validate_issue_comments_fixture(
			const QJsonDocument& aDocument) -> bool
		{
			if (
				!aDocument.isArray()
				|| aDocument.array().size() > maximum_fixture_page_records) {
				return false;
			}
			return std::ranges::all_of(aDocument.array(), [](const QJsonValue& aValue) {
				if (
					!aValue.isObject()
					|| !has_exact_keys(
						aValue.toObject(),
						{
							"body",
							"node_id",
							"user",
						})) {
					return false;
				}
				const auto object = aValue.toObject();
				return nonempty_string(object.value("node_id"))
					&& optional_rest_actor(object.value("user"))
					&& object.value("body").isString();
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
			const QJsonDocument& aDocument) -> bool
		{
			if (
				!aDocument.isArray()
				|| aDocument.array().size() > maximum_fixture_page_records) {
				return false;
			}
			return std::ranges::all_of(aDocument.array(), [](const QJsonValue& aValue) {
				if (
					!aValue.isObject()
					|| !has_required_fixture_keys(
						aValue.toObject(),
						{
							"body",
							"id",
							"path",
							"user",
						})) {
					return false;
				}
				const auto object = aValue.toObject();
				const auto reply  = object.value("in_reply_to_id");
				if (
					!reply.isUndefined() && !reply.isNull()
					&& !exact_integer(reply, true)) {
					return false;
				}
				const auto user = object.value("user");
				if (
					!user.isNull()
					&& (!user.isObject()
						|| !has_required_fixture_keys(user.toObject(),
													  { "login", "node_id" })
						|| !nonempty_string(user.toObject().value("login"))
						|| !nonempty_string(user.toObject().value("node_id")))) {
					return false;
				}
				return exact_integer(object.value("id"), true)
					&& nonempty_string(object.value("path"))
					&& object.value("body").isString();
			});
		}

		[[nodiscard]] auto validate_checks_fixture(const QJsonDocument& aDocument)
			-> bool
		{
			if (
				!aDocument.isObject()
				|| !has_exact_keys(
					aDocument.object(),
					{ "check_runs", "total_count" })
				|| !exact_integer(aDocument.object().value("total_count"))
				|| !bounded_object_array(
					aDocument.object().value("check_runs"),
					{
						"conclusion",
						"head_sha",
						"name",
						"node_id",
						"status",
					})) {
				return false;
			}
			const auto runs = aDocument.object().value("check_runs").toArray();
			if (
				aDocument.object().value("total_count").toDouble()
				!= static_cast<double>(runs.size())) {
				return false;
			}
			return std::ranges::all_of(runs, [](const QJsonValue& aValue) {
				const auto object    = aValue.toObject();
				const auto completed = object.value("status").toString() == "completed";
				return nonempty_string(object.value("node_id"))
					&& nonempty_string(object.value("name"))
					&& valid_json_oid(object.value("head_sha"))
					&& one_of(
						   object.value("status"),
						   {
							   "queued",
							   "in_progress",
							   "completed",
							   "waiting",
							   "requested",
							   "pending",
						   })
					&& (object.value("conclusion").isNull()
						|| one_of(
							object.value("conclusion"),
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
							? !object.value("conclusion").isNull()
							: object.value("conclusion").isNull());
			});
		}

		[[nodiscard]] auto exact_search_item(const QJsonValue& aValue) -> bool
		{
			if (
				!aValue.isObject()
				|| !has_exact_keys(
					aValue.toObject(),
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
			const auto object      = aValue.toObject();
			const auto pullRequest = object.value("pull_request");
			return nonempty_string(object.value("node_id"))
				&& valid_pull_request_number(object.value("number"))
				&& one_of(object.value("state"), { "open" })
				&& nonempty_string(object.value("title"))
				&& object.value("draft").isBool()
				&& nonempty_string(object.value("repository_url"))
				&& nonempty_string(object.value("html_url"))
				&& pullRequest.isObject()
				&& has_exact_keys(pullRequest.toObject(), { "url" })
				&& nonempty_string(pullRequest.toObject().value("url"));
		}

		[[nodiscard]] auto validate_search_fixture(const QJsonDocument& aDocument)
			-> bool
		{
			if (
				!aDocument.isObject()
				|| !has_exact_keys(
					aDocument.object(),
					{ "incomplete_results", "items", "total_count" })
				|| !aDocument.object().value("incomplete_results").isBool()
				|| aDocument.object().value("incomplete_results").toBool()
				|| !exact_integer(aDocument.object().value("total_count"))
				|| !aDocument.object().value("items").isArray()
				|| aDocument.object().value("items").toArray().size()
					> maximum_fixture_page_records) {
				return false;
			}
			const auto items = aDocument.object().value("items").toArray();
			return aDocument.object().value("total_count").toDouble()
				== static_cast<double>(items.size())
				&& std::ranges::all_of(items, exact_search_item);
		}

		[[nodiscard]] auto response_is_incomplete(
			const read_operation aOperation,
			const QJsonDocument& aDocument) -> bool
		{
			if (aOperation == read_operation::list_checks && aDocument.isObject()) {
				const auto object = aDocument.object();
				return object.value("total_count").isDouble()
					&& object.value("check_runs").isArray()
					&& object.value("total_count").toDouble()
					> static_cast<double>(
						   object.value("check_runs").toArray().size());
			}
			if (
				aOperation == read_operation::search_pull_requests
				&& aDocument.isObject()) {
				const auto object = aDocument.object();
				return (
						   object.value("incomplete_results").isBool()
						   && object.value("incomplete_results").toBool())
					|| (object.value("total_count").isDouble()
						&& object.value("items").isArray()
						&& object.value("total_count").toDouble()
							> static_cast<double>(
								object.value("items").toArray().size()));
			}
			if (
				aOperation != read_operation::list_review_threads
				|| !aDocument.isObject()) {
				return false;
			}

			const auto pullRequest = aDocument.object()
										 .value("data")
										 .toObject()
										 .value("repository")
										 .toObject()
										 .value("pullRequest")
										 .toObject();
			const auto files       = pullRequest.value("files").toObject();
			if (files.value("pageInfo")
					.toObject()
					.value("hasNextPage")
					.toBool()) {
				return true;
			}

			const auto threads = pullRequest.value("reviewThreads").toObject();
			if (threads.value("pageInfo")
					.toObject()
					.value("hasNextPage")
					.toBool()) {
				return true;
			}
			return std::ranges::any_of(
				threads.value("nodes").toArray(),
				[](const QJsonValue& aValue) {
					return aValue.toObject()
						.value("comments")
						.toObject()
						.value("pageInfo")
						.toObject()
						.value("hasNextPage")
						.toBool();
				});
		}

		[[nodiscard]] auto response_shape_is_valid(
			const read_operation aOperation,
			const QJsonDocument& aDocument) -> bool
		{
			switch (aOperation) {
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
			case read_operation::list_checks:
				return validate_checks_fixture(aDocument);
			case read_operation::search_pull_requests:
				return validate_search_fixture(aDocument);
			case read_operation::list_review_comments:
				return validate_review_comments_fixture(aDocument);
			}
			return false;
		}
	}

	auto operation_name(const read_operation aOperation) noexcept
		-> std::string_view
	{
		switch (aOperation) {
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
		case read_operation::list_checks:
			return "ListChecks";
		case read_operation::search_pull_requests:
			return "SearchPullRequests";
		case read_operation::list_review_comments:
			return "ListReviewComments";
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

		constexpr std::array<std::string_view, 8> mutations{
			"MarkFileViewed",
			"MarkFileUnviewed",
			"CreateReview",
			"SubmitReview",
			"CreateThread",
			"ReplyToThread",
			"ResolveThread",
			"UnresolveThread",
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
		const auto headOidRequired = aRequest.Operation == read_operation::list_checks;
		if (
			(headOidRequired && !hexadecimal_oid(aRequest.HeadOid))
			|| (!headOidRequired && !aRequest.HeadOid.empty())) {
			return failure(contract_failure_code::invalid_commit_oid);
		}
		const auto searchTermsRequired = aRequest.Operation == read_operation::search_pull_requests;
		if (
			(
				searchTermsRequired
				&& !safe_search_terms(aRequest.SearchTerms))
			|| (!searchTermsRequired
				&& !aRequest.SearchTerms.empty())) {
			return failure(contract_failure_code::invalid_search_terms);
		}

		const auto graphql = aRequest.Operation == read_operation::list_review_threads;
		if (
			(!graphql && aRequest.GraphqlPage.has_value())
			|| (aRequest.GraphqlPage.has_value()
				&& !valid_graphql_page(
					*aRequest.GraphqlPage,
					aRequest.Page))
			|| (graphql && !aRequest.GraphqlPage.has_value()
				&& aRequest.Page != 1U)) {
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

		const auto graphql = aRequest.Operation == read_operation::list_review_threads;
		if (graphql) {
			if (aRequest.Method != http_method::post) {
				return failure(contract_failure_code::unexpected_method);
			}
			if (const auto validation = validate_graphql_body(
					aRequest.Body,
					aRequest.Semantic.GraphqlPage.has_value());
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

			QJsonParseError parseError;
			const auto      document = QJsonDocument::fromJson(
				QByteArray{
					aResponse.Body.data(),
					static_cast<qsizetype>(aResponse.Body.size()),
				},
				&parseError);
			if (parseError.error != QJsonParseError::NoError) {
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
