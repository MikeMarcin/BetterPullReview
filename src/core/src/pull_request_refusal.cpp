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

		/// [[BPR-2048]]. Too large to read here: what was too large, and the
		/// place it can still be reviewed. The owner, 2026-11-10: "We probably
		/// need something better for recovery. Possibly suggesting opening it
		/// on GitHub."
		[[nodiscard]] auto too_large(const pull_request_refusal& aRefusal) -> pull_request_refusal_wording
		{
			auto what = std::string{ };
			switch (aRefusal.Limit) {
			case github_response_limit::files:
				what = "It changes more files than BetterPullReview can read: GitHub lists at most 3,000 of a pull request's files.";
				break;
			case github_response_limit::commits:
				what = "It has more commits than BetterPullReview reads safely.";
				break;
			case github_response_limit::conversation:
				what = "Its reviews and comments are more than BetterPullReview reads safely.";
				break;
			case github_response_limit::checks:
				what = "Its checks are more than BetterPullReview reads safely.";
				break;
			case github_response_limit::unknown:
				what = "Part of it is larger than BetterPullReview reads safely.";
				break;
			}
			return {
				.Heading     = named(aRefusal) + " is too large to open here",
				.Explanation = what + " GitHub shows all of it.",
				.Actions     = { pull_request_refusal_action::open_on_github, pull_request_refusal_action::retry },
			};
		}
	}

	auto word_pull_request_refusal(
		const pull_request_refusal&         aRefusal,
		const pull_request_refusal_context& aContext)
		-> std::optional<pull_request_refusal_wording>
	{
		if (aRefusal.Reason == github_reason_code::response_too_large) {
			return too_large(aRefusal);
		}
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
		case pull_request_refusal_action::open_on_github:
			return "Open on GitHub";
		case pull_request_refusal_action::retry:
			return "Retry";
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
		case pull_request_refusal_action::open_on_github:
			return "Opens the pull request in your browser.";
		case pull_request_refusal_action::retry:
			return "Asks GitHub for the pull request again.";
		}
		return "Opens the sign-in dialog and begins a device sign-in.";
	}
}
