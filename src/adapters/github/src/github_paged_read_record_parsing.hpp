#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PAGED_READ_RECORD_PARSING_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PAGED_READ_RECORD_PARSING_HPP
#include "bpr/core/home_discovery_plan.hpp"
#include "github_http_helpers.hpp"
#include "github_paged_read.hpp"
#include "github_paged_read_page_wire.hpp"
#include "github_review_comment_grouping.hpp"

#include <QString>

#include <cstddef>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Actor, state, connection and record parsers, and the collection reads, the paged GitHub reads share.
namespace bpr::adapters::github::detail {
	/// \brief [[BPR-1321]] Maps a provider-stated account type; nothing is inferred from logins or IDs.
	[[nodiscard]] auto actor_kind(const std::string_view aType) noexcept
		-> application::github_actor_kind;

	[[nodiscard]] auto parse_rest_actor(const page_json_value& aValue)
		-> std::optional<application::github_actor_record>;

	struct optional_actor_parse
	{
		bool                                            Valid = false;
		std::optional<application::github_actor_record> Actor;
	};

	[[nodiscard]] auto optional_rest_actor(const page_json_value& aValue)
		-> optional_actor_parse;

	[[nodiscard]] auto optional_graphql_actor(const page_json_value& aValue)
		-> optional_actor_parse;

	[[nodiscard]] auto pull_request_state(const QString& aState,
										  const bool     aMerged) noexcept
		-> application::github_pull_request_state;

	[[nodiscard]] auto review_state(const QString& aValue) noexcept
		-> application::github_review_state;

	[[nodiscard]] auto check_status(const QString& aValue) noexcept
		-> application::github_check_status;

	[[nodiscard]] auto check_conclusion(const page_json_value& aValue) noexcept
		-> application::github_check_conclusion;

	[[nodiscard]] auto bounded_graphql_token(const std::string_view aValue,
											 const std::size_t      aMaximum) noexcept
		-> bool;

	struct graphql_connection_page
	{
		std::uint64_t              TotalCount = 0U;
		page_json_array            Nodes;
		std::optional<std::string> NextCursor;
	};

	[[nodiscard]] auto
	parse_graphql_connection(const page_json_value&          aValue,
							 const github_paged_read_limits& aLimits)
		-> std::expected<graphql_connection_page, github_paged_read_failure>;

	// These are optional only for old recordings. A remote body without the new
	// observations remains readable but cannot be claimed as a raw tour baseline.
	[[nodiscard]] auto parse_comment_observation(const page_json_value& aValue, bool aGraphql)
		-> std::expected<application::github_comment_observation, github_paged_read_failure>;

	[[nodiscard]] auto graphql_repository_and_pull_request(
		const parsed_page&                             aPage,
		const application::github_repository_record&   aRepository,
		const application::github_pull_request_record& aPullRequest,
		page_json_object*                              aDataOutput,
		page_json_object*                              aRepositoryOutput,
		page_json_object*                              aPullRequestOutput)
		-> std::expected<void, github_paged_read_failure>;

	[[nodiscard]] auto provider_request_valid(
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		const core::provider_snapshot_id                         aSnapshot,
		const github_paged_read_limits&                          aLimits,
		const github_provider_read_authority                     aReadAuthority) noexcept -> bool;

	[[nodiscard]] auto parse_timeline_timestamp(const std::string_view aValue) -> std::optional<std::int64_t>;

	struct parsed_pull_ref
	{
		std::string                        Ref;
		std::string                        Oid;
		std::optional<core::repository_id> Repository;
		std::optional<std::string>         FullName;
	};

	[[nodiscard]] auto parse_pull_ref(const page_json_value& aValue,
									  const bool             aRepositoryRequired)
		-> std::optional<parsed_pull_ref>;

	struct parsed_pull_request
	{
		application::github_pull_request_record Record;
		std::size_t                             ChangedFiles = 0U;
		std::size_t                             Commits      = 0U;
	};

	[[nodiscard]] auto
	parse_pull_request(const parsed_page&                           aPage,
					   const core::pull_request_request_context&    aContext,
					   const application::github_repository_record& aRepository,
					   const application::github_read_limits&       aLimits)
		-> std::expected<parsed_pull_request, github_paged_read_failure>;

