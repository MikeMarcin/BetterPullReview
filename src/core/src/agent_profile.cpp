#include "bpr/core/agent_profile.hpp"

#include <algorithm>
#include <ranges>

namespace bpr::core {
	auto stable_name(const agent_protocol aProtocol) noexcept -> std::string_view
	{
		switch (aProtocol) {
		case agent_protocol::codex_app_server:
			return "codex_app_server";
		case agent_protocol::codex_jsonl_exec:
			return "codex_jsonl_exec";
		case agent_protocol::claude_stream_json:
			return "claude_stream_json";
		case agent_protocol::external_terminal:
			return "external_terminal";
		}
		return { };
	}

	auto stable_name(const agent_workspace_policy aPolicy) noexcept
		-> std::string_view
	{
		switch (aPolicy) {
		case agent_workspace_policy::disposable_review_workspace:
			return "disposable_review_workspace";
		case agent_workspace_policy::existing_checkout:
			return "existing_checkout";
		}
		return { };
	}

	auto stable_name(const agent_invocation_refusal aRefusal) noexcept
		-> std::string_view
	{
		switch (aRefusal) {
		case agent_invocation_refusal::none:
			return "none";
		case agent_invocation_refusal::host_not_mapped:
			return "host_not_mapped";
		case agent_invocation_refusal::profile_forbidden_on_host:
			return "profile_forbidden_on_host";
		case agent_invocation_refusal::profile_not_required_one:
			return "profile_not_required_one";
		case agent_invocation_refusal::profile_invalid:
			return "profile_invalid";
		case agent_invocation_refusal::profile_unknown:
			return "profile_unknown";
		}
		return { };
	}

	auto valid(const agent_profile& aProfile) noexcept -> bool
	{
		if (
			aProfile.Id.value() == 0U || aProfile.DisplayName.empty()
			|| aProfile.DisplayName.size() > maximum_agent_text_bytes
			|| aProfile.Executable.empty()
			|| aProfile.Executable.size() > maximum_agent_text_bytes
			|| aProfile.AllowedEnvironment.size() > maximum_environment_allowlist) {
			return false;
		}
		// Names only, and bounded ones. An empty name would allow nothing and
		// read as allowing everything, which is the wrong way for a list like
		// this to be ambiguous.
		return std::ranges::all_of(
			aProfile.AllowedEnvironment, [](const std::string& aName) {
				return !aName.empty()
					&& aName.size() <= maximum_environment_name_bytes;
			});
	}

	auto decide_agent_invocation(
		const agent_profile&                    aProfile,
		const std::optional<agent_host_policy>& aPolicy) noexcept
		-> agent_invocation_verdict
	{
		if (!valid(aProfile)) {
			return { .Refusal = agent_invocation_refusal::profile_invalid };
		}
		if (!aPolicy.has_value()) {
			// Deliberately before every other check. An unmapped host permits
			// nothing at all, so a reviewer cannot reach a permissive answer by
			// naming a profile that happens to be well formed.
			return { .Refusal = agent_invocation_refusal::host_not_mapped };
		}
		const auto allowed = std::ranges::find(aPolicy->Allowed, aProfile.Id)
			!= aPolicy->Allowed.end();
		if (!allowed) {
			return {
				.Refusal   = agent_invocation_refusal::profile_forbidden_on_host,
				.RefusedBy = aPolicy->Host,
			};
		}
		if (aPolicy->Required.has_value() && *aPolicy->Required != aProfile.Id) {
			// Being on the allowed list is not enough where the host requires
			// one. A corporate host that merely preferred its harness would
			// keep no review context inside it.
			return {
				.Refusal         = agent_invocation_refusal::profile_not_required_one,
				.RefusedBy       = aPolicy->Host,
				.RequiredInstead = aPolicy->Required,
			};
		}
		return { .Permitted = true };
	}

	auto policy_for_host(
		const std::span<const agent_host_policy> aPolicies,
		const git_host_id                        aHost) noexcept
		-> std::optional<agent_host_policy>
	{
		const auto found = std::ranges::find(
			aPolicies, aHost, &agent_host_policy::Host);
		if (found == aPolicies.end()) {
			return std::nullopt;
		}
		return *found;
	}
} // namespace bpr::core
