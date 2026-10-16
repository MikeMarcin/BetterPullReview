#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_ANONYMOUS_READ_ADMISSION_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_ANONYMOUS_READ_ADMISSION_HPP

#include "github_read_contract.hpp"

#include "bpr/application/github_capability_cache.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/application/github_provider_port.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/core/anonymous_read_policy.hpp"

#include <cstdint>
#include <expected>

namespace bpr::adapters::github::detail {
	/// \brief Selects the authority a read runs under.
	///
	/// \details One read path, one normalizer, one fixture set. The selection
	/// decides which transport is asked for and which operations are admitted;
	/// it does not introduce a second route.
	enum class github_provider_read_authority : std::uint8_t {
		authenticated,
		anonymous_public,
	};

	/// \brief Names why an anonymous read authority refuses an operation.
	///
	/// \details RFC-BPR-0017 requires the viewer-identity and GraphQL
	/// refusals to be distinguishable from each other and from the GHES
	/// refusal, and requires all three to happen before a request exists.
	/// Collapsing them into one provider failure would make the three
	/// indistinguishable at the only place a test can observe them, so the
	/// reason is carried as its own type and mapped outward once.
	enum class anonymous_read_refusal : std::uint8_t {
		/// The operation names the authenticated viewer. An anonymous
		/// authority has no account, so there is nothing to name.
		identity_requires_credential,
		/// The operation routes through GraphQL, which GitHub.com serves
		/// only to an authenticated caller.
		graphql_surface_unavailable,
		/// The host is not GitHub.com. Anonymous selection is refused for
		/// every GHES profile with no request sent.
		server_contract_unsupported,
	};

	/// \brief Reports whether an operation is reachable without a credential.
	///
	/// \details The eligible set is the REST read surface: repository, pull
	/// request, files, commits, reviews, inline review comments, issue
	/// comments, checks, and search.
	/// The two exclusions are deliberate rather than incidental — GraphQL is
	/// authenticated-only and viewer identity has no anonymous meaning — so
	/// this switch is exhaustive and a new operation cannot default into the
	/// eligible set.
	[[nodiscard]] constexpr auto anonymous_read_eligible(
		const read_operation aOperation) noexcept -> bool
	{
		switch (aOperation) {
		case read_operation::get_repository:
		case read_operation::get_pull_request:
		case read_operation::list_pull_request_files:
		case read_operation::list_pull_request_commits:
		case read_operation::list_pull_request_timeline:
		case read_operation::list_reviews:
		case read_operation::list_issue_comments:
		case read_operation::list_review_comments:
		case read_operation::list_commit_statuses:
		case read_operation::list_checks:
		case read_operation::search_pull_requests:
			return true;
		case read_operation::get_viewer_identity:
		case read_operation::list_review_threads:
		// [[BPR-0988]]. Discovery names a viewer; a signed-out read has none.
		case read_operation::search_personal_pull_requests:
		case read_operation::list_requested_reviewers:
		case read_operation::list_review_requests:
		case read_operation::get_merge_assessment:
		case read_operation::resolve_body_images:
		case read_operation::list_branch_rules:
		case read_operation::get_branch_protection:
			return false;
		}
		return false;
	}

	/// \brief Decides whether an anonymous authority admits one read.
	///
	/// \details Pure, so the whole verdict table is observable without a
	/// transport, a network, or a provider. The contract is checked first:
	/// a GHES profile is refused for every operation, including the ones
	/// GitHub.com would admit, which is what makes "zero requests sent on
	/// GHES" a property of this function rather than of a later guard.
	[[nodiscard]] constexpr auto admit_anonymous_read(
		const read_operation                      aOperation,
		const application::github_server_contract aContract) noexcept
		-> std::expected<void, anonymous_read_refusal>
	{
		if (aContract
			!= application::github_server_contract::github_com_2022_11_28) {
			return std::unexpected{
				anonymous_read_refusal::server_contract_unsupported
			};
		}
		// [[BPR-0988]]. A personal discovery read names the viewer too.
		if (aOperation == read_operation::get_viewer_identity
			|| aOperation == read_operation::search_personal_pull_requests
			|| aOperation == read_operation::list_requested_reviewers) {
			return std::unexpected{
				anonymous_read_refusal::identity_requires_credential
			};
		}
		if (graphql_read(aOperation)) {
			return std::unexpected{
				anonymous_read_refusal::graphql_surface_unavailable
			};
		}
		if (!anonymous_read_eligible(aOperation)) {
			return std::unexpected{
				anonymous_read_refusal::graphql_surface_unavailable
			};
		}
		return { };
	}

	/// \brief Reports whether a read presents the synthesised signed-out reader.
	///
	/// \details A signed-out session never probes: the capability probe is the
	/// GraphQL surface and `admit_anonymous_read` refuses that before a request
	/// exists. So the evidence such a read carries cannot be evidence an
	/// observation produced, and the freshness and projection rules the
	/// authenticated path applies to it have nothing to check. What it carries
	/// instead is synthesised by the host and is admitted only in exactly this
	/// shape: no reader, no account anywhere in the authority, every capability
	/// unsupported by policy, a read shape that claims nothing, and no
	/// observation window at all. The two epochs being zero is the load-bearing
	/// term -- a probe always sets them -- so evidence that was observed can
	/// never enter through here, and evidence synthesised here can never pass
	/// for something observed.
	///
	/// Shared by the provider and the paged read because both guard the same
	/// request against the same rule, and two copies of it could disagree.
	[[nodiscard]] inline auto signed_out_read_identity(
		const application::github_pull_request_provider_request& aRequest) noexcept
		-> bool
	{
		const auto& evidence = aRequest.CapabilityEvidence;
		return aRequest.Context.Mode == core::github_access_mode::public_preview
			&& application::anonymous(aRequest.Viewer)
			&& aRequest.Context.Account.value() == 0U
			&& evidence.Authority.Account.value() == 0U
			&& evidence.Capabilities == core::anonymous_capabilities()
			&& application::reads_nothing(evidence.ReadShape)
			&& evidence.Revision.value() != 0U
			&& evidence.ObservedAt.value() == 0U
			&& evidence.ExpiresBefore.value() == 0U;
	}

	/// \brief Projects a refusal onto the provider's failure vocabulary.
	[[nodiscard]] constexpr auto provider_failure_for(
		const anonymous_read_refusal aRefusal) noexcept
		-> application::github_provider_failure
	{
		switch (aRefusal) {
		case anonymous_read_refusal::identity_requires_credential:
			return application::github_provider_failure::authentication;
		case anonymous_read_refusal::graphql_surface_unavailable:
			return application::github_provider_failure::unsupported_capability;
		case anonymous_read_refusal::server_contract_unsupported:
			return application::github_provider_failure::api_version;
		}
		return application::github_provider_failure::unavailable;
	}
} // namespace bpr::adapters::github::detail

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_ANONYMOUS_READ_ADMISSION_HPP
