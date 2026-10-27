#include "bpr/application/agent_workspace.hpp"

#include "bpr/core/epoch_transition.hpp"

namespace bpr::application {
	namespace {
		[[nodiscard]] auto binding_matches_authority(
			const core::agent_revision_binding& aBinding,
			const core::pull_request_authority& aAuthority) noexcept -> bool
		{
			return aBinding.Host == aAuthority.Host
				&& aBinding.Repository == aAuthority.Repository
				&& aBinding.PullRequest == aAuthority.PullRequest;
		}
	} // namespace

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

	auto stable_name(const agent_workspace_unavailable_reason aReason) noexcept
		-> std::string_view
	{
		switch (aReason) {
		case agent_workspace_unavailable_reason::copy_failed:
			return "copy_failed";
		case agent_workspace_unavailable_reason::download_failed:
			return "download_failed";
		case agent_workspace_unavailable_reason::connection_failed:
			return "connection_failed";
		case agent_workspace_unavailable_reason::sign_in_refused:
			return "sign_in_refused";
		case agent_workspace_unavailable_reason::too_large:
			return "too_large";
		case agent_workspace_unavailable_reason::too_large_not_in_clone:
			return "too_large_not_in_clone";
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
		if (aExistingCheckoutChosen
			|| aProfile.Workspace == core::agent_workspace_policy::existing_checkout) {
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

	auto valid(const agent_workspace_materialization_request& aRequest) noexcept
		-> bool
	{
		return aRequest.ApplicationSession.value() != 0U
			&& core::valid(aRequest.Authority)
			&& aRequest.Session.Id.value() != 0U
			&& aRequest.Session.Profile.value() != 0U
			&& binding_matches_authority(
				   aRequest.Session.Binding, aRequest.Authority)
			&& aRequest.Session.Binding.HeadOid.valid()
			&& aRequest.Session.Binding.HeadOid.algorithm()
			== aRequest.Session.Binding.BaseOid.algorithm()
			&& aRequest.ParentDirectory.is_absolute()
			&& !aRequest.ParentDirectory.empty()
			&& (!aRequest.HistoryClone || (aRequest.HistoryClone->is_absolute() && !aRequest.HistoryClone->empty()))
			&& (!aRequest.HeadFromClone || aRequest.HistoryClone.has_value());
	}

	auto valid(const owned_agent_workspace& aWorkspace) noexcept -> bool
	{
		return aWorkspace.Workspace.Policy
			== core::agent_workspace_policy::disposable_review_workspace
			&& aWorkspace.Workspace.PinnedHead.has_value()
			&& aWorkspace.Workspace.PinnedHead->valid()
			&& !aWorkspace.Workspace.Path.empty()
			&& aWorkspace.ApplicationSession.value() != 0U
			&& core::valid(aWorkspace.Authority)
			&& aWorkspace.Session.value() != 0U
			&& aWorkspace.ParentDirectory.is_absolute()
			&& !aWorkspace.OwnershipMarker.empty()
			&& valid(aWorkspace.DirectoryIdentity);
	}
} // namespace bpr::application
