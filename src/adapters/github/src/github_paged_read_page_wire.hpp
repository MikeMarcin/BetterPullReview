#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PAGED_READ_PAGE_WIRE_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PAGED_READ_PAGE_WIRE_HPP
#include "bpr/core/home_discovery_plan.hpp"
#include "bpr/core/json_preflight.hpp"
#include "github_http_helpers.hpp"
#include "github_paged_read.hpp"
#include "github_review_comment_grouping.hpp"

#include <QString>

#include <cstddef>
#include <expected>
#include <functional>
#include <glaze/json.hpp>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// Page limits, JSON access, failures, rate limits and link paging the paged GitHub reads share.
namespace bpr::adapters::github::detail {
	constexpr std::size_t maximum_header_value_bytes = 8U * 1024U;

	constexpr std::size_t maximum_production_page_bytes = 4U * 1024U * 1024U;

	constexpr std::size_t maximum_production_aggregate_bytes = 64U * 1024U * 1024U;

	constexpr std::size_t maximum_production_pages = 1'024U;

	constexpr std::size_t maximum_production_page_records = 100U;

	constexpr std::size_t maximum_production_cursor_bytes = 1'024U;

	struct page_json_options : glz::opts
	{
		// Glaze discovers these option overrides by their exact library-defined names.
		// NOLINTNEXTLINE(readability-identifier-naming)
		bool escape_control_characters = true;
		// NOLINTNEXTLINE(readability-identifier-naming)
		bool validate_skipped = true;
		// NOLINTNEXTLINE(readability-identifier-naming)
		bool validate_trailing_whitespace = true;
		constexpr page_json_options()
		{
			null_terminated       = false;
			error_on_missing_keys = true;
		}
	};

	[[nodiscard]] auto provider_error(
		const application::github_provider_failure           aFailure,
		const std::optional<std::uint32_t>                   aRetryAfter     = std::nullopt,
		application::github_rate_limit_state                 aRateLimits     = { },
		std::optional<application::github_rate_limit_record> aRateLimitCause = std::nullopt) -> application::github_provider_error;

	[[nodiscard]] auto
	failed(const application::github_provider_failure           aFailure,
		   const std::optional<std::uint32_t>                   aRetryAfter     = std::nullopt,
		   application::github_rate_limit_state                 aRateLimits     = { },
		   std::optional<application::github_rate_limit_record> aRateLimitCause = std::nullopt) -> std::unexpected<github_paged_read_failure>;

	[[nodiscard]] auto failed(const application::github_provider_error& aError)
		-> std::unexpected<github_paged_read_failure>;

	[[nodiscard]] auto
	rate_limit_state(const application::github_rate_limit_record& aRecord) noexcept
		-> application::github_rate_limit_state;

	[[nodiscard]] auto cancelled() -> std::unexpected<github_paged_read_failure>;

	[[nodiscard]] auto ascii_fold(const char aValue) noexcept -> char;

	[[nodiscard]] auto ascii_equal_fold(const std::string_view aLeft,
										const std::string_view aRight) noexcept
		-> bool;

	[[nodiscard]] auto trim_ows(std::string_view aValue) noexcept -> std::string_view;

	[[nodiscard]] auto decimal(std::string_view aValue) noexcept
		-> std::optional<std::uint64_t>;

	using json_preflight_result = core::json_preflight_result;

	/// [[BPR-1433]]. The shared core check with the paged read's ceilings.
	class json_structural_preflight
	{
	public:
		json_structural_preflight(const std::string_view aInput, const std::size_t aMaximumStringBytes) noexcept;

		[[nodiscard]] auto run() const noexcept -> json_preflight_result;

	private:
		std::string_view Input;
		std::size_t      MaximumStringBytes;
	};

	// A fetched page outlives its transport completion. Every indexed child
	// shares the immutable body, including copied GraphQL connection nodes.
	// Only Glaze parses JSON; the checks below retain missing versus null.
	struct page_json_value
	{
		std::shared_ptr<const std::string> Owner;
		std::string_view                   Json;
	};

	using page_json_object = std::map<std::string, page_json_value, std::less<>>;

	using page_json_array = std::vector<page_json_value>;

	[[nodiscard]] auto json_tag(const page_json_value& aValue) noexcept -> char;

	[[nodiscard]] auto json_is_object(const page_json_value& aValue) noexcept -> bool;

	[[nodiscard]] auto json_is_array(const page_json_value& aValue) noexcept -> bool;

	[[nodiscard]] auto json_is_string(const page_json_value& aValue) noexcept -> bool;

