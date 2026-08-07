/// \file
/// \brief Proves no credential crosses, and nothing sensitive travels in argv.

#include "bpr/application/agent_launch.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
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

	/// The parent this process pretends to be. It carries three secrets of
	/// different shapes on purpose: a test that searched for one name would
	/// pass against a child that leaked either of the others.
	[[nodiscard]] auto parent_environment() -> std::map<std::string, std::string>
	{
		return {
			{ "HOME", "/Users/reviewer" },
			{ "PATH", "/usr/bin:/bin" },
			{ "GITHUB_TOKEN", "ghp_the_reviewers_own_authority" },
			{ "AWS_SECRET_ACCESS_KEY", "aws_secret_value" },
			{ "ANTHROPIC_API_KEY", "sk-ant-secret" },
		};
	}

	[[nodiscard]] auto reader() -> application::environment_reader
	{
		return [](const std::string& aName) -> std::optional<std::string> {
			const auto parent = parent_environment();
			const auto found  = parent.find(aName);
			return found == parent.end() ? std::nullopt
										 : std::optional{ found->second };
		};
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto profile() -> core::agent_profile
	{
		return {
			.Id          = core::agent_profile_id{ 1U },
			.DisplayName = "Codex",
			.Executable  = "/opt/agents/codex",
			.Protocol    = core::agent_protocol::codex_jsonl_exec,
			.Workspace
			= core::agent_workspace_policy::disposable_review_workspace,
			.AllowedEnvironment = { "HOME", "PATH" },
		};
	}

	[[nodiscard]] auto supplement(const std::string_view aNote)
		-> core::agent_context_supplement
	{
		auto request = core::agent_supplement_request{
			.Binding = {
				.Host        = core::git_host_id{ 10U },
				.Repository  = core::repository_id{ 20U },
				.PullRequest = core::pull_request_id{ 30U },
				.BaseOid     = commit(1U),
				.HeadOid     = commit(2U),
				.LeftEpoch   = core::epoch_ordinal{ 1 },
				.RightEpoch  = core::epoch_ordinal{ 2 },
			},
		};
		if (!aNote.empty()) {
			request.SelectedNotes.push_back({ .Text = std::string{ aNote } });
		}
		const auto built = core::build_agent_supplement(request);
		expect(built.has_value(), "the fixture supplement builds");
		return *built;
	}

	/// \brief The child's environment is read in full, not searched.
	void test_no_credential_crosses()
	{
		const auto request = application::build_agent_launch(
			profile(), supplement({ }), reader());

		// Read whole. Every name the child got, compared against the whole
		// allowlist — so a fourth variable arriving fails here even though no
		// assertion names it.
		auto names = std::vector<std::string>{ };
		for (const auto& [name, value] : request.Environment) {
			names.push_back(name);
		}
		std::ranges::sort(names);
		expect(
			names == std::vector<std::string>{ "HOME", "PATH" },
			"the child's environment is exactly the profile's allowlist, name "
			"for name");

		// And the parent really did have the secrets, so the absence above is
		// about the allowlist rather than about a parent that had nothing.
		const auto parent = parent_environment();
		for (const auto& secret :
			 { "GITHUB_TOKEN", "AWS_SECRET_ACCESS_KEY", "ANTHROPIC_API_KEY" }) {
			expect(
				parent.contains(secret),
				"the parent this process pretends to be carries the secret");
			expect(
				std::ranges::none_of(
					request.Environment,
					[secret](const auto& aPair) { return aPair.first == secret; }),
				"and the child does not");
		}
	}

	/// \brief A name outside the allowlist cannot arrive however set.
	void test_the_allowlist_is_not_a_denylist()
	{
		auto permissive = profile();
		permissive.AllowedEnvironment.clear();
		const auto request = application::build_agent_launch(
			permissive, supplement({ }), reader());
		expect(
			request.Environment.empty(),
			"a profile that allows nothing gives the child nothing, rather "
			"than everything minus a list of known-bad names");
	}

	/// \brief The supplement travels on the input channel and nowhere else.
	void test_the_supplement_never_reaches_argv()
	{
		constexpr auto note = std::string_view{
			"unreleased Falcon pricing for customer Northwind"
		};
		const auto request = application::build_agent_launch(
			profile(), supplement(note), reader());

		expect(
			request.Input.find(note) != std::string::npos,
			"the note reaches the agent over its input channel");
		// argv read in full. Every argument compared against what the protocol
		// needs, so anything else appearing fails here whether or not it is
		// the note.
		expect(
			request.Arguments == std::vector<std::string>{ "exec", "--json" },
			"argv is exactly the protocol switch, argument for argument");
		for (const auto& argument : request.Arguments) {
			expect(
				argument.find(note) == std::string::npos,
				"and no argument carries the note, which `ps` would show to "
				"every other user of a shared machine");
		}
		expect(
			request.Input.find("schema\t1") != std::string::npos,
			"the encoded supplement leads with its schema version, so a tool "
			"can tell what it received before parsing the rest");
	}

	/// \brief A note carrying a separator cannot forge a record.
	void test_a_note_cannot_forge_a_record()
	{
		const auto request = application::build_agent_launch(
			profile(), supplement("first line\nnote\tsecond"), reader());
		const auto encoded = request.Input;
		auto       records = std::size_t{ 0U };
		for (const auto character : encoded) {
			records += character == '\n' ? 1U : 0U;
		}
		expect(
			records == 9U,
			"the eight binding fields and the one note make nine records, so "
			"the note's own newline did not become a tenth");
	}
} // namespace

auto main() -> int
{
	test_no_credential_crosses();
	test_the_allowlist_is_not_a_denylist();
	test_the_supplement_never_reaches_argv();
	test_a_note_cannot_forge_a_record();
	std::cout << "agent-launch-tests:pass\n";
	return EXIT_SUCCESS;
}
