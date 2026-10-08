#ifndef BPR_ADAPTERS_SQLITE_RELEASE_MAINTENANCE_FILES_HPP
#define BPR_ADAPTERS_SQLITE_RELEASE_MAINTENANCE_FILES_HPP

#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/profile_runtime_lease.hpp"
#include "bpr/application/release_profile_cleanup.hpp"
#include <filesystem>

namespace bpr::adapters::sqlite {
	/// \brief Native private-root maintenance under the already admitted runtime lease.
	/// \details Construction/readiness and all methods except identity queries are effect-lane
	/// operations. POSIX and Windows reuse native private-storage admission; unsupported
	/// filesystems or unsafe native capabilities refuse admission.
	/// Quiesce producers before destructive preparation. An unresolved workspace must first
	/// recover through its existing Git owner. Shared registry/updater/sibling roots survive.
	class release_maintenance_files final : public application::release_maintenance_storage_port
	{
	public:
		release_maintenance_files(std::filesystem::path aRoot, std::shared_ptr<application::profile_runtime_lease> aLease,
								  core::bpr_profile_id aProfile, core::application_session_id aSession,
								  application::release_profile_cleanup_port*            aCredentials = nullptr,
								  std::shared_ptr<application::diagnostic_control_port> aDiagnostics = { });
		~release_maintenance_files() override;
		auto authority() const noexcept -> core::release_maintenance_authority override;
		auto capabilities() const noexcept -> core::release_maintenance_capabilities override;
		auto prepare(const core::release_maintenance_context& aContext, std::stop_token aStop)
			-> std::expected<std::shared_ptr<const application::release_maintenance_preparation>, core::release_maintenance_reason> override;
		auto revalidate(const core::release_maintenance_context&            aContext,
						const application::release_maintenance_preparation& aPreparation) -> application::release_maintenance_result override;
		auto erase(const core::release_maintenance_context&            aContext,
				   const application::release_maintenance_preparation& aPreparation, std::stop_token aStop,
				   const std::function<bool()>& aBeginCommit) -> application::release_maintenance_result override;
		/// \brief Replays only a durable exact-root inventory; changed/unknown objects remain untouched.
		/// \details Run after producer/store quiescence and before admitting a new maintenance host.
		auto recover() -> application::release_maintenance_result;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};

	/// \brief Observes a writable local output directory without creating or changing anything.
	/// \details Reuses the output parent admission. This is a current observation, not a write grant.
	[[nodiscard]] auto admit_release_diagnostic_directory(const std::filesystem::path& aPath) -> application::release_maintenance_result;

	/// \brief Admits a new local file beneath a retained native owner-controlled parent.
	/// \details Refuses existing destinations and links. Commit is an exclusive atomic link of
	/// a fully written private temporary file (exclusive handle rename on Windows);
	/// no overwrite, credential access or upload exists.
	[[nodiscard]] auto admit_release_diagnostic_output(const std::filesystem::path& aPath)
		-> std::expected<std::shared_ptr<application::release_diagnostic_output>, core::release_maintenance_reason>;
}
#endif
