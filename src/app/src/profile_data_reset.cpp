#include "bpr/app/profile_data_reset.hpp"

#include "bpr/adapters/git/agent_workspace_ownership.hpp"
#include "bpr/adapters/git/installed_git_process.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/adapters/sqlite/device_binding_store.hpp"
#include "bpr/adapters/sqlite/queue_enrollment_store.hpp"
#include "bpr/adapters/sqlite/release_maintenance_files.hpp"
#include "bpr/adapters/sqlite/remembered_connection_store.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"
#include "bpr/app/diagnostic_export_directory.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/application/agent_setup_preferences.hpp"
#include "bpr/application/agent_workspace_expiry.hpp"
#include "bpr/application/clone_link_host.hpp"
#include "bpr/application/release_profile_cleanup.hpp"
#include "bpr/application/review_skill_host.hpp"

#include <QDateTime>
#include <QString>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <system_error>

namespace bpr::app {
#if defined(BPR_AGENT_INTEGRATIONS)
	// Defined with the agent composition (github_application/agent_composition.cpp).
	[[nodiscard]] auto save_review_skill_copy(const std::string& aText) -> std::optional<std::string>;
#endif

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

		/// [[BPR-NNNN]]. The retained workspaces' files, through their exact
		/// ownership records; none where agents are not built in.
		[[nodiscard]] auto retained_files(const std::filesystem::path& aRoot) -> std::unique_ptr<application::retained_workspace_files_port>
		{
#if defined(BPR_AGENT_INTEGRATIONS) && (defined(Q_OS_UNIX) || defined(Q_OS_WIN))
			return adapters::git::make_installed_retained_workspace_files(aRoot / "agent-workspace-ownership-v1", adapters::git::make_installed_workspace_owners());
#else
			static_cast<void>(aRoot);
			return { };
#endif
		}

		[[nodiscard]] auto store_present(const std::filesystem::path& aFile) -> bool
		{
			auto present = std::error_code{ };
			return std::filesystem::exists(aFile, present) && !present;
		}

		/// [[BPR-NNNN]]. Agent workspaces: every retained workspace the
		/// conversations name and no other process holds is removed through
		/// its exact ownership record; the conversations stay (their next turn
		/// makes a fresh one). The agents' own native session files are not
		/// BetterPullReview's and are never read or deleted.
		void remove_agent_workspaces(const std::filesystem::path& aRoot, core::profile_data_reset_outcome& aOutcome, const std::stop_token aStop)
		{
			if (!store_present(adapters::sqlite::agent_sessions_store_file(aRoot))) {
				return;
			}
			auto summary = application::retained_workspace_summary{ .Unavailable = true };
			try {
				const auto store = adapters::sqlite::make_native_agent_session_store(aRoot);
				const auto files = retained_files(aRoot);
				if (files) {
					summary = application::sweep_retained_workspaces(*store, *files, application::retained_workspace_sweep::remove_all, core::agent_workspace_expiry_policy{ }, 0, aStop);
				} else if (const auto listed = store->retained_workspaces(); listed) {
					summary = { .Count = listed->size(), .Failed = listed->size() };
				}
			} catch (...) {
				summary = { .Unavailable = true };
			}
			aOutcome.WorkspacesRemoved = static_cast<std::uint32_t>(summary.Removed);
			aOutcome.WorkspacesLeft    = summary.Unavailable ? 0U : static_cast<std::uint32_t>(summary.Count);
			if (summary.Unavailable || summary.Count != 0U) {
				aOutcome.Failed = aOutcome.Failed.with(core::profile_data_reset::agent_workspaces);
			}
		}

