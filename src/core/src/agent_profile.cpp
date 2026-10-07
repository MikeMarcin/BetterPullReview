#include "bpr/core/agent_profile.hpp"

#include <algorithm>
#include <array>
#include <ranges>

namespace bpr::core {
	namespace {
		[[nodiscard]] constexpr auto ascii_upper(const char aByte) noexcept -> char
		{
			return aByte >= 'a' && aByte <= 'z'
				? static_cast<char>(aByte - 'a' + 'A')
				: aByte;
		}

		[[nodiscard]] auto ascii_equal_ignoring_case(
			const std::string_view aLeft,
			const std::string_view aRight) noexcept -> bool
		{
			return aLeft.size() == aRight.size()
				&& std::ranges::equal(aLeft, aRight, [](const char aLeftByte, const char aRightByte) {
					   return ascii_upper(aLeftByte) == ascii_upper(aRightByte);
				   });
		}

		[[nodiscard]] auto contains_ignoring_case(
			const std::string_view aText,
			const std::string_view aFragment) noexcept -> bool
		{
			if (aFragment.size() > aText.size()) {
				return false;
			}
			for (auto begin = std::size_t{ 0U };
				 begin <= aText.size() - aFragment.size(); ++begin) {
				if (ascii_equal_ignoring_case(
						aText.substr(begin, aFragment.size()), aFragment)) {
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] auto starts_with_ignoring_case(
			const std::string_view aText,
			const std::string_view aPrefix) noexcept -> bool
		{
			return aText.size() >= aPrefix.size()
				&& ascii_equal_ignoring_case(
					   aText.substr(0U, aPrefix.size()), aPrefix);
		}

		[[nodiscard]] auto contains_forbidden_environment_word(
			const std::string_view aName) noexcept -> bool
		{
			constexpr auto credential = std::array{
				'C',
				'R',
				'E',
				'D',
				'E',
				'N',
				'T',
				'I',
				'A',
				'L',
			};
			constexpr auto fragments = std::array{
				std::string_view{ "API_KEY" },
				std::string_view{ "APIKEY" },
				std::string_view{ "ACCESS_KEY" },
				std::string_view{ "PRIVATE_KEY" },
			};
			if (
				contains_ignoring_case(
					aName, std::string_view{ credential.data(), credential.size() })
				|| std::ranges::any_of(
					fragments,
					[aName](const auto aFragment) {
						return contains_ignoring_case(aName, aFragment);
					})) {
				return true;
			}

			constexpr auto gitInjectionNames = std::array{
				std::string_view{ "GIT_ASKPASS" },
				std::string_view{ "SSH_ASKPASS" },
				std::string_view{ "SSH_ASKPASS_REQUIRE" },
				std::string_view{ "GIT_SSH" },
				std::string_view{ "GIT_SSH_COMMAND" },
				std::string_view{ "GIT_PROXY_COMMAND" },
				std::string_view{ "GIT_EXEC_PATH" },
			};
			if (std::ranges::any_of(gitInjectionNames, [aName](const auto aForbidden) {
					return ascii_equal_ignoring_case(aName, aForbidden);
				})) {
				return true;
			}
			constexpr auto gitInjectionPrefixes = std::array{
				std::string_view{ "GIT_CONFIG" },
				std::string_view{ "GCM_" },
			};
			if (std::ranges::any_of(
					gitInjectionPrefixes,
					[aName](const auto aForbidden) {
						return starts_with_ignoring_case(aName, aForbidden);
					})) {
				return true;
			}

			constexpr auto forbiddenWords = std::array{
				std::string_view{ "AUTH" },
				std::string_view{ "AUTHORIZATION" },
				std::string_view{ "OAUTH" },
				std::string_view{ "PASSWORD" },
				std::string_view{ "PASSWD" },
				std::string_view{ "PAT" },
				std::string_view{ "SECRET" },
				std::string_view{ "TOKEN" },
			};
			for (auto begin = std::size_t{ 0U }; begin <= aName.size();) {
				const auto end  = aName.find('_', begin);
				const auto word = aName.substr(
					begin, end == std::string_view::npos ? aName.size() - begin : end - begin);
				if (std::ranges::any_of(forbiddenWords, [word](const auto aForbidden) {
						return ascii_equal_ignoring_case(word, aForbidden);
					})) {
					return true;
				}
				if (end == std::string_view::npos) {
					break;
				}
				begin = end + 1U;
			}
			return false;
		}
	} // namespace

	auto agent_search_path_variable() noexcept -> std::string_view { return "PATH"; }

	auto built_in_agent_profiles() -> std::vector<agent_profile>
	{
		return {
			{
				.Id                 = codex_app_server_profile_id,
				.DisplayName        = "Codex App Server",
				.Executable         = "codex",
				.Protocol           = agent_protocol::codex_app_server,
				.Workspace          = agent_workspace_policy::disposable_review_workspace,
				.AllowedEnvironment = { "HOME", "PATH", "LANG", "LC_ALL", "TMPDIR", "TEMP", "TMP" },
			},
			{
				.Id                 = codex_jsonl_profile_id,
				.DisplayName        = "Codex JSONL",
				.Executable         = "codex",
				.Protocol           = agent_protocol::codex_jsonl_exec,
				.Workspace          = agent_workspace_policy::disposable_review_workspace,
				.AllowedEnvironment = { "HOME", "PATH", "LANG", "LC_ALL", "TMPDIR", "TEMP", "TMP" },
			},
			{
				.Id                 = claude_stream_json_profile_id,
				.DisplayName        = "Approved Claude stream JSON",
				.Executable         = "claude",
				.Protocol           = agent_protocol::claude_stream_json,
				.Workspace          = agent_workspace_policy::disposable_review_workspace,
				.AllowedEnvironment = { "HOME", "PATH", "LANG", "LC_ALL", "TMPDIR", "TEMP", "TMP" },
			},
			{
				.Id                 = external_terminal_profile_id,
				.DisplayName        = "External terminal harness",
				.Executable         = "bpr-agent-terminal",
				.Protocol           = agent_protocol::external_terminal,
				.Workspace          = agent_workspace_policy::disposable_review_workspace,
				.AllowedEnvironment = { "HOME", "PATH", "LANG", "LC_ALL", "TMPDIR", "TEMP", "TMP" },
			},
		};
	}

	auto built_in_agent_policy(
		const git_host_id              aHost,
		const built_in_agent_host_kind aKind) -> agent_host_policy
	{
		auto policy = agent_host_policy{ .Host = aHost };
		if (aKind == built_in_agent_host_kind::github_com) {
			const auto profiles = built_in_agent_profiles();
			policy.Allowed.reserve(profiles.size());
			for (const auto& profile : profiles) {
				policy.Allowed.push_back(profile.Id);
			}
		} else {
			policy.Allowed = {
				claude_stream_json_profile_id,
				external_terminal_profile_id,
			};
		}
		return policy;
	}

	auto harness_name(const agent_profile& aProfile) -> std::string
	{
		switch (aProfile.Protocol) {
		case agent_protocol::codex_app_server:
		case agent_protocol::codex_jsonl_exec:   return "Codex";
		case agent_protocol::claude_stream_json: return "Claude";
		case agent_protocol::external_terminal:  break;
		}
		return aProfile.DisplayName;
	}

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

	auto permitted_agent_environment_name(const std::string_view aName) noexcept
		-> bool
	{
		if (aName.empty() || aName.size() > maximum_environment_name_bytes) {
			return false;
		}
		const auto isAlpha = [](const char aByte) {
			return (aByte >= 'A' && aByte <= 'Z')
				|| (aByte >= 'a' && aByte <= 'z');
		};
		const auto isDigit = [](const char aByte) {
			return aByte >= '0' && aByte <= '9';
		};
		if (aName.front() != '_' && !isAlpha(aName.front())) {
			return false;
		}
		if (std::ranges::any_of(aName.substr(1U), [isAlpha, isDigit](const char aByte) {
				return aByte != '_' && !isAlpha(aByte) && !isDigit(aByte);
			})) {
			return false;
		}
		return !contains_forbidden_environment_word(aName);
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
		for (auto index = std::size_t{ 0U };
			 index < aProfile.AllowedEnvironment.size(); ++index) {
			const auto& name = aProfile.AllowedEnvironment[index];
			if (!permitted_agent_environment_name(name)) {
				return false;
			}
			for (auto prior = std::size_t{ 0U }; prior < index; ++prior) {
				if (ascii_equal_ignoring_case(
						name, aProfile.AllowedEnvironment[prior])) {
					return false;
				}
			}
		}
		return true;
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
