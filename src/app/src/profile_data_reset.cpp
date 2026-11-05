#include "bpr/app/profile_data_reset.hpp"

#include "bpr/adapters/sqlite/device_binding_store.hpp"
#include "bpr/adapters/sqlite/queue_enrollment_store.hpp"
#include "bpr/adapters/sqlite/release_maintenance_files.hpp"
#include "bpr/adapters/sqlite/remembered_connection_store.hpp"
#include "bpr/app/diagnostic_export_directory.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/application/release_profile_cleanup.hpp"

#include <QDateTime>
#include <QString>

#include <system_error>

namespace bpr::app {
	auto profile_data_reset_copy_name() -> std::string
	{
		return QStringLiteral("BetterPullReview reset %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH.mm.ss"))).toStdString();
	}

	namespace {
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
	}

	auto run_profile_data_reset(const std::filesystem::path& aRoot, std::shared_ptr<application::profile_runtime_lease> aLease,
								const core::bpr_profile_id aProfile, const core::profile_data_reset_request& aRequest,
								application::github_credential_vault_port* const aVault, const std::stop_token aStop) -> profile_data_reset_report
	{
		auto report           = profile_data_reset_report{ };
		report.Outcome.Reset  = aRequest.Reset;
		report.Outcome.Reason = core::admit_profile_data_reset(aRequest);
		if (report.Outcome.Reason != core::release_maintenance_reason::none) {
			return report;
		}
		try {
			if (aRequest.Copy) {
				const auto directory = resolve_diagnostic_export_directory(diagnostic_export_locations());
				if (!directory) {
					report.Outcome.Reason = directory.error();
					return report;
				}
				report.CopyFolder = native_filesystem_path(QString::fromStdString(*directory)) / native_filesystem_path(QString::fromStdString(profile_data_reset_copy_name()));
			}
			// [[BPR-1858]]. A reset that signs out deletes this profile's own
			// vault entries, exactly those its device bindings name, through
			// the purge's journalled credential cleanup. Without a vault it
			// refuses before removing anything if a binding exists.
			const auto session     = core::application_session_id{ 1U };
			auto       credentials = std::unique_ptr<application::release_profile_credentials>{ };
			if (core::profile_data_reset_signs_out(aRequest.Reset) && aVault != nullptr) {
				credentials              = std::make_unique<application::release_profile_credentials>(aProfile, session, [root = aRoot, aProfile] { return adapters::sqlite::make_device_binding_store(root, aProfile); }, *aVault);
				const auto held          = credentials->inspect(aStop);
				report.Outcome.SignedOut = held && held->Entries != 0U;
			}
			adapters::sqlite::release_maintenance_files files{ aRoot, std::move(aLease), aProfile, session, credentials.get() };
			const auto                                  removed = files.reset(aRequest.Reset, report.CopyFolder, aStop);
			if (!removed) {
				report.Outcome.Reason    = removed.error();
				report.Outcome.SignedOut = false;
				report.CopyFolder.reset();
				return report;
			}
			report.Outcome.Removed = *removed;
			report.Outcome.Copied  = report.CopyFolder.has_value();
		} catch (...) {
			report.Outcome.Reason = core::release_maintenance_reason::io_failure;
			report.CopyFolder.reset();
			return report;
		}
		if (core::profile_data_reset_restores_queue_display(aRequest.Reset)) {
			try {
				auto       store                    = adapters::sqlite::make_queue_enrollment_store({ .StorageRoot = aRoot });
				const auto restored                 = store ? store->store_preferences(application::queue_presentation_preferences{ }, aStop) : std::unexpected(application::queue_store_failure::unavailable);
				report.Outcome.QueueDisplayRestored = restored.has_value();
			} catch (...) {
				report.Outcome.QueueDisplayRestored = false;
			}
		}
		if (core::profile_data_reset_signs_out(aRequest.Reset)) {
			forget_connections(aRoot, aProfile, report.Outcome, aStop);
		}
		return report;
	}
}
