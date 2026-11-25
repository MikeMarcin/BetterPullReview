#include "bpr/app/profile_data_reset.hpp"

#include "bpr/adapters/git/agent_workspace_ownership.hpp"
#include "bpr/adapters/git/installed_git_process.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/adapters/sqlite/device_binding_store.hpp"
#include "bpr/adapters/sqlite/queue_enrollment_store.hpp"
#include "bpr/adapters/sqlite/release_maintenance_files.hpp"
#include "bpr/adapters/sqlite/remembered_connection_store.hpp"
#include "bpr/app/diagnostic_export_directory.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/application/agent_session_management.hpp"
#include "bpr/application/release_profile_cleanup.hpp"

#include <QDateTime>
#include <QString>

#include <algorithm>
#include <memory>
#include <optional>
#include <system_error>

namespace bpr::app {
	auto profile_data_reset_copy_name() -> std::string
	{
		return QStringLiteral("BetterPullReview reset %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH.mm.ss"))).toStdString();
	}

	namespace {
		using step = core::profile_data_reset_step;

		/// [[BPR-1858]]. A reset that signs out forgets every remembered
		/// connection, native or GitHub CLI, in the store it keeps; core keeps
		/// the identity high-water marks. gh's own sign-in is never asked about
		/// or changed. An absent or unreadable store holds nothing reusable, so
		/// only a failed save of rows it did read is reported.
		void forget_connections(const std::filesystem::path& aRoot, const core::bpr_profile_id aProfile, core::profile_data_reset_outcome& aOutcome, const std::stop_token aStop)
		{
			auto present = std::error_code{ };
			if (!std::filesystem::exists(aRoot / "state" / "v1" / "remembered-connections.sqlite3", present) || present) {
				return;
			}
			try {
				auto       store  = adapters::sqlite::make_remembered_connection_store(aRoot, aProfile);
				const auto loaded = store ? store->load(aStop) : std::unexpected(application::remembered_connection_store_failure::unavailable);
				if (!loaded || !core::holds_remembered_connection(*loaded)) {
					return;
				}
				const auto next              = core::forget_remembered_connections(*loaded, loaded->Revision);
				const auto saved             = next && store->save(*next, loaded->Revision, aStop).has_value();
				aOutcome.SignedOut           = aOutcome.SignedOut || saved;
				aOutcome.ConnectionsRetained = !saved;
			} catch (...) {
				aOutcome.ConnectionsRetained = true;
			}
		}

		/// [[BPR-NNNN]]. Agent sessions and workspaces: every retained
		/// workspace the agent setup store names is removed through its exact
		/// ownership record, then every conversation is forgotten. A profile
		/// with no agent setup store has none. The agents' own native session
		/// files are not BetterPullReview's and are never read or deleted.
		void clear_agent_sessions(const std::filesystem::path& aRoot, core::profile_data_reset_outcome& aOutcome, const std::stop_token aStop)
		{
			auto present = std::error_code{ };
			if (!std::filesystem::exists(aRoot / "state" / "v1" / "agent-setup.sqlite3", present) || present) {
				return;
			}
			auto report = application::agent_sessions_clear_report{ };
			try {
				const auto store = adapters::sqlite::make_native_agent_session_store(aRoot);
				auto       files = std::unique_ptr<application::retained_workspace_files_port>{ };
#if defined(BPR_AGENT_INTEGRATIONS) && (defined(Q_OS_UNIX) || defined(Q_OS_WIN))
				files = adapters::git::make_installed_retained_workspace_files(aRoot / "agent-workspace-ownership-v1", adapters::git::make_installed_workspace_owners());
#endif
				report = application::clear_agent_sessions(*store, files.get(), aStop);
			} catch (...) {
				report = { };
			}
			aOutcome.WorkspacesRemoved = static_cast<std::uint32_t>(report.Workspaces.Removed);
			aOutcome.WorkspacesLeft    = report.Workspaces.Unavailable ? 0U : static_cast<std::uint32_t>(report.Workspaces.Count);
			if (!report.Cleared) {
				aOutcome.Failed = aOutcome.Failed.with(core::profile_data_reset::agent_sessions);
			}
		}

		[[nodiscard]] auto runs(const core::profile_data_reset_plan& aPlan, const step aStep) -> bool
		{
			return std::ranges::find(aPlan.Steps, aStep) != aPlan.Steps.end();
		}
	}

	auto run_profile_data_reset(const std::filesystem::path& aRoot, std::shared_ptr<application::profile_runtime_lease> aLease,
								const core::bpr_profile_id aProfile, const core::profile_data_reset_request& aRequest,
								application::github_credential_vault_port* const aVault, const std::stop_token aStop) -> profile_data_reset_report
	{
		const auto plan       = core::plan_profile_data_reset(aRequest.Reset);
		auto       report     = profile_data_reset_report{ };
		report.Outcome.Reset  = plan.Selection;
		report.Outcome.Reason = core::admit_profile_data_reset(aRequest);
		if (report.Outcome.Reason != core::release_maintenance_reason::none) {
			return report;
		}
		if (aRequest.Copy) {
			try {
				const auto directory = resolve_diagnostic_export_directory(diagnostic_export_locations());
				if (!directory) {
					report.Outcome.Reason = directory.error();
					return report;
				}
				report.CopyFolder = native_filesystem_path(QString::fromStdString(*directory)) / native_filesystem_path(QString::fromStdString(profile_data_reset_copy_name()));
			} catch (...) {
				report.Outcome.Reason = core::release_maintenance_reason::io_failure;
				return report;
			}
		}
		// [[BPR-NNNN]]. First, while the store that names the workspaces is
		// still there.
		if (runs(plan, step::agent_sessions)) {
			clear_agent_sessions(aRoot, report.Outcome, aStop);
		}
		const auto agentRan = runs(plan, step::agent_sessions) && !report.Outcome.Failed.contains(core::profile_data_reset::agent_sessions);
		// A workspace left on disk keeps the store that records it.
		const auto heldBack   = core::profile_data_reset_held_back(report.Outcome.WorkspacesLeft);
		const auto files      = plan.Selection.without(heldBack);
		report.Outcome.Failed = report.Outcome.Failed.with(plan.Selection.without(files));
		if (core::profile_data_reset_removes_files(files)) {
			auto failure = std::optional<core::release_maintenance_reason>{ };
			try {
				// [[BPR-1858]]. A reset that signs out deletes this profile's
				// own vault entries, exactly those its device bindings name,
				// through the purge's journalled credential cleanup.
				const auto session     = core::application_session_id{ 1U };
				auto       credentials = std::unique_ptr<application::release_profile_credentials>{ };
				if (core::profile_data_reset_signs_out(files) && aVault != nullptr) {
					credentials              = std::make_unique<application::release_profile_credentials>(aProfile, session, [root = aRoot, aProfile] { return adapters::sqlite::make_device_binding_store(root, aProfile); }, *aVault);
					const auto held          = credentials->inspect(aStop);
					report.Outcome.SignedOut = held && held->Entries != 0U;
				}
				adapters::sqlite::release_maintenance_files maintenance{ aRoot, std::move(aLease), aProfile, session, credentials.get() };
				const auto                                  removed = maintenance.reset(files, report.CopyFolder, aStop);
				if (removed) {
					report.Outcome.Removed = *removed;
					report.Outcome.Copied  = report.CopyFolder.has_value();
				} else {
					failure = removed.error();
				}
			} catch (...) {
				failure = core::release_maintenance_reason::io_failure;
			}
			if (failure) {
				const auto reason        = *failure;
				report.Outcome.SignedOut = false;
				report.CopyFolder.reset();
				// Nothing of the files was removed. With the agent sessions
				// cleared already, the clear was partial; otherwise it did
				// nothing, and an unfinished one stops the profile's start.
				if (agentRan && reason != core::release_maintenance_reason::recovery_required) {
					report.Outcome.Failed = report.Outcome.Failed.with(files.without(core::profile_data_reset::agent_sessions));
					return report;
				}
				report.Outcome.Reason = reason;
				return report;
			}
		}
		if (core::profile_data_reset_restores_queue_display(files)) {
			try {
				auto       store                    = adapters::sqlite::make_queue_enrollment_store({ .StorageRoot = aRoot });
				const auto restored                 = store ? store->store_preferences(application::queue_presentation_preferences{ }, aStop) : std::unexpected(application::queue_store_failure::unavailable);
				report.Outcome.QueueDisplayRestored = restored.has_value();
			} catch (...) {
				report.Outcome.QueueDisplayRestored = false;
			}
		}
		if (core::profile_data_reset_signs_out(files)) {
			forget_connections(aRoot, aProfile, report.Outcome, aStop);
		}
		return report;
	}
}
