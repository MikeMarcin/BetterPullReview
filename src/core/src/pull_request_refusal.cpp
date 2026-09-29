#include "bpr/core/pull_request_refusal.hpp"

#include <string>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto named(const pull_request_refusal& aRefusal) -> std::string
		{
			return aRefusal.Repository.Owner + "/" + aRefusal.Repository.Name + "#"
				+ std::to_string(aRefusal.Number.value());
		}

		[[nodiscard]] auto signed_out(
			const pull_request_refusal&         aRefusal,
			const pull_request_refusal_context& aContext) -> pull_request_refusal_wording
		{
			// Signed out, GitHub answers a private repository exactly as it
			// answers a missing one, so the words name both rather than guess.
			auto wording = pull_request_refusal_wording{
				.Heading     = "Sign in to open " + named(aRefusal),
				.Explanation = "You are not signed in, so only public pull requests open. "
							   "Without an account GitHub does not show this one: its "
							   "repository is private, or it does not exist.",
				.Actions     = { pull_request_refusal_action::sign_in },
			};
			if (aContext.GithubCliOffered) {
				wording.Actions.push_back(pull_request_refusal_action::use_github_cli);
			}
			return wording;
		}

		[[nodiscard]] auto account(
			const pull_request_refusal&         aRefusal,
			const pull_request_refusal_context& aContext) -> pull_request_refusal_wording
		{
			const auto known = !aContext.Login.empty();
			const auto who   = known ? aContext.Login : std::string{ "This account" };
			const auto to    = known ? aContext.Login : std::string{ "this account" };
			// The app's own sign-in can be refused where the same person's
			// browser is not: an organization may not allow this app
			// ([[BPR-0643]]). GitHub CLI signs in through its own app.
			const auto cli = aContext.GithubCliOffered && !aContext.ThroughGithubCli;
			auto       why = aContext.ThroughGithubCli
				? "GitHub does not show it to " + to + " through GitHub CLI. "
													   "The repository may be private to other accounts, or it may not exist. "
				: "GitHub does not show it to " + to + " through this app's sign-in. "
													   "The repository may be private to other accounts, it may not exist, "
													   "or its organization may not allow this app. ";
			why += cli ? "Another account, or GitHub CLI, may be able to open it."
					   : "Another account may be able to open it.";
			auto wording = pull_request_refusal_wording{
				.Heading     = who + " cannot open " + named(aRefusal),
				.Explanation = std::move(why),
				.Actions     = { pull_request_refusal_action::use_another_account },
			};
			if (cli) {
				wording.Actions.push_back(pull_request_refusal_action::use_github_cli);
			}
			return wording;
		}
	}

	auto word_pull_request_refusal(
		const pull_request_refusal&         aRefusal,
		const pull_request_refusal_context& aContext)
		-> std::optional<pull_request_refusal_wording>
	{
		if (aRefusal.Reason != github_reason_code::permission_denied) {
			return std::nullopt;
		}
		switch (aRefusal.Connection) {
		case refused_connection::signed_out:
			return signed_out(aRefusal, aContext);
		case refused_connection::account:
			return account(aRefusal, aContext);
		}
		return std::nullopt;
	}

	auto label(const pull_request_refusal_action aAction) noexcept -> std::string_view
	{
		switch (aAction) {
		case pull_request_refusal_action::sign_in:
			return "Sign in";
		case pull_request_refusal_action::use_github_cli:
			return "Use GitHub CLI";
		case pull_request_refusal_action::use_another_account:
			return "Use another account";
		}
		return "Sign in";
	}

	auto consequence(const pull_request_refusal_action aAction) noexcept -> std::string_view
	{
		switch (aAction) {
		case pull_request_refusal_action::sign_in:
			return "Opens the sign-in dialog and begins a device sign-in.";
		case pull_request_refusal_action::use_github_cli:
			return "Checks the active authenticated hosts in your installed gh after you choose it.";
		case pull_request_refusal_action::use_another_account:
			return "Opens the sign-in dialog, where this server's account can be signed out and another signed in.";
		}
		return "Opens the sign-in dialog and begins a device sign-in.";
	}
}
