#include "bpr/app/profile_data_reset.hpp"

#include "bpr/adapters/sqlite/queue_enrollment_store.hpp"
#include "bpr/adapters/sqlite/release_maintenance_files.hpp"
#include "bpr/app/diagnostic_export_directory.hpp"
#include "bpr/app/native_filesystem_path.hpp"

#include <QDateTime>
#include <QString>

namespace bpr::app {
	auto profile_data_reset_copy_name() -> std::string
	{
		return QStringLiteral("BetterPullReview reset %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH.mm.ss"))).toStdString();
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
		return report;
	}
}