		/// [[BPR-NNNN]]. Agent setup, the former Reset agents: an edited review
		/// skill is saved to Downloads first, and which agent each pull
		/// request was last asked with is forgotten (a new agent may reuse an
		/// old one's id). Unless Settings removes the agent setup store
		/// (`aStoreGoes`), every agent and its settings, the clone folders and
		/// linked clones, the edited review skill and the old Claude wrapper
		/// and model go back to their defaults in it.
		void reset_agent_setup(const std::filesystem::path& aRoot, const bool aStoreGoes, core::profile_data_reset_outcome& aOutcome, const std::stop_token aStop)
		{
			auto done = true;
			try {
				// With Settings the whole store goes; its copy is Save a copy's.
				if (!aStoreGoes && store_present(adapters::sqlite::agent_setup_store_file(aRoot))) {
					const auto skills = adapters::sqlite::make_review_skill_store(aRoot);
					const auto skill  = skills->load_review_skill();
					if (skill && *skill) {
#if defined(BPR_AGENT_INTEGRATIONS)
						done = save_review_skill_copy((**skill).Text).has_value();
#endif
					}
					if (done) {
						auto fresh        = application::agent_setup_preferences{ };
						fresh.AgentsMoved = true;
						done              = adapters::sqlite::make_agent_setup_store(aRoot)->store(fresh);
						const auto links  = adapters::sqlite::make_clone_link_store(aRoot);
						if (const auto listed = links->load_links(); listed) {
							for (const auto& link : *listed) {
								done = links->forget_link(link.Host, link.Repository, link.Owner, link.Name) && done;
							}
						}
						done = links->store_clone_folders({ }) && done;
						done = (!skill || !*skill || skills->forget_review_skill()) && done;
						if (auto workbench = adapters::sqlite::make_workbench_defaults_store({ .StorageRoot = aRoot }, aStop); workbench) {
							if (auto preferences = (*workbench)->load_preferences(aStop); preferences && (!preferences->ClaudeWrapper.empty() || !preferences->ClaudeModel.empty())) {
								preferences->ClaudeWrapper.clear();
								preferences->ClaudeModel.clear();
								done = (*workbench)->store_preferences(*preferences, aStop).has_value() && done;
							}
						}
					}
				}
				if (store_present(adapters::sqlite::agent_sessions_store_file(aRoot))) {
					done = adapters::sqlite::make_agent_conversation_store(aRoot)->forget_agents() && done;
				}
			} catch (...) {
				done = false;
			}
			if (!done) {
				aOutcome.Failed = aOutcome.Failed.with(core::profile_data_reset::agent_setup);
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
		// [[BPR-NNNN]]. An earlier build's conversations move out of the agent
		// setup store first, so neither store's removal takes the other's.
		if (runs(plan, step::move_sessions) && store_present(adapters::sqlite::agent_setup_store_file(aRoot)) && !adapters::sqlite::prepare_agent_sessions_store(aRoot)) {
			report.Outcome.Reason = core::release_maintenance_reason::io_failure;
			return report;
		}
		// Then the workspaces, while the conversations that name them are there.
		if (runs(plan, step::agent_workspaces)) {
			remove_agent_workspaces(aRoot, report.Outcome, aStop);
		}
		if (runs(plan, step::agent_setup)) {
			reset_agent_setup(aRoot, plan.Selection.contains(core::profile_data_reset::settings), report.Outcome, aStop);
		}
		const auto agentRan = !report.Outcome.Failed.contains(core::profile_data_reset::agent_workspaces)
			&& (runs(plan, step::agent_workspaces) || (runs(plan, step::agent_setup) && !report.Outcome.Failed.contains(core::profile_data_reset::agent_setup)));
		// A workspace left on disk keeps the conversations that record it.
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
					report.Outcome.Failed = report.Outcome.Failed.with(files.without(core::profile_data_reset::agent_setup).without(core::profile_data_reset::agent_workspaces));
					return report;
				}
				report.Outcome.Reason = reason;
				return report;
			}
		}
		// [[BPR-NNNN]]. The chats' queued follow-ups go with the sessions they
		// were waiting for; with Settings the store that keeps them is gone.
		if (files.contains(core::profile_data_reset::agent_sessions) && !files.contains(core::profile_data_reset::settings) && !adapters::sqlite::clear_agent_queues(aRoot)) {
			report.Outcome.Failed = report.Outcome.Failed.with(core::profile_data_reset::agent_sessions);
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