	[[nodiscard]] auto parse_review(const page_json_value& aValue)
		-> std::expected<application::github_review_record,
						 github_paged_read_failure>;

	[[nodiscard]] auto parse_check(const page_json_value& aValue,
								   const std::string_view aHeadOid)
		-> std::expected<application::github_check_record,
						 github_paged_read_failure>;

	[[nodiscard]] auto parse_commit_status(const page_json_value& aValue, const std::string_view aHeadOid)
		-> std::expected<application::github_check_record, github_paged_read_failure>;

	template <typename Record, typename Parser>
	[[nodiscard]] auto
	read_rest_array_collection(production_read_execution&           aExecution,
							   const validated_github_host_profile& aProfile,
							   semantic_read_request                aSemantic,
							   std::vector<Record>&                 aOutput,
							   const std::size_t                    aMaximumRecords,
							   const std::size_t                    aMaximumPageRecords,
							   Parser&&                             aParser)
		-> std::expected<void, github_paged_read_failure>
	{
		while (true) {
			auto page = aExecution.fetch(aSemantic);
			if (!page.has_value()) {
				return std::unexpected{ page.error() };
			}
			if (!json_is_array(page->Document)) {
				return failed(
					application::github_provider_failure::malformed_response);
			}
			const auto records = json_array(page->Document);
			const auto count   = static_cast<std::size_t>(records.size());
			if (count > aMaximumPageRecords || count > aMaximumRecords
				|| aOutput.size() > aMaximumRecords - count) {
				return failed(application::github_provider_failure::response_limit);
			}
			aOutput.reserve(aOutput.size() + count);
			for (const auto& value : records) {
				auto record = aParser(value);
				if (!record.has_value()) {
					return std::unexpected{ record.error() };
				}
				aOutput.push_back(std::move(*record));
			}
			const auto repository = aExecution.bound_repository();
			if (!repository.has_value()) {
				return failed(
					application::github_provider_failure::malformed_response);
			}
			const auto hasNext = rest_has_next_page(aProfile, *repository, *page, count);
			if (!hasNext.has_value()) {
				return std::unexpected{ hasNext.error() };
			}
			if (!*hasNext) {
				return { };
			}
			if (aOutput.size() == aMaximumRecords) {
				return failed(application::github_provider_failure::response_limit);
			}
			if (aSemantic.Page == std::numeric_limits<std::uint32_t>::max()) {
				return failed(application::github_provider_failure::response_limit);
			}
			++aSemantic.Page;
		}
	}

	using observation = application::github_observation_availability;

	[[nodiscard]] auto fatal_optional_read_failure(const github_paged_read_failure& aFailure) -> bool;

	[[nodiscard]] auto observation_failure(const github_paged_read_failure& aFailure) -> observation;

	[[nodiscard]] auto merge_pull_object(const parsed_page& aPage, const application::github_pull_request_snapshot& aSnapshot)
		-> std::expected<page_json_object, github_paged_read_failure>;

	/// The merge state, review decision and auto-merge of a pull request
	/// object that asked for them; [[BPR-1680]] reads them from Home's
	/// batched query as well as from their own.
	[[nodiscard]] auto apply_merge_fields(const page_json_object& aPull, application::github_pull_request_snapshot& aSnapshot)
		-> std::expected<void, github_paged_read_failure>;

	[[nodiscard]] auto read_merge_assessment(production_read_execution& aExecution, server_contract aServer, const core::pull_request_request_context& aContext, application::github_pull_request_snapshot& aSnapshot)
		-> std::expected<void, github_paged_read_failure>;

	[[nodiscard]] auto parse_pending_request(const page_json_value& aValue) -> std::expected<application::github_review_request_record, github_paged_read_failure>;

	[[nodiscard]] auto read_pending_requests(production_read_execution& aExecution, server_contract aServer, const core::pull_request_request_context& aContext, application::github_pull_request_snapshot& aSnapshot, const github_paged_read_limits& aLimits)
		-> std::expected<void, github_paged_read_failure>;

	[[nodiscard]] auto
	read_checks(production_read_execution&                     aExecution,
				const validated_github_host_profile&           aProfile,
				semantic_read_request                          aSemantic,
				const std::string_view                         aHeadOid,
				std::vector<application::github_check_record>& aOutput,
				const github_paged_read_limits&                aLimits)
		-> std::expected<void, github_paged_read_failure>;
}
#endif
