/// \file
/// \brief Proves the dangerous workspace mode needs a choice, not a setting.

#include "bpr/application/agent_workspace.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto session() -> core::agent_session
	{
		return {
			.Id      = core::agent_session_id{ 7U },
			.Profile = core::agent_profile_id{ 1U },
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
	}

	[[nodiscard]] auto profile(const core::agent_workspace_policy aPolicy)
		-> core::agent_profile
	{
		return {
			.Id                 = core::agent_profile_id{ 1U },
			.DisplayName        = "Codex",
			.Executable         = "/opt/agents/codex",
			.Protocol           = core::agent_protocol::codex_jsonl_exec,
			.Workspace          = aPolicy,
			.AllowedEnvironment = { "HOME" },
		};
	}

	/// \brief A disposable workspace pins the session's head, not the checkout's.
	void test_a_disposable_workspace_pins_the_session_head()
	{
		const auto checkout = application::checkout_observation{
			.Path        = "/Users/reviewer/project",
			.Cleanliness = application::checkout_cleanliness::dirty,
		};
		const auto prepared = application::prepare_agent_workspace(
			profile(core::agent_workspace_policy::disposable_review_workspace),
			session(),
			checkout,
			false,
			"/tmp/bpr-review-workspace");

		expect(prepared.Workspace.has_value(), "the workspace is prepared");
		expect(
			prepared.Workspace->PinnedHead == std::optional{ commit(2U) },
			"the workspace is pinned to the session's own head");
		expect(
			prepared.Workspace->Path == "/tmp/bpr-review-workspace"
				&& prepared.Workspace->Path != checkout.Path,
			"and is somewhere other than the active worktree, so changes made "
			"there afterwards cannot reach it");
		expect(
			prepared.Workspace->Cleanliness
				== application::checkout_cleanliness::clean,
			"a workspace materialised from an immutable revision has nothing "
			"uncommitted in it, whatever the reviewer's checkout is doing");
	}

	/// \brief Later changes to the checkout cannot move a pinned workspace.
	void test_the_pin_does_not_follow_the_worktree()
	{
		const auto pinned = application::prepare_agent_workspace(
			profile(core::agent_workspace_policy::disposable_review_workspace),
			session(),
			application::checkout_observation{ .Path = "/Users/reviewer/project" },
			false,
			"/tmp/bpr-review-workspace");

		// The reviewer commits, rebases, and dirties the worktree.
		const auto later = application::prepare_agent_workspace(
			profile(core::agent_workspace_policy::disposable_review_workspace),
			session(),
			application::checkout_observation{
				.Path        = "/Users/reviewer/project",
				.Cleanliness = application::checkout_cleanliness::dirty,
			},
			false,
			"/tmp/bpr-review-workspace");

		expect(
			pinned.Workspace->PinnedHead == later.Workspace->PinnedHead,
			"the pin comes from the session, so nothing about the worktree "
			"moves it");
	}

	/// \brief Existing-checkout mode is never reached without a choice.
	void test_existing_checkout_needs_an_explicit_choice()
	{
		const auto checkout = application::checkout_observation{
			.Path        = "/Users/reviewer/project",
			.Cleanliness = application::checkout_cleanliness::dirty,
		};
		const auto refused = application::prepare_agent_workspace(
			profile(core::agent_workspace_policy::existing_checkout),
			session(),
			checkout,
			false,
			"/tmp/bpr-review-workspace");
		expect(
			!refused.Workspace.has_value()
				&& refused.Refusal
					== application::workspace_refusal::existing_checkout_not_chosen,
			"a profile configured for the reviewer's checkout cannot opt itself "
			"into their uncommitted work");

		const auto chosen = application::prepare_agent_workspace(
			profile(core::agent_workspace_policy::existing_checkout),
			session(),
			checkout,
			true,
			"/tmp/bpr-review-workspace");
		expect(
			chosen.Workspace.has_value()
				&& chosen.Workspace->Path == checkout.Path,
			"and is available once someone chooses it now, so the refusal "
			"above is about the choice rather than the mode being unreachable");
	}

	/// \brief Dirty state is reported, and unknown stays unknown.
	void test_dirtiness_is_reported_rather_than_assumed()
	{
		for (const auto state :
			 { application::checkout_cleanliness::clean,
			   application::checkout_cleanliness::dirty,
			   application::checkout_cleanliness::unknown }) {
			const auto prepared = application::prepare_agent_workspace(
				profile(core::agent_workspace_policy::existing_checkout),
				session(),
				application::checkout_observation{
					.Path = "/Users/reviewer/project", .Cleanliness = state },
				true,
				"/tmp/bpr-review-workspace");
			expect(
				prepared.Workspace.has_value()
					&& prepared.Workspace->Cleanliness == state,
				"the observed state travels unchanged, including unknown - a "
				"host that could not determine it must not pick the "
				"reassuring answer");
		}
	}
} // namespace

auto main() -> int
{
	test_a_disposable_workspace_pins_the_session_head();
	test_the_pin_does_not_follow_the_worktree();
	test_existing_checkout_needs_an_explicit_choice();
	test_dirtiness_is_reported_rather_than_assumed();
	std::cout << "agent-workspace-tests:pass\n";
	return EXIT_SUCCESS;
}
