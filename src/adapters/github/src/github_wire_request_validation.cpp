#include "github_wire_request_validation.hpp"

#include <concepts>
#include <optional>
#include <type_traits>
#include <variant>

namespace bpr::adapters::github::detail {
	namespace {
		[[nodiscard]] auto expected_server_contract(
			const validated_github_host_profile& aProfile) noexcept
			-> std::optional<server_contract>
		{
			switch (aProfile.server_contract()) {
			case application::github_server_contract::github_com_2022_11_28:
				return server_contract::github_com;
			case application::github_server_contract::ghes_3_18_2022_11_28:
				return server_contract::ghes_3_18;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto valid_parent_correlation(
			const github_wire_request&           aRequest,
			const validated_github_host_profile& aProfile) noexcept -> bool
		{
			if (aRequest.Correlation.Parent.valueless_by_exception()) {
				return false;
			}
			return std::visit(
				[&aRequest, &aProfile](const auto& aParent) noexcept {
					using parent_type = std::remove_cvref_t<decltype(aParent)>;
					if constexpr (
						std::same_as<
							parent_type,
							core::github_session_request_context>) {
						return application::detail::valid_context(aParent)
							&& aParent.Profile == aProfile.profile()
							&& aRequest.Correlation.Operation
							== read_operation::get_viewer_identity;
					} else if constexpr (
						std::same_as<
							parent_type,
							core::repository_request_context>) {
						// [[BPR-0988]]. A repository context admits the personal
						// search of exactly its repository, and, [[BPR-1680]],
						// Home's batched read of its pull requests; nothing else.
						const auto& semantic = aRequest.Wire.Semantic;
						return (aRequest.Correlation.Operation == read_operation::search_personal_pull_requests
								|| aRequest.Correlation.Operation == read_operation::list_home_pull_requests
								|| aRequest.Correlation.Operation == read_operation::list_home_review_membership)
							&& application::detail::valid_context(aParent)
							&& aParent.Profile == aProfile.profile()
							&& aParent.Host == aProfile.host()
							&& semantic.Owner == aParent.Repository.Owner
							&& semantic.Repository == aParent.Repository.Name
							&& semantic.PullRequestNumber == 0U;
					} else {
						if (
							aRequest.Correlation.Operation
								== read_operation::get_viewer_identity
							|| aRequest.Correlation.Operation
								== read_operation::search_personal_pull_requests
							|| aRequest.Correlation.Operation
								== read_operation::list_home_pull_requests
							|| aRequest.Correlation.Operation == read_operation::list_home_review_membership
							|| !application::detail::valid_context(aParent)
							|| aParent.Profile != aProfile.profile()
							|| aParent.Host != aProfile.host()) {
							return false;
						}
						const auto& semantic = aRequest.Wire.Semantic;
						if (
							semantic.Owner != aParent.Repository.Owner
							|| semantic.Repository != aParent.Repository.Name) {
							return false;
						}
						return semantic.PullRequestNumber == 0U
							|| semantic.PullRequestNumber
							== aParent.Number.value();
					}
				},
				aRequest.Correlation.Parent);
		}
	}

	auto validate_github_wire_request(
		const validated_github_host_profile& aProfile,
		const github_wire_request&           aRequest)
		-> std::expected<void, github_wire_request_validation_failure>
	{
		if (
			aRequest.Correlation.ProfileRevision != aProfile.revision()
			|| aRequest.Correlation.Host != aProfile.host()
			|| aRequest.Correlation.Operation != aRequest.Wire.Operation
			|| aRequest.Correlation.Operation
				!= aRequest.Wire.Semantic.Operation
			|| aRequest.Correlation.Page != aRequest.Wire.Semantic.Page
			|| !valid_parent_correlation(aRequest, aProfile)) {
			return std::unexpected{
				github_wire_request_validation_failure::
					profile_correlation_mismatch
			};
		}

		const auto server = expected_server_contract(aProfile);
		if (
			!server.has_value() || aRequest.Wire.Server != *server
			|| aRequest.Wire.Semantic.Server != *server) {
			return std::unexpected{
				github_wire_request_validation_failure::
					server_contract_mismatch
			};
		}

		if (!validate_outbound_read_request(aRequest.Wire).has_value()) {
			return std::unexpected{
				github_wire_request_validation_failure::
					invalid_wire_request
			};
		}
		return { };
	}

	auto validate_github_capability_probe_wire_request(
		const validated_github_host_profile&        aProfile,
		const github_capability_probe_wire_request& aRequest)
		-> std::expected<void, github_wire_request_validation_failure>
	{
		const auto& correlation = aRequest.Correlation;
		if (
			!application::detail::valid_context(correlation.Session)
			|| correlation.Session.Profile != aProfile.profile()
			|| correlation.ProfileRevision != aProfile.revision()
			|| correlation.Host != aProfile.host()
			|| correlation.Part != aRequest.Wire.Part
			|| correlation.Part >= capability_probe_part_count
			|| correlation.ObservedAccount.value() == 0U
			|| aRequest.Wire.Profile != correlation.Session.Profile
			|| aRequest.Wire.ProfileRevision != correlation.ProfileRevision
			|| aRequest.Wire.Host != correlation.Host) {
			return std::unexpected{
				github_wire_request_validation_failure::
					profile_correlation_mismatch
			};
		}

		if (aRequest.Wire.Contract != aProfile.server_contract()) {
			return std::unexpected{
				github_wire_request_validation_failure::
					server_contract_mismatch
			};
		}

		const auto canonical = compile_capability_probe_request(aProfile, { }, correlation.Part);
		if (
			!canonical.has_value()
			|| aRequest.Wire != *canonical
			|| !capability_probe_document_is_query_only(
				capability_probe_graphql_document(correlation.Part))) {
			return std::unexpected{
				github_wire_request_validation_failure::
					invalid_wire_request
			};
		}
		return { };
	}
}
