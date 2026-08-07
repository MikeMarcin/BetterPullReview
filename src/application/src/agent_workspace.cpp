#include "bpr/application/agent_workspace.hpp"

namespace bpr::application {
	auto stable_name(const checkout_cleanliness aCleanliness) noexcept
		-> std::string_view
	{
		switch (aCleanliness) {
		case checkout_cleanliness::unknown:
			return "unknown";
		case checkout_cleanliness::clean:
			return "clean";
		case checkout_cleanliness::dirty:
			return "dirty";
		}
		return { };
	}

	auto stable_name(const workspace_refusal aRefusal) noexcept
		-> std::string_view
	{
		switch (aRefusal) {
		case workspace_refusal::none:
			return "none";
		case workspace_refusal::existing_checkout_not_chosen:
			return "existing_checkout_not_chosen";
		case workspace_refusal::no_checkout_available:
			return "no_checkout_available";
		}
		return { };
	}

	auto prepare_agent_workspace(
		const core::agent_profile&                 aProfile,
		const core::agent_session&                 aSession,
		const std::optional<checkout_observation>& aCheckout,
		const bool                                 aExistingCheckoutChosen,
		const std::string&                         aDisposableRoot)
		-> workspace_preparation
	{
		if (aProfile.Workspace == core::agent_workspace_policy::existing_checkout) {
			// A profile cannot opt itself into the reviewer's uncommitted work.
			// The policy says the mode is available; the choice says it is
			// wanted now. Without the second, this is refused.
			if (!aExistingCheckoutChosen) {
				return {
					.Refusal = workspace_refusal::existing_checkout_not_chosen,
				};
			}
			if (!aCheckout.has_value()) {
				return { .Refusal = workspace_refusal::no_checkout_available };
			}
			return {
				.Workspace = prepared_workspace{
					.Policy     = core::agent_workspace_policy::existing_checkout,
					.PinnedHead = std::nullopt,
					.Path       = aCheckout->Path,
					// Reported, never assumed. `unknown` travels as `unknown`:
					// a host that could not determine the state must not pick
					// the reassuring answer on the reviewer's behalf.
					.Cleanliness = aCheckout->Cleanliness,
				},
			};
		}

		// The session's head, not the checkout's. A disposable workspace is
		// materialised from an immutable revision, which is what makes it
		// unaffected by anything the reviewer does to the active worktree
		// afterwards.
		return {
			.Workspace = prepared_workspace{
				.Policy      = core::agent_workspace_policy::disposable_review_workspace,
				.PinnedHead  = aSession.Binding.HeadOid,
				.Path        = aDisposableRoot,
				.Cleanliness = checkout_cleanliness::clean,
			},
		};
	}
} // namespace bpr::application
