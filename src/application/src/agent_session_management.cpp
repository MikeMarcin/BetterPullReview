#include "bpr/application/agent_session_management.hpp"

#include "bpr/core/agent_session_management.hpp"

#include <algorithm>
#include <memory>
#include <utility>

namespace bpr::application {
	auto rename_agent_session(native_agent_session_store_port& aStore, const std::string& aScope, const std::string& aId, const std::string_view aTyped)
		-> agent_session_edit_receipt
	{
		const auto name = core::tidy_agent_session_name(aTyped);
		if (!name) {
			return { .Result = name.error() == core::agent_session_name_refusal::too_long ? agent_session_edit_result::name_too_long : agent_session_edit_result::name_not_text };
		}
		const auto found = aStore.removal(aScope, aId);
		if (!found) {
			return { .Result = agent_session_edit_result::unavailable };
		}
		if (!found->Found) {
			return { .Result = agent_session_edit_result::not_found };
		}
		return { .Result = aStore.rename(aScope, aId, *name) ? agent_session_edit_result::done : agent_session_edit_result::unavailable };
	}

	auto delete_agent_session(native_agent_session_store_port& aStore, retained_workspace_files_port* const aFiles, const std::string& aScope, const std::string& aId,
							  const std::vector<std::string>& aLeaseKeys, const bool aTurnActive, const std::stop_token aStop) -> agent_session_edit_receipt
	{
		const auto found = aStore.removal(aScope, aId);
		if (!found) {
			return { .Result = agent_session_edit_result::unavailable };
		}
		// Every lease a turn on this session would hold, taken for the whole
		// deletion so no turn starts on it meanwhile.
		auto leases = std::vector<std::unique_ptr<agent_workspace_turn_lease>>{ };
		auto held   = false;
		auto keys   = aLeaseKeys;
		keys.insert(keys.end(), found->Keys.begin(), found->Keys.end());
		if (found->Workspace) {
			keys.push_back(agent_workspace_path_lease(found->Workspace->Workspace.Path));
		}
		// A key taken twice would refuse itself.
		std::ranges::sort(keys);
		keys.erase(std::ranges::unique(keys).begin(), keys.end());
		for (const auto& key : keys) {
			if (key.empty()) {
				continue;
			}
			auto lease = aStore.try_lock_workspace(key);
			if (!lease) {
				held = true;
				break;
			}
			leases.push_back(std::move(lease));
		}
		const auto plan = core::plan_agent_session_deletion({
			.Found          = found->Found,
			.Running        = aTurnActive || held,
			.HoldsWorkspace = found->Workspace.has_value(),
			.OtherHolders   = found->OtherHolders,
		});
		if (plan.Refusal == core::agent_session_deletion_refusal::not_found) {
			return { .Result = agent_session_edit_result::not_found };
		}
		if (plan.Refusal == core::agent_session_deletion_refusal::running) {
			return { .Result = agent_session_edit_result::running };
		}
		auto receipt = agent_session_edit_receipt{ .Result = agent_session_edit_result::done, .WorkspaceShared = plan.WorkspaceShared };
		if (plan.RemoveWorkspace) {
			const auto removal = aFiles == nullptr ? retained_workspace_removal::refused : aFiles->remove(*found->Workspace, aStop);
			if (removal == retained_workspace_removal::in_use) {
				return { .Result = agent_session_edit_result::running };
			}
			if (removal == retained_workspace_removal::refused) {
				return { .Result = agent_session_edit_result::workspace_refused };
			}
			// Gone either way; a catalog that could not be written still
			// names it, and the next sweep finds it gone and clears it.
			static_cast<void>(aStore.forget_workspace(*found->Workspace));
			receipt.WorkspaceRemoved = removal == retained_workspace_removal::removed;
		}
		if (!aStore.erase(aScope, aId)) {
			receipt.Result = agent_session_edit_result::unavailable;
		}
		return receipt;
	}

	auto clear_agent_sessions(native_agent_session_store_port& aStore, retained_workspace_files_port* const aFiles, const std::stop_token aStop) -> agent_sessions_clear_report
	{
		auto report = agent_sessions_clear_report{ };
		auto listed = aStore.retained_workspaces();
		if (!listed) {
			report.Workspaces.Unavailable = true;
			return report;
		}
		if (!listed->empty()) {
			if (aFiles == nullptr) {
				report.Workspaces.Count  = listed->size();
				report.Workspaces.Failed = listed->size();
				return report;
			}
			report.Workspaces = sweep_retained_workspaces(aStore, *aFiles, retained_workspace_sweep::remove_all, core::agent_workspace_expiry_policy{ }, 0, aStop);
			if (report.Workspaces.Unavailable || report.Workspaces.Count != 0U || report.Workspaces.Failed != 0U) {
				return report;
			}
		}
		report.Cleared = aStore.erase_all();
		return report;
	}
}
