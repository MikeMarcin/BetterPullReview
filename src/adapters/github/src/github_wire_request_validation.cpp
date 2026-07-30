#include "github_wire_request_validation.hpp"

#include <concepts>
#include <optional>
#include <type_traits>
#include <variant>

namespace bpr::adapters::github::detail
{
namespace
{
[[nodiscard]] auto expected_server_contract(
    const validated_github_host_profile& profile) noexcept
    -> std::optional<server_contract>
{
    switch(profile.server_contract())
    {
    case application::github_server_contract::github_com_2022_11_28:
        return server_contract::github_com;
    case application::github_server_contract::ghes_3_18_2022_11_28:
        return server_contract::ghes_3_18;
    }
    return std::nullopt;
}

[[nodiscard]] auto valid_parent_correlation(
    const github_wire_request& request,
    const validated_github_host_profile& profile) noexcept -> bool
{
    if(request.Correlation.Parent.valueless_by_exception())
    {
        return false;
    }
    return std::visit(
        [&request, &profile](const auto& parent) noexcept {
            using parent_type = std::remove_cvref_t<decltype(parent)>;
            if constexpr(
                std::same_as<
                    parent_type,
                    core::github_session_request_context>)
            {
                return application::detail::valid_context(parent)
                    && parent.Profile == profile.profile()
                    && request.Correlation.Operation
                        == read_operation::get_viewer_identity;
            }
            else
            {
                if(
                    request.Correlation.Operation
                        == read_operation::get_viewer_identity
                    || !application::detail::valid_context(parent)
                    || parent.Profile != profile.profile()
                    || parent.Host != profile.host())
                {
                    return false;
                }
                const auto& semantic = request.Wire.Semantic;
                if(
                    semantic.Owner != parent.Repository.Owner
                    || semantic.Repository != parent.Repository.Name)
                {
                    return false;
                }
                return semantic.PullRequestNumber == 0U
                    || semantic.PullRequestNumber
                        == parent.Number.value();
            }
        },
        request.Correlation.Parent);
}
}

auto validate_github_wire_request(
    const validated_github_host_profile& profile,
    const github_wire_request& request)
    -> std::expected<void, github_wire_request_validation_failure>
{
    if(
        request.Correlation.ProfileRevision != profile.revision()
        || request.Correlation.Host != profile.host()
        || request.Correlation.Operation != request.Wire.Operation
        || request.Correlation.Operation
            != request.Wire.Semantic.Operation
        || request.Correlation.Page != request.Wire.Semantic.Page
        || !valid_parent_correlation(request, profile))
    {
        return std::unexpected{
            github_wire_request_validation_failure::
                profile_correlation_mismatch};
    }

    const auto server = expected_server_contract(profile);
    if(
        !server.has_value() || request.Wire.Server != *server
        || request.Wire.Semantic.Server != *server)
    {
        return std::unexpected{
            github_wire_request_validation_failure::
                server_contract_mismatch};
    }

    if(!validate_outbound_read_request(request.Wire).has_value())
    {
        return std::unexpected{
            github_wire_request_validation_failure::
                invalid_wire_request};
    }
    return {};
}

auto validate_github_capability_probe_wire_request(
    const validated_github_host_profile& profile,
    const github_capability_probe_wire_request& request)
    -> std::expected<void, github_wire_request_validation_failure>
{
    const auto& correlation = request.Correlation;
    if(
        !application::detail::valid_context(correlation.Session)
        || correlation.Session.Profile != profile.profile()
        || correlation.ProfileRevision != profile.revision()
        || correlation.Host != profile.host()
        || correlation.ObservedAccount.value() == 0U
        || request.Wire.Profile != correlation.Session.Profile
        || request.Wire.ProfileRevision != correlation.ProfileRevision
        || request.Wire.Host != correlation.Host)
    {
        return std::unexpected{
            github_wire_request_validation_failure::
                profile_correlation_mismatch};
    }

    if(request.Wire.Contract != profile.server_contract())
    {
        return std::unexpected{
            github_wire_request_validation_failure::
                server_contract_mismatch};
    }

    const auto canonical = compile_capability_probe_request(profile);
    if(
        !canonical.has_value()
        || request.Wire != *canonical
        || !capability_probe_document_is_query_only(
            capability_probe_graphql_document()))
    {
        return std::unexpected{
            github_wire_request_validation_failure::
                invalid_wire_request};
    }
    return {};
}
}
