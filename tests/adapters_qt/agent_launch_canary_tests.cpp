/// \file
/// \brief Launches a real child and reads back its argv and environment.
///
/// \details The request-level suite proves what the request carries. This one
/// proves what a process actually started from it *receives*, which is a
/// different claim: Qt hands a child a copy of the parent's environment unless
/// told otherwise, so the boundary could hold perfectly in the request and be
/// lost entirely at the call.

#include "bpr/adapters/agent/agent_process_launcher.hpp"
#include "bpr/application/agent_launch.hpp"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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

	/// The three secrets this process really does carry while the child runs.
	/// Set here rather than assumed, so the absence asserted below is about
	/// the launcher and not about a parent that had nothing to leak.
	void plant_parent_secrets()
	{
		static constexpr auto secrets = std::array{
			std::pair{ "GITHUB_TOKEN", "ghp_the_reviewers_own_authority" },
			std::pair{ "AWS_SECRET_ACCESS_KEY", "aws_secret_value" },
			std::pair{ "ANTHROPIC_API_KEY", "sk-ant-secret" },
		};
		for (const auto& [name, value] : secrets) {
			qputenv(name, QByteArray{ value });
			expect(
				qEnvironmentVariableIsSet(name),
				"the parent this test runs as really carries the secret");
		}
	}

	[[nodiscard]] auto lines(const std::string& aOutput, const std::string_view aTag)
		-> std::vector<std::string>
	{
		auto result = std::vector<std::string>{ };
		auto begin  = std::size_t{ 0U };
		while (begin < aOutput.size()) {
			const auto end  = aOutput.find('\n', begin);
			const auto line = aOutput.substr(
				begin, end == std::string::npos ? std::string::npos : end - begin);
			if (line.starts_with(std::string{ aTag } + "\t")) {
				result.push_back(line.substr(aTag.size() + 1U));
			}
			if (end == std::string::npos) {
				break;
			}
			begin = end + 1U;
		}
		return result;
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto canary_profile() -> core::agent_profile
	{
		return {
			.Id          = core::agent_profile_id{ 1U },
			.DisplayName = "Canary",
			.Executable  = BPR_AGENT_CANARY_EXECUTABLE,
			.Protocol    = core::agent_protocol::codex_jsonl_exec,
			.Workspace
			= core::agent_workspace_policy::disposable_review_workspace,
			.AllowedEnvironment = { "BPR_CANARY_ALLOWED" },
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
		request.SelectedNotes.push_back({ .Text = std::string{ aNote } });
		const auto built = core::build_agent_supplement(request);
		expect(built.has_value(), "the fixture supplement builds");
		return *built;
	}

	/// \brief What a real child receives, read in full from the child itself.
	void test_the_child_receives_only_the_allowlist()
	{
		plant_parent_secrets();
		qputenv("BPR_CANARY_ALLOWED", QByteArray{ "allowed-value" });

		constexpr auto note = std::string_view{
			"unreleased Falcon pricing for customer Northwind"
		};
		const auto request = application::build_agent_launch(
			canary_profile(),
			supplement(note),
			[](const std::string& aName) -> std::optional<std::string> {
				return qEnvironmentVariableIsSet(aName.c_str())
					? std::optional{ qEnvironmentVariable(aName.c_str())
										 .toStdString() }
					: std::nullopt;
			});

		auto       launcher = adapters::agent::agent_process_launcher{ };
		const auto result   = launcher.run(request, 20'000);
		expect(
			result.Outcome == adapters::agent::agent_run_outcome::completed,
			"the canary ran to completion");
		expect(
			result.Output.find("canary-complete") != std::string::npos,
			"and reported everything it was given");

		// The child's whole environment, read from the child. Not searched for
		// a known name: a search for GITHUB_TOKEN would pass against a child
		// that inherited AWS_SECRET_ACCESS_KEY instead.
		const auto environment = lines(result.Output, "env");
		auto       names       = std::vector<std::string>{ };
		for (const auto& entry : environment) {
			names.push_back(entry.substr(0U, entry.find('=')));
		}
		std::ranges::sort(names);
		expect(
			names == std::vector<std::string>{ "BPR_CANARY_ALLOWED" },
			"the child's entire environment is the profile's allowlist - one "
			"name, read from the process itself");

		// And the parent still holds all three while the child holds none.
		for (const auto& secret :
			 { "GITHUB_TOKEN", "AWS_SECRET_ACCESS_KEY", "ANTHROPIC_API_KEY" }) {
			expect(
				qEnvironmentVariableIsSet(secret),
				"the parent carries the secret at the moment the child ran");
			expect(
				std::ranges::none_of(
					names,
					[secret](const std::string& aName) { return aName == secret; }),
				"and the child did not receive it");
		}
	}

	/// \brief argv is read from the child, and carries nothing sensitive.
	void test_the_child_argv_carries_no_context()
	{
		constexpr auto note    = std::string_view{ "private drafting notes" };
		const auto     request = application::build_agent_launch(
			canary_profile(),
			supplement(note),
			[](const std::string&) -> std::optional<std::string> {
				return std::nullopt;
			});
		auto       launcher = adapters::agent::agent_process_launcher{ };
		const auto result   = launcher.run(request, 20'000);
		expect(
			result.Outcome == adapters::agent::agent_run_outcome::completed,
			"the canary ran");

		const auto arguments = lines(result.Output, "argv");
		expect(
			arguments.size() == 3U,
			"the child saw its own path and the two protocol switches, and "
			"nothing else");
		expect(
			arguments.at(1) == "exec" && arguments.at(2) == "--json",
			"argv is the protocol switch, read from the child");
		for (const auto& argument : arguments) {
			expect(
				argument.find(note) == std::string::npos,
				"and no argument carries the note, which ps would show to "
				"every other user of a shared machine");
		}

		// It did arrive, over the channel that is not world-readable.
		const auto input = lines(result.Output, "stdin");
		expect(
			std::ranges::any_of(
				input,
				[note](const std::string& aLine) {
					return aLine.find(note) != std::string::npos;
				}),
			"the note reached the agent over standard input");
	}

	/// \brief A cancelled run publishes nothing.
	void test_cancellation_publishes_nothing()
	{
		auto request = application::build_agent_launch(
			canary_profile(),
			supplement("note"),
			[](const std::string&) -> std::optional<std::string> {
				return std::nullopt;
			});
		// The canary reads standard input until it closes. Never closing it
		// leaves the child running, so the timeout is what ends this run.
		auto launcher = adapters::agent::agent_process_launcher{ };
		request.Arguments.emplace_back("--wait-forever");
		const auto result = launcher.run(request, 1'000);
		expect(
			result.Outcome != adapters::agent::agent_run_outcome::completed,
			"a run the launcher had to end did not complete");
		expect(
			result.Output.empty(),
			"and published nothing, because a partial answer from an agent is "
			"indistinguishable from a complete one that happened to be short");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QCoreApplication application{ argc, argv };
	test_the_child_receives_only_the_allowlist();
	test_the_child_argv_carries_no_context();
	test_cancellation_publishes_nothing();
	std::cout << "agent-launch-canary-tests:pass\n";
	return EXIT_SUCCESS;
}
