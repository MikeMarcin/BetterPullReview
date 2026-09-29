#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_PULL_REQUEST_REFUSAL_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_PULL_REQUEST_REFUSAL_HPP

/// \file
/// \brief [[BPR-1200]]. What a refused first read says, and what it offers.
///
/// \details The owner, signed out, pasted a private pull request and was told
/// in one line of helper text that "this app's GitHub connection" could not
/// read it and that a browser might have different access. The line spoke of
/// a connection that did not exist and did not say that signing in might open
/// the pull request. The words now follow from two typed facts, the reason and
/// the connection, so a signed-out refusal and an account's refusal cannot say
/// the same thing, and every refusal offers the step that could change it.

#include "bpr/core/github_model.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief A step a refusal offers the reviewer.
	enum class pull_request_refusal_action : std::uint8_t {
		/// Sign in to the refused host with an account.
		sign_in,
		/// Connect through the account the installed GitHub CLI is signed in to.
		use_github_cli,
		/// Replace the refused account with another.
		use_another_account,
	};

	/// \brief The window's facts the wording needs beside the refusal itself.
	struct pull_request_refusal_context
	{
		/// The refused account's login; empty signed out or while unknown.
		std::string Login;
		/// This composition offers GitHub CLI as a way to sign in.
		bool GithubCliOffered = false;
		/// The refused account is the one GitHub CLI is signed in to.
		bool ThroughGithubCli = false;

		auto operator<=>(const pull_request_refusal_context&) const = default;
	};

	/// \brief A refusal as the reviewer reads it.
	///
	/// \details `Heading` names the pull request and what would open it;
	/// `Explanation` says why in terms of the connection. `Actions` is never
	/// empty, and its first entry is the step the heading names.
	struct pull_request_refusal_wording
	{
		std::string                              Heading;
		std::string                              Explanation;
		std::vector<pull_request_refusal_action> Actions;

		auto operator<=>(const pull_request_refusal_wording&) const = default;
	};

	/// \brief Decides the words and steps of a refused first read.
	///
	/// \details Only a permission refusal is worded here: it is the one whose
	/// remedy is a different connection. Every other reason keeps its
	/// diagnostic sentence and yields nothing.
	[[nodiscard]] auto word_pull_request_refusal(
		const pull_request_refusal&         aRefusal,
		const pull_request_refusal_context& aContext)
		-> std::optional<pull_request_refusal_wording>;

	/// \brief The verb on the control that takes a step.
	[[nodiscard]] auto label(pull_request_refusal_action aAction) noexcept
		-> std::string_view;

	/// \brief The sentence that says what taking a step does.
	[[nodiscard]] auto consequence(pull_request_refusal_action aAction) noexcept
		-> std::string_view;
}

#endif
