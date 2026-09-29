/// \file
/// \brief Proves agent invocation legality, and that no type here holds a secret.

#include "bpr/core/agent_profile.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	constexpr auto personal      = core::agent_profile_id{ 1U };
	constexpr auto corporate     = core::agent_profile_id{ 2U };
	constexpr auto corporateHost = core::git_host_id{ 10U };
	constexpr auto unmappedHost  = core::git_host_id{ 11U };

	[[nodiscard]] auto profile(
		const core::agent_profile_id aId,
		const std::string_view       aName) -> core::agent_profile
	{
		return {
			.Id          = aId,
			.DisplayName = std::string{ aName },
			.Executable  = "/opt/agents/" + std::string{ aName },
			.Protocol    = core::agent_protocol::codex_jsonl_exec,
			.Workspace
			= core::agent_workspace_policy::disposable_review_workspace,
			.AllowedEnvironment = { "HOME", "PATH" },
		};
	}

	/// \brief A corporate host refuses the personal provider by name.
	void test_a_forbidden_provider_is_refused_and_says_who_refused_it()
	{
		const auto policy = core::agent_host_policy{
			.Host     = corporateHost,
			.Allowed  = { corporate },
			.Required = corporate,
		};
		const auto verdict = core::decide_agent_invocation(
			profile(personal, "codex"), policy);

		expect(!verdict.Permitted, "the personal provider is refused");
		expect(
			verdict.Refusal
				== core::agent_invocation_refusal::profile_forbidden_on_host,
			"the refusal names the reason rather than being a bare no");
		expect(
			verdict.RefusedBy == std::optional{ corporateHost },
			"and it names the host that forbade it, which is the part a "
			"reviewer can act on");
	}

	/// \brief Being allowed is not enough where the host requires one.
	void test_a_required_profile_excludes_the_merely_allowed()
	{
		const auto policy = core::agent_host_policy{
			.Host     = corporateHost,
			.Allowed  = { personal, corporate },
			.Required = corporate,
		};
		const auto verdict = core::decide_agent_invocation(
			profile(personal, "codex"), policy);
		expect(
			!verdict.Permitted
				&& verdict.Refusal
					== core::agent_invocation_refusal::profile_not_required_one,
			"a host that requires a harness refuses one it merely allows");
		expect(
			verdict.RequiredInstead == std::optional{ corporate },
			"and names the profile it requires instead");
		expect(
			core::decide_agent_invocation(profile(corporate, "harness"), policy)
				.Permitted,
			"the required profile itself is permitted, so the refusal above is "
			"about which profile and not about the policy refusing everything");
	}

	/// \brief An unmapped host permits nothing.
	void test_an_unmapped_host_permits_nothing()
	{
		const auto policies = std::vector<core::agent_host_policy>{
			{ .Host = corporateHost, .Allowed = { corporate } },
		};
		const auto found = core::policy_for_host(policies, unmappedHost);
		expect(!found.has_value(), "the unmapped host has no policy");

		for (const auto id : { personal, corporate }) {
			const auto verdict = core::decide_agent_invocation(
				profile(id, "any"), found);
			expect(
				!verdict.Permitted
					&& verdict.Refusal
						== core::agent_invocation_refusal::host_not_mapped,
				"an unmapped host permits nothing, rather than defaulting to "
				"permitting everything");
		}
	}

	/// \brief No type in this slice has a field a credential could occupy.
	///
	/// \details Read off the type, not off a construction site. A test that
	/// built a profile and looked for a secret in it would pass for every
	/// profile nobody thought to put one in.
	void test_no_type_can_carry_a_credential()
	{
		// The whole of `agent_profile`, field by field. Any addition changes
		// this size and this assertion, which is the point: the next person to
		// add a `Token` field has to come here and say so.
		using profile_type = core::agent_profile;
		static_assert(
			std::is_same_v<decltype(profile_type::Id), core::agent_profile_id>);
		static_assert(
			std::is_same_v<decltype(profile_type::DisplayName), std::string>);
		static_assert(
			std::is_same_v<decltype(profile_type::Executable), std::string>);
		static_assert(
			std::is_same_v<decltype(profile_type::Protocol), core::agent_protocol>);
		static_assert(std::is_same_v<
					  decltype(profile_type::Workspace),
					  core::agent_workspace_policy>);
		static_assert(std::is_same_v<
					  decltype(profile_type::AllowedEnvironment),
					  std::vector<std::string>>);
		// `DefaultSettings` is the model and effort the profile asks for. A run
		// records both in its provenance, so they are never where a secret goes.
		static_assert(std::is_same_v<
					  decltype(profile_type::DefaultSettings),
					  core::agent_setting_facts>);
		static_assert(
			sizeof(core::agent_setting_facts)
			== sizeof(std::optional<std::string>) * 2U);
		static_assert(
			sizeof(profile_type)
				== sizeof(core::agent_profile_id) + sizeof(std::string) * 2U
					+ sizeof(std::vector<std::string>)
					+ sizeof(core::agent_protocol)
					+ sizeof(core::agent_workspace_policy)
					+ sizeof(core::agent_setting_facts) + 6U,
			"agent_profile gained or lost a field. If it gained one, say here "
			"why it cannot hold a credential.");

		// The environment allowlist is names, so there is nowhere for a value
		// to live even if a caller wanted to smuggle one through it.
		static_assert(
			std::is_same_v<
				decltype(profile_type::AllowedEnvironment)::value_type,
				std::string>,
			"the allowlist is a list of names; a map would give every name a "
			"value, and a value is where a secret goes");

		const auto built = profile(personal, "codex");
		expect(
			built.AllowedEnvironment.size() == 2U
				&& built.AllowedEnvironment.front() == "HOME",
			"the allowlist carries names the host may pass through, and only "
			"names");
	}

	/// \brief A malformed profile is refused before any policy is consulted.
	void test_a_malformed_profile_is_refused()
	{
		auto broken = profile(personal, "codex");
		broken.Executable.clear();
		const auto policy = core::agent_host_policy{
			.Host    = corporateHost,
			.Allowed = { personal },
		};
		const auto verdict = core::decide_agent_invocation(broken, policy);
		expect(
			!verdict.Permitted
				&& verdict.Refusal
					== core::agent_invocation_refusal::profile_invalid,
			"a profile with no executable is refused as malformed");

		auto oversize = profile(personal, "codex");
		oversize.AllowedEnvironment.assign(
			core::maximum_environment_allowlist + 1U, "NAME");
		expect(
			!core::valid(oversize),
			"an allowlist beyond its bound is refused rather than truncated");
	}

	void test_secret_bearing_environment_names_are_structurally_refused()
	{
		for (const auto name : {
				 "GITHUB_TOKEN",
				 "gh_token",
				 "GitHub_PaT",
				 "ANTHROPIC_API_KEY",
				 "claude_code_oauth_token",
				 "OPENAI_APIKEY",
				 "AWS_ACCESS_KEY_ID",
				 "aws_secret_access_key",
				 "AWS_SHARED_CREDENTIALS_FILE",
				 "SSH_AUTH_SOCK",
				 "DATABASE_PASSWORD",
				 "GIT_ASKPASS",
				 "ssh_askpass",
				 "SSH_ASKPASS_REQUIRE",
				 "GIT_CONFIG_COUNT",
				 "git_config_key_0",
				 "GIT_CONFIG_VALUE_0",
				 "GIT_CONFIG_PARAMETERS",
				 "GIT_CONFIG_GLOBAL",
				 "GIT_SSH",
				 "git_ssh_command",
				 "GIT_PROXY_COMMAND",
				 "GIT_EXEC_PATH",
				 "GCM_CREDENTIAL_STORE",
			 }) {
			expect(
				!core::permitted_agent_environment_name(name),
				"credential aliases and case variants are refused by name");
			auto candidate               = profile(personal, "codex");
			candidate.AllowedEnvironment = { std::string{ name } };
			expect(
				!core::valid(candidate),
				"a profile cannot make a credential-bearing name valid");
		}

		for (const auto name : {
				 "HOME",
				 "PATH",
				 "LANG",
				 "AWS_REGION",
				 "BPR_CANARY_ALLOWED",
				 "GIT_TERMINAL_PROMPT",
				 "GIT_HTTP_LOW_SPEED_LIMIT",
			 }) {
			expect(
				core::permitted_agent_environment_name(name),
				"safe explicit process configuration remains available");
		}

		expect(
			!core::permitted_agent_environment_name("BAD=NAME")
				&& !core::permitted_agent_environment_name("9STARTS_WITH_NUMBER"),
			"environment names use a conservative portable syntax");
		auto duplicate               = profile(personal, "codex");
		duplicate.AllowedEnvironment = { "PATH", "path" };
		expect(
			!core::valid(duplicate),
			"case-insensitive aliases cannot select one parent value twice");
	}

	void test_built_in_profiles_are_safe_immutable_data()
	{
		const auto profiles = core::built_in_agent_profiles();
		expect(profiles.size() == 4U, "all four shipped protocols have profiles");
		expect(
			profiles[0].Id == core::codex_app_server_profile_id
				&& profiles[0].Protocol == core::agent_protocol::codex_app_server
				&& profiles[1].Id == core::codex_jsonl_profile_id
				&& profiles[1].Protocol == core::agent_protocol::codex_jsonl_exec
				&& profiles[2].Id == core::claude_stream_json_profile_id
				&& profiles[2].Protocol == core::agent_protocol::claude_stream_json
				&& profiles[3].Id == core::external_terminal_profile_id
				&& profiles[3].Protocol == core::agent_protocol::external_terminal,
			"the stable IDs select the documented protocol rather than a label");
		for (const auto& candidate : profiles) {
			expect(core::valid(candidate), "every shipped profile is valid");
			expect(
				candidate.Workspace
					== core::agent_workspace_policy::disposable_review_workspace,
				"every shipped profile defaults to the pinned disposable workspace");
			for (const auto& name : candidate.AllowedEnvironment) {
				expect(
					core::permitted_agent_environment_name(name),
					"a shipped profile names only a safe explicit environment input");
			}
		}
	}

	void test_built_in_host_policy_refuses_codex_on_explicit_ghes()
	{
		const auto profiles = core::built_in_agent_profiles();
		const auto github   = core::built_in_agent_policy(
			core::git_host_id{ 1U }, core::built_in_agent_host_kind::github_com);
		const auto ghes = core::built_in_agent_policy(
			core::git_host_id{ 2U }, core::built_in_agent_host_kind::explicit_ghes);

		for (const auto& candidate : profiles) {
			expect(
				core::decide_agent_invocation(candidate, github).Permitted,
				"GitHub.com offers every shipped profile");
			const auto verdict = core::decide_agent_invocation(candidate, ghes);
			const auto corporateAlternative
				= candidate.Id == core::claude_stream_json_profile_id
				|| candidate.Id == core::external_terminal_profile_id;
			expect(
				verdict.Permitted == corporateAlternative,
				"explicit GHES permits only corporate Claude and external alternatives");
		}
	}
} // namespace

auto main() -> int
{
	test_a_forbidden_provider_is_refused_and_says_who_refused_it();
	test_a_required_profile_excludes_the_merely_allowed();
	test_an_unmapped_host_permits_nothing();
	test_no_type_can_carry_a_credential();
	test_a_malformed_profile_is_refused();
	test_secret_bearing_environment_names_are_structurally_refused();
	test_built_in_profiles_are_safe_immutable_data();
	test_built_in_host_policy_refuses_codex_on_explicit_ghes();
	std::cout << "agent-profile-tests:pass\n";
	return EXIT_SUCCESS;
}