	[[nodiscard]] auto json_is_null(const page_json_value& aValue) noexcept -> bool;

	[[nodiscard]] auto json_is_undefined(const page_json_value& aValue) noexcept -> bool;

	[[nodiscard]] auto json_is_bool(const page_json_value& aValue) noexcept -> bool;

	[[nodiscard]] auto parse_json_document(const std::string_view aBody) -> std::optional<page_json_value>;

	[[nodiscard]] auto json_object(const page_json_value& aValue) -> page_json_object;

	[[nodiscard]] auto json_array(const page_json_value& aValue) -> page_json_array;

	[[nodiscard]] auto json_member(const page_json_object& aObject, const std::string_view aKey) -> page_json_value;

	[[nodiscard]] auto text(const page_json_value& aValue) -> std::string;

	[[nodiscard]] auto json_text(const page_json_value& aValue) -> QString;

	[[nodiscard]] auto json_bool(const page_json_value& aValue) -> bool;

	[[nodiscard]] auto exact_integer(const page_json_value& aValue, const bool aPositive = false) noexcept -> bool;

	[[nodiscard]] auto integer(const page_json_value& aValue) noexcept -> std::uint64_t;

	[[nodiscard]] auto uint32(const page_json_value& aValue) noexcept
		-> std::optional<std::uint32_t>;

	[[nodiscard]] auto nonempty_string(const page_json_value& aValue) -> bool;

	[[nodiscard]] auto bounded_text(const std::string_view aValue,
									const std::size_t      aMaximum,
									const bool             aRequired = false) noexcept -> bool;

	[[nodiscard]] auto sorted_keys(const page_json_object& aObject)
		-> std::vector<std::string>;

	[[nodiscard]] auto
	has_exact_keys(const page_json_object&                 aObject,
				   const std::span<const std::string_view> aExpected) -> bool;

	[[nodiscard]] auto
	has_exact_keys(const page_json_object&                       aObject,
				   const std::initializer_list<std::string_view> aExpected) -> bool;

	[[nodiscard]] auto
	has_required_keys(const page_json_object&                 aObject,
					  const std::span<const std::string_view> aRequired) -> bool;

	[[nodiscard]] auto
	has_required_keys(const page_json_object&                       aObject,
					  const std::initializer_list<std::string_view> aRequired)
		-> bool;

	[[nodiscard]] auto graphql_success_envelope(const page_json_object& aObject) -> bool;

	[[nodiscard]] auto one_of(const page_json_value&                        aValue,
							  const std::initializer_list<std::string_view> aAllowed)
		-> bool;

	[[nodiscard]] auto hexadecimal_oid(const page_json_value& aValue) -> bool;

	[[nodiscard]] auto valid_line(const page_json_value& aValue) noexcept -> bool;

	[[nodiscard]] auto optional_line(const page_json_value& aValue)
		-> std::optional<std::uint32_t>;

	[[nodiscard]] auto response_byte_count(const wire_response& aResponse) noexcept
		-> std::optional<std::size_t>;

	[[nodiscard]] auto limits_valid(const github_paged_read_limits& aLimits) noexcept
		-> bool;

	[[nodiscard]] auto ascii_contains_fold(const std::string_view aText,
										   const std::string_view aPhrase) noexcept
		-> bool;

	[[nodiscard]] auto
	secondary_rate_limit_message(const page_json_value* const aDocument) -> bool;

	[[nodiscard]] auto graphql_error_rate_limited(const page_json_value& aErrors) -> bool;

	[[nodiscard]] constexpr auto
	expected_rate_limit_resource(const read_operation aOperation) noexcept
		-> application::github_rate_limit_resource
	{
		if (graphql_read(aOperation)) {
			return application::github_rate_limit_resource::graphql;
		}
		if (aOperation == read_operation::search_pull_requests
			|| aOperation == read_operation::search_personal_pull_requests) {
			return application::github_rate_limit_resource::search;
		}
		return application::github_rate_limit_resource::core;
	}

	[[nodiscard]] constexpr auto rate_limits_are_empty_or_exact(
		const application::github_rate_limit_state&   aState,
		const application::github_rate_limit_resource aExpected) noexcept -> bool
	{
		if (!application::valid(aState) || !application::has_observations(aState)) {
			return application::valid(aState);
		}
		const auto count = static_cast<std::size_t>(aState.Core.has_value())
			+ static_cast<std::size_t>(aState.Search.has_value())
			+ static_cast<std::size_t>(aState.Graphql.has_value());
		return count == 1U && application::find(aState, aExpected) != nullptr;
	}

