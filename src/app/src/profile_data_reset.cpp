#include "bpr/app/profile_data_reset.hpp"

#include "bpr/adapters/sqlite/queue_enrollment_store.hpp"
#include "bpr/adapters/sqlite/release_maintenance_files.hpp"
#include "bpr/adapters/sqlite/remembered_connection_store.hpp"
#include "bpr/app/diagnostic_export_directory.hpp"
#include "bpr/app/native_filesystem_path.hpp"

#include <QDateTime>
#include <QString>

#include <system_error>

namespace bpr::app {
	auto profile_data_reset_copy_name() -> std::string
	{
		return QStringLiteral("BetterPullReview reset %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH.mm.ss"))).toStdString();
	}

	namespace {
		/// [[BPR-1858]]. Reset everything leaves the profile not connected
		/// through gh: core forgets each CLI row, this saves the result. gh's
		/// own sign-in is never asked about or changed. A store that is absent
		/// or cannot be read holds no reusable CLI choice, so only a failed save
		/// of a CLI row it did read is reported.
		void forget_github_cli(const std::filesystem::path& aRoot, const core::bpr_profile_id aProfile, core::profile_data_reset_outcome& aOutcome, const std::stop_token aStop)
		{
			auto present = std::error_code{ };
			if (!std::filesystem::exists(aRoot / "state" / "v1" / "remembered-connections.sqlite3", present) || present) {
				return;
			}
			try {
				auto       store  = adapters::sqlite::make_remembered_connection_store(aRoot, aProfile);
				const auto loaded = store ? store->load(aStop) : std::unexpected(application::remembered_connection_store_failure::unavailable);
				if (!loaded || !core::holds_github_cli_connection(*loaded)) {
					return;
				}
				const auto next                = core::forget_github_cli_connections(*loaded, loaded->Revision);
				const auto saved               = next && store->save(*next, loaded->Revision, aStop).has_value();
				aOutcome.GithubCliDisconnected = saved;
				aOutcome.GithubCliRetained     = !saved;
			} catch (...) {
				aOutcome.GithubCliRetained = true;
			}
		}
	}

	auto run_profile_data_reset(const std::filesystem::path& aRoot, std::shared_ptr<application::profile_runtime_lease> aLease,
								const core::bpr_profile_id aProfile, const core::profile_data_reset_request& aRequest,
								const std::stop_token aStop) -> profile_data_reset_report
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
			adapters::sqlite::release_maintenance_files files{ aRoot, std::move(aLease), aProfile, core::application_session_id{ 1U } };
			const auto                                  removed = files.reset(aRequest.Reset, report.CopyFolder, aStop);
			if (!removed) {
				report.Outcome.Reason = removed.error();
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
		if (core::profile_data_reset_forgets_github_cli(aRequest.Reset)) {
			forget_github_cli(aRoot, aProfile, report.Outcome, aStop);
		}
		return report;
	}
}
