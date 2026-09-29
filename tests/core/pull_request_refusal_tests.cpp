#include "bpr/core/pull_request_refusal.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using action = core::pull_request_refusal_action;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto contains(const std::string& aText, const std::string_view aPart) -> bool
	{
		return aText.find(aPart) != std::string::npos;
	}

	[[nodiscard]] auto refusal(
		const core::refused_connection aConnection,
		const core::github_reason_code aReason = core::github_reason_code::permission_denied)
		-> core::pull_request_refusal
	{
		return core::pull_request_refusal{
			.Reason     = aReason,
			.Connection = aConnection,
			.Repository = core::repository_locator{ .Owner = "octo-org", .Name = "private-repository" },
			.Number     = core::pull_request_number{ 17 },
		};
	}

	// The owner's case: signed out, a private pull request. The words say that
	// the app is signed out and that signing in may open it, and never speak of
	// an app connection or a browser.
	void test_signed_out_says_sign_in_and_names_the_pull_request()
	{
		const auto wording = core::word_pull_request_refusal(
			refusal(core::refused_connection::signed_out),
			core::pull_request_refusal_context{ .GithubCliOffered = true });
		expect(wording.has_value(), "a signed-out permission refusal is worded");
		expect(wording->Heading == "Sign in to open octo-org/private-repository#17",
			   "the heading names the pull request and the step that may open it");
		expect(contains(wording->Explanation, "not signed in")
				   && contains(wording->Explanation, "private")
				   && contains(wording->Explanation, "does not exist"),
			   "signed out, the explanation says so and names both answers GitHub gives alike");
		expect(!contains(wording->Heading + wording->Explanation, "browser")
				   && !contains(wording->Heading + wording->Explanation, "connection"),
			   "the signed-out words do not speak of a connection or a browser that are not there");
		expect(wording->Actions == std::vector{ action::sign_in, action::use_github_cli },
			   "Sign in comes first, then GitHub CLI where it is offered");

		const auto withoutCli = core::word_pull_request_refusal(
			refusal(core::refused_connection::signed_out), core::pull_request_refusal_context{ });
		expect(withoutCli.has_value() && withoutCli->Actions == std::vector{ action::sign_in },
			   "where GitHub CLI is not offered, it is not offered here either");
	}

	void test_an_account_is_named_and_offered_another()
	{
		const auto wording = core::word_pull_request_refusal(
			refusal(core::refused_connection::account),
			core::pull_request_refusal_context{ .Login = "octo-reviewer", .GithubCliOffered = true });
		expect(wording.has_value(), "an account's permission refusal is worded");
		expect(wording->Heading == "octo-reviewer cannot open octo-org/private-repository#17",
			   "the heading names the account and the pull request");
		expect(contains(wording->Explanation, "octo-reviewer")
				   && contains(wording->Explanation, "this app's sign-in")
				   && contains(wording->Explanation, "organization may not allow this app"),
			   "the explanation says why in terms of the account and this app's sign-in");
		expect(!contains(wording->Explanation, "not signed in"),
			   "an account's refusal does not say it is signed out");
		expect(wording->Actions == std::vector{ action::use_another_account, action::use_github_cli },
			   "another account comes first, then GitHub CLI");

		const auto throughCli = core::word_pull_request_refusal(
			refusal(core::refused_connection::account),
			core::pull_request_refusal_context{ .Login = "octo-reviewer", .GithubCliOffered = true, .ThroughGithubCli = true });
		expect(throughCli.has_value() && throughCli->Actions == std::vector{ action::use_another_account },
			   "an account already read through GitHub CLI is not offered GitHub CLI again");
		expect(contains(throughCli->Explanation, "through GitHub CLI")
				   && !contains(throughCli->Explanation, "allow this app"),
			   "through GitHub CLI, the words name GitHub CLI rather than this app");

		const auto unknown = core::word_pull_request_refusal(
			refusal(core::refused_connection::account), core::pull_request_refusal_context{ });
		expect(unknown.has_value() && unknown->Heading == "This account cannot open octo-org/private-repository#17"
				   && unknown->Actions == std::vector{ action::use_another_account },
			   "an account whose login is not yet known is still named as an account");
	}

	void test_only_a_permission_refusal_is_worded()
	{
		constexpr auto others = std::array{
			core::github_reason_code::network_unavailable,
			core::github_reason_code::authentication_failed,
			core::github_reason_code::rate_limited,
			core::github_reason_code::timed_out,
			core::github_reason_code::none,
		};
		for (const auto reason : others) {
			for (const auto connection : { core::refused_connection::signed_out, core::refused_connection::account }) {
				expect(!core::word_pull_request_refusal(refusal(connection, reason), core::pull_request_refusal_context{ .GithubCliOffered = true }).has_value(),
					   "a refusal another connection would not change keeps its diagnostic sentence");
			}
		}
	}

	void test_every_action_has_a_verb_and_a_consequence()
	{
		expect(core::label(action::sign_in) == "Sign in" && core::label(action::use_github_cli) == "Use GitHub CLI"
				   && core::label(action::use_another_account) == "Use another account",
			   "each step is a verb, in the words the rest of Home uses");
		for (const auto step : { action::sign_in, action::use_github_cli, action::use_another_account }) {
			expect(!core::consequence(step).empty() && core::consequence(step).back() == '.',
				   "each step says what it does in a sentence");
		}
	}
}

auto main() -> int
{
	test_signed_out_says_sign_in_and_names_the_pull_request();
	test_an_account_is_named_and_offered_another();
	test_only_a_permission_refusal_is_worded();
	test_every_action_has_a_verb_and_a_consequence();
	std::cout << "All pull request refusal wording tests passed\n";
	return EXIT_SUCCESS;
}
