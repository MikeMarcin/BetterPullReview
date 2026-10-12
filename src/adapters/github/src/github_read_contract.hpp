#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_READ_CONTRACT_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_READ_CONTRACT_HPP

#include "bpr/application/github_read_model.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::adapters::github::detail {
	enum class server_contract : std::uint8_t {
		github_com,
		ghes_3_18,
	};

	enum class read_operation : std::uint8_t {
		get_viewer_identity,
		get_repository,
		get_pull_request,
		list_pull_request_files,
		list_pull_request_commits,
		list_reviews,
		list_review_threads,
		list_issue_comments,
		list_checks,
		list_commit_statuses,
		search_pull_requests,
		list_review_comments,
		list_pull_request_timeline,
		/// [[BPR-0988]]. Personal discovery: open pull requests of one
		/// repository authored by, or with a review requested of, one login.
		search_personal_pull_requests,
		/// [[BPR-0988]]. The users and teams a pull request still awaits.
		list_requested_reviewers,
	};

	/// \brief Names the ten operations the recorded contract manifest holds.
	///
	/// \details This is the recorded authenticated wire shape and its exact
	/// order: a contract fixture manifest is checked against it entry by
	/// entry, so the catalog is a recording contract rather than the set of
	/// operations the adapter can compile. `all_read_operations` is the
	/// latter. Adding an operation here changes what every recorded manifest
	/// must contain; adding one there does not.
	inline constexpr std::array<read_operation, 10> read_operation_catalog{
		read_operation::get_viewer_identity,
		read_operation::get_repository,
		read_operation::get_pull_request,
		read_operation::list_pull_request_files,
		read_operation::list_pull_request_commits,
		read_operation::list_reviews,
		read_operation::list_review_threads,
		read_operation::list_issue_comments,
		read_operation::list_checks,
		read_operation::search_pull_requests,
	};

	/// \brief Names every read operation the contract compiles.
	///
	/// \details `list_review_comments` is the REST inline review-comment read
	/// RFC-BPR-0017 needs, and it is deliberately outside the recorded
	/// manifest catalog above: the authenticated read never issues it, so the
	/// authenticated recording stays at ten interactions. The two personal
	/// discovery reads ([[BPR-0988]]) are outside it for the same reason: they
	/// are not part of a pull request snapshot. Route, admission, and name
	/// lookups work from this array so a compiled operation cannot be unknown
	/// to them.
	inline constexpr std::array<read_operation, 15> all_read_operations{
		read_operation::get_viewer_identity,
		read_operation::get_repository,
		read_operation::get_pull_request,
		read_operation::list_pull_request_files,
		read_operation::list_pull_request_commits,
		read_operation::list_reviews,
		read_operation::list_review_threads,
		read_operation::list_issue_comments,
		read_operation::list_checks,
		read_operation::search_pull_requests,
		read_operation::list_review_comments,
		read_operation::list_pull_request_timeline,
		read_operation::search_personal_pull_requests,
		read_operation::list_requested_reviewers,
		read_operation::list_commit_statuses,
	};

	/// \brief Names the baseline snapshot operations a signed-out public read issues.
	///
	/// \details The independent timeline read is admitted separately and can fail
	/// without erasing this snapshot. The other two absent operations are absent for
	/// different reasons that RFC-BPR-0017 requires to stay distinguishable:
	/// viewer identity has no anonymous answer and the GraphQL operation is
	/// served only to an authenticated caller.
	inline constexpr std::array<read_operation, 9> anonymous_read_operations{
		read_operation::get_repository,
		read_operation::get_pull_request,
		read_operation::list_pull_request_files,
		read_operation::list_pull_request_commits,
		read_operation::list_reviews,
		read_operation::list_review_comments,
		read_operation::list_issue_comments,
		read_operation::list_checks,
		read_operation::search_pull_requests,
	};

	enum class http_method : std::uint8_t {
		get,
		post,
		put,
		patch,
		delete_,
	};

	[[nodiscard]] constexpr auto parse_recorded_http_method(
		const std::string_view aMethod) noexcept -> std::optional<http_method>
	{
		if (aMethod == "GET") {
			return http_method::get;
		}
		if (aMethod == "POST") {
			return http_method::post;
		}
		return std::nullopt;
	}

	struct http_header
	{
		std::string Name;
		std::string Value;

		auto operator==(const http_header&) const -> bool = default;
	};

	enum class github_graphql_page_kind : std::uint8_t {
		initial,
		files,
		threads,
		comments,
	};

	struct github_graphql_page_request
	{
		github_graphql_page_kind       Kind = github_graphql_page_kind::initial;
		application::github_read_shape Shape;
		std::string                    AfterCursor;
		std::string                    ThreadId;

		auto operator==(const github_graphql_page_request&) const -> bool = default;
	};

	struct semantic_read_request
	{
		server_contract                            Server    = server_contract::github_com;
		read_operation                             Operation = read_operation::get_viewer_identity;
		std::string                                Owner;
		std::string                                Repository;
		std::uint32_t                              PullRequestNumber = 0;
		std::string                                HeadOid;
		std::string                                SearchTerms;
		std::uint32_t                              Page = 1;
		std::optional<github_graphql_page_request> GraphqlPage;

		auto operator==(const semantic_read_request&) const -> bool = default;
	};

	struct wire_request
	{
		semantic_read_request    Semantic;
		server_contract          Server    = server_contract::github_com;
		read_operation           Operation = read_operation::get_viewer_identity;
		http_method              Method    = http_method::get;
		std::string              Path;
		std::string              Query;
		std::vector<http_header> Headers;
		std::string              Body;

		auto operator==(const wire_request&) const -> bool = default;
	};

	enum class page_completeness : std::uint8_t {
		unknown,
		complete,
		has_next_page,
	};

	struct wire_response
	{
		int                      Status = 0;
		std::vector<http_header> Headers;
		std::string              Body;
		page_completeness        Completeness = page_completeness::unknown;

		auto operator==(const wire_response&) const -> bool = default;
	};

	enum class contract_failure_code : std::uint8_t {
		unsupported_server_contract,
		unsupported_operation,
		mutation_operation,
		invalid_repository,
		invalid_pull_request_number,
		invalid_commit_oid,
		invalid_search_terms,
		invalid_page,
		unsafe_header,
		unexpected_operation,
		unexpected_method,
		unexpected_path,
		unexpected_query,
		unexpected_headers,
		unexpected_body,
		recording_exhausted,
		invalid_response_status,
		invalid_response_headers,
		malformed_response,
		unexpected_response_shape,
		unexpected_response_fields,
		identity_mismatch,
		duplicate_identity,
		response_limit,
	};

	struct contract_failure
	{
		contract_failure_code Code = contract_failure_code::unsupported_operation;
	};

	using request_result   = std::expected<wire_request, contract_failure>;
	using response_result  = std::expected<wire_response, contract_failure>;
	using operation_result = std::expected<read_operation, contract_failure>;

	[[nodiscard]] auto operation_name(read_operation aOperation) noexcept
		-> std::string_view;

	[[nodiscard]] auto read_operation_from_name(std::string_view aName) noexcept
		-> operation_result;

	[[nodiscard]] auto compile_read_request(
		const semantic_read_request& aRequest) -> request_result;

	[[nodiscard]] auto validate_outbound_read_request(
		const wire_request& aRequest) -> std::expected<void, contract_failure>;

	[[nodiscard]] auto validate_strict_fixture_response(
		read_operation       aOperation,
		const wire_response& aResponse) -> std::expected<void, contract_failure>;

	class github_transport
	{
	public:
		github_transport()                                           = default;
		github_transport(const github_transport&)                    = delete;
		auto operator=(const github_transport&) -> github_transport& = delete;
		github_transport(github_transport&&)                         = delete;
		auto operator=(github_transport&&) -> github_transport&      = delete;
		virtual ~github_transport()                                  = default;

		[[nodiscard]] virtual auto perform(const wire_request& aRequest)
			-> response_result = 0;
	};

	struct recorded_exchange
	{
		wire_request  Request;
		wire_response Response;
	};

	struct operation_response
	{
		read_operation Operation = read_operation::get_viewer_identity;
		wire_response  Response;
	};

	struct github_read_normalization_context
	{
		server_contract                      Server = server_contract::github_com;
		core::github_session_request_context SessionRequest;
		core::pull_request_request_context   PullRequestRequest;
		core::host_profile_id                Profile;
		core::git_host_id                    Host;
		core::github_account_id              Account;
		core::repository_id                  Repository;
		core::pull_request_id                PullRequest;
		core::provider_snapshot_id           Snapshot;
		core::capability_revision            CapabilityRevision;
		core::github_capabilities            Capabilities;
		application::github_rate_limit_state RateLimits;
		application::github_read_limits      Limits;
		std::string                          SearchTerms = "is:open";
	};

	struct normalized_github_read
	{
		application::github_session_read_model    Session;
		application::github_pull_request_snapshot PullRequest;

		auto operator<=>(const normalized_github_read&) const = default;
	};

	using normalized_read_result = std::expected<normalized_github_read, contract_failure>;

	/// \brief Describes the operations and shape one normalized read admits.
	///
	/// \details The normalizer used to iterate the whole recorded catalog and
	/// hardcode an all-true read shape, which made it unusable for any
	/// profile that reaches fewer operations with fewer capabilities. The
	/// admitted set and the shape are now one record the caller supplies, and
	/// the response count is checked against that set rather than against the
	/// catalog. `Viewer` answers the one question the operation set cannot:
	/// when the viewer-identity operation is not admitted there is no `/user`
	/// document to build a viewer from, so the caller states the viewer the
	/// profile is bound to instead of the normalizer inventing one.
	struct github_read_admission
	{
		std::span<const read_operation> Operations = read_operation_catalog;
		application::github_read_shape  Shape{
			.ReviewThreads       = true,
			.ViewedFileState     = true,
			.ResolvePermission   = true,
			.UnresolvePermission = true,
		};
		std::optional<application::github_viewer_record> Viewer;
	};

	/// \brief Yields the admission the recorded authenticated contract uses.
	[[nodiscard]] auto authenticated_read_admission() -> github_read_admission;

	/// \brief Yields the admission a signed-out public read uses.
	///
	/// \details The nine credential-free operations and an all-false shape.
	/// Viewer identity and the GraphQL operation are absent because an
	/// anonymous authority refuses them before a request exists, so nothing
	/// here can issue them.
	[[nodiscard]] auto anonymous_read_admission(
		const application::github_viewer_record& aViewer)
		-> github_read_admission;

	class github_transport;

	[[nodiscard]] auto normalize_github_read(
		const github_read_normalization_context& aContext,
		std::span<const operation_response>      aResponses,
		const github_read_admission&             aAdmission
		= authenticated_read_admission()) -> normalized_read_result;

	[[nodiscard]] auto perform_normalized_github_read(
		github_transport&                        aTransport,
		const github_read_normalization_context& aContext,
		const github_read_admission&             aAdmission
		= authenticated_read_admission()) -> normalized_read_result;

	class strict_recorded_transport final : public github_transport
	{
	public:
		explicit strict_recorded_transport(
			std::vector<recorded_exchange> aExchanges);

		[[nodiscard]] auto perform(const wire_request& aRequest)
			-> response_result override;

		[[nodiscard]] auto consumed_all() const noexcept -> bool;

		// Restarts the script at its first exchange. A recording describes one
		// session's traffic; a course that establishes a second session sends
		// the same requests again, and the answers are the same answers. The
		// cursor is the only state a replay has, so this is the whole of it.
		void rewind() noexcept;
		/// \brief [[BPR-0657]]. Moves the cursor forward to the next recorded
		/// exchange of one operation, or to the end when there is none: a
		/// fixture replaying its script for a refresh starts where the
		/// refresh starts.
		void skip_until(read_operation aOperation) noexcept;

	private:
		std::vector<recorded_exchange> Exchanges;
		std::size_t                    NextExchange = 0;
	};
}

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_READ_CONTRACT_HPP
