#include "bpr/application/agent_workspace_expiry.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

namespace bpr::application {
	namespace {
		using lease_set = std::vector<std::unique_ptr<agent_workspace_turn_lease>>;

		/// Every lease that guards \p aRecord, or none when a turn holds any.
		[[nodiscard]] auto take_leases(native_agent_session_store_port& aStore, const retained_workspace_record& aRecord) -> std::optional<lease_set>
		{
			auto leases = lease_set{ };
			auto path   = aStore.try_lock_workspace(agent_workspace_path_lease(aRecord.Workspace.Workspace.Path));
			if (!path) {
				return std::nullopt;
			}
			leases.push_back(std::move(path));
			for (const auto& key : aRecord.Keys) {
				auto lease = aStore.try_lock_workspace(key);
				if (!lease) {
					return std::nullopt;
				}
				leases.push_back(std::move(lease));
			}
			return leases;
		}

		struct observed_workspace
		{
			retained_workspace_record     Record;
			core::retained_workspace_fact Fact;
		};

		/// Measures each record; one whose folder is gone is cleared from the
		/// catalog and left out.
		[[nodiscard]] auto observe(native_agent_session_store_port& aStore, retained_workspace_files_port& aFiles, std::vector<retained_workspace_record> aRecords, const std::stop_token& aStop)
			-> std::vector<observed_workspace>
		{
			auto observed = std::vector<observed_workspace>{ };
			for (auto& record : aRecords) {
				if (aStop.stop_requested()) {
					break;
				}
				const auto measured = aFiles.measure(record.Workspace, aStop);
				if (!measured.Present) {
					if (auto leases = take_leases(aStore, record); leases) {
						static_cast<void>(aStore.forget_workspace(record.Workspace));
					}
					continue;
				}
				auto fact           = core::retained_workspace_fact{ };
				fact.Bytes          = measured.Bytes;
				fact.LastUseSeconds = std::max(record.LastUse.value_or(0), measured.NewestSeconds);
				fact.InUse          = !take_leases(aStore, record).has_value();
				fact.Ended          = record.Ended;
				observed.push_back({ .Record = std::move(record), .Fact = fact });
			}
			return observed;
		}
	} // namespace

	auto sweep_retained_workspaces(native_agent_session_store_port& aStore, retained_workspace_files_port& aFiles, const retained_workspace_sweep aSweep,
								   const core::agent_workspace_expiry_policy& aPolicy, const std::int64_t aNowSeconds, const std::stop_token aStop)
		-> retained_workspace_summary
	{
		auto summary = retained_workspace_summary{ };
		auto records = aStore.retained_workspaces();
		if (!records) {
			summary.Unavailable = true;
			return summary;
		}
		const auto observed = observe(aStore, aFiles, std::move(*records), aStop);
		auto       facts    = std::vector<core::retained_workspace_fact>{ };
		facts.reserve(observed.size());
		for (const auto& item : observed) {
			facts.push_back(item.Fact);
			summary.Bytes += item.Fact.Bytes;
			summary.InUse += item.Fact.InUse ? 1U : 0U;
		}
		summary.Count     = observed.size();
		const auto chosen = aSweep == retained_workspace_sweep::policy ? core::choose_expired_workspaces(facts, aPolicy, aNowSeconds)
			: aSweep == retained_workspace_sweep::remove_all           ? core::choose_all_idle_workspaces(facts)
																	   : std::vector<core::workspace_expiry_choice>{ };
		for (const auto& choice : chosen) {
			if (aStop.stop_requested()) {
				break;
			}
			const auto& item   = observed[choice.Index];
			const auto  leases = take_leases(aStore, item.Record);
			if (!leases) {
				++summary.InUse;
				continue;
			}
			const auto removal = aFiles.remove(item.Record.Workspace, aStop);
			if (removal == retained_workspace_removal::in_use) {
				++summary.InUse;
				continue;
			}
			if (removal == retained_workspace_removal::refused) {
				++summary.Failed;
				continue;
			}
			// The folder is gone either way. A catalog that could not be
			// written still names it; the next sweep finds it gone and clears it.
			if (!aStore.forget_workspace(item.Record.Workspace)) {
				++summary.Failed;
			}
			++summary.Removed;
			summary.RemovedBytes += item.Fact.Bytes;
			--summary.Count;
			summary.Bytes -= item.Fact.Bytes;
		}
		return summary;
	}
} // namespace bpr::application
