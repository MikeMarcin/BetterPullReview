#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PAGED_READ_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PAGED_READ_HPP

#include "github_anonymous_read_admission.hpp"
#include "github_host_profile.hpp"
#include "github_wire_transport.hpp"

#include "bpr/application/markdown_image_ports.hpp"
#include "bpr/application/personal_discovery_port.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail {
	struct github_paged_read_limits
	{
		application::github_read_limits Records;
		std::size_t                     MaximumPageBytes              = 4U * 1024U * 1024U;
		std::size_t                     MaximumAggregateResponseBytes = 64U * 1024U * 1024U;
		std::size_t                     MaximumTotalPages             = 1'024U;
		std::size_t                     MaximumPageRecords            = 100U;
		std::size_t                     MaximumCursorBytes            = 1'024U;

		auto operator<=>(const github_paged_read_limits&) const = default;
	};

	struct github_response_metadata
	{
		std::optional<std::string>                           Link;
		std::optional<application::github_rate_limit_record> RateLimit;
		std::optional<std::uint32_t>                         RetryAfterSeconds;

		auto operator<=>(const github_response_metadata&) const = default;
	};

	using github_response_metadata_result = std::expected<github_response_metadata, application::github_provider_error>;

	[[nodiscard]] auto
	parse_github_response_metadata(const wire_response& aResponse) noexcept
		-> github_response_metadata_result;

	[[nodiscard]] auto github_error_body_is_secondary_rate_limit(
		std::string_view aBody, std::size_t aMaximumBytes) noexcept -> bool;

	[[nodiscard]] auto github_graphql_error_body_is_rate_limited(
		std::string_view aBody, std::size_t aMaximumBytes) noexcept -> bool;

	struct observed_github_viewer
	{
		application::github_viewer_record    Viewer;
		application::github_rate_limit_state RateLimits;

		auto operator<=>(const observed_github_viewer&) const = default;
	};

	using observed_github_viewer_result = std::expected<observed_github_viewer, application::github_provider_error>;

	[[nodiscard]] auto normalize_observed_github_viewer(
		const wire_response&            aResponse,
		server_contract                 aServer = server_contract::github_com,
		const github_paged_read_limits& aLimits = { }) noexcept
		-> observed_github_viewer_result;

	struct github_paged_read_cancelled
	{
		auto operator<=>(const github_paged_read_cancelled&) const = default;
	};

	using github_paged_read_failure = std::variant<application::github_provider_error,
												   github_paged_read_cancelled>;

	using github_paged_read_result = std::expected<application::github_pull_request_snapshot,
												   github_paged_read_failure>;

	/// \brief An exhausted timeline; failures carry no partial records.
	struct github_timeline_read
	{
		std::vector<application::github_force_push_event> ForcePushEvents;
		application::github_rate_limit_state              RateLimits;
	};

	using github_timeline_read_result = std::expected<github_timeline_read, github_paged_read_failure>;

	/// \brief Receives only correlated HTTP response facts before payload validation.
	class github_paged_response_observer
	{
	public:
		virtual ~github_paged_response_observer()                                                = default;
		virtual void observe(read_operation aOperation, int aHttpStatus) noexcept                = 0;
		virtual void request_started(read_operation aOperation) noexcept                         = 0;
		virtual void validation_failed(application::github_read_validation aValidation) noexcept = 0;
		virtual void overview_observed(application::github_pull_request_overview_observation aObservation) noexcept
		{
			static_cast<void>(aObservation);
		}
	};

	/// \brief Reads at most ten timeline pages, 100 records each, including ignored events.
	/// \details The repository id must come from the matching accepted live snapshot.
	/// This independent read preserves the live snapshot if history is unavailable.
	[[nodiscard]] auto perform_paged_github_timeline_read(
		github_wire_transport_port&                              aTransport,
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		core::repository_id                                      aRepository,
		std::stop_token                                          aStop,
		const github_paged_read_limits&                          aLimits        = { },
		github_provider_read_authority                           aReadAuthority = github_provider_read_authority::authenticated,
		github_paged_response_observer*                          aObserver      = nullptr) noexcept
		-> github_timeline_read_result;

	/// \brief [[BPR-1560]]. The merge base GitHub's compare named for one
	/// exact head, and the base commit it was computed against.
	struct github_merge_base_read
	{
		std::string                          MergeBaseOid;
		std::string                          BaseCommitOid;
		application::github_rate_limit_state RateLimits;
	};

	/// \brief [[BPR-1560]]. What the merge-base read compares the head with:
	/// the base branch by name (an open pull request: GitHub resolves its
	/// current tip) or one exact commit.
	struct github_merge_base_basis
	{
		std::string BaseRef;
		std::string BaseOid;
	};

	using github_merge_base_read_result = std::expected<github_merge_base_read, github_paged_read_failure>;

	/// \brief [[BPR-1560]]. Asks GitHub's compare which commit the pull
	/// request's Files changed compares from: `merge_base_commit` of the
	/// basis and exactly `aHead`, the snapshot's head.
	/// \details One bounded page that carries no file list. The answer must
	/// name a full base commit (the basis commit itself when the basis is a
	/// commit) and a merge base of the head's object format, or the read
	/// fails; a failure never stands in for any base. The repository id must
	/// come from the matching accepted live snapshot.
	[[nodiscard]] auto perform_github_merge_base_read(
		github_wire_transport_port&                              aTransport,
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		core::repository_id                                      aRepository,
		const github_merge_base_basis&                           aBasis,
		std::string_view                                         aHead,
		std::stop_token                                          aStop,
		const github_paged_read_limits&                          aLimits        = { },
		github_provider_read_authority                           aReadAuthority = github_provider_read_authority::authenticated,
		github_paged_response_observer*                          aObserver      = nullptr) noexcept
		-> github_merge_base_read_result;

	/// \brief [[BPR-1560]]. Whether the pull request GitHub describes now is
	/// the one a snapshot was read from.
	struct github_pull_request_confirmation
	{
		bool                                 Unchanged = false;
		application::github_rate_limit_state RateLimits;
	};

	using github_pull_request_confirmation_result = std::expected<github_pull_request_confirmation, github_paged_read_failure>;

	/// \brief [[BPR-1560]]. Reads the pull request's metadata again after the
	/// merge-base read and reports whether its head, recorded base, base
	/// branch, state and changed-file count are still the snapshot's, which
	/// binds the snapshot's file list and the merge base to one state of the
	/// pull request.
	[[nodiscard]] auto perform_github_pull_request_confirmation(
		github_wire_transport_port&                              aTransport,
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		const application::github_pull_request_snapshot&         aSnapshot,
		std::stop_token                                          aStop,
		const github_paged_read_limits&                          aLimits        = { },
		github_provider_read_authority                           aReadAuthority = github_provider_read_authority::authenticated,
		github_paged_response_observer*                          aObserver      = nullptr) noexcept
		-> github_pull_request_confirmation_result;

	/// \brief [[BPR-0988]]. What one personal discovery read found.
	struct github_personal_discovery_read
	{
		std::vector<application::personal_discovery_row> Rows;
		application::personal_discovery_completeness     Completeness = application::personal_discovery_completeness::complete;
		application::github_rate_limit_state             RateLimits;
	};

	using github_personal_discovery_read_result = std::expected<github_personal_discovery_read, github_paged_read_failure>;

	/// \brief [[BPR-0988]]. Reads the viewer's open pull requests of one
	/// repository: one bounded search page, then each row's requested
	/// reviewers up to `maximum_personal_discovery_details`.
	///
	/// \details Independent of the pull request read: it issues no snapshot,
	/// file, commit, thread, comment, check, timeline or diff request. An
	/// authored row is kept only when its author is the viewer; a requested
	/// row only when its requested reviewers name the viewer as a user. A
	/// search GitHub calls incomplete, a match longer than one page, or more
	/// requested rows than the detail reads could confirm make the answer
	/// partial. Any failed request fails the whole read, which then says
	/// nothing about what is absent.
	[[nodiscard]] auto perform_personal_discovery_read(
		github_wire_transport_port&                    aTransport,
		const validated_github_host_profile&           aProfile,
		const application::personal_discovery_request& aRequest,
		std::stop_token                                aStop,
		const github_paged_read_limits&                aLimits = { }) noexcept
		-> github_personal_discovery_read_result;

	struct github_markdown_image_resolution
	{
		std::vector<application::markdown_image_resolved_body> Bodies;
		application::github_rate_limit_state                   RateLimits;
	};

	using github_markdown_image_resolution_result = std::expected<github_markdown_image_resolution, github_paged_read_failure>;

	/// \brief RFC-BPR-0084. One bounded GraphQL read of the bodies' rendered HTML.
	/// \details The answer must name the accepted repository and pull request
	/// and answer every requested node in order with its exact identity; a
	/// deleted body answers null and serves nothing. Each `bodyHTML` is read
	/// through `core::served_markdown_images` and never leaves this adapter.
	[[nodiscard]] auto perform_markdown_image_resolution(
		github_wire_transport_port&                           aTransport,
		const validated_github_host_profile&                  aProfile,
		const application::markdown_image_resolution_request& aRequest,
		std::stop_token                                       aStop,
		const github_paged_read_limits&                       aLimits = { }) noexcept
		-> github_markdown_image_resolution_result;

	[[nodiscard]] auto perform_paged_github_read(
		github_wire_transport_port&                              aTransport,
		const validated_github_host_profile&                     aProfile,
		const application::github_pull_request_provider_request& aRequest,
		core::provider_snapshot_id                               aSnapshot,
		std::stop_token                                          aStop,
		const github_paged_read_limits&                          aLimits = { },
		github_provider_read_authority                           aReadAuthority
		= github_provider_read_authority::authenticated,
		github_paged_response_observer* aObserver = nullptr) noexcept
		-> github_paged_read_result;
}

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PAGED_READ_HPP
