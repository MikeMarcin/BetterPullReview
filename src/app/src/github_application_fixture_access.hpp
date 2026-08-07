#ifndef BPR_SRC_APP_SRC_GITHUB_APPLICATION_FIXTURE_ACCESS_HPP
#define BPR_SRC_APP_SRC_GITHUB_APPLICATION_FIXTURE_ACCESS_HPP

#if !defined(BPR_BUILD_TESTING)
	#error "The GitHub application fixture access is test-build only"
#endif

#include "bpr/app/github_application.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace bpr::app::detail {
	enum class github_public_path_fixture {
		github_com,
		ghes_3_18,
	};

	enum class github_epoch_fixture_phase {
		single,
		seed,
		reuse,
	};

	struct github_public_path_fixture_options
	{
		std::optional<std::filesystem::path> ApplicationDataRoot;
		github_epoch_fixture_phase           EpochPhase = github_epoch_fixture_phase::single;
	};

	struct github_public_path_fixture_evidence
	{
		bool        RecordingConsumed          = false;
		bool        CapabilityProbeConsumed    = false;
		bool        BootstrapTransportSelected = false;
		bool        AccountTransportSelected   = false;
		bool        CredentialStored           = false;
		bool        CredentialRetrieved        = false;
		bool        CredentialDeleted          = false;
		bool        SecretRetained             = false;
		int         RecordingFailure           = -1;
		int         AccountReadCount           = 0;
		int         LastReadOperation          = -1;
		int         SmartAcquireCount          = 0;
		int         SmartDiscoveryCount        = 0;
		int         SmartLsRefsCount           = 0;
		int         SmartFetchCount            = 0;
		std::string BaseOid;
		std::string HeadOneOid;
		std::string HeadTwoOid;
		std::string BaseBlobOid;
		std::string HeadOneBlobOid;
		std::string HeadTwoBlobOid;
	};

	struct github_epoch_maintenance_fixture_evidence
	{
		std::size_t  PruneIntentDispatchCount          = 0U;
		std::size_t  DeleteIntentDispatchCount         = 0U;
		std::size_t  CancelIntentDispatchCount         = 0U;
		std::int64_t CatalogRevision                   = 0;
		bool         TargetBaseRefPresent              = false;
		bool         TargetHeadRefPresent              = false;
		bool         NonTargetBaseRefPresent           = false;
		bool         NonTargetHeadRefPresent           = false;
		bool         RepositoryPresent                 = false;
		bool         CacheDeletionMarkerPresent        = false;
		bool         TargetBaseCacheDeleted            = false;
		bool         TargetHeadCacheDeleted            = false;
		bool         RetiredRepositoryPresent          = false;
		bool         RetiredCacheDeletionMarkerPresent = false;
		int          TargetBaseAvailability            = -1;
		int          TargetHeadAvailability            = -1;
	};

	class github_application_fixture_access final
	{
	public:
		[[nodiscard]] static auto make(
			github_public_path_fixture         aFixture,
			github_public_path_fixture_options aOptions = { })
			-> std::unique_ptr<github_application>;
		[[nodiscard]] static auto evidence(
			const github_application& aApplication) noexcept
			-> github_public_path_fixture_evidence;
		[[nodiscard]] static auto maintenance_evidence(
			const github_application& aApplication,
			core::review_epoch_id     aTarget) noexcept
			-> github_epoch_maintenance_fixture_evidence;
		/// \brief Requests the deterministic fixture file through the real host/cache path.
		[[nodiscard]] static auto request_fixture_diff(
			github_application& aApplication,
			bool                aUseSecondHead = false) -> bool;
		/// \brief Replays an accepted metadata-only or changed-head fixture handoff.
		[[nodiscard]] static auto reaccept_fixture_revision(
			github_application& aApplication,
			bool                aChangeHead) -> bool;
		/// \brief Reports what the composed review ledger already holds.
		///
		/// \details Read before anything is written, so a second run against the
		/// same application data root can tell persistence from a fresh write.
		[[nodiscard]] static auto review_ledger_receipt_present(
			github_application& aApplication) -> bool;
		/// \brief Selects a different appearance mode through the composed host.
		///
		/// \details Exists so a probe can measure what a theme change costs.
		/// The token-span cache is keyed by blob identity and definition
		/// version alone, so this must repaint and must not reparse; the
		/// submission count in the application status is what says which
		/// happened.
		///
		/// \returns Whether the composed host applied a different mode.
		[[nodiscard]] static auto select_other_appearance_mode(
			github_application& aApplication) -> bool;
		/// \brief Commits a receipt through the composed ledger and reads it back.
		///
		/// \details The read goes through the same store the application
		/// composed, so it reaches the database file the application actually
		/// opened rather than one the probe chose.
		[[nodiscard]] static auto review_ledger_round_trip(
			github_application& aApplication) -> bool;
	};
}

#endif // BPR_SRC_APP_SRC_GITHUB_APPLICATION_FIXTURE_ACCESS_HPP