	[[nodiscard]] auto status_failure(const int                       aStatus,
									  const github_response_metadata& aMetadata,
									  const bool                      aSecondaryRateLimit = false)
		-> application::github_provider_error;

	struct parsed_page
	{
		wire_request             Request;
		github_response_metadata Metadata;
		page_completeness        Completeness = page_completeness::unknown;
		page_json_value          Document;
	};

	struct shared_home_budget
	{
		std::size_t Pages = 0, Bytes = 0;
	};

	class production_read_execution
	{
	public:
		/// [[BPR-0988]]. A discovery read's parent is a repository context
		/// or, for one pull request's requested reviewers, a pull request
		/// context; every other read's is a pull request context.
		production_read_execution(github_wire_transport_port&                 aTransport,
								  const validated_github_host_profile&        aProfile,
								  github_wire_parent_context                  aParent,
								  const github_paged_read_limits&             aLimits,
								  const std::stop_token                       aStop,
								  application::github_rate_limit_state&       aRateLimits,
								  std::optional<core::application_session_id> aAuthenticationSession,
								  github_paged_response_observer* aObserver, shared_home_budget* aHomeBudget = nullptr);

		[[nodiscard]] auto
		enrich(application::github_provider_error aError) const noexcept
			-> application::github_provider_error;

		[[nodiscard]] auto enrich(github_paged_read_failure aFailure) const noexcept
			-> github_paged_read_failure;

		[[nodiscard]] auto
		terminal_failure(const application::github_provider_failure aFailure) const
			-> std::unexpected<github_paged_read_failure>;

		[[nodiscard]] auto fetch(const semantic_read_request& aSemantic)
			-> std::expected<parsed_page, github_paged_read_failure>;

		[[nodiscard]] auto rate_limits() const noexcept
			-> const application::github_rate_limit_state&;

		[[nodiscard]] auto
		bind_repository(const core::repository_id aRepository) noexcept -> bool;

		[[nodiscard]] auto bound_repository() const noexcept
			-> std::optional<core::repository_id>;

	private:
		github_wire_transport_port&                 Transport;
		const validated_github_host_profile&        Profile;
		github_wire_parent_context                  Parent;
		const github_paged_read_limits&             Limits;
		std::stop_token                             Stop;
		std::size_t                                 Pages          = 0U;
		std::size_t                                 AggregateBytes = 0U;
		application::github_rate_limit_state&       RateLimits;
		std::optional<core::repository_id>          BoundRepository;
		std::optional<core::application_session_id> AuthenticationSession;
		github_paged_response_observer*             Observer;
		shared_home_budget*                         HomeBudget;
	};

	[[nodiscard]] auto
	server_contract_for(const validated_github_host_profile& aProfile)
		-> std::optional<server_contract>;

	[[nodiscard]] auto
	semantic_request_for(const server_contract                     aServer,
						 const core::pull_request_request_context& aContext,
						 const read_operation                      aOperation,
						 const std::uint32_t                       aPage = 1U) -> semantic_read_request;

	[[nodiscard]] auto singleton_complete(const parsed_page& aPage) noexcept -> bool;

	struct link_relation
	{
		std::string Url;
		std::string Relation;
	};

	/// The strict pagination form GitHub writes: at most four links, each
	/// with exactly one parameter, a quoted `rel` naming one pagination
	/// relation, and no relation twice.
	[[nodiscard]] auto parse_link_relations(const std::string_view aValue)
		-> std::optional<std::vector<link_relation>>;

	struct url_query_parameter
	{
		std::string_view Name;
		std::string_view Value;
	};

	[[nodiscard]] auto parse_url_query(const std::string_view aQuery)
		-> std::optional<std::vector<url_query_parameter>>;

	[[nodiscard]] auto
	numeric_repository_alias(const wire_request&       aRequest,
							 const core::repository_id aRepository)
		-> std::optional<std::string>;

	[[nodiscard]] auto query_value_matches(const read_operation   aOperation,
										   const std::string_view aName,
										   const std::string_view aActual,
										   const std::string_view aExpected) noexcept
		-> bool;

	[[nodiscard]] auto
	page_number_from_url(const validated_github_host_profile& aProfile,
						 const parsed_page&                   aPage,
						 const core::repository_id            aRepository,
						 const std::string_view               aUrl) -> std::optional<std::uint32_t>;

	[[nodiscard]] auto
	rest_has_next_page(const validated_github_host_profile& aProfile,
					   const core::repository_id            aRepository,
					   const parsed_page&                   aPage,
					   const std::size_t                    aRecordCount)
		-> std::expected<bool, github_paged_read_failure>;
}
#endif
