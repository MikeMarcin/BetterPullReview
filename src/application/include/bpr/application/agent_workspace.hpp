#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_WORKSPACE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_WORKSPACE_HPP

#include "bpr/core/agent_profile.hpp"
#include "bpr/core/agent_session.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::application {
	/// \brief Whether an existing checkout has uncommitted work in it.
	///
	/// \details Three states, because "not asked" is not "clean". An agent
	/// reasoning about a reviewer's uncommitted work should be *known* to be
	/// doing so, and a host that could not determine the state must say that
	/// rather than pick the reassuring answer.
	enum class checkout_cleanliness : std::uint8_t {
		unknown,
		clean,
		dirty,
	};

	[[nodiscard]] auto stable_name(checkout_cleanliness aCleanliness) noexcept
		-> std::string_view;

	/// \brief What a host reports about the reviewer's active worktree.
	struct checkout_observation
	{
		std::string          Path;
		checkout_cleanliness Cleanliness = checkout_cleanliness::unknown;

		auto operator<=>(const checkout_observation&) const = default;
	};

	/// \brief Why a workspace could not be prepared.
	enum class workspace_refusal : std::uint8_t {
		none,
		/// The profile asks for the reviewer's checkout and the reviewer did
		/// not choose it for this invocation.
		existing_checkout_not_chosen,
		/// No checkout was observed at all.
		no_checkout_available,
	};

	[[nodiscard]] auto stable_name(workspace_refusal aRefusal) noexcept
		-> std::string_view;

	/// \brief The workspace one agent invocation will run in.
	struct prepared_workspace
	{
		core::agent_workspace_policy Policy
			= core::agent_workspace_policy::disposable_review_workspace;
		/// The revision the workspace is materialised at. Always the session's
		/// own head for a disposable workspace; for an existing checkout it is
		/// what the reviewer happens to have.
		std::optional<core::git_commit_oid> PinnedHead;
		std::string                         Path;
		/// Meaningful only for an existing checkout. A disposable workspace is
		/// built from an immutable revision, so it has nothing uncommitted in
		/// it by construction and reports `clean`.
		checkout_cleanliness Cleanliness = checkout_cleanliness::clean;

		auto operator<=>(const prepared_workspace&) const = default;
	};

	/// \brief The verdict on preparing one workspace.
	struct workspace_preparation
	{
		std::optional<prepared_workspace> Workspace;
		workspace_refusal                 Refusal = workspace_refusal::none;

		auto operator<=>(const workspace_preparation&) const = default;
	};

	/// \brief Prepares the workspace one invocation runs in.
	///
	/// \details `aExistingCheckoutChosen` is the reviewer's explicit choice for
	/// **this invocation**, and a profile whose policy is `existing_checkout`
	/// is refused without it. That is the shape the card asks for: the
	/// dangerous mode cannot be reached by configuration alone, only by
	/// someone deciding it now.
	///
	/// A disposable workspace pins the session's head. It is materialised from
	/// an immutable revision, so nothing the reviewer does to the active
	/// worktree afterwards can change what the agent sees — that is why the
	/// pin is the session's `HeadOid` and never the checkout's.
	[[nodiscard]] auto prepare_agent_workspace(
		const core::agent_profile&                 aProfile,
		const core::agent_session&                 aSession,
		const std::optional<checkout_observation>& aCheckout,
		bool                                       aExistingCheckoutChosen,
		const std::string&                         aDisposableRoot) -> workspace_preparation;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_WORKSPACE_HPP
